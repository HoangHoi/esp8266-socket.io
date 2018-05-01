#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <SocketIOClient.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>
#include <SerialCommand.h>

#define PIN_RESET 2
#define PIN_CONTROL D5  //GPI14
#define RESPONSE_LENGTH 600

#define HOST "device.garagefarmily.com"
//#define IDENTIFY_DEFAULT "GARA-9HH7NGE"
#define PASSWORD_DEFAULT "12344321"
#define HTTP_PORT 80
#define SSID_PREFIX "GARA-"

#define REGISTER_STRING "IRGT"

#define EEPROM_WIFI_SSID_START 0
#define EEPROM_WIFI_SSID_END 50

#define EEPROM_WIFI_PASS_START 51
#define EEPROM_WIFI_PASS_END 100

#define EEPROM_USER_ID_START 101
#define EEPROM_USER_ID_END 117

#define EEPROM_IS_REGISTER_START 118
#define EEPROM_IS_REGISTER_END 128

#define EEPROM_IDENTIFY_START 129
#define EEPROM_IDENTIFY_END 179

#define EEPROM_PASSWORD_START 180
#define EEPROM_PASSWORD_END 230

#define EEPROM_WIFI_SSID_START2 231
#define EEPROM_WIFI_SSID_END2 281

#define EEPROM_WIFI_PASS_START2 282
#define EEPROM_WIFI_PASS_END2 332

#define EEPROM_WIFI_SSID_START3 333
#define EEPROM_WIFI_SSID_END3 383

#define EEPROM_WIFI_PASS_START3 384
#define EEPROM_WIFI_PASS_END3 434


#define LOGIN_TIMEOUT_INTERVAL	60000
#define RECONNECT_TIMEOUT_INTERVAL 60000

#define LOGIN_INTERVAL	3000

//all of function
void authenticated (String payload);
void setupAP();                   //phat ra wifi gara-1, no password
void handleRoot();                    //kiem tra ket noi voi http sever
void handleStatus();                  //kiem tra ket noi voi http sever
void handleWifis();                   //kiem tra cac wifi co the ket noi dc
void handleConnectTo();               //ket noi toi wifi
void handleCleareeprom();             //xoa bo nho EEPROM
void handleOK();                      //sendhearder cho sever
bool testWifi(String nssid, String npass);                  //kiem tra ket noi toi wifi
void login();                         //ket noi http port to get token
void switchOn (String payload);      //
void getstatus (String payload);
void getswifis (String payload);
void handleaction (String payload);
void connectwifi (String payload);
void updatefirmware (String payload);
void restartsystem (String payload);
void Reset_uart();                    //Reset lai he thong qua uart
// void update_url_uart();                //update url via uart
void handleNormalMode();
void handleApMode();
void connectToServer();
bool connectToWifi(String nssid, String npass);
String readEEPROM(int begin, int end);
void writeEEPROM(String data, int begin, int endMax);
void clearEEPROM(int start, int end);
void commitEEPROM();
void sendRejectRequest(String requestId);
void emitStatus(String rqid);
void sendLoginRequest(String identifyCode, String devicePassword);
void connectToUser();
void startConfigServer();
void getSession();
void initConnectSocket();
void onAuthenticated(String payload);
void onUnauthorized(String payload);
void onDisconnect(String payload);
bool checkRegister();
void registerToServer();
void logout();
bool reconnecttoSSID();
void handleheartbeatTimeout();
void handleloginTimeout();
String getAuthToken();
void sendRegisterRequest();
SocketIOClient socket;
SerialCommand sCmd; // Khai báo biến sử dụng thư viện Serial Command
// IPAddress APIP(192, 168, 1, 69);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 255, 0);
String token, authenticateToken, user_id;
unsigned long prevTime;
String data, statusRequestId;
String currentSsid, currentPass;
String currentSsid2, currentPass2;
String currentSsid3, currentPass3;
bool Ssid1 = false,Ssid2 = false,Ssid3 = false;
long int time_reset;
int count_reset;
int flag_status_device;     //kiem tra tinh trang cua device. neu la on thi bang 1, neu la off thi bang 0
long int time_off_device;
ESP8266WebServer server(80);
bool normalMode = true;
bool isLogin = false;
bool isRegister = false;

bool flag_PIN_CONTROL = false;
uint8_t mac[WL_MAC_ADDR_LENGTH];


