#include <ESP8266WiFi.h>
#include <SocketIOClient2.h>

#define HOST "garage.lc"
#define PORT 3000

SocketIOClient2 socket;

const char* ssid = "Tang-4";
const char* password = "!@#$%^&*o9";

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

void abcHandle(String data) {
    Serial.print("Data: ");
    Serial.println(data);
}

void setup() {
    Serial.begin(115200);
    setupNetwork();
    socket.begin(HOST, PORT);
    // socket.on("abc", abcHandle);
    socket.connect();
}

void loop() {
}
