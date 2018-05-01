
// #include <base64.h>
#include "SocketIOClient2.h"

#ifdef DEBUG
#define ECHO(m) Serial.println(m)
#else
#define ECHO(m)
#endif

/**
 * constructor
 */
SocketIOClient2::SocketIOClient2()
{
}

/**
 * destructor
 */
SocketIOClient2::~SocketIOClient2()
{
    if(_tcp) {
        _tcp->stop();
    }
    // if(_responseHeaders) {
    //     delete[] _responseHeaders;
    // }
}

int SocketIOClient2::on(String id, functionPointer function)
{
    if (_onIndex == MAX_ON_HANDLERS) {
        return handleError(TOO_MANY_ON_FUNCTION);
    } // oops... to many...
    _onFunction[_onIndex] = function;
    _onId[_onIndex] = id;
    _onIndex++;

    return RETURN_OK;
}

int SocketIOClient2::heartbeat(int select)
{
    randomSeed(analogRead(0));
    String mask = "";
    String message = "";
    if (select == 0) {
        message = "2";
    } else {
        message = "3";
    }

    String payload = "";
    payload += (char) 0x81; //has to be sent for proper communication
    payload += (char) 129; //size of the message (1) + 128 because message has to be masked
    for (int i = 0; i < 4; i++) { //generate a random mask, 4 bytes, ASCII 0 to 9
        char a = random(48, 57);
        mask += a;
    }

    payload += mask;
    for (int i = 0; i < message.length(); i++) {
        payload += (char) (message[i] ^ mask[i % 4]); //apply the "mask" to the message ("2" : ping or "3" : pong)
    }

    size_t payloadSize = payload.length();
    if(_tcp->write((const uint8_t *) payload.c_str(), payloadSize) != payloadSize) {
        return handleError(HTTPC_ERROR_SEND_HEARTBEAT_FAILED);
    }
}

bool SocketIOClient2::begin(String host) {
    return begin(host, (uint16_t) 80);
}

bool SocketIOClient2::begin(String host, uint16_t port) {
    ECHO(String("[SocketIOClient2][begin] Socket begin: ") + host + String(":") + String(port));
    _host = host;
    _port = port;
    _transportTrait.reset(nullptr);
    _transportTrait = (TransportTraitPtr) new TransportTrait();
    clear();
    return true;
}

bool SocketIOClient2::begin(String host, uint16_t port, String httpsFingerprint) {
    ECHO(String("[SocketIOClient2][begin] Socket begin: ") + host + String(":") + String(port));
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

bool SocketIOClient2::begin(String host, String httpsFingerprint) {
    return begin(host, (uint16_t) 443, httpsFingerprint);
}

void SocketIOClient2::clear()
{
    ECHO(String("[SocketIOClient2][clear] Clear all data"));
    _responseCode = 0;
    _size = -1;
    _headers = "";
    _cookies = "";
    _canReuse = false;
    end();
}

void SocketIOClient2::end()
{
    if(isConnected()) {
        if(_tcp->available() > 0) {
            ECHO(String("[SocketIOClient2][end] still data in buffer (") + String(_tcp->available()) + String("), clean up."));
            while(_tcp->available() > 0) {
                _tcp->read();
            }
        }
        if(_reuse && _canReuse) {
            ECHO("[SocketIOClient2][end] tcp keep open for reuse");
        } else {
            ECHO("[SocketIOClient2][end] tcp stop");
            _tcp->stop();
        }
    } else {
        ECHO("[SocketIOClient2][end] tcp is closed");
    }
}

void SocketIOClient2::setReuse(bool reuse)
{
    _reuse = reuse;
}

void SocketIOClient2::setAuthToken(String authToken)
{
    _authToken = authToken;
}

int SocketIOClient2::writeToStream(Stream * stream)
{
    if(!stream) {
        return handleError(HTTPC_ERROR_NO_STREAM);
    }

    if(!isConnected()) {
        return handleError(HTTPC_ERROR_NOT_CONNECTED);
    }

    // get length of document (is -1 when Server sends no Content-Length header)
    int len = _size;
    int ret = 0;

    ret = writeToStreamDataBlock(stream, len);

    // have we an error?
    if(ret < 0) {
        return handleError(ret);
    }

    return ret;
}

int SocketIOClient2::writeToStreamDataBlock(Stream * stream, int size)
{
    int buff_size = HTTP_TCP_BUFFER_SIZE;
    int len = size;
    int bytesWritten = 0;
    unsigned long lastDataTime = millis();

    // if possible create smaller buffer then HTTP_TCP_BUFFER_SIZE
    if((len > 0) && (len < HTTP_TCP_BUFFER_SIZE)) {
        buff_size = len;
    }

    // create buffer for read
    uint8_t * buff = (uint8_t *) malloc(buff_size);

    if(!buff) {
        ECHO(String("[SocketIOClient2][writeToStreamDataBlock] too less ram! need ") + String(HTTP_TCP_BUFFER_SIZE));
        return HTTPC_ERROR_TOO_LESS_RAM;
    }

    // read all data from server
    while(isConnected() && (len > 0 || len == -1)) {

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
            lastDataTime = millis();

            // write it to Stream
            int bytesWrite = stream->write(buff, bytesRead);
            bytesWritten += bytesWrite;

            // are all Bytes a writen to stream ?
            if(bytesWrite != bytesRead) {
                ECHO(String("[SocketIOClient2][writeToStream] short write asked for ") + String(bytesRead) + String(" but got ") + String(bytesWrite) + String(" retry..."));

                // check for write error
                if(stream->getWriteError()) {
                    ECHO(String("[SocketIOClient2][writeToStreamDataBlock] stream write error ") + String(stream->getWriteError()));

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
                    ECHO(String("[SocketIOClient2][writeToStream] short write asked for ") + String(leftBytes) + String(" but got ") + String(bytesWrite) + String(" failed."));
                    free(buff);
                    return HTTPC_ERROR_STREAM_WRITE;
                }
            }

            // check for write error
            if(stream->getWriteError()) {
                ECHO(String("[SocketIOClient2][writeToStreamDataBlock] stream write error ") + String(stream->getWriteError()));
                free(buff);
                return HTTPC_ERROR_STREAM_WRITE;
            }

            // count bytes to read left
            if(len > 0) {
                len -= readBytes;
            }

            delay(0);
        } else {
            if((millis() - lastDataTime) > _tcpTimeout) {
                return HTTPC_ERROR_READ_TIMEOUT;
            }
            delay(1);
        }
    }

    free(buff);

    ECHO(String("[SocketIOClient2][writeToStreamDataBlock] connection closed or file end (written: ") + String(bytesWritten) + String(")."));

    if((size > 0) && (size != bytesWritten)) {
        ECHO(String("[SocketIOClient2][writeToStreamDataBlock] bytesWritten ") + String(bytesWritten) + String(" and size ") + String(size) + String(" mismatch!"));
        return HTTPC_ERROR_STREAM_WRITE;
    }

    return bytesWritten;
}