bool connectssidTimeout = false;
bool loginTimeout = false;
bool hearbeatTimeout = false;
bool resetdeviceTimeout = false;
bool ssidconnected = false;

static unsigned long heartbeat_ms;
static unsigned long login_ms;
static unsigned long reconnect_ms;
static unsigned long timeout_ms;

static unsigned long loginTimer = 0;
static unsigned long reconnectTimer = 0;

int login_counter = 0, reconnect_counter = 0;

String identify;
String password;

void getMacId() {
  String macID;
  WiFi.mode(WIFI_AP);
  WiFi.softAPmacAddress(mac);
  macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  identify = SSID_PREFIX + macID;
}

bool checkRegister() {
    String registerString = readEEPROM(EEPROM_IS_REGISTER_START, EEPROM_IS_REGISTER_END);
	Serial.print("\r\nRegister String: ");
    Serial.println(registerString);
    if (registerString == REGISTER_STRING) {
        //identify = readEEPROM(EEPROM_IDENTIFY_START, EEPROM_IDENTIFY_END);
        Serial.print("\r\nIdentify code: ");
        Serial.println(identify);

        password = readEEPROM(EEPROM_PASSWORD_START, EEPROM_PASSWORD_END);
        Serial.print("\r\nPassword: ");
        Serial.println(password);

        isRegister = true;
        return true;
        Serial.println("checkRegister true");
    }
    Serial.println("checkRegister false");
    isRegister = false;
    return false;
}

void registerToServer() {
    Serial.println("Begin register To Server");
    getSession();
	Serial.println("Log in with default password");
    sendLoginRequest(identify, PASSWORD_DEFAULT);

    if (isLogin) {
        sendRegisterRequest();
    }
    // while(1);
}

void sendRegisterRequest() {
    Serial.println("--------sendRegisterRequest---------");
    char responseChar[RESPONSE_LENGTH];
    String resetPath = F("session/change-password");
    String data = "";
    Serial.println("token:");
    String postResponse = socket.postREST(HOST, HTTP_PORT, resetPath, token, data);
    Serial.println("postResponse: ");
	if(postResponse == "Connect failed"){
		Serial.println(postResponse);
        return;
	}
    // Serial.println(postResponse);

    postResponse.toCharArray(responseChar, RESPONSE_LENGTH);
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(responseChar);
    if (!root.success()) {
        return;
    }

    String status = root["status"];

    if (status != "success") {
        return;
    }

    //String identifyCode = root["data"]["identify_code"];
    String devicePassword = root["new_pass"];
    writeEEPROM(REGISTER_STRING, EEPROM_IS_REGISTER_START, EEPROM_IS_REGISTER_END);
    //writeEEPROM(identifyCode, EEPROM_IDENTIFY_START, EEPROM_IDENTIFY_END);
    writeEEPROM(devicePassword, EEPROM_PASSWORD_START, EEPROM_PASSWORD_END);
    commitEEPROM();
    Serial.println("Da lay dc password: " + devicePassword);
}

void authenticated (String payload){
    Serial.println("device da ket noi voi server: " + payload);
}

void handleRoot() {
    server.send(200, "text/html", "<h1>You are connected</h1>");
}

void handleStatus() {
    String response = "{\"identify_code\":\"";
    response += identify;
    response += "\",\"status\":\"ok\"}";
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json; charset=utf-8", response);
}

void handleWifis() {
    Serial.println("scan start");
    String wifis = "[";

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    yield();
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
            delay(10);
            wifis += "{\"name\":\"";
            wifis += WiFi.SSID(i);
            wifis += "\",\"quality\":\"";
            wifis += WiFi.RSSI(i);
            wifis += "\"}";
            if (i != n - 1) {
                wifis += ",\r\n";
            }
        }
    }
    wifis += "]";
    Serial.println(wifis);
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json; charset=utf-8", wifis);
}

void writeEEPROM(String data, int begin, int endMax) {
    int end = data.length() + begin;
    if (end - 1 > endMax) {
        Serial.println("Size too large");
        return;
    }
    clearEEPROM(begin, endMax);

    for (int i = begin; i < end; i++) {
        EEPROM.write(i, data[i - begin]);
        Serial.print("Wrote EEPROM: ");
        Serial.println(data[i - begin]);
    }
}

