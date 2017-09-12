#include <SocketIOClient.h>
#include <ArduinoJson.h>

#define POWER D3
#define LED_PIN D4
#define RESPONSE_LENGTH 500

const char* ssid = "Tang-4";
const char* password = "!@#$%^&*o9";
String host = "device.garage.lc";
int httpPort = 80;
String token, authenticateToken;
unsigned long prevTime;
long interval = 5000;
SocketIOClient socket;
StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;

void authenticated (String payload){
  Serial.println("device da ket noi voi server: " + payload);
}
void setupNetwork() {
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    setupNetwork();
    String path = "session";
    char responseChar[RESPONSE_LENGTH];
    char postResponseChar[RESPONSE_LENGTH];
    String response = socket.getREST(host, httpPort, path);
    Serial.println("Day la chuoi REST "+response);

    response.toCharArray(responseChar, RESPONSE_LENGTH);
    JsonObject& root = jsonBuffer.parseObject(responseChar);

    String newToken = root["token"];
    token = newToken;
    Serial.println("Chuoi token: " + token);

    char*  loginPath = "session/login";
    char*  data = "{\"identify_code\":\"gara-1\",\"password\":\"12344321\"}";
    String postResponse = socket.postREST(host, httpPort, loginPath, token, data);
    Serial.println("Day la chuoi postREST " + postResponse);

    postResponse.toCharArray(postResponseChar, RESPONSE_LENGTH);
    JsonObject& postRoot = jsonBuffer.parseObject(postResponseChar);

    String newAuthToken = postRoot["data"]["auth_token"];
    Serial.println("Day la chuoi newAuthToken ");
    Serial.println(newAuthToken);
    authenticateToken = newAuthToken;

    String auth = "authenticate";
    String auth_token_data = "{\"token\": \"";
    auth_token_data += authenticateToken;
    auth_token_data += "\"}";
    Serial.println("Auth token: ");
    Serial.println(authenticateToken);
    socket.setAuthToken(authenticateToken);
    socket.connect(host, 3000);

}

void loop() {
  if(prevTime + interval < millis() || prevTime == 0){
    prevTime = millis();
    socket.emit("current_state", "{\"temperature\":\"39*C\"}");
  }
  socket.monitor();
}
