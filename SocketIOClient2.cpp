
// #include <base64.h>
#include "SocketIOClient2.h"

#ifdef DEBUG
#define ECHO(m) Serial.println(m)
#define FREE_HEAP() ({ECHO(String("FREE_HEAP: ") + String(ESP.getFreeHeap()));})
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

    return sendDataToTcp(payload);
}

int SocketIOClient2::sendDataToTcp(String payload)
{
    size_t payloadSize = payload.length();
    if(_tcp->write((const uint8_t *) payload.c_str(), payloadSize) != payloadSize) {
        return handleError(SOCKET_ERROR_SEND_DATA_FAILED);
    }

    return RETURN_OK;
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
    clearResponse();
    clearRequest();
    end();
}

void SocketIOClient2::clearResponse()
{
    ECHO(String("[SocketIOClient2][clearResponse] Clear Response"));
    _responseCode = 0;
    _size = -1;
    _canReuse = false;
}

void SocketIOClient2::clearRequest()
{
    ECHO(String("[SocketIOClient2][clearRequest] Clear Request"));
    _headers = "";
    _cookies = "";
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

void SocketIOClient2::stop()
{
    if(isConnected()) {
        if(_tcp->available() > 0) {
            ECHO(String("[SocketIOClient2][stop] still data in buffer (") + String(_tcp->available()) + String("), clean up."));
            while(_tcp->available() > 0) {
                _tcp->read();
            }
        }
        ECHO("[SocketIOClient2][end] tcp stop");
        _tcp->stop();
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

void SocketIOClient2::setCookie(String cookie)
{
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
    clearResponse();
    size_t payloadSize = payload.length();
    if(!tcpConnect()) {
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
    if (_headers.indexOf("Connection") == -1) {
        header += F("\r\nConnection: ");
        if(_reuse) {
            header += F("keep-alive");
        } else {
            header += F("close");
        }
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

void SocketIOClient2::addHeader(const String& name, const String& value)
{
    // not allow set of Header handled by code
    if(!name.equalsIgnoreCase(F("User-Agent")) &&
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

int SocketIOClient2::handleHeaderResponse()
{
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

                if(headerName.equalsIgnoreCase("Sec-WebSocket-Accept")) {
                    _secWebSocketAccept = headerValue;
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

String SocketIOClient2::getResponseString()
{
    if (_size <= 0) {
        return String("");
    }

    StreamString sstring;

    if(_size) {
        // try to reserve needed memmory
        if(!sstring.reserve((_size + 1))) {
            ECHO(String("[SocketIOClient2][getString] not enough memory to reserve a string! need: ") + String(_size + 1));
            return "";
        }
    }

    writeToStream(&sstring);
    return sstring;
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

bool SocketIOClient2::connect()
{
    ECHO(F("Connect to host: "));
    ECHO(_host);
    ECHO(F("Connect to port: "));
    ECHO(_port);
    clear();
    int handshakeResponse = handshake();
    if (handshakeResponse < 0) {
        return handshakeResponse;
    }

    int authenticateResponse = authenticate();
    if (authenticateResponse < 0) {
        return authenticateResponse;
    }

    int connectViaSocketResponse = connectViaSocket();
    if (connectViaSocketResponse < 0) {
        return connectViaSocketResponse;
    }

    return RETURN_OK;
}

int SocketIOClient2::connectViaSocket()
{
    addHeader("Sec-WebSocket-Key", _sid);
    addHeader("Sec-WebSocket-Version", "13");
    addHeader("Upgrade", "websocket");
    addHeader("Connection", "Upgrade");
    ECHO("[SocketIOClient2][connectViaSocket] Begin connectViaSocket");
    int connectViaSocketCode = sendRequest(String(UPGRADE_URL) + _sid, "GET");

    if (connectViaSocketCode != 101) {
        return handleError(connectViaSocketCode);
    }

    randomSeed(analogRead(0));
    String mask = "";
    String masked = "52";
    String message = "52";
    for (int i = 0; i < 4; i++) { //generate a random mask, 4 bytes, ASCII 0 to 9
        char a = random(48, 57);
        mask += a;
    }
    for (int i = 0; i < message.length(); i++) {
        masked[i] = message[i] ^ mask[i % 4]; //apply the "mask" to the message ("52")
    }

    String data;
    data += (char) 0x81;
    data += (char) 130;
    data += mask;
    data += masked;
    int dataSize = data.length();
    ECHO(data);
    if(_tcp->write((const uint8_t *) data.c_str(), dataSize) != dataSize) {
        return handleError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
    }
    _canReuse = true;
    _pingTimer = millis() + PING_INTERVAL;
    return RETURN_OK;
}

int SocketIOClient2::authenticate()
{
    if (_authToken.length() == 0) {
        return handleError(SOCKET_ERROR_AUTH_TOKEN_NOT_FOUND);
    }

    String body = String(_authToken.length() + 31);
    body += ":42[\"authenticate\",{\"token\":\"";
    body += _authToken;
    body += "\"}]";

    ECHO("[SocketIOClient2][authenticate] Begin authenticate");
    ECHO(body);
    int authenticateCode = sendRequest(String(UPDATE_URL) + _sid, "POST", body);


    ECHO(authenticateCode);
    return handleAuthenticateResponse(getResponseString());
}

int SocketIOClient2::handleAuthenticateResponse(String data)
{
    if (data.equalsIgnoreCase(SOCKET_AUTHENTICATE_OK_STRING)) {
        return RETURN_OK;
    }

    return handleError(SOCKET_ERROR_AUTHENTICATE_FAILED);
}

int SocketIOClient2::handshake()
{
    int handshakeCode = sendRequest(HANDSHAKE_URL, "GET");
    ECHO("[SocketIOClient2][handshake] Begin handshake");
    ECHO(handshakeCode);
    if (handshakeCode != 200) {
        return handleHttpError(handshakeCode);
    }

    String response = getResponseString();
    int lengthEndPoint = response.indexOf(':');
    if(lengthEndPoint <= 0) {
        return handleError(SOCKET_ERROR_UNDEFIENED_LENGTH);
    }

    String packetSizeString = response.substring(0, lengthEndPoint);
    String packetString = response.substring(lengthEndPoint + 1);
    _packetSize = packetSizeString.toInt();

    if (_packetSize != packetString.length()) {
        return handleError(SOCKET_ERROR_LENGTH_NOT_MATCH);
    }

    return readSid(packetString);
}

int SocketIOClient2::readSid(String data)
{
    String packetTypeString = data.substring(0, 0);
    _packetType = packetTypeString.toInt();
    ECHO(data);
    ECHO("[SocketIOClient2][readSid] Packet type");
    ECHO(_packetType);
    if (_packetType != SOCKET_CONNECT) {
        return handleError(SOCKET_ERROR_HANDSHAKE_FAILED);
    }

    data = data.substring(1);
    const size_t dataSize = data.length();
    ECHO("[SocketIOClient2][readSid] Begin parse handshake packet");
    ECHO(data);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonObject& root = jsonBuffer.parseObject(data.c_str());
    if (!root.success()) {
        return handleError(ERROR_PARSE_JSON_ERROR);
    }

    String sid = root["sid"];
    String pingInterval = root["pingInterval"];
    String pingTimeout = root["pingTimeout"];

    _sid = sid;
    _pingInterval = pingInterval.toInt();
    _pingTimeout = pingTimeout.toInt();

    ECHO(_sid);
    ECHO(_pingInterval);
    ECHO(_pingTimeout);

    return RETURN_OK;
}

bool SocketIOClient2::isConnected()
{
    if(_tcp) {
        return (_tcp->connected() || (_tcp->available() > 0));
    }
    return false;
}

bool SocketIOClient2::tcpConnect()
{
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

void SocketIOClient2::monitor()
{
    if (!isConnected()) {
        if (millis() >= _reconnectTimer) {
            ECHO("[SocketIOClient2][monitor] Reconnect....");
            clearRequest();
            clearResponse();
            stop();
            connect();
            _reconnectTimer = millis() + RECONNECT_INTERVAL;
        }
        return;
    }

    if (_isPing && millis() >= _lastPingTimeout) {
        ECHO("[SocketIOClient2][monitor] Ping time out!");
        _isPing = false;
        clear();
        return;
    }

    // the PING_INTERVAL from the negotiation phase should be used
    // this is a temporary hack
    if (millis() >= _pingTimer) {
        heartbeat(0);
        ECHO("[SocketIOClient2][monitor] Send ping");
        _pingTimer = millis() + PING_INTERVAL;
        if (!_isPing) {
            _lastPingTimeout = millis() + PING_TIME_OUT;
            _isPing = true;
        }
    }

    eventHandler();
}

int SocketIOClient2::emit(String id, String data) {
    String message = "42[\"" + id + "\"," + data + "]";
    ECHO("[emit] message content: " + message);
    int msglength = message.length();
    ECHO("[emit] message length: " + String(msglength));

    String payload = String((char) 0x81);
    // Depending on the size of the message
    if (msglength <= 125) {
        payload += ((char) (msglength + 128)); //size of the message + 128 because message has to be masked
    } else if (msglength >= 126 && msglength <= 65535) {
        payload += ((char) (126 + 128));
        payload += ((char) ((msglength >> 8) & 255));
        payload += ((char) ((msglength) & 255));
    } else {
        payload += ((char) (127 + 128));
        for (uint8_t i = 0; i >= 8; i = i - 8) {
            payload += ((char) ((msglength >> i) & 255));
        }
    }

    randomSeed(analogRead(0));
    String mask = "";
    for (int i = 0; i < 4; i++) {
        char a = random(48, 57);
        mask += a;
    }

    payload += mask;

    for (int i = 0; i < msglength; i++) {
        payload += ((char) (message[i] ^ mask[i % 4]));
    }

    if(!isConnected()) {
        return handleError(HTTPC_ERROR_NOT_CONNECTED);
    }

    return sendDataToTcp(payload);
}

int SocketIOClient2::readMessageLength()
{
    unsigned long lastDataTime = millis();
    int big = 0;

    while(isConnected()) {
        // get available data size
        size_t sizeAvailable = _tcp->available();
        if(sizeAvailable) {
            char byteRead = _tcp->read();
            lastDataTime = millis();
            // Serial.println(byteRead, DEC);

            if (byteRead == (char) 126 && big == 0) {
                big = -1;
                continue;
            }

            if (byteRead == (char) 127 && big == 0) {
                return -1;
            }

            if (big < 0) {
                big = ((int) byteRead) << 8;
                continue;
            }

            return big | byteRead;
        } else {
            if((millis() - lastDataTime) > _tcpTimeout) {
                return HTTPC_ERROR_READ_TIMEOUT;
            }
            delay(1);
        }
    }

    return handleError(HTTPC_ERROR_NOT_CONNECTED);
}

void SocketIOClient2::eventHandler()
{
    if (!isConnected()) {
        ECHO("[SocketIOClient2][eventHandler] Client not connected.");
        ECHO("[SocketIOClient2][eventHandler] Reconnect....");
        return;
    }

    size_t sizeAvailable = _tcp->available();
    if (!sizeAvailable) {
        return;
    }

    char c = _tcp->read();
    if (c != (char) 0x81) {
        ECHO(String("[SocketIOClient2][eventHandler] Clear data: ") + String(c));
        return;
    }

    ECHO("[SocketIOClient2][eventHandler] Begin handle event");
    _size = readMessageLength();
    ECHO("MESSAGE SIZE");
    ECHO(_size);
    String data = getResponseString();
    ECHO(data);

    switch (data[0]) {
        case '2':
            ECHO("[eventHandler] Ping received - Sending Pong");
            heartbeat(1);
            break;
        case '3':
            ECHO("[eventHandler] Pong received - All good");
            _isPing = false;
            break;
        case '4':
            runEventFunction(data);
            break;
        default: handleError(SOCKET_ERROR_UNKNOWN_MESSAGE_TYPE);
    }
}

void SocketIOClient2::checkMessageType(String data)
{
    switch (data[1]) {
        case '0':
            ECHO("[SocketIOClient2][runEventFunction] Upgrade to WebSocket confirmed");
            break;
        case '2':
            runEventFunction(data);
            break;
        default: handleError(SOCKET_ERROR_UNKNOWN_MESSAGE_TYPE);
    }
}

void SocketIOClient2::runEventFunction(String data)
{
    //data.replace("\\\\", "\\");
    ECHO(data);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonArray& root = jsonBuffer.parseArray(data.substring(2).c_str());
    if (!root.success()) {
        handleError(ERROR_PARSE_JSON_ERROR);
        return;
    }

    String id = root[0];
    String receiverData = root[1];

    ECHO("[SocketIOClient2][runEventFunction] id = " + id);

    ECHO("[SocketIOClient2][runEventFunction] data = " + receiverData);

    for (uint8_t i = 0; i < _onIndex; i++) {
        ECHO(_onId[i]);
        if (id == _onId[i]) {
            ECHO("[SocketIOClient2][eventHandler] Found handler = " + String(i));
            (*_onFunction[i])(receiverData);
            return;
        }
    }
    handleError(SOCKET_ERROR_ON_ID_NOT_MATCH);
}

int SocketIOClient2::handleError(int error)
{
    switch (error) {
        case HTTPC_ERROR_CONNECTION_REFUSED:
            ECHO("HTTPC_ERROR_CONNECTION_REFUSED");
            break;
        case HTTPC_ERROR_SEND_HEARTBEAT_FAILED:
            ECHO("HTTPC_ERROR_SEND_HEARTBEAT_FAILED");
            break;
        case HTTPC_ERROR_SEND_EMIT_FAILED:
            ECHO("HTTPC_ERROR_SEND_EMIT_FAILED");
            break;
        case HTTPC_ERROR_NOT_CONNECTED:
            ECHO("HTTPC_ERROR_NOT_CONNECTED");
            break;
        case HTTPC_ERROR_CONNECTION_LOST:
            ECHO("HTTPC_ERROR_CONNECTION_LOST");
            break;
        case HTTPC_ERROR_NO_STREAM:
            ECHO("HTTPC_ERROR_NO_STREAM");
            break;
        case HTTPC_ERROR_NO_HTTP_SERVER:
            ECHO("HTTPC_ERROR_NO_HTTP_SERVER");
            break;
        case HTTPC_ERROR_TOO_LESS_RAM:
            ECHO("HTTPC_ERROR_TOO_LESS_RAM");
            break;
        case HTTPC_ERROR_ENCODING:
            ECHO("HTTPC_ERROR_ENCODING");
            break;
        case HTTPC_ERROR_STREAM_WRITE:
            ECHO("HTTPC_ERROR_STREAM_WRITE");
            break;
        case HTTPC_ERROR_READ_TIMEOUT:
            ECHO("HTTPC_ERROR_READ_TIMEOUT");
            break;
        case SOCKET_ERROR_UNDEFIENED_LENGTH:
            ECHO("SOCKET_ERROR_UNDEFIENED_LENGTH");
            break;
        case SOCKET_ERROR_LENGTH_NOT_MATCH:
            ECHO("SOCKET_ERROR_LENGTH_NOT_MATCH");
            break;
        case SOCKET_ERROR_HANDSHAKE_FAILED:
            ECHO("SOCKET_ERROR_HANDSHAKE_FAILED");
            break;
        case SOCKET_ERROR_AUTH_TOKEN_NOT_FOUND:
            ECHO("SOCKET_ERROR_AUTH_TOKEN_NOT_FOUND");
            break;
        case SOCKET_ERROR_AUTHENTICATE_FAILED:
            ECHO("SOCKET_ERROR_AUTHENTICATE_FAILED");
            break;
        case SOCKET_ERROR_UPGRADE_FAILED:
            ECHO("SOCKET_ERROR_UPGRADE_FAILED");
            break;
        case ERROR_PARSE_JSON_ERROR:
            ECHO("ERROR_PARSE_JSON_ERROR");
            break;
        case UNKNOWN_ERROR:
            ECHO("UNKNOWN_ERROR");
            break;
    }
    return error;
}

int SocketIOClient2::handleHttpError(int error)
{
    return error;
}