String readEEPROM(int begin, int end) {
    Serial.print("Read eeprom: ");
    Serial.print(begin);
    Serial.print(" ");
    Serial.println(end);
    String data;
    char c;
    for (int i = begin; i <= end; ++i) {
        c = (char) EEPROM.read(i);
        if (c != 0) {
            data += c;
        }
    }

    return data;
}

void clearEEPROM(int start, int end) {
    Serial.print("Clearing eeprom: ");
    Serial.print(start);
    Serial.print(" ");
    Serial.println(end);
    for (int i = start; i <= end; ++i) {
        EEPROM.write(i, 0);
    }
}

void commitEEPROM() {
    Serial.println("Commit eeprom");
    EEPROM.commit();
}

bool connectToWifi(String nssid, String npass) {
	int counter;
    if (testWifi(nssid.c_str(), npass.c_str())) {
		if(!Ssid1){
			currentSsid = nssid;
			writeEEPROM(nssid, EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
			currentPass = npass;
			writeEEPROM(npass, EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);
			Serial.println("Done writing SSID 1!");
			return true;
		}
		if(!Ssid2){
			currentSsid2 = nssid;
			writeEEPROM(nssid, EEPROM_WIFI_SSID_START2, EEPROM_WIFI_SSID_END2);
			currentPass2 = npass;
			writeEEPROM(npass, EEPROM_WIFI_PASS_START2, EEPROM_WIFI_PASS_END2);

			Serial.println("Done writing SSID 2!");
			return true;
		}
		if(!Ssid3){
			currentSsid3 = nssid;
			writeEEPROM(nssid, EEPROM_WIFI_SSID_START3, EEPROM_WIFI_SSID_END3);
			currentPass3 = npass;
			writeEEPROM(npass, EEPROM_WIFI_PASS_START3, EEPROM_WIFI_PASS_END3);

			Serial.println("Done writing SSID 3!");
			return true;
		}
    }

    return false;
}

void handleConnectTo() {
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    Serial.println(server.arg("plain"));
    JsonObject& rootData = jsonBuffer.parseObject(server.arg("plain"));
    Serial.println("--------------");

    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
    //server.stop();

    if (rootData.success()) {

        String nssid = rootData["ssid"];
        String npass = rootData["pass"];
        String userId = rootData["user_id"];
        user_id = userId;
        Serial.print("Wifi new name: ");
        Serial.println(nssid);
        Serial.print("Wifi new password: ");
        Serial.println(npass);
        Serial.print("User id: ");
        Serial.println(userId);
        if (connectToWifi(nssid, npass)) {
            Serial.println("");
            Serial.println("writing eeprom user id:");
            writeEEPROM(userId, EEPROM_USER_ID_START, EEPROM_USER_ID_END);
            commitEEPROM();
            connectToServer();
            return;
        }

        Serial.println("Wrong wifi!!!");
        setupAP();
        startConfigServer();
        return;
    }

    Serial.println("Wrong data!!!");
}

void handleOk() {
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Methods", "*");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
}

void handleCleareeprom() {
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.sendHeader("Access-Control-Allow-Methods", "*");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", "<h1>eeprom is cleared</h1>");
    Serial.println("clearing eeprom");
    clearEEPROM(EEPROM_WIFI_SSID_START, EEPROM_WIFI_PASS_END3);
    commitEEPROM();
    Serial.println("eeprom is cleared");
    // ESP.restart();
    // resetBoard();
}

void sendRejectRequest(String requestId) {
    String datasend = "{\"request_id\":\"" + requestId + "\",\"error\":\"0\"}";
    socket.emit("reject_request", datasend);
}

void onAuthenticated(String payload) {
    Serial.println("Authenticated");
    Serial.println(payload);
}

void onUnauthorized(String payload) {
    Serial.println("Unauthorized");
    Serial.println(payload);
}

void onDisconnect(String payload) {
    Serial.println("Disconnect");
    Serial.println(payload);
}

void switchOn(String payload) {
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonArray& root = jsonBuffer.parse(payload);
    if (!root.success()) {
        return;
    }
    String datasend = F("{\"request_id\":\"");
    String id = root[1]["id"];
    if(flag_status_device == 1){
        sendRejectRequest(id);
        return;
    }

    flag_status_device = 1;
    time_off_device = millis() + 3000;
    digitalWrite(PIN_CONTROL, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    datasend +=  id;
    datasend += F("\",\"result\":\"1\"}");
    socket.emit("switch_is_on", datasend);
    statusRequestId = id;
}

void emitStatus(String rqid) {
    String datasend = F("{\"request_id\": \"");
    datasend += rqid;

    if(flag_status_device == 1){
        datasend += F("\", \"status\": \"1\"}");
        return;
    }

    datasend += F("\", \"status\": \"1\"}");
    socket.emit("device_status", datasend); //khi hoat dong binh thuong
}

void getstatus(String payload) {
    if (statusRequestId.length() > 0) {
        emitStatus(statusRequestId);
        statusRequestId = "";
        return;
    }

    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonArray& root = jsonBuffer.parse(payload);
    if (!root.success()) {
        return;
    }
    String id = root[1]["id"];
    emitStatus(id);
}

void getswifis(String payload) {
    Serial.println("-------------");
    String wifis = "{ \"request_id\": \"g_ 1515951068238\", \"wifis\": [";
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    yield();
    for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.print(")");
        Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
        delay(10);
        wifis += "{\"name\":\"";
        wifis += WiFi.SSID(i);
        wifis += "\",\"quality\":\"";
        wifis += WiFi.RSSI(i);
        wifis += "\"}";
        if (i != n - 1) {
            wifis += ",\r\n";
        }
    }
    socket.emit("wifi_list", wifis);
}

void handleaction(String payload) {
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonArray& root = jsonBuffer.parse(payload);
    if (!root.success()) {
        return;
    }
    String datasend = F("{\"request_id\":\"");
    String id = root[1]["id"];
    String command = root[1]["action"];
    datasend += id;
    datasend += F("\" }");

    socket.emit("action_is_taken", datasend);
    if (command == "0") {
        ESP.restart();
    } else if (command == "1") {
        login();
    }
}

void connectwifi(String payload) {
    Serial.println("-------------");
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonArray& root = jsonBuffer.parse(payload);
    if (!root.success()) {
        return;
    }
    String idConnect = root[1]["id"];
    String nssid = root[1]["ssid"];
    String npass = root[1]["pass"];

    if (connectToWifi(nssid, npass)) {
        commitEEPROM();
        connectToServer();
        return;
    }
    if (testWifi(currentSsid.c_str(), currentPass.c_str())) {
        connectToServer();
        return;
    }

    // ESP.restart();
  setup();
}

void updatefirmware(String payload) {
    Serial.println("-------------");
    String urlPath = F("http://");
    urlPath += HOST;
    urlPath += F("/storage/firmwares/");
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonArray& root = jsonBuffer.parse(payload);
    if (!root.success()) {
        return;
    }
    String firmwareId = root[1]["id"];
    String firmwareFile = root[1]["url"];
    String firmwareVersion = root[1]["version"];
    String firmwareUrl = urlPath + firmwareFile;
    Serial.println("---------");
    Serial.println(firmwareUrl);

    String datasend = F("{ \"request_id\": \"");
    datasend += firmwareId;
    datasend += F("\" }");
    socket.emit("firmware_updating", datasend);

    String dataError = F("{\"request_id\":\"");
    dataError += firmwareId;
    dataError += F("\",\"result\":\"0\",\"message\":\"\",\"error_code\":\"1\"}");

    String dataSuccess = F("{\"request_id\":\"");
    dataSuccess += firmwareId;
    dataSuccess += F("\",\"result\":\"1\",\"message\":\"\",\"error_code\":\"0\"}");

    t_httpUpdate_return ret = ESPhttpUpdate.update(firmwareUrl);

    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            socket.emit("firmware_updated", dataError);
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            socket.emit("firmware_updated", dataSuccess);
            break;
    }
}

