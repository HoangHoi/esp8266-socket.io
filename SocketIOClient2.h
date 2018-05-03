#ifndef SocketIoClient2_H
#define SocketIoClient2_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include "TransportTrait.h"
#include <HttpClientH.h>
#include <WiFiClientSecure.h>
#include <StreamString.h>
#include <ArduinoJson.h>

#define DEBUG

#define HOST "device.garage.lc"
#define PORT 3000

// Maxmimum number of 'on' handlers
#define MAX_ON_HANDLERS 13

#define JSON_BUFFER_LENGTH 1460

#define PING_INTERVAL 5000
#define PING_TIME_OUT 20000
#define DEFAULT_FINGERPRINT ""


#define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)
/// size for the stream handling
#define HTTP_TCP_BUFFER_SIZE (1460)

#define RETURN_OK (1)

/// HTTP client errors
#define HTTPC_ERROR_CONNECTION_REFUSED      (-1)
#define HTTPC_ERROR_SEND_HEARTBEAT_FAILED   (-2)
#define HTTPC_ERROR_SEND_EMIT_FAILED        (-3)
#define HTTPC_ERROR_NOT_CONNECTED           (-4)
#define HTTPC_ERROR_CONNECTION_LOST         (-5)
#define HTTPC_ERROR_NO_STREAM               (-6)
#define HTTPC_ERROR_NO_HTTP_SERVER          (-7)
#define HTTPC_ERROR_TOO_LESS_RAM            (-8)
#define HTTPC_ERROR_ENCODING                (-9)
#define HTTPC_ERROR_STREAM_WRITE            (-10)
#define HTTPC_ERROR_READ_TIMEOUT            (-11)

// SOCKET error
#define SOCKET_ERROR_UNDEFIENED_LENGTH      (-12)
#define SOCKET_ERROR_LENGTH_NOT_MATCH       (-13)

#define SOCKET_ERROR_HANDSHAKE_FAILED       (-14)
#define SOCKET_ERROR_AUTHENTICATE_FAILED    (-15)
#define SOCKET_ERROR_AUTH_TOKEN_NOT_FOUND   (-16)
#define SOCKET_ERROR_UPGRADE_FAILED         (-17)
#define SOCKET_ERROR_TCP_NOT_AVAIABLE       (-18)

// #define SOCKET_ERROR_UNDEFIENED_PACKET_LENGTH        (-12)

#define ERROR_PARSE_JSON_ERROR              (-40)
#define UNKNOWN_ERROR                       (-41)

#define SOCKET_AUTHENTICATE_OK_STRING        "ok"

typedef enum {
    SOCKET_CONNECT = 0,
    SOCKET_DISCONNECT = 1,
    SOCKET_EVENT = 2,
    SOCKET_ACK = 3,
    SOCKET_ERROR = 4,
    SOCKET_BINARY_EVENT = 5,
    SOCKET_BINARY_ACK = 6
} t_socket_codes;

/// Socket error
#define TOO_MANY_ON_FUNCTION        (-12)

#define HANDSHAKE_URL "/socket.io/?transport=polling&b64=true"
#define UPDATE_URL "/socket.io/?EIO=3&transport=polling&sid="
#define UPGRADE_URL "/socket.io/?transport=websocket&b64=true&sid="

typedef void (*functionPointer)(String data);

class SocketIOClient2 {
public:
    SocketIOClient2();
    ~SocketIOClient2();
    void setup(String host, int port);
    void setup(String host, int port, String authToken);
    void setAuthToken(String authToken);

    bool begin(String host);
    bool begin(String host, String httpsFingerprint);
    bool begin(String host, uint16_t port);
    bool begin(String host, uint16_t port, String httpsFingerprint);

    void setFingerprint(String httpsFingerprint);
    void setReuse(bool reuse);

    void clear();
    void clearResponse();
    void clearRequest();
    void end();
    void stop();

    bool connect();
    // bool connectHTTP(String thehostname, int port = 80);
    // bool connected();
    // void disconnect();
    void monitor();
   	// void begin(const char* host, const int port = DEFAULT_PORT, const char* url = DEFAULT_URL);
    int on(String id, functionPointer f);
    // void emit(String id, String data);
    int heartbeat(int select);
private:
    std::unique_ptr<WiFiClient> _tcp;
    TransportTraitPtr _transportTrait;

    HttpClientH *_http;
    functionPointer _onFunction[MAX_ON_HANDLERS];
    String _onId[MAX_ON_HANDLERS];
    uint8_t _onIndex = 0;
    String _host = "";
    uint16_t _port = 3000;
    String _authToken = "";

    bool _reuse = true;
    uint16_t _tcpTimeout = HTTPCLIENT_DEFAULT_TCP_TIMEOUT;
    String _headers = "";
    String _cookies = "";
    String _sid = "";

    int _responseCode = 0;
    int _size = -1;
    bool _canReuse = false;

    // socket io
    String sid = "";
    int _packetSize = -1;
    int _packetType = -1;
    int _pingInterval = PING_INTERVAL;
    int _pingTimeout = PING_TIME_OUT;
    String _secWebSocketAccept = "";
    unsigned long _pingTimer = 0;
    unsigned long _lastPingTimeout = 0;
    bool _isPing = false;

    bool isConnected();
    bool tcpConnect();
    int sendRequest(String path, const char *  method = "GET", String payload = "");
    bool sendHeader(String path, const char * method, size_t payloadSize);
    void addHeader(const String& name, const String& value);
    int handleHeaderResponse();
    int writeToStream(Stream * stream);
    int writeToStreamDataBlock(Stream * stream, int size);
    void setCookie(String cookie);
    String getResponseString();

    int handshake();
    int authenticate();
    int connectViaSocket();

    int handleAuthenticateResponse(String data);
    int readSid(String data);

    int readMessageLength();

    void eventHandler();

    int handleError(int error);
    int handleHttpError(int error);
};
#endif // SocketIoClient2_H