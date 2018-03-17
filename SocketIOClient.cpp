/*
socket.io-arduino-client: a Socket.IO client for the Arduino
Based on the Kevin Rohling WebSocketClient & Bill Roy Socket.io Lbrary
Copyright 2015 Florent Vidal
Supports Socket.io v1.x
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Modified by RoboJay
 */

/**
 * Modified by Hoang Hoi
 */

#include <SocketIOClient.h>

#ifdef DEBUG
#define ECHO(m) Serial.println(m)
#else
#define ECHO(m)
#endif

#define ORIGIN F("Origin: ArduinoSocketIOClient\r\n")

SocketIOClient::SocketIOClient() {
    for (int i = 0; i < MAX_ON_HANDLERS; i++) {
        onId[i] = "";
    }
}

void SocketIOClient::setAuthToken(String newAuthToken) {
    authToken = newAuthToken;
}

bool SocketIOClient::connect(String thehostname, int theport) {
    thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
    Serial.println("Connection established");
    port = theport;
    ECHO(F("Connect to host: "));
    ECHO(hostname);
    ECHO(F("Connect to port: "));
    ECHO(port);
    bool status;
    if (handshake() && authenticate() && connectViaSocket()) {
        status = true;
    } else {
        status = false;
    }
    return status;
}

bool SocketIOClient::handshake() {
    if (!beginConnect()) {
        return false;
    }
    ECHO(F("[handshake] Begin send Handshake"));
    ECHO("[handshake] Server: " + String(hostname));
    sendHandshake();

    if (!waitForInput()) {
        ECHO(F("[handshake] Time out"));
        return false;
    }

    ECHO(F("[handshake] Read Handshake"));
    return readHandshake();
}

bool SocketIOClient::readHandshake() {
    // check for happy "HTTP/1.1 200" response
    if (!checkResponseStatus(200)) {
        return false;
    }

    eatHeader();
    if (!readSid()) {
        return false;
    }

    stopConnect();
}

bool SocketIOClient::authenticate() {
    ECHO(authToken);
    if (authToken.length() == 0) {
        return true;
    }

    if (!beginConnect()) {
        return false;
    }

    sendRequestAuthenticate();

    if (!waitForInput()) {
        ECHO(F("[sendRequestAuthenticate] Time out"));
        return false;
    }

    if (!checkResponseStatus(200)) {
        return false;
    }
    stopConnect();
}

void SocketIOClient::sendRequestAuthenticate() {
    ECHO("[sendRequestAuthenticate] Authenticate with token: " + authToken);
    String body = String(authToken.length() + 31);
    body += ":42[\"authenticate\",{\"token\":\"";
    body += authToken;
    body += "\"}]";

    String request = "";
    request += F("POST /socket.io/?EIO=3&transport=polling&sid=");
    request += sid;
    request += F(" HTTP/1.1\r\n");
    if (port == 80) {
        request += F("Host: ");
        request += hostname;
        request += F("\r\n");
    } else {
        request += F("Host: ");
        request += hostname;
        request += F(":");
        request += port;
        request += F("\r\n");
    }

    request += ORIGIN;
    request += F("Content-Type: text/plain;charset=UTF-8\r\n");
    request += F("Content-Length: ");
    request += body.length();
    request += F("\r\n");
    request += F("Connection: keep-alive\r\n\r\n");
    request += body;
    request += "\r\n\r\n";

    ECHO(F("\r\n[sendRequestAuthenticate] Send request........................."));
    ECHO(request);
    ECHO(F("[sendRequestAuthenticate] .........................send request done\r\n"));
    internets.print(request);

    // request += "43:42[\"subscribe\",{\"channel\":\"feed-the-fish\"}]\r\n\r\n";
}

bool SocketIOClient::connectViaSocket() {
    bool status = true;
    if (!beginConnect()) {
        return false;
    }

    ECHO(F("[connectViaSocket] Send connect to Websocket"));

    sendConnectToSocket();
    if (!waitForInput()) {
        status = false;
        return false;
    }

    if (!checkResponseStatus(101)) {
        ECHO(F("[connectViaSocket] Updrage to Websocket failed"));
        status = false;
        return false;
    }

    ECHO(F("[connectViaSocket] Updrage to Websocket successful"));
    readLine();
    readLine();
    readLine();
    for (int i = 0; i < 28; i++) {
        key[i] = databuffer[i + 22]; //key contains the Sec-WebSocket-Accept, could be used for verification
    }

    ECHO(F("[connectViaSocket] Read Sec-WebSocket-Accept key successful"));
    ECHO("[connectViaSocket] Key: " + String(key));

    eatHeader();

    /*
    Generating a 32 bits mask requiered for newer version
    Client has to send "52" for the upgrade to websocket
     */
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

    internets.print((char) 0x81); //has to be sent for proper communication
    internets.print((char) 130); //size of the message (2) + 128 because message has to be masked
    internets.print(mask);
    internets.print(masked);

    monitor(); // treat the response as input
    status = true;
    return true;

}