void restartsystem (String payload){
    ESP.restart();
}

void setupAP() {
    Serial.println("Open AP....");

    WiFi.softAPdisconnect();
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);


    WiFi.softAP(identify.c_str(), "12345678");
    Serial.println(identify);

    Serial.println("softap is running!");
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
}

void startConfigServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/wifis", HTTP_GET, handleWifis);
    server.on("/connect-to", HTTP_POST, handleConnectTo);
    server.on("/", HTTP_OPTIONS, handleOk);
    server.on("/status", HTTP_OPTIONS, handleOk);
    server.on("/wifis", HTTP_OPTIONS, handleOk);
    server.on("/connect-to", HTTP_OPTIONS, handleOk);
    server.on("/cleareeprom", HTTP_GET, handleCleareeprom);
    server.on("/cleareeprom", HTTP_OPTIONS, handleOk);
    server.begin();
    Serial.println("HTTP server started");
}

bool testWifi(String nssid, String npass) {
    int c = 0;
	Serial.println("Wifi Station Mode");
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.println("Waiting for Wifi to connect to " + nssid);
	WiFi.begin(nssid.c_str(), npass.c_str());
    while (c < 20) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\rWifi connected!");
            Serial.print("Local IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("SoftAP IP: ");
            Serial.println(WiFi.softAPIP());
			login_ms = millis();
			connectssidTimeout = false;
			reconnect_counter = 0;
            return true;
        }
        delay(500);
        Serial.print(".");
        c++;
    }
    Serial.println("");
    Serial.println("Connect timed out");
	connectssidTimeout = ((millis()-reconnect_ms)>RECONNECT_TIMEOUT_INTERVAL ? true:false) ;
    return false;
}

