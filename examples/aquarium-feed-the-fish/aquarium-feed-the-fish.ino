#include <SocketIOClient.h>

#define FEED_MOTOR D3
#define FEED_BUTTON D7
#define THRESHOLD 3000

bool feedMotorRun = false;
unsigned long feedRunTime = 0;
bool runStopFeedMotor = false;
bool runStartFeedMotor = false;

unsigned long runTime = 0;

//const char* ssid = "Tang-4";
//const char* password = "!@#$%^&*o9";

//const char HexLookup[17] = "0123456789ABCDEF";
//
//String host = "aquarium-nodejs.herokuapp.com";
//int port = 80;
//bool clicked = false;
//
//SocketIOClient socket;
//
//void setupNetwork() {

//    WiFi.begin(ssid, password);
//    uint8_t i = 0;
//    while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
//    if (i == 21) {
//        while (1) delay(500);
//    }
//
//}

void changeFeedState(uint8_t val) {
    digitalWrite(FEED_MOTOR, val);
}

void stopFeedMotor() {
    changeFeedState(LOW);
    feedMotorRun = false;
    feedRunTime = 0;
    runStopFeedMotor = false;
    Serial.println("Stop feed the fish");
}

void startFeedMotor() {
    changeFeedState(HIGH);
    feedMotorRun = true;
    feedRunTime = millis();
    runStartFeedMotor = false;
    Serial.println("Start feed the fish");
}

void feedInterrupt() {
    if (millis() > feedRunTime + THRESHOLD && feedMotorRun) {
        runStopFeedMotor = true;
    }
}
//void click() {
//    clicked = true;
//}

//void light(String state) {
//    Serial.println("[light] " + state);
//    if (state == "\"state\":true") {
//        Serial.println("[light] ON");
//        digitalWrite(LedPin, HIGH);
//    } else {
//        Serial.println("[light] off");
//        digitalWrite(LedPin, LOW);
//    }
//}

//
// This code runs only once
//

void setup() {

    pinMode(FEED_MOTOR, OUTPUT);

    pinMode(FEED_BUTTON, INPUT);
    attachInterrupt(digitalPinToInterrupt(FEED_BUTTON), feedInterrupt, RISING);
    // set up our pins
    //    pinMode(LedPin, OUTPUT);
    //    pinMode(ButtonPin, INPUT);
    //
    //    digitalWrite(LedPin, LOW);
    //
        Serial.begin(115200);
    //
    //    setupNetwork();
    //
    //    attachInterrupt(digitalPinToInterrupt(ButtonPin), click, FALLING);
    //    socket.setChannel("feed-the-fish");
    //    socket.on("App\\Events\\FeedTheFish", light);

    //    socket.connect(host, port);
    runTime = millis();
}

//void clickCheck() {
//    if (clicked) {
//        Serial.println("[click]");
//        socket.emit("toggle", "{\"state\":true}");
//        clicked = false;
//    }
//}

//
// This code runs over and over again
//

void loop() {
    if (runStartFeedMotor) {
        startFeedMotor();
    }

    if (runStopFeedMotor) {
        stopFeedMotor();
    }

    if (millis() > runTime + 10000) {
        runStartFeedMotor = true;
        runTime = millis();
    }
    //    socket.monitor();
    //    clickCheck();
}


