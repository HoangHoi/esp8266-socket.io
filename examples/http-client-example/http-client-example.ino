#include <ESP8266WiFi.h>
#include <HttpClientH.h>

HttpClientH http;

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

void setup() {
    Serial.begin(115200);
    setupNetwork();
    http.begin("garagefarmily.com");
}

void loop() {
    http.get("/session");
    Serial.println(http.getResponseString());
}