void sendLoginRequest(String identifyCode, String devicePassword) {
    char responseChar[RESPONSE_LENGTH];
    String loginPath = F("session/login");
    String data = "{\"identify_code\":\"";
    data += identifyCode;
    data += "\",\"password\":\"";
    data += devicePassword;
    data += "\"}";
	Serial.println(data);
    // Serial.println("token: ");
    String postResponse = socket.postREST(HOST, HTTP_PORT, loginPath, token, data);
	if(postResponse == "Connect failed"){
		Serial.println("postResponse: " + postResponse);
        return;
	}
    // Serial.println("postResponse: ");
    // Serial.println(postResponse);
    postResponse.toCharArray(responseChar, RESPONSE_LENGTH);
    StaticJsonBuffer<RESPONSE_LENGTH> postJsonBuffer;
    JsonObject& postRoot = postJsonBuffer.parseObject(responseChar);
    if (!postRoot.success()) {
		Serial.println("Root fail!!! ");
        isLogin = false;
        return;
    }
    String loginStatus = postRoot["status"];
    Serial.println(loginStatus);
    if (loginStatus != "success") {
        Serial.println("Login fail!!! ");
		isLogin = false;
        return;
    }

    String newAuthToken = postRoot["data"]["auth_token"];
    authenticateToken = newAuthToken;
	loginTimeout = false;
    isLogin = true;
	login_counter = 0;
	if(devicePassword == PASSWORD_DEFAULT)
		Serial.println("dang nhap voi mat khau defaut done!");
    if (isRegister && (int)(EEPROM.read(EEPROM_USER_ID_START)) != 0) {
        connectToUser();
    }
}

void connectToUser() {
    Serial.println("ADD DEVICE ON SEVER");
    String userId = readEEPROM(EEPROM_USER_ID_START, EEPROM_USER_ID_END);
    Serial.println(userId);
    String connectuserPath = F("connect-user/");
    connectuserPath += userId;
    String getResponse = socket.getREST(HOST, HTTP_PORT, connectuserPath);
	if(getResponse == "Connect failed"){
		Serial.println("getResponse: " + getResponse);
        return;
	}
    // Serial.println("Day la chuoi REST " + getResponse);
    clearEEPROM(EEPROM_USER_ID_START, EEPROM_USER_ID_END);
    commitEEPROM();
    logout();
    isLogin = false;
}

void getSession() {
    String path = "session";
    char responseChar[RESPONSE_LENGTH];
    String getResponse = socket.getREST(HOST, HTTP_PORT, path);
    // Serial.println("Day la chuoi REST " + getResponse);
	if(getResponse == "Connect failed"){
		Serial.println("getResponse: " + getResponse);
		isLogin = false;
        return;
	}
    getResponse.toCharArray(responseChar, RESPONSE_LENGTH);
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(responseChar);
    if (!root.success()) {
        isLogin = false;
        return;
    }
    String newToken = root["token"];
    String status = root["status"];
    token = newToken;
    // Serial.println("Chuoi token: ");

    if (status == "success") {
        String newAuthToken1 = root["data"]["auth_token"];
        authenticateToken = newAuthToken1;
        isLogin = true;
		Serial.println("Session passed");
        return;
    }
	Serial.println("Session failed");
    isLogin = false;
}

