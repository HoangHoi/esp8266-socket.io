#ifndef SESSION_H_
#define SESSION_H_

#include <HttpClientH.h>

// Request status
#define REQUEST_OK (1)
#define ERROR_NOT_FOUND (-1)
#define ERROR_UNAUTHENTICATED (-2)
#define ERROR_TOKEN_MISMATCH (-3)
#define ERROR_PARSE_JSON_ERROR (-4)
#define UNKNOWN_ERROR (-5)

#define RESPONSE_CODE_OK 200
#define RESPONSE_CODE_NOT_FOUND 404
#define RESPONSE_CODE_UNAUTHENTICATED 401
#define RESPONSE_CODE_TOKEN_MISMATCH 400

#define DEBUG

#define HOST "device.garage.lc"
#define PORT 80

#define SESSION_PATH "/session"
#define LOGIN_PATH "/session/login"
#define LOGOUT_PATH "/session/logout"
#define AUTH_TOKEN_PATH "/session/auth-token"
#define CONNECT_TO_USER_PATH "/connect-user/"

#define JSON_BUFFER_LENGTH 1024

class Session
{
    public:
        Session();
        ~Session();
        void setup(HttpClientH *http);

        // Send request
        int login(String payload);
        int login(String username, String password);
        int login(const char * username, const char * password);
        int getSession();
        int logout();
        int authToken();

        int azLogin(String username, String password);
        int azLogin(const char * username, const char * password);
        int azLogin(String payload);

        int connectUser(String userId);

        // Get data
        String getAuthToken();
        // String getToken();
        bool getLoginStatus();
    protected:
        HttpClientH *_http;
        bool _isLogin = false;
        // String _token = "";
        String _authToken = "";

        int parseSession(String sessionResponse);
        int parseLogin(String sessionResponse);
        int parseLogout(String response);
        int parseConnectUser(String response);
        int parseAuthToken(String response);

        int handleResponseError(int error);
        void handleErrorTokenMismatch();
        void handleErrorUnauthenticated();

        // Helpper
        int returnError(int error);
        void renewToken(String newToken);
};

#endif /* SESSION_H_ */
