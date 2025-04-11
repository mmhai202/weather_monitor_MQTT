#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <PubSubClient.h>

// Khởi tạo led
const int ledPin = 16;
bool ledStatus = LOW;

// Khởi tạo cảm biến DHT
#define DHTPIN  26
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
int dhtTemp, dhtHumi;

// Khởi tạo LCD1602
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
byte degreeC[8] = {
  0b01110, 0b01010, 0b01110, 0b00000,
  0b00000, 0b00000, 0b00000, 0b00000
};

// Thông tin WiFi
const char* ssid = "ABC";
const char* password = "03072002";

// API thời tiết
String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?q=Hanoi&appid=e2029e14dfbb872bf7a67b3b8b03a3c5";
String weatherJsonBuffer;
int apiTemp, apiHumi;

// Thông tin MQTT broker
#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 1883

// Chủ đề MQTT
#define MQTT_DHTTEMP_TOPIC "mamaha/dhtTemp"
#define MQTT_DHTHUMI_TOPIC "mamaha/dhtHumi"
#define MQTT_APITEMP_TOPIC "mamaha/apiTemp"
#define MQTT_APIHUMI_TOPIC "mamaha/apiHumi"
#define MQTT_LED_TOPIC "mamaha/led"

WiFiClient wifiClient;           // Đối tượng WiFiClient cho kết nối WiFi
PubSubClient client(wifiClient); // Đối tượng PubSubClient cho MQTT

// Hàm khởi tạo ban đầu
void init () {
  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(0, degreeC);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledStatus);
}

// Hàm kết nối MQTT broker
void connect_to_broker() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String clientId = "mamaha";  // Tạo ID ngẫu nhiên cho ESP32
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(MQTT_LED_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);  // Chờ 2 giây và thử lại
    }
  }
  Serial.println("Reconnected to broker");
}

// Hàm xử lý tin nhắn từ broker
void callback(char* topic, byte *payload, unsigned int length) {
  char status[20];  // Chuỗi lưu trạng thái từ payload
  Serial.println("-------new message from broker-----");
  Serial.print("topic: ");
  Serial.println(topic);
  Serial.print("message: ");
  Serial.write(payload, length);
  Serial.println();
  
  // Sao chép payload vào chuỗi status
  for (int i = 0; i < length; i++) status[i] = payload[i];
  status[length] = '\0'; // Đảm bảo kết thúc chuỗi

  // Kiểm tra message
  if (String(topic) == MQTT_LED_TOPIC) {
    if (strcmp(status, "ON") == 0) {
      digitalWrite(ledPin, HIGH);
    } else if (strcmp(status, "OFF") == 0) {
      digitalWrite(ledPin, LOW);
    }
  }
}

// Hàm kết nối WiFi
void connect_to_wifi () {
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {}
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

// Hàm GET request để lấy API
String httpGETRequest (String Url) {
  HTTPClient http;
  http.begin(Url);
  int responseCode = http.GET();
  String responseBody = "{}";
  if (responseCode > 0) {
    responseBody = http.getString();
  } else {
    Serial.print("Error Code: ");
    Serial.println(responseCode);
  }
  http.end();
  return responseBody;
}

// Đọc cảm biến DHT
void readDHT (int* dhtTemp, int* dhtHumi) {
  *dhtTemp = dht.readTemperature();
  *dhtHumi = dht.readHumidity();
  if (isnan(*dhtTemp) || isnan(*dhtHumi)) Serial.println(F("Failed to read from DHT sensor!"));
}

// Đọc dữ liệu API
void readAPI (int* apiTemp, int* apiHumi) {
  weatherJsonBuffer = httpGETRequest(weatherUrl);
  JSONVar weatherJson = JSON.parse(weatherJsonBuffer);
  *apiTemp = int(weatherJson["main"]["temp"]) - 273;  // Đổi từ độ K sang độ C
  *apiHumi = int(weatherJson["main"]["humidity"]);
}

// Hiển thị dữ liệu lên LCD
void printLCD(int t, int h, String c) {
  if (c == "API") {
    lcd.setCursor(0, 0);
    lcd.print("HaNoi  ");
    lcd.print(t);
    lcd.write(byte(0));
    lcd.print("C  ");
    lcd.print(h);
    lcd.print("%");
  } else if (c == "DHT") {
    lcd.setCursor(0, 1);
    lcd.print("DHT11  ");
    lcd.print(t);
    lcd.write(byte(0));
    lcd.print("C  ");
    lcd.print(h);
    lcd.print("%");
  }
}

void setup() {
  Serial.begin(115200);
  init(); // Hàm khởi tạo
  connect_to_wifi(); // Kết nối wifi
  client.setServer(MQTT_SERVER, MQTT_PORT); // Thiết lập thông tin MQTT broker
  client.setCallback(callback); // Gán hàm callback để xử lý tin nhắn MQTT
  connect_to_broker(); // Kết nối đến broker MQTT
  Serial.println("Ready!");

  /* Gửi dữ liệu ban đầu*/
  readDHT(&dhtTemp, &dhtHumi);
  printLCD(dhtTemp, dhtHumi, "DHT");
  client.publish(MQTT_DHTTEMP_TOPIC, String(dhtTemp).c_str());
  client.publish(MQTT_DHTHUMI_TOPIC, String(dhtHumi).c_str());
  readAPI(&apiTemp, &apiHumi);
  printLCD(apiTemp, apiHumi, "API");
  client.publish(MQTT_APITEMP_TOPIC, String(apiTemp).c_str());
  client.publish(MQTT_APIHUMI_TOPIC, String(apiHumi).c_str());
}

unsigned long previousMillisDHT = 0;
unsigned long previousMillisAPI = 0;
const long intervalDHT = 10000;
const long intervalAPI = 10000;

void loop() {
  client.loop();
  if (!client.connected()) connect_to_broker();
  if (WiFi.status() != WL_CONNECTED) connect_to_wifi();

  unsigned long currentMillis = millis();

  // Đọc cảm biến DHT
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;
    readDHT(&dhtTemp, &dhtHumi);
    printLCD(dhtTemp, dhtHumi, "DHT");
    client.publish(MQTT_DHTTEMP_TOPIC, String(dhtTemp).c_str());
    client.publish(MQTT_DHTHUMI_TOPIC, String(dhtHumi).c_str());
  }

  // Đọc API thời tiết
  if (currentMillis - previousMillisAPI >= intervalAPI) {
    previousMillisAPI = currentMillis;
    readAPI(&apiTemp, &apiHumi);
    printLCD(apiTemp, apiHumi, "API");
    client.publish(MQTT_APITEMP_TOPIC, String(apiTemp).c_str());
    client.publish(MQTT_APIHUMI_TOPIC, String(apiHumi).c_str());
  }
}
