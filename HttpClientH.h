#ifndef HttpClientH_H_
#define HttpClientH_H_

#include <memory>
#include <Arduino.h>
#include <WiFiClient.h>

#define HTTPCLIENT_DEFAULT_TCP_TIMEOUT (5000)

/// HTTP client errors
#define HTTPC_ERROR_CONNECTION_REFUSED  (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED  (-2)
#define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
#define HTTPC_ERROR_NOT_CONNECTED       (-4)
#define HTTPC_ERROR_CONNECTION_LOST     (-5)
#define HTTPC_ERROR_NO_STREAM           (-6)
#define HTTPC_ERROR_NO_HTTP_SERVER      (-7)
#define HTTPC_ERROR_TOO_LESS_RAM        (-8)
#define HTTPC_ERROR_ENCODING            (-9)
#define HTTPC_ERROR_STREAM_WRITE        (-10)
#define HTTPC_ERROR_READ_TIMEOUT        (-11)

/// size for the stream handling
#define HTTP_TCP_BUFFER_SIZE (1460)

#define DEBUG

/// HTTP codes see RFC7231
typedef enum {
    HTTP_CODE_CONTINUE = 100,
    HTTP_CODE_SWITCHING_PROTOCOLS = 101,
    HTTP_CODE_PROCESSING = 102,
    HTTP_CODE_OK = 200,
    HTTP_CODE_CREATED = 201,
    HTTP_CODE_ACCEPTED = 202,
    HTTP_CODE_NON_AUTHORITATIVE_INFORMATION = 203,
    HTTP_CODE_NO_CONTENT = 204,
    HTTP_CODE_RESET_CONTENT = 205,
    HTTP_CODE_PARTIAL_CONTENT = 206,
    HTTP_CODE_MULTI_STATUS = 207,
    HTTP_CODE_ALREADY_REPORTED = 208,
    HTTP_CODE_IM_USED = 226,
    HTTP_CODE_MULTIPLE_CHOICES = 300,
    HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_FOUND = 302,
    HTTP_CODE_SEE_OTHER = 303,
    HTTP_CODE_NOT_MODIFIED = 304,
    HTTP_CODE_USE_PROXY = 305,
    HTTP_CODE_TEMPORARY_REDIRECT = 307,
    HTTP_CODE_PERMANENT_REDIRECT = 308,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_UNAUTHORIZED = 401,
    HTTP_CODE_PAYMENT_REQUIRED = 402,
    HTTP_CODE_FORBIDDEN = 403,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_METHOD_NOT_ALLOWED = 405,
    HTTP_CODE_NOT_ACCEPTABLE = 406,
    HTTP_CODE_PROXY_AUTHENTICATION_REQUIRED = 407,
    HTTP_CODE_REQUEST_TIMEOUT = 408,
    HTTP_CODE_CONFLICT = 409,
    HTTP_CODE_GONE = 410,
    HTTP_CODE_LENGTH_REQUIRED = 411,
    HTTP_CODE_PRECONDITION_FAILED = 412,
    HTTP_CODE_PAYLOAD_TOO_LARGE = 413,
    HTTP_CODE_URI_TOO_LONG = 414,
    HTTP_CODE_UNSUPPORTED_MEDIA_TYPE = 415,
    HTTP_CODE_RANGE_NOT_SATISFIABLE = 416,
    HTTP_CODE_EXPECTATION_FAILED = 417,
    HTTP_CODE_MISDIRECTED_REQUEST = 421,
    HTTP_CODE_UNPROCESSABLE_ENTITY = 422,
    HTTP_CODE_LOCKED = 423,
    HTTP_CODE_FAILED_DEPENDENCY = 424,
    HTTP_CODE_UPGRADE_REQUIRED = 426,
    HTTP_CODE_PRECONDITION_REQUIRED = 428,
    HTTP_CODE_TOO_MANY_REQUESTS = 429,
    HTTP_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    HTTP_CODE_NOT_IMPLEMENTED = 501,
    HTTP_CODE_BAD_GATEWAY = 502,
    HTTP_CODE_SERVICE_UNAVAILABLE = 503,
    HTTP_CODE_GATEWAY_TIMEOUT = 504,
    HTTP_CODE_HTTP_VERSION_NOT_SUPPORTED = 505,
    HTTP_CODE_VARIANT_ALSO_NEGOTIATES = 506,
    HTTP_CODE_INSUFFICIENT_STORAGE = 507,
    HTTP_CODE_LOOP_DETECTED = 508,
    HTTP_CODE_NOT_EXTENDED = 510,
    HTTP_CODE_NETWORK_AUTHENTICATION_REQUIRED = 511
} t_http_codes;

typedef enum {
    HTTPC_TE_IDENTITY,
    HTTPC_TE_CHUNKED
} transferEncoding_t;

class TransportTrait;
typedef std::unique_ptr<TransportTrait> TransportTraitPtr;

class HttpClientH
{
public:
    HttpClientH();
    ~HttpClientH();
    bool begin(String host);
    bool begin(String host, String httpsFingerprint);
    bool begin(String host, uint16_t port);
    bool begin(String host, uint16_t port, String httpsFingerprint);

    void setFingerprint(String httpsFingerprint);

    void clear();
    void end();


    void setReuse(bool reuse);

    int get(String path);
    int post(String path, String payload);

    String getResponseString();
    int getResponseCode();

protected:
    struct Argument {
        String key;
        String value;
    };

    bool connect();
    bool connected();
    int sendRequest(String path, const char *  method, String payload);
    int sendRequest(String path, const char * method = "GET", uint8_t * payload = NULL, size_t payloadSize = 0);
    bool sendHeader(String path, const char * method, size_t payloadSize);
    int returnError(int error);

    int handleHeaderResponse();
    void setCookie(String cookie);
    int writeToStream(Stream * stream);
    int writeToStreamDataBlock(Stream * stream, int size);


    void printFreeHeap();

    std::unique_ptr<WiFiClient> _tcp;
    TransportTraitPtr _transportTrait;

    String _host;
    uint16_t _port = 0;
    bool _reuse = true;
    uint16_t _tcpTimeout = HTTPCLIENT_DEFAULT_TCP_TIMEOUT;
    Argument* _headers = nullptr;
    size_t _headersCount = 0;
    String _cookies = "";
    // size_t _cookiesCount = 0;

    String _token = "";

    int _responseCode = 0;
    int _size = -1;
    bool _canReuse = false;

    Argument* _responseHeaders = nullptr;
    size_t           _responseHeadersCount = 0;
    transferEncoding_t _transferEncoding = HTTPC_TE_IDENTITY;
};


#endif /* HttpClientH_H_ */