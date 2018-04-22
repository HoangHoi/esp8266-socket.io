#include <Arduino.h>
// #include <ESP8266WiFi.h>
// #include <WiFiClientSecure.h>
// #include <StreamString.h>
// #include <base64.h>
#include "Session.h"
#include <ArduinoJson.h>

#ifdef DEBUG
#define ECHO(m) Serial.println(m)
#define FREE_HEAP() ({ECHO(String(ESP.getFreeHeap()));})
#else
#define ECHO(m)
#endif

/**
 * constructor
 */
Session::Session()
{
}

/**
 * destructor
 */
Session::~Session()
{
}

void Session::setup(HttpClientH *http)
{
    _http = http;
    _http->begin(HOST, PORT);
}

int Session::getSession()
{
    int statusCode = _http->get(SESSION_PATH);
    if (statusCode == RESPONSE_CODE_OK) {
        return parseSession(_http->getResponseString());
    }
    return handleResponseError(statusCode);
}

int Session::login(String username, String password)
{
    String payload = "{\"identify_code\":\"";
    payload += username;
    payload += "\",\"password\":\"";
    payload += password;
    payload += "\"}";
    return login(payload);
}

int Session::login(const char * username, const char * password)
{
    String payload = "{\"identify_code\":\"";
    payload += username;
    payload += "\",\"password\":\"";
    payload += password;
    payload += "\"}";
    return login(payload);
}

int Session::login(String payload)
{
    ECHO("[Session][login] Login with data:");
    ECHO(payload);
    int statusCode = _http->post(LOGIN_PATH, payload);

    if (statusCode == RESPONSE_CODE_OK) {
        return parseLogin(_http->getResponseString());
    }

    return handleResponseError(statusCode);
}

int Session::azLogin(const char * username, const char * password)
{
    String payload = "{\"identify_code\":\"";
    payload += username;
    payload += "\",\"password\":\"";
    payload += password;
    payload += "\"}";
    return azLogin(payload);
}

int Session::azLogin(String username, String password)
{
    String payload = "{\"identify_code\":\"";
    payload += username;
    payload += "\",\"password\":\"";
    payload += password;
    payload += "\"}";
    return azLogin(payload);
}

int Session::azLogin(String payload)
{
    int sessionResponse = getSession();
    if (sessionResponse) {
        int loginResponse = login(payload);
        authToken();
        return loginResponse;
    }

    return sessionResponse;
}

int Session::logout()
{
    int statusCode = _http->post(LOGOUT_PATH, "");

    if (statusCode == RESPONSE_CODE_OK) {
        return parseLogout(_http->getResponseString());
    }

    return handleResponseError(statusCode);
}

int Session::connectUser(String userId)
{
    String connectuserPath = CONNECT_TO_USER_PATH;
    connectuserPath += userId;

    int statusCode = _http->get(connectuserPath);
    if (statusCode == RESPONSE_CODE_OK) {
        return parseConnectUser(_http->getResponseString());
    }

    return handleResponseError(statusCode);
}

int Session::authToken()
{
    int statusCode = _http->get(AUTH_TOKEN_PATH);
    if (statusCode == RESPONSE_CODE_OK) {
        return parseAuthToken(_http->getResponseString());
    }

    return handleResponseError(statusCode);
}

String Session::getAuthToken()
{
    return _authToken;
}

// String Session::getToken()
// {
//     return _token;
// }

bool Session::getLoginStatus()
{
    return _isLogin;
}

int Session::parseSession(String sessionResponse)
{
    const size_t responseSize = sessionResponse.length();
    ECHO("[Session][parseSession] Begin parse session");
    ECHO(sessionResponse);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonObject& root = jsonBuffer.parseObject(sessionResponse.c_str());
    if (!root.success()) {
        return returnError(ERROR_PARSE_JSON_ERROR);
    }

    String status = root["status"];
    ECHO(status);
    if (status != "success") {
        return returnError(UNKNOWN_ERROR);
    }

    String newToken = root["token"];
    renewToken(newToken);

    String sessionStatus = root["session"];
    if (sessionStatus == "login") {
        _isLogin = true;
    } else {
        _isLogin = false;
    }

    ECHO(String(_isLogin));

    FREE_HEAP();

    return REQUEST_OK;
}

