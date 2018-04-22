#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <StreamString.h>
#include <base64.h>
#include "HttpClientH.h"

#ifdef DEBUG
#define ECHO(m) Serial.println(m)
#else
#define ECHO(m)
#endif

class TransportTrait
{
public:
    virtual ~TransportTrait()
    {
    }

    virtual std::unique_ptr<WiFiClient> create()
    {
        return std::unique_ptr<WiFiClient>(new WiFiClient());
    }

    virtual bool verify(WiFiClient& client, const char* host)
    {
        (void)client;
        (void)host;
        return true;
    }
};

class TLSTrait : public TransportTrait
{
public:
    TLSTrait(const String& fingerprint) :
        _fingerprint(fingerprint)
    {
    }

    std::unique_ptr<WiFiClient> create() override
    {
        return std::unique_ptr<WiFiClient>(new WiFiClientSecure());
    }

    bool verify(WiFiClient& client, const char* host) override
    {
        auto wcs = static_cast<WiFiClientSecure&>(client);
        return wcs.verify(_fingerprint.c_str(), host);
    }

protected:
    String _fingerprint;
};

/**
 * constructor
 */
HttpClientH::HttpClientH()
{
}

/**
 * destructor
 */
HttpClientH::~HttpClientH()
{
    if(_tcp) {
        _tcp->stop();
    }
    // if(_responseHeaders) {
    //     delete[] _responseHeaders;
    // }
}

bool HttpClientH::begin(String host) {
    return begin(host, (uint16_t) 80);
}

bool HttpClientH::begin(String host, uint16_t port) {
    _host = host;
    _port = port;
    _transportTrait.reset(nullptr);
    _transportTrait = (TransportTraitPtr) new TransportTrait();
    clear();
    return true;
}

bool HttpClientH::begin(String host, uint16_t port, String httpsFingerprint) {
    _host = host;
    _port = port;
    _transportTrait.reset(nullptr);
    if (httpsFingerprint.length() == 0) {
        return false;
    }

    _transportTrait = (TransportTraitPtr) new TLSTrait(httpsFingerprint);
    clear();
    return true;
}

bool HttpClientH::begin(String host, String httpsFingerprint) {
    return begin(host, (uint16_t) 443, httpsFingerprint);
}

void HttpClientH::clear()
{
    _responseCode = 0;
    _size = -1;
    _headers = "";
    _cookies = "";
    _canReuse = false;
    end();
}

bool HttpClientH::connected()
{
    if(_tcp) {
        return (_tcp->connected() || (_tcp->available() > 0));
    }
    return false;
}

bool HttpClientH::connect() {
    if(connected()) {
        ECHO("[HttpClientH] connect. already connected, try reuse!");
        while(_tcp->available() > 0) {
            _tcp->read();
        }
        return true;
    }

    if (!_transportTrait) {
        ECHO("[HttpClientH] connect: HTTPClientH::begin was not called or returned error");
        return false;
    }

    _tcp = _transportTrait->create();
    _tcp->setTimeout(_tcpTimeout);

    ECHO(String("[HttpClientH] Begin connect to: ") + _host + String(":") + String(_port));

    if(!_tcp->connect(_host.c_str(), _port)) {
        ECHO("[HttpClientH] failed connect");
        return false;
    }

    ECHO("[HttpClientH] connected");

    if (!_transportTrait->verify(*_tcp, _host.c_str())) {
        ECHO("[HttpClientH] transport level verify failed");
        _tcp->stop();
        return false;
    }


#ifdef ESP8266
    _tcp->setNoDelay(true);
#endif
    return connected();
}