bool SocketIOClient::checkResponseStatus(int status) {
    // check for happy "HTTP/1.1 200" response
    readLine();
    if (atoi(&databuffer[9]) != status) {
        while (internets.available()) readLine();
        internets.stop();
        return false;
    }

    return true;
}

bool SocketIOClient::readSid() {
    readLine();
    String tmp = databuffer;

    int sidindex = tmp.indexOf("sid");
    if (sidindex < 0) {
        ECHO(F("[readSid] SID not exist"));
        return false;
    }

    int sidendindex = tmp.indexOf("\"", sidindex + 6);
    int count = sidendindex - sidindex - 6;

    for (int i = 0; i < count; i++) {
        sid[i] = databuffer[i + sidindex + 6];
    }

    ECHO("[readSid] SID: " + String(sid));
    return true;
}

bool SocketIOClient::beginConnect() {
    if (!internets.connect(hostname, port)) {
        ECHO(F("[beginConnect] Connect failed"));
        return false;
    }

    ECHO(F("[beginConnect] Connect successful"));
    return true;
}

bool SocketIOClient::stopConnect() {
    while (internets.available()) {
        readLine();
    }

    internets.stop();
    delay(100);
    ECHO(F("[stopConnect] Connect was stopped"));
    return true;
}

void SocketIOClient::sendConnectToSocket() {
    String request = "";
    request += F("GET /socket.io/1/websocket/?transport=websocket&b64=true&sid=");
    request += sid;
    request += F(" HTTP/1.1\r\n");
    if (port == 80) {
        request += F("Host: ");
        request += hostname;
        request += F("\r\n");
    } else {
        request += F("Host: ");
        request += hostname;
        request += F(":");
        request += port;
        request += F("\r\n");
    }

    request += ORIGIN;
    request += F("Sec-WebSocket-Key: ");
    request += sid;
    request += F("\r\n");
    request += F("Sec-WebSocket-Version: 13\r\n");
    request += F("Upgrade: websocket\r\n");
    request += F("Connection: Upgrade\r\n\r\n");

    ECHO(F("\r\n[sendConnectToSocket] Send request........................."));
    ECHO(request);
    ECHO(F("[sendConnectToSocket] .........................send request done\r\n"));
    internets.print(request);
}

bool SocketIOClient::connectHTTP(String thehostname, int theport) {
    thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
    port = theport;
    if (!internets.connect(hostname, theport)) {
        return false;
    }
    return true;
}

bool SocketIOClient::connected() {
    return internets.connected();
}

void SocketIOClient::disconnect() {
    internets.stop();
}

void SocketIOClient::eventHandler(int index) {
    String id = "";
    String data = "";
    String rcvdmsg = "";
    int sizemsg = databuffer[index + 1]; // 0-125 byte, index ok Fix provide by Galilei11. Thanks
    if (databuffer[index + 1] > 125) {
        sizemsg = databuffer[index + 2]; // 126-255 byte
        index += 1; // index correction to start
    }

    ECHO("[eventHandler] Message size = " + String(sizemsg));

    for (int i = index + 2; i < index + sizemsg + 2; i++) {
        rcvdmsg += (char) databuffer[i];
    }

    ECHO("[eventHandler] Received message = " + String(rcvdmsg));

    switch (rcvdmsg[0]) {
        case '2':
            ECHO("[eventHandler] Ping received - Sending Pong");
            heartbeat(1);
            break;
        case '3':
            ECHO("[eventHandler] Pong received - All good");
            break;
        case '4':
            switch (rcvdmsg[1]) {
                case '0':
                    ECHO("[eventHandler] Upgrade to WebSocket confirmed");
                    break;
                case '2':
                    rcvdmsg.replace("\\\\", "\\");
                    id = rcvdmsg.substring(4, rcvdmsg.indexOf("\","));

                    ECHO("[eventHandler] id = " + id);

                    data = rcvdmsg.substring(2);
                    ECHO("[eventHandler] data = " + data);

                    ECHO(onIndex);
                    for (uint8_t i = 0; i < onIndex; i++) {
                        ECHO(onId[i]);
                        if (id == onId[i]) {
                            ECHO("[eventHandler] Found handler = " + String(i));
                            (*onFunction[i])(data);
                        }
                    }
                    break;
            }
    }
}

