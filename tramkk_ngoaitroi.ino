//donee

#define BLYNK_TEMPLATE_ID "TMPL6KCeIxCqk"
#define BLYNK_TEMPLATE_NAME "SP2"
#define BLYNK_AUTH_TOKEN "reuF91ToVRiSIMXy9ttDIalNKBVDXX7g"

#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Arduino.h>
#include <ML8511.h>
#include <Wire.h>
#include <AHT20.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <MQ135.h>

// char ssid[] = "MSI-Gaming";
// char pass[] = "12345678";
#define PIN_MQ135 32
#define BLYNK_PRINT Serial
#define LATITUDE "21.0787038"    // Ex: "10.77"
#define LONGITUDE "105.8656097"  // Ex: "106.65"
#define API_KEY "e6fa53bea61f102d5f3b16a80d468d22"
HardwareSerial zh03bSerial(2);
unsigned long lastTime = 0;
#define TIME_UPDATE 30000UL  // Unit (ms)

//// API "Current weather data" ////
#define NAME_URL "https://api.openweathermap.org/data/3.0/onecall"
#define PARAM_LAT "?lat="
#define PARAM_LON "&lon="
#define PARAM_UNITS "&units=metric"
#define PARAM_EXCLUDE "&exclude=hourly,daily"
#define PARAM_API "&appid="

String URL = String(NAME_URL) + String(PARAM_LAT) + String(LATITUDE) + String(PARAM_LON) + String(LONGITUDE) + String(PARAM_UNITS) + String(PARAM_EXCLUDE) + String(PARAM_API) + String(API_KEY);
// String URL = "https://api.openweathermap.org/data/3.0/onecall?lat=21.018605&lon=105.7958367&units=metric&appid=e6fa53bea61f102d5f3b16a80d468d22";
// #define RatioMQ135CleanAir 3.6

bool isAlert = false;
bool previousAlertStatus = false;
unsigned long alertCheckInterval = 5000;  // 5 giây
unsigned long lastAlertCheck = 0;
WiFiManager wm;
Servo myServo;  // Khai báo đối tượng servo

int servoPin = 14;  // Chân PWM cho servo
int angle = 0;
bool clkk = 0;

String jsonBuffer;
JSONVar myObject;
AHT20 aht20;
MQ135 mq135_sensor(32);

float AQI, temp, humid;
uint16_t PM2_5;
uint16_t last_PM2_5 = 0;
double Windspeed, Cloud, WindDir, UV;
String benh;

BlynkTimer timer;

void setup() {
  Serial.begin(115200);
  Serial.println("Connecting to Wi-Fi...");
  bool res = wm.autoConnect("AutoConnectAP", "12345678");
  Serial.println("Connected to Wi-Fi");
  if (!res) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();  // Khởi động lại nếu không kết nối thành công
  } else {
    Serial.println("Connected to Wi-Fi successfully");
  }
  String ssid = WiFi.SSID();  // Lấy SSID của mạng Wi-Fi
  String pass = WiFi.psk();

  // Kết nối Blynk sau khi Wi-Fi được kết nối thành công
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid.c_str(), pass.c_str());
  zh03bSerial.begin(9600, SERIAL_8N1, 16, 17);
  Wire.begin();
  aht20.begin();
  myServo.attach(servoPin);
}

void loop() {
  Blynk.run();  // Chạy Blynk
  delay(1000);
  readSensorData();
  delay(1000);
  GetAPI();
  delay(1000);
  // Đọc dữ liệu cảm biến mỗi giây
  pushtoBlynk();
  delay(1000);  // Chạy các tác vụ của BlynkTimer
}

// void reconnectWiFi() {
//   if (WiFi.status() != WL_CONNECTED) {
//     Serial.println("Reconnecting to Wi-Fi...");
//     WiFi.begin(ssid, pass);
//     unsigned long startAttemptTime = millis();

//     while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
//       delay(1000);
//       Serial.print(".");
//     }

//     if (WiFi.status() == WL_CONNECTED) {
//       Serial.println("\nReconnected to Wi-Fi");
//     } else {
//       Serial.println("\nFailed to reconnect. Retrying...");
//     }
//   }
// }

