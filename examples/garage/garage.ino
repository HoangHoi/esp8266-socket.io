#include <ESP8266WiFi.h>
#include <HttpClientH.h>
#include <Session.h>
#include <SocketIOClient2.h>

#define HOST "device.garage.lc"
#define PORT 3000

HttpClientH http;
Session session;
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

void setup() {
    Serial.begin(115200);
    setupNetwork();
    session.setup(&http);
    // session.getSession();
    // session.login("", "12344321");
    // session.login("ASDF", "12344321");
    // session.getSession();
    session.azLogin("ASDF", "12344321");

    Serial.print("Auth token: ");
    Serial.println(session.getAuthToken());

    socket.begin(HOST, PORT);
    socket.setAuthToken(session.getAuthToken());
    socket.connect();
    // session.connectUser("1");
    // session.logout();
    // session.getSession();
}

void loop() {
    socket.monitor();
}