void SocketIOClient2::setCookie(String cookie) {
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
}

int SocketIOClient2::sendRequest(String path, const char * method, String payload)
{
    size_t payloadSize = payload.length();
    if(!connect()) {
        return handleError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    ECHO("[HttpClientH][sendRequest] Begin send request");
    // send Header
    if(!sendHeader(path, method, payloadSize)) {
        return handleError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    // send Payload if needed
    if(payloadSize > 0) {
        ECHO(payload);
        if(_tcp->write((const uint8_t *) payload.c_str(), payloadSize) != payloadSize) {
            return handleError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        }
    }

    // handle Server Response (Header)
    return handleHeaderResponse();
}

bool SocketIOClient2::sendHeader(String path, const char * method, size_t payloadSize)
{
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

    if(!isConnected()) {
        return HTTPC_ERROR_NOT_CONNECTED;
    }

    ECHO(header);
    return (_tcp->write((const uint8_t *) header.c_str(), header.length()) == header.length());
}

int SocketIOClient2::handleHeaderResponse() {
    if(!isConnected()) {
        return handleError(HTTPC_ERROR_NOT_CONNECTED);
    }

    String transferEncoding;
    _responseCode = -1;
    _size = -1;
    // _transferEncoding = HTTPC_TE_IDENTITY;
    unsigned long lastDataTime = millis();

    while(isConnected()) {
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
            }

            if(headerLine == "") {
                ECHO(String("[SocketIOClient2][handleHeaderResponse] code: ") + String(_responseCode));

                if(_size > 0) {
                    ECHO(String("[SocketIOClient2][handleHeaderResponse] size: ") + String(_size));
                }

                if(_responseCode) {
                    return _responseCode;
                } else {
                    ECHO("[SocketIOClient2][handleHeaderResponse] Remote host is not an HTTP Server!");
                    return handleError(HTTPC_ERROR_NO_HTTP_SERVER);
                }
            }

        } else {
            if((millis() - lastDataTime) > _tcpTimeout) {
                return handleError(HTTPC_ERROR_READ_TIMEOUT);
            }
            delay(0);
        }
    }

    return handleError(HTTPC_ERROR_CONNECTION_LOST);
}

void SocketIOClient2::addHeader(const String& name, const String& value)
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

bool SocketIOClient2::connect() {
    ECHO(F("Connect to host: "));
    ECHO(_host);
    ECHO(F("Connect to port: "));
    ECHO(_port);

    if (handshake()) {
        return false;
    }

    // if (authenticate()) {
    //     return false;
    // }

    // if (connectViaSocket()) {
    //     return false;
    // }

    return true;
}

int SocketIOClient2::handshake() {
    // if (!beginConnect()) {
    //     return false;
    // }
    int handshakeCode = sendRequest(HANDSHAKE_URL, "GET");
    if (handshakeCode == 200) {
        return readHandshake();
    }
    // ECHO(F("[handshake] Begin send Handshake"));
    // ECHO("[handshake] Server: " + String(hostname));
    // sendHandshake();

    // if (!waitForInput()) {
    //     ECHO(F("[handshake] Time out"));
    //     return false;
    // }

    // ECHO(F("[handshake] Read Handshake"));
    // return readHandshake();
}

bool SocketIOClient2::isConnected()
{
    if(_tcp) {
        return (_tcp->connected() || (_tcp->available() > 0));
    }
    return false;
}

bool SocketIOClient2::tcpConnect() {
    if(isConnected()) {
        ECHO("[SocketIOClient2] connect. already connected, try reuse!");
        while(_tcp->available() > 0) {
            _tcp->read();
        }
        return true;
    }

    if (!_transportTrait) {
        ECHO("[SocketIOClient2] connect: SocketIOClient2::begin was not called or returned error");
        return false;
    }

    _tcp = _transportTrait->create();
    _tcp->setTimeout(_tcpTimeout);

    ECHO(String("[SocketIOClient2] Begin connect to: ") + _host + String(":") + String(_port));

    if(!_tcp->connect(_host.c_str(), _port)) {
        ECHO("[SocketIOClient2] failed connect");
        return false;
    }

    ECHO("[SocketIOClient2] connected");

    if (!_transportTrait->verify(*_tcp, _host.c_str())) {
        ECHO("[SocketIOClient2] transport level verify failed");
        _tcp->stop();
        return false;
    }


#ifdef ESP8266
    _tcp->setNoDelay(true);
#endif
    return isConnected();
}

int SocketIOClient2::handleError(int error)
{
    return error;
}