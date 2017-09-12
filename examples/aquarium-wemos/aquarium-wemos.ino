#include <ESP8266WiFi.h>

#define DATA_BUFFER_LEN 1024

const char* ssid = "Tang-4";
const char* password = "!@#$%^&*o9";
String host = "iot-farm.vn";
String cookie = "";
int httpPort = 80;
unsigned long prevTime;
long interval = 5000;
String path = "device/session";
WiFiClient internets;
char databuffer[DATA_BUFFER_LEN];
char *dataptr;
// Hàm chờ response
bool waitForInput(void) {
    unsigned long now = millis();

    // Chờ cho đến khi có dữ liệu trong internets hoặc là thời gian chờ > 30s
    while (!internets.available() && ((millis() - now) < 30000UL)) {
        ;
    }
    return internets.available();
}

// Hàm đọc từng dòng của dữ liệu trả về
void readLine() {
    dataptr = databuffer;

    // Khi mà dữ liệu vẫn còn hoặc chưa tràn bộ nhớ thì đọc tiếp
    while (internets.available() != false && (dataptr < &databuffer[DATA_BUFFER_LEN - 2])) {
        char c = internets.read();
        // Nếu gặp ký tự xuống dòng thì không đọc nữa.
        if (c == '\r') {
            break;
        } else if (c != '\n') {
            // Nếu không phải ký tự xuống dòng thì thêm dữ liệu vào databuffer
            *dataptr++ = c;
        }
    }
    *dataptr = 0;
}

void receiveGetREST() {
    // Chờ response trả về
    if (!waitForInput()) {
        Serial.println(F("[getRest] Time out"));
    }

    while (internets.available()) {
        readLine();
        // In các dòng dữ liệu ra cửa sổ Serial
        Serial.println(databuffer);
    }

    // Sau khi hoàn tất hãy nhớ đóng kết nối
    internets.stop();
    delay(100);
    Serial.println(F("[stopConnect] Connect was stopped"));
}
void sendGetREST(String host, int port, String path) {
    char hostname[128];
    // Chuyển String thành array char vì hàm connect chỉ nhận char array.
    // Ở ngoài ta sử dụng String của arduino để dễ dàng thao tác
    host.toCharArray(hostname, 128);

    // Thực hiện tạo kết nối
    if (!internets.connect(hostname, port)) {
        Serial.println(F("Connect failed"));
    }

    /* Tạo request
        GET /{path}  HTTP/1.1
        Host: {host}
        Cookie: {cookie}
        Accept: application/json
        Origin: WemosSocketIOClient
        Connection: keep-alive
    */
    String request = "";
    request += F("GET /");
    request += path;
    request += F(" HTTP/1.1\r\n");
    request += F("Host: ");
    request += host;
    request += F("\r\n");
    request += F("Cookie: ");
    request += cookie;
    request += F("\r\n");
    request += F("Accept: application/json\r\n");
    request += F("Origin: WemosSocketIOClient\r\n");
    request += F("Connection: keep-alive\r\n\r\n");

    // Gửi request
    internets.print(request);
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
}

void loop() {
    if(prevTime + interval < millis() || prevTime == 0){
        prevTime = millis();
        sendGetREST(host, httpPort, path);
        receiveGetREST();
    }
}