void SocketIOClient::monitor() {
    int index = -1;
    int index2 = -1;
    String tmp = "";
    *databuffer = 0;
    static unsigned long pingTimer = 0;

    if (!connected()) {
        ECHO("[monitor] Client not connected.");
        ECHO("[monitor] Reconnect....");
        stopConnect();
        if (!connect(hostname, port)) {
            ECHO("[monitor] Can't connect. Aborting.");
            return;
        } else {
            ECHO("[monitor] Reconnected!");
        }
    }

    // the PING_INTERVAL from the negotiation phase should be used
    // this is a temporary hack
    if (connected() && millis() >= pingTimer) {
        heartbeat(0);
        ECHO("[monitor] Heartbeat");
        pingTimer = millis() + PING_INTERVAL;
    }

    if (!internets.available()) {
        return;
    }

    while (internets.available()) { // read availible internets
        readLine();
        tmp = databuffer;
        dataptr = databuffer;
        index = tmp.indexOf((char) 129); //129 DEC = 0x81 HEX = sent for proper communication
        index2 = tmp.indexOf((char) 129, index + 1);
        if (index != -1) {
            eventHandler(index);
        }
        if (index2 != -1) {
            eventHandler(index2);
        }
    }
}

void SocketIOClient::sendHandshake() {
    ECHO(hostname);
    String request = "";
    request += F("GET /socket.io/?transport=polling&b64=true HTTP/1.1\r\n");
    if (port == 80) {
        request += F("Host: ");
        request += hostname;
        request += F("\r\n");
    } else {
        request += F("Host: ");
        request += hostname;
        request += F(":");
        request += port;
        request += F("\r\n");
    }
    request += F("Origin: Arduino\r\n");
    request += F("Connexion: keep-alive\r\n\r\n");
    ECHO(F("\r\n[sendHandshake] Send request........................."));
    ECHO(request);
    ECHO(F("[sendHandshake] .........................send request done\r\n"));
    internets.print(request);
}

bool SocketIOClient::waitForInput(void) {
    unsigned long now = millis();
    while (!internets.available() && ((millis() - now) < 30000UL)) {
        ;
    }
    return internets.available();
}

void SocketIOClient::eatHeader(void) {
    while (internets.available()) { // consume the header
        readLine();
        if (strlen(databuffer) == 0) {
            break;
        }
    }
}

void SocketIOClient::setCookie(String data) {
    int a = data.indexOf(";");
    String newCookie = data.substring(12, a);
    String newCookieKey = newCookie.substring(0, newCookie.indexOf("="));
    int oldCookieStart = cookie.indexOf(newCookieKey);
    if (oldCookieStart >= 0) {
        int oldCookieEnd = cookie.indexOf(";", oldCookieStart);
        String newCookies = cookie.substring(0, oldCookieStart);
        newCookies += newCookie;
        newCookies += cookie.substring(oldCookieEnd);
        cookie = newCookies;
    } else {
        cookie += newCookie;
        cookie += ";";
    }
    ECHO("cookie");
    ECHO(cookie);
}

String SocketIOClient::getREST(String host,int port, String path){
    char hostname[128];
    host.toCharArray(hostname, MAX_HOSTNAME_LEN);
    if (!internets.connect(hostname, port)) {
        ECHO(F("Connect failed"));
    }
    String request = "";
    request += F("GET /");
    request += path;
    request += F(" HTTP/1.1\r\n");
    request += F("Host: ");
    request += host;
    request += F("\r\n");
    request += F("Cookie: ");
    request += cookie;
    request += F("\r\n");
    request += F("Accept: application/json\r\n");
    request += ORIGIN;
    request += F("Connection: keep-alive\r\n\r\n");

    ECHO(F("\r\n[getREST] Send request........................."));
    ECHO(request);
    ECHO(F("[getREST] .........................send request done\r\n"));

    internets.print(request);
    if (!waitForInput()) {
        ECHO(F("[getRest] Time out"));
    }
    eatHeader();

    readLine();
    String tmp = databuffer;
    stopConnect();
    ECHO(tmp);
    return tmp;
}

String SocketIOClient::postREST(String host, int port, String path, String token, String data) {
    char hostname[128];
    host.toCharArray(hostname, MAX_HOSTNAME_LEN);
    if (!internets.connect(hostname, port)) {
        ECHO(F("Connect failed"));
    }

    String request = "";
    request += F("POST /");
    request += path;
    request += F(" HTTP/1.1\r\n");
    request += F("Host: ");
    request += hostname;
    request += F("\r\n");
    request += F("Accept: application/json\r\n");
    request += F("X-CSRF-TOKEN: ");
    request += token;
    request += F("\r\n");
    request += F("Cookie: ");
    request += cookie;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Content-Length: ");
    request += data.length();
    request += F("\r\n");
    request += F("Content-Type: application/json\r\n");
    request += F("Connection: keep-alive\r\n\r\n");
    request += data;
    request += F("\r\n");

    ECHO(F("\r\n[postREST] Send request........................."));
    ECHO(request);
    ECHO(F("[postREST] .........................send request done\r\n"));

    internets.print(request);
    if (!waitForInput()) {
        ECHO(F("[postRest] Time out"));
    }
    eatHeader();
    readLine();
    stopConnect();

    return databuffer;
}