void logout() {
    Serial.println("Begin logout");
    getSession();

    if (!isLogin) {
        return;
    }

    String loginPath = F("session/logout");
    String data = "";
    String postResponse = socket.postREST(HOST, HTTP_PORT, loginPath, token, data);
	if(postResponse == "Connect failed"){
		Serial.println("postResponse: " + postResponse);
		isLogin = false;
        return;
	}
    isLogin = false;
}

void login() {
    Serial.println("Begin login");
    getSession();

    if (!isLogin) {
        sendLoginRequest(identify, password);
    }

    // Serial.println("Auth token: ");
    // Serial.println(authenticateToken);
}

void connectSocket() {
    // server.stop();
    socket.setAuthToken(authenticateToken);
    socket.connect(HOST, 3000);
}

String getAuthToken()  {
    String path = "session/auth-token";
    String getResponse = socket.getREST(HOST, HTTP_PORT, path);
	if(getResponse == "Connect failed"){
		Serial.println("getResponse: " + getResponse);
        return getResponse;
	}
    return getResponse;
}

void initConnectSocket() {
    //nhan du lieu tu sever
    socket.on("authenticated", onAuthenticated);
    socket.on("unauthorized", onUnauthorized);
    // socket.on("disconnect", onDisconnect);

    socket.on("switch_on", switchOn); //thay doi trang thai cua device
    socket.on("get_status", getstatus); //lay trang thai hien tai cua device
    // socket.on("get_wifis", getswifis); //lay trang cac wifi xung quanh
    socket.on("handle_action", handleaction); //thuc hien cac hanh dong khac nhu restart, reconnect...
    // socket.on("connect_wifi", connectwifi); //thuc hien ket noi toi mang wifi ma sever gui ve
    socket.on("update_firmware", updatefirmware); //update firmware
    // socket.on("restart_system", restartsystem);          //restart lai he thong vao cuoi ngay
}

void Reset_uart() {
    char *arg = sCmd.next();
    Serial.println("clearing eeprom");
    clearEEPROM(0, 96);
    commitEEPROM();
    ESP.restart();
}

void beginServerResetEERom() {
    server.stop();
    server.on("/cleareeprom", HTTP_GET, handleCleareeprom);
    server.on("/cleareeprom", HTTP_OPTIONS, handleOk);
    server.begin();
}

void connectToServer() {
    Serial.println("Begin connectToServer");
    normalMode = true;
    isLogin = false;
    beginServerResetEERom();
    if (!checkRegister()) {
        registerToServer();
        // ESP.restart();
		setup();
        return;
    }
}
bool reconnecttoSSID(){
	while(WiFi.status() != WL_CONNECTED)
	{
		if (Ssid1) {
			if (testWifi(currentSsid.c_str(), currentPass.c_str())) {
				connectToServer();
				return true;
			}
		}
		if (Ssid2) {
			if (testWifi(currentSsid2.c_str(), currentPass2.c_str())) {
				connectToServer();
				return true;
			}
		}
		if (Ssid3) {
			if (testWifi(currentSsid3.c_str(), currentPass3.c_str())) {
				connectToServer();
				return true;
			}
		}
		else if((!Ssid1)&&(!Ssid2)&&(!Ssid3))
			return false;
	}
	// ESP.restart();
}
void setup() {
    getMacId();
    // put your setup code here, to run once:
    //pinMode(PIN_RESET, INPUT);
    pinMode(PIN_CONTROL, INPUT);     // Initialize the PIN_CONTROL pin as an output
    pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
    Serial.begin(115200);
    EEPROM.begin(4096);
    ArduinoOTA.begin();
    delay(10);
    //WiFi.softAPConfig(APIP, gateway, subnet);
    // WiFi.softAPdisconnect();
    // WiFi.disconnect();
    delay(100);
    Serial.println();
    Serial.println();
    Serial.println("Startup");

    // read eeprom for ssid and pass
    Serial.println("Reading Wifi list from EEPROM");
    currentSsid = readEEPROM(EEPROM_WIFI_SSID_START, EEPROM_WIFI_SSID_END);
	Ssid1 = (currentSsid.length()>1 ? true:false);
	currentPass = readEEPROM(EEPROM_WIFI_PASS_START, EEPROM_WIFI_PASS_END);

	currentSsid2 = readEEPROM(EEPROM_WIFI_SSID_START2, EEPROM_WIFI_SSID_END2);
	Ssid2 = (currentSsid2.length()>1 ? true:false);
	currentPass2 = readEEPROM(EEPROM_WIFI_PASS_START2, EEPROM_WIFI_PASS_END2);

	currentSsid3 = readEEPROM(EEPROM_WIFI_SSID_START3, EEPROM_WIFI_SSID_END3);
	Ssid3 = (currentSsid3.length()>1 ? true:false);
	currentPass3 = readEEPROM(EEPROM_WIFI_PASS_START3, EEPROM_WIFI_PASS_END3);

    Serial.println("WIFI1 SSID: " + currentSsid + " PASS: " + currentPass);
	Serial.println("WIFI2 SSID: " + currentSsid2 + " PASS: " + currentPass2);
	Serial.println("WIFI3 SSID: " + currentSsid3 + " PASS: " + currentPass3);

    sCmd.addCommand("Reset", Reset_uart);     // ham nay dung de nhan data tu STM
    initConnectSocket();

	reconnect_ms = millis();
	if (Ssid1) {
		if (testWifi(currentSsid.c_str(), currentPass.c_str())) {
			connectToServer();
			return;
		}
	}
	if (Ssid2) {
		if (testWifi(currentSsid2.c_str(), currentPass2.c_str())) {
			connectToServer();
			return;
		}
	}
	if (Ssid3) {
		if (testWifi(currentSsid3.c_str(), currentPass3.c_str())) {
			connectToServer();
			return;
		}
	}

    setupAP();
    startConfigServer();
    normalMode = false;
}