void readSensorData() {
  // Đọc dữ liệu từ cảm biến AHT20 (nhiệt độ và độ ẩm)
  temp = aht20.getTemperature();
  humid = aht20.getHumidity();
  //  Serial.println(temp);
  //   Serial.println(humid);
  delay(100);
  myServo.write(90);

  // Đọc dữ liệu từ cảm biến MQ135 (chất lượng không khí)//
  float rzero = mq135_sensor.getRZero();
  float correctedRZero = mq135_sensor.getCorrectedRZero(temp, humid);
  float resistance = mq135_sensor.getResistance();
  float ppm = mq135_sensor.getPPM();
  float correctedPPM = mq135_sensor.getCorrectedPPM(temp, humid);
  delay(100);
  // Serial.println(correctedPPM);


  uint8_t data[9];                     // Mảng để lưu dữ liệu từ khung PM2
  if (zh03bSerial.available() >= 9) {  // PM2 giao thức gửi khung dài 9 byte
    if (zh03bSerial.read() == 0x42) {  // Kiểm tra byte bắt đầu là 0x42
      data[0] = 0x42;
      if (zh03bSerial.read() == 0x4D) {  // Kiểm tra byte tiếp theo là 0x4D
        data[1] = 0x4D;
        for (int i = 2; i < 9; i++) {
          data[i] = zh03bSerial.read();  // Đọc các byte còn lại
        }

        // Giải mã dữ liệu (PM1.0, PM2.5, PM10)
        uint16_t pm2_5 = (data[4] << 8) + data[5];
        PM2_5 = pm2_5;
        // Serial.println(PM2);
      }
    }
  }

  delay(1000);                // Đọc mỗi giây
  if (PM2_5 != last_PM2_5) {  // Nếu PM2.5 thay đổi
    last_PM2_5 = PM2_5;

    if (PM2_5 <= 12.0) {
      AQI = ((50 - 0) / (12.0 - 0.0)) * (PM2_5 - 0.0) + 0;
    } else if (PM2_5 <= 35.4) {
      AQI = ((100 - 51) / (35.4 - 12.1)) * (PM2_5 - 12.1) + 51;
    } else if (PM2_5 <= 55.4) {
      AQI = ((150 - 101) / (55.4 - 35.5)) * (PM2_5 - 35.5) + 101;
    } else if (PM2_5 <= 150.4) {
      AQI = ((200 - 151) / (150.4 - 55.5)) * (PM2_5 - 55.5) + 151;
    } else if (PM2_5 <= 250.4) {
      AQI = ((300 - 201) / (250.4 - 150.5)) * (PM2_5 - 150.5) + 201;
    } else if (PM2_5 <= 500.4) {
      AQI = ((500 - 301) / (500.4 - 250.5)) * (PM2_5 - 250.5) + 301;
    }
  }
  delay(1000);


if (AQI > 75) {
    benh = "Air quality is poor, please be cautious!";
    isAlert = true;
  } else {
    benh = "Air quality is stable!";
    isAlert = false;
  }

  // Kiểm tra nếu trạng thái cảnh báo thay đổi thì điều khiển servo
  if (isAlert != previousAlertStatus) {
    if (isAlert) {
      Blynk.logEvent("Caution", benh + "\n");
      servotsangx();  // Quay servo từ tốt sang xấu
    } else {
      Blynk.logEvent("Caution", benh + "\n");
      servoxsangt();  // Quay servo từ xấu sang tốt
    }
    previousAlertStatus = isAlert;  // Cập nhật trạng thái cảnh báo trước đó
  }
  delay(100);
}

void pushtoBlynk() {
  // Đẩy dữ liệu lên Blynk
  Blynk.virtualWrite(V0, AQI);
  Blynk.virtualWrite(V1, UV);
  Blynk.virtualWrite(V2, temp);
  Blynk.virtualWrite(V3, humid);
  Blynk.virtualWrite(V4, PM2_5);
  Blynk.virtualWrite(V5, benh + "\n");
  Blynk.virtualWrite(V6, Windspeed);
  Blynk.virtualWrite(V7, Cloud);
  Blynk.virtualWrite(V8, WindDir);
}

void GetAPI() {
  if ((millis() - lastTime) >= TIME_UPDATE) {
    /* ------------- Check WiFi connection status ------------ */
    if (WiFi.status() == WL_CONNECTED) {
      jsonBuffer = httpGETRequest(URL.c_str());
      Serial.println(jsonBuffer);
      myObject = JSON.parse(jsonBuffer);

      // Check received JSON packet has data?
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println(F("Parsing input failed!"));
        return;
      }

      Windspeed = double(myObject["current"]["wind_speed"]);
      UV = double(myObject["current"]["uvi"]);
      WindDir = double(myObject["current"]["wind_deg"]);
      Cloud = double(myObject["current"]["clouds"]);



    } else {
      Serial.println(F("WiFi Disconnected"));
    }
    /* ------------------------------------------------------- */
    lastTime = millis();
  }
}



String httpGETRequest(const char *serverName) {
  WiFiClientSecure client;  // Sử dụng WiFiClientSecure cho HTTPS
  HTTPClient http;

  client.setInsecure();  // Tạm thời bỏ qua việc xác thực chứng chỉ SSL

  // Bắt đầu yêu cầu HTTPS
  http.begin(client, serverName);

  // Gửi yêu cầu GET
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print(F("HTTP Response code: "));
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print(F("Error code: "));
    Serial.println(httpResponseCode);
  }

  // Giải phóng tài nguyên
  http.end();

  return payload;
}
void servotsangx() {
  myServo.write(0);
  delay(140);
  myServo.write(90);
}

void servoxsangt() {
  myServo.write(0);
  delay(218);
  myServo.write(90);
}