void SocketIOClient::putREST(String host, String path, String type, String data) {
    String request = "";
    request += F("PUT /");
    request += path;
    request += F("/ HTTP/1.1\r\n");
    request += F("Host: ");
    request += host;
    request += F("\r\n");
    request += F("Cookie: ");
    request += cookie;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Content-Length: ");
    request += data.length();
    request += F("\r\n");
    request += F("Content-Type: ");
    request += type;
    request += F("\r\n");
    request += F("Connection: close\r\n\r\n");
    request += data;
    request += F("\r\n");

    ECHO(F("\r\n[putREST] Send request........................."));
    ECHO(request);
    ECHO(F("[putREST] .........................send request done\r\n"));
    internets.print(request);
}

void SocketIOClient::deleteREST(String host, String path) {
    String request = "";
    request += F("DELETE /");
    request += path;
    request += F("/ HTTP/1.1\r\n");
    request += F("Host: ");
    request += hostname;
    request += F("\r\n");
    request += F("Cookie: ");
    request += cookie;
    request += F("\r\n");
    request += ORIGIN;
    request += F("Connection: close\r\n\r\n");

    ECHO(F("\r\n[deleteREST] Send request........................."));
    ECHO(request);
    ECHO(F("[deleteREST] .........................send request done\r\n"));
    internets.print(request);
}

void SocketIOClient::readLine() {
    dataptr = databuffer;
    while (dataptr < &databuffer[DATA_BUFFER_LEN - 2]) {
        if (!internets.available() && !internets.available()) {
            break;
        }
        char c = internets.read();
        if (c == '\r') {
            break;
        } else if (c != '\n') {
            *dataptr++ = c;
        }
    }
    *dataptr = 0;
    String data = databuffer;
    ECHO("[readLine] databuffer: " + data);
    if (data.indexOf("Set-Cookie:") >=0) {
        setCookie(data);
    }
}

void SocketIOClient::emit(String id, String data) {
    String message = "42[\"" + id + "\"," + data + "]";
    ECHO("[emit] message content" + message);
    int msglength = message.length();
    ECHO("[emit] message length: " + String(msglength));
    randomSeed(analogRead(0));
    String mask = "";
    String masked = message;
    for (int i = 0; i < 4; i++) {
        char a = random(48, 57);
        mask += a;
    }
    for (int i = 0; i < msglength; i++) {
        masked[i] = message[i] ^ mask[i % 4];
    }

    String request = "";
    request += String((char) 0x81); // has to be sent for proper communication
    // Depending on the size of the message
    if (msglength <= 125) {
        request += String((char) (msglength + 128)); //size of the message + 128 because message has to be masked
    } else if (msglength >= 126 && msglength <= 65535) {
        request += String((char) (126 + 128));
        request += String((char) ((msglength >> 8) & 255));
        request += String((char) ((msglength) & 255));
    } else {
        request += String((char) (127 + 128));
        for (uint8_t i = 0; i >= 8; i = i - 8) {
            request += String((char) ((msglength >> i) & 255));
        }
    }
    request += String(mask);
    request += String(masked);
    internets.print(request);
}

void SocketIOClient::heartbeat(int select) {
    randomSeed(analogRead(0));
    String mask = "";
    String masked = "";
    String message = "";
    if (select == 0) {
        masked = "2";
        message = "2";
    } else {
        masked = "3";
        message = "3";
    }
    for (int i = 0; i < 4; i++) { //generate a random mask, 4 bytes, ASCII 0 to 9
        char a = random(48, 57);
        mask += a;
    }
    for (int i = 0; i < message.length(); i++) {
        masked[i] = message[i] ^ mask[i % 4]; //apply the "mask" to the message ("2" : ping or "3" : pong)
    }

    String request = "";
    request += String((char) 0x81); //has to be sent for proper communication
    request += String((char) 129); //size of the message (1) + 128 because message has to be masked
    request += String(mask);
    request += String(masked);
    internets.print(request);
}

void SocketIOClient::on(String id, functionPointer function) {
    if (onIndex == MAX_ON_HANDLERS) {
        return;
    } // oops... to many...
    onFunction[onIndex] = function;
    onId[onIndex] = id;
    onIndex++;
}