int HttpClientH::sendRequest(String path, const char * method, String payload) {
    size_t payloadSize = payload.length();
    if(!connect()) {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    ECHO("[HttpClientH][sendRequest] Begin send request");
    // send Header
    if(!sendHeader(path, method, payloadSize)) {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    // send Payload if needed
    if(payloadSize > 0) {
        ECHO(payload);
        if(_tcp->write((const uint8_t *) payload.c_str(), payloadSize) != payloadSize) {
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        }
    }

    // handle Server Response (Header)
    return returnError(handleHeaderResponse());
}

bool HttpClientH::sendHeader(String path, const char * method, size_t payloadSize) {
    String header = "";
    header += String(method);
    header += F(" ");
    header += path;
    header += F(" HTTP/1.1");
    header += F("\r\nHost: ");
    header += _host;
    header += F("\r\nCookie: ");
    header += _cookies;
    header += F("\r\nAccept: application/json");
    header += F("\r\nContent-Type: application/json");
    // header += F("\r\nX-CSRF-TOKEN: ");
    // header += _token;
    header += F("\r\nConnection: ");
    if(_reuse) {
        header += F("keep-alive");
    } else {
        header += F("close");
    }
    if (payloadSize > 0) {
        header += F("\r\nContent-Length: ");
        header += String(payloadSize);
    }

    if (_headers.length() > 0) {
        header += "\r\n";
        header += _headers;
        header += "\r\n";
    } else {
        header += "\r\n\r\n";
    }

    if(!connected()) {
        return HTTPC_ERROR_NOT_CONNECTED;
    }

    ECHO(header);
    return (_tcp->write((const uint8_t *) header.c_str(), header.length()) == header.length());
}

/**
 * adds Header to the request
 * @param name
 * @param value
 */
void HttpClientH::addHeader(const String& name, const String& value)
{
    // not allow set of Header handled by code
    if(!name.equalsIgnoreCase(F("Connection")) &&
       !name.equalsIgnoreCase(F("User-Agent")) &&
       !name.equalsIgnoreCase(F("Host")) &&
       !(name.equalsIgnoreCase(F("Authorization")))
    ){

        String headerLine = name;
        headerLine += ": ";

        int headerStart = _headers.indexOf(headerLine);
        if (headerStart != -1) {
            int headerEnd = _headers.indexOf('\n', headerStart);
            _headers = _headers.substring(0, headerStart) + _headers.substring(headerEnd + 1);
        }

        headerLine += value;
        headerLine += "\r\n";
        _headers = headerLine + _headers;
    }
}

int HttpClientH::handleHeaderResponse() {
    if(!connected()) {
        return HTTPC_ERROR_NOT_CONNECTED;
    }

    String transferEncoding;
    _responseCode = -1;
    _size = -1;
    _transferEncoding = HTTPC_TE_IDENTITY;
    unsigned long lastDataTime = millis();

    while(connected()) {
        size_t len = _tcp->available();
        if(len > 0) {
            String headerLine = _tcp->readStringUntil('\n');
            headerLine.trim(); // remove \r

            lastDataTime = millis();

            ECHO(String("[HttpClient][handleHeaderResponse] RX: ") + String(headerLine));

            if(headerLine.startsWith("HTTP/1.")) {
                _responseCode = headerLine.substring(9, headerLine.indexOf(' ', 9)).toInt();
            } else if(headerLine.indexOf(':') > 0) {
                String headerName = headerLine.substring(0, headerLine.indexOf(':'));
                String headerValue = headerLine.substring(headerLine.indexOf(':') + 1);
                headerValue.trim();

                if(headerName.equalsIgnoreCase("Content-Length")) {
                    _size = headerValue.toInt();
                }

                if(headerName.equalsIgnoreCase("Connection")) {
                    _canReuse = headerValue.equalsIgnoreCase("keep-alive");
                }

                if(headerName.equalsIgnoreCase("Set-Cookie")) {
                    setCookie(headerValue);
                }

                if(headerName.equalsIgnoreCase("Transfer-Encoding")) {
                    transferEncoding = headerValue;
                }

                // for(size_t i = 0; i < _responseHeadersCount; i++) {
                //     if(_responseHeaders[i].key.equalsIgnoreCase(headerName)) {
                //         _responseHeaders[i].value = headerValue;
                //         break;
                //     }
                // }
            }

            if(headerLine == "") {
                ECHO(String("[HttpClientH][handleHeaderResponse] code: ") + String(_responseCode));

                if(_size > 0) {
                    ECHO(String("[HttpClientH][handleHeaderResponse] size: ") + String(_size));
                }

                if(transferEncoding.length() > 0) {
                    ECHO(String("[HttpClientH][handleHeaderResponse] Transfer-Encoding: ") + String(transferEncoding.c_str()));
                    if(transferEncoding.equalsIgnoreCase("chunked")) {
                        _transferEncoding = HTTPC_TE_CHUNKED;
                    } else {
                        return HTTPC_ERROR_ENCODING;
                    }
                } else {
                    _transferEncoding = HTTPC_TE_IDENTITY;
                }

                if(_responseCode) {
                    return _responseCode;
                } else {
                    ECHO("[HttpClientH][handleHeaderResponse] Remote host is not an HTTP Server!");
                    return HTTPC_ERROR_NO_HTTP_SERVER;
                }
            }

        } else {
            if((millis() - lastDataTime) > _tcpTimeout) {
                return HTTPC_ERROR_READ_TIMEOUT;
            }
            delay(0);
        }
    }

    return HTTPC_ERROR_CONNECTION_LOST;
}

void HttpClientH::setCookie(String cookie) {
    int p = cookie.indexOf('=');
    int q = cookie.indexOf(';');
    if(p < 1 || q < 1 || p > q) {
        return;
    }

    String cookieName = cookie.substring(0, p);
    String cookieValue = cookie.substring(p + 1, q);
    bool cookieNotExist = true;

    cookieName += "=";

    int cookieStart = _cookies.indexOf(cookieName);
    if (cookieStart != -1) {
        int cookieEnd = _cookies.indexOf(';', cookieStart);
        if (cookieEnd == -1) {
            _cookies = _cookies.substring(0, cookieStart);
        } else {
            _cookies = _cookies.substring(0, cookieStart) + _cookies.substring(cookieEnd + 1);
        }
    }

    _cookies += cookieName;
    _cookies += cookieValue;
    _cookies += ";";

    ECHO(_cookies);
    printFreeHeap();
}

String HttpClientH::getResponseString()
{
    StreamString sstring;

    if(_size) {
        // try to reserve needed memmory
        if(!sstring.reserve((_size + 1))) {
            ECHO(String("[HttpClientH][getString] not enough memory to reserve a string! need: ") + String(_size + 1));
            return "";
        }
    }

    writeToStream(&sstring);
    return sstring;
}

/**
 * write all  message body / payload to Stream
 * @param stream Stream *
 * @return bytes written ( negative values are error codes )
 */
int HttpClientH::writeToStream(Stream * stream)
{

    if(!stream) {
        return returnError(HTTPC_ERROR_NO_STREAM);
    }

    if(!connected()) {
        return returnError(HTTPC_ERROR_NOT_CONNECTED);
    }

    // get length of document (is -1 when Server sends no Content-Length header)
    int len = _size;
    int ret = 0;

    if(_transferEncoding == HTTPC_TE_IDENTITY) {
        ret = writeToStreamDataBlock(stream, len);

        // have we an error?
        if(ret < 0) {
            return returnError(ret);
        }
    } else if(_transferEncoding == HTTPC_TE_CHUNKED) {
        int size = 0;
        while(1) {
            if(!connected()) {
                return returnError(HTTPC_ERROR_CONNECTION_LOST);
            }
            String chunkHeader = _tcp->readStringUntil('\n');

            if(chunkHeader.length() <= 0) {
                return returnError(HTTPC_ERROR_READ_TIMEOUT);
            }

            chunkHeader.trim(); // remove \r

            // read size of chunk
            len = (uint32_t) strtol((const char *) chunkHeader.c_str(), NULL, 16);
            size += len;
            ECHO(String("[HttpClientH] read chunk len: ") + String(len));

            // data left?
            if(len > 0) {
                int r = writeToStreamDataBlock(stream, len);
                if(r < 0) {
                    // error in writeToStreamDataBlock
                    return returnError(r);
                }
                ret += r;
            } else {

                // if no length Header use global chunk size
                if(_size <= 0) {
                    _size = size;
                }

                // check if we have write all data out
                if(ret != _size) {
                    return returnError(HTTPC_ERROR_STREAM_WRITE);
                }
                break;
            }

            // read trailing \r\n at the end of the chunk
            char buf[2];
            auto trailing_seq_len = _tcp->readBytes((uint8_t*)buf, 2);
            if (trailing_seq_len != 2 || buf[0] != '\r' || buf[1] != '\n') {
                return returnError(HTTPC_ERROR_READ_TIMEOUT);
            }

            delay(0);
        }
    } else {
        return returnError(HTTPC_ERROR_ENCODING);
    }

    end();
    return ret;
}

/**
 * end
 * called after the payload is handled
 */
void HttpClientH::end()
{
    if(connected()) {
        if(_tcp->available() > 0) {
            ECHO(String("[HttpClientH][end] still data in buffer (") + String(_tcp->available()) + String("), clean up."));
            while(_tcp->available() > 0) {
                _tcp->read();
            }
        }
        if(_reuse && _canReuse) {
            ECHO("[HttpClientH][end] tcp keep open for reuse");
        } else {
            ECHO("[HttpClientH][end] tcp stop");
            _tcp->stop();
        }
    } else {
        ECHO("[HttpClientH][end] tcp is closed");
    }
}

/**
 * write one Data Block to Stream
 * @param stream Stream *
 * @param size int
 * @return < 0 = error >= 0 = size written
 */
int HttpClientH::writeToStreamDataBlock(Stream * stream, int size)
{
    int buff_size = HTTP_TCP_BUFFER_SIZE;
    int len = size;
    int bytesWritten = 0;

    // if possible create smaller buffer then HTTP_TCP_BUFFER_SIZE
    if((len > 0) && (len < HTTP_TCP_BUFFER_SIZE)) {
        buff_size = len;
    }

    // create buffer for read
    uint8_t * buff = (uint8_t *) malloc(buff_size);

    if(buff) {
        // read all data from server
        while(connected() && (len > 0 || len == -1)) {

            // get available data size
            size_t sizeAvailable = _tcp->available();

            if(sizeAvailable) {

                int readBytes = sizeAvailable;

                // read only the asked bytes
                if(len > 0 && readBytes > len) {
                    readBytes = len;
                }

                // not read more the buffer can handle
                if(readBytes > buff_size) {
                    readBytes = buff_size;
                }

                // read data
                int bytesRead = _tcp->readBytes(buff, readBytes);

                // write it to Stream
                int bytesWrite = stream->write(buff, bytesRead);
                bytesWritten += bytesWrite;

                // are all Bytes a writen to stream ?
                if(bytesWrite != bytesRead) {
                    ECHO(
                        String("[HttpClientH][writeToStream] short write asked for ") +
                        String(bytesRead) +
                        String(" but got ") +
                        String(bytesWrite) +
                        String(" retry...")
                    );

                    // check for write error
                    if(stream->getWriteError()) {
                        ECHO(
                            String("[HttpClientH][writeToStreamDataBlock] stream write error ") +
                            String(stream->getWriteError())
                        );

                        //reset write error for retry
                        stream->clearWriteError();
                    }

                    // some time for the stream
                    delay(1);

                    int leftBytes = (readBytes - bytesWrite);

                    // retry to send the missed bytes
                    bytesWrite = stream->write((buff + bytesWrite), leftBytes);
                    bytesWritten += bytesWrite;

                    if(bytesWrite != leftBytes) {
                        // failed again
                        ECHO(
                            String("[HttpClientH][writeToStream] short write asked for ") +
                            String(leftBytes) + String(" but got ") + String(bytesWrite) + String(" failed.")
                        );
                        free(buff);
                        return HTTPC_ERROR_STREAM_WRITE;
                    }
                }

                // check for write error
                if(stream->getWriteError()) {
                    ECHO(String("[HttpClientH][writeToStreamDataBlock] stream write error ") + String(stream->getWriteError()));
                    free(buff);
                    return HTTPC_ERROR_STREAM_WRITE;
                }

                // count bytes to read left
                if(len > 0) {
                    len -= readBytes;
                }

                delay(0);
            } else {
                delay(1);
            }
        }

        free(buff);

        ECHO(String("[HttpClientH][writeToStreamDataBlock] connection closed or file end (written: ") + String(bytesWritten) + String(")."));

        if((size > 0) && (size != bytesWritten)) {
            ECHO(String("[HttpClientH][writeToStreamDataBlock] bytesWritten ") + String(bytesWritten) + String(" and size ") + String(size) + String(" mismatch!"));
            return HTTPC_ERROR_STREAM_WRITE;
        }

    } else {
        ECHO(String("[HttpClientH][writeToStreamDataBlock] too less ram! need ") + String(HTTP_TCP_BUFFER_SIZE));
        return HTTPC_ERROR_TOO_LESS_RAM;
    }

    return bytesWritten;
}

int HttpClientH::getResponseCode() {
    return _responseCode;
}

int HttpClientH::returnError(int error)
{
    if(error < 0) {
        ECHO(String("[HttpClientH][returnError] error") + String(error));
        if(connected()) {
            ECHO("[HttpClientH][returnError] tcp stop\n");
            _tcp->stop();
        }
    }
    return error;
}


int HttpClientH::get(String path) {
    return sendRequest(path);
}

int HttpClientH::post(String path, String payload) {
    return sendRequest(path, "POST", payload);
}

void HttpClientH::printFreeHeap() {
    ECHO(String(ESP.getFreeHeap()));
}

/**
 * try to reuse the connection to the server
 * keep-alive
 * @param reuse bool
 */
void HttpClientH::setReuse(bool reuse)
{
    _reuse = reuse;
}