void handleNormalMode() {
    sCmd.readSerial();
    server.handleClient();
    if ((!isLogin)&&(!loginTimeout)) {
        login();
        connectSocket();
        return;
    }
    socket.monitor();

    //neu bat cong tac thi sau 3s se tat
    if(flag_status_device == 1 && time_off_device <= millis()){
        flag_status_device = 0;
        digitalWrite(PIN_CONTROL, LOW);
        digitalWrite(LED_BUILTIN, LOW);
        String datasend = "{ \"request_id\": \""+ statusRequestId + "\" }";
        socket.emit("switch_is_off", datasend);
    }
}

void handleApMode() {
    server.handleClient();
    sCmd.readSerial();
}
void handleheartbeatTimeout(){
	switch (login_counter){
		case 0:
			loginTimer = millis();
			loginTimeout = false;
			isLogin = false;
			login_counter ++;
			break;
		case 10:
			loginTimeout = true;
			login_counter = 0;
			normalMode = false;
			break;
		default:
			if(millis() >= loginTimer){
				Serial.println("Try to login to server..." + login_counter);
				login();
				connectSocket();
				loginTimer = millis() + LOGIN_INTERVAL;
				login_counter++;
			}
			break;
	}
}
void handleloginTimeout(){
	Serial.println("Login Timeout");
	Serial.println("Try to reconnect to ssid..." + reconnect_counter++);
	reconnect_ms = millis();
	connectssidTimeout = false;
	if(reconnecttoSSID()){
		loginTimeout = false;
	};
}
void loop() {
    if (digitalRead(PIN_CONTROL) == LOW && flag_PIN_CONTROL == false) {
        String datasend = "{ \"request_id\": \""+ statusRequestId + "\", \"status\": \"0\" }";
        socket.emit("device_status", datasend);
        flag_PIN_CONTROL = true;
        flag_status_device = 0;
        digitalWrite(LED_BUILTIN, LOW);
        Serial.println("-------thiet bi da dong----------------");
    }

    if (digitalRead(PIN_CONTROL) == HIGH && flag_PIN_CONTROL == true) {
        String datasend = "{ \"request_id\": \""+ statusRequestId + "\", \"status\": \"1\" }";
        socket.emit("device_status", datasend);
        flag_PIN_CONTROL = false;
        flag_status_device = 1;
        digitalWrite(LED_BUILTIN, HIGH);
        Serial.println("-------thiet bi da mo----------------");
    }
	server.handleClient();
	if((socket.heartbeatTimeout)&&(!loginTimeout)) {
		handleheartbeatTimeout();
		return;
	}
    if (normalMode) {
        handleNormalMode();
        return;
    }
	if(loginTimeout){
		handleloginTimeout();
		return;
	}
}