int Session::parseLogin(String response)
{
    const size_t responseSize = response.length();
    ECHO("[Session][parseLogin] Begin parse login");
    ECHO(response);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonObject& root = jsonBuffer.parseObject(response.c_str());
    if (!root.success()) {
        return returnError(ERROR_PARSE_JSON_ERROR);
    }

    String status = root["status"];
    ECHO(status);
    if (status != "success") {
        return returnError(UNKNOWN_ERROR);
    }

    String sessionStatus = root["session"];
    if (sessionStatus == "login") {
        _isLogin = true;
    } else {
        _isLogin = false;
    }

    ECHO(String(_isLogin));

    FREE_HEAP();

    return REQUEST_OK;
}

int Session::parseLogout(String response)
{
    const size_t responseSize = response.length();
    ECHO("[Session][parseLogin] Begin parse login");
    ECHO(response);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonObject& root = jsonBuffer.parseObject(response.c_str());
    if (!root.success()) {
        return returnError(ERROR_PARSE_JSON_ERROR);
    }

    String status = root["status"];
    ECHO(status);
    if (status != "success") {
        return returnError(UNKNOWN_ERROR);
    }

    _isLogin = false;
    ECHO("[Session][parseLogin] Logout success");

    FREE_HEAP();

    return REQUEST_OK;
}

int Session::parseConnectUser(String response)
{
    const size_t responseSize = response.length();
    ECHO("[Session][parseConnectUser] Begin parse login");
    ECHO(response);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonObject& root = jsonBuffer.parseObject(response.c_str());
    if (!root.success()) {
        return returnError(ERROR_PARSE_JSON_ERROR);
    }

    String status = root["status"];
    ECHO(status);
    if (status != "success") {
        return returnError(UNKNOWN_ERROR);
    }

    ECHO("[Session][parseLogin] Connect success");

    FREE_HEAP();

    return REQUEST_OK;
}

int Session::parseAuthToken(String response)
{
    FREE_HEAP();
    ECHO("[Session][parseAuthToken] Parse Auth Token");
    ECHO(response);

    if (response.length() > 0) {
        _authToken = response;
        FREE_HEAP();
        return REQUEST_OK;
    }

    return returnError(UNKNOWN_ERROR);
}

int Session::returnError(int error)
{
    if(error < 0) {
        ECHO(String("[Session][returnError] error") + String(error));
    }
    return error;
}

int Session::handleResponseError(int statusCode)
{
    int error;
    switch (statusCode) {
        case RESPONSE_CODE_TOKEN_MISMATCH:
            error = ERROR_TOKEN_MISMATCH;
            handleErrorTokenMismatch();
            break;
        case RESPONSE_CODE_UNAUTHENTICATED:
            handleErrorUnauthenticated();
            error = ERROR_UNAUTHENTICATED;
            break;
        case RESPONSE_CODE_NOT_FOUND:
            error = ERROR_NOT_FOUND;
            break;
        default: error = UNKNOWN_ERROR;
    }

    return returnError(error);
}

void Session::handleErrorTokenMismatch()
{
    String response = _http->getResponseString();

    const size_t responseSize = response.length();
    ECHO("[Session][handleErrorTokenMismatch] Begin parse response");
    ECHO(response);
    FREE_HEAP();
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    FREE_HEAP();
    JsonObject& root = jsonBuffer.parseObject(response.c_str());
    if (!root.success()) {
        return;
    }

    String status = root["status"];
    ECHO(status);
    if (status != "error") {
        return;
    }

    String newToken = root["token"];
    renewToken(newToken);
}

void Session::handleErrorUnauthenticated()
{
    _isLogin = false;
    _authToken = "";
}

void Session::renewToken(String newToken)
{
    if (newToken.length() > 0) {
        _http->addHeader("X-CSRF-TOKEN", newToken);
        // _token = newToken;
    }
}
