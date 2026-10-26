// =====================================================
// YD-ESP32-23 - ĐO ÁNH SÁNG + TDS + pH + MQ GAS + DHT11
// Board: ESP32-S3
// Arduino IDE
// =====================================================

#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Thay YOUR_WIFI_SSID và YOUR_WIFI_PASSWORD bằng thông tin mạng Wi-Fi của bạn.
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Thay IP_MAY_TINH bằng IPv4 của máy tính chạy server (không dùng localhost).
const char* serverUrl = "http://IP_MAY_TINH:3000/api/sensor";
const unsigned long SENSOR_INTERVAL_MS = 2000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
unsigned long lastSensorReadAt = 0;
unsigned long lastWiFiAttemptAt = 0;

// ---------------- PIN CẢM BIẾN ----------------

#define LIGHT_PIN 4
#define TDS_PIN   5
#define PH_PIN    6

#define DHT_PIN   7
#define MQ_PIN    8


// ---------------- DHT11 ----------------

#define DHTTYPE DHT11

DHT dht(DHT_PIN, DHTTYPE);

void maintainWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWiFiAttemptAt >= WIFI_RETRY_INTERVAL_MS) {
    Serial.println("Wi-Fi disconnected; retrying connection...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWiFiAttemptAt = now;
  }
}

void postSensorData(
  float lightADC,
  float lightVoltage,
  float tdsADC,
  float tdsVoltage,
  float phADC,
  float phVoltage,
  float phValue,
  float gasADC,
  float gasVoltage,
  float temperature,
  float humidity
) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sensor data not sent: Wi-Fi is disconnected.");
    return;
  }

  String payload = "{";
  payload += "\"light_adc\":" + String(lightADC, 0);
  payload += ",\"light_voltage\":" + String(lightVoltage, 3);
  payload += ",\"tds_adc\":" + String(tdsADC, 0);
  payload += ",\"tds_voltage\":" + String(tdsVoltage, 3);
  payload += ",\"ph_adc\":" + String(phADC, 0);
  payload += ",\"ph_voltage\":" + String(phVoltage, 3);
  payload += ",\"ph\":" + String(phValue, 2);
  payload += ",\"gas_adc\":" + String(gasADC, 0);
  payload += ",\"gas_voltage\":" + String(gasVoltage, 3);

  if (!isnan(temperature)) {
    payload += ",\"temperature\":" + String(temperature, 1);
  }
  if (!isnan(humidity)) {
    payload += ",\"humidity\":" + String(humidity, 1);
  }
  payload += "}";

  HTTPClient http;
  http.setTimeout(1500);
  if (!http.begin(serverUrl)) {
    Serial.println("Sensor POST failed: invalid server URL.");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  int responseCode = http.POST(payload);
  if (responseCode > 0) {
    Serial.print("Sensor POST HTTP status: ");
    Serial.println(responseCode);
  } else {
    Serial.print("Sensor POST failed: ");
    Serial.println(http.errorToString(responseCode));
  }
  http.end();
}


// ---------------- ADC ----------------

#define ADC_MAX 4095.0
#define ADC_VOLTAGE 3.3


// ---------------- pH ----------------

// Giá trị mặc định tham khảo.
// Cần hiệu chuẩn lại bằng dung dịch chuẩn pH.

#define PH_NEUTRAL_VOLTAGE 2.50
#define PH_SLOPE 0.18


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiAttemptAt = millis();
  Serial.println("Starting Wi-Fi connection...");

  // Độ phân giải ADC của ESP32-S3
  analogReadResolution(12);

  // Khởi động DHT11
  dht.begin();

  Serial.println();
  Serial.println("======================================");
  Serial.println("      HE THONG DO CHAT LUONG CAY");
  Serial.println("          YD-ESP32-23");
  Serial.println("======================================");
  Serial.println();

}


// =====================================================
// ĐỌC ĐIỆN ÁP ADC
// =====================================================

float readVoltage(int pin) {

  int adcValue = analogRead(pin);

  float voltage = (adcValue * ADC_VOLTAGE) / ADC_MAX;

  return voltage;
}


// =====================================================
// ĐỌC GIÁ TRỊ TRUNG BÌNH ADC
// =====================================================

float readAverageADC(int pin, int samples) {

  long total = 0;

  for (int i = 0; i < samples; i++) {

    total += analogRead(pin);

    delay(5);
  }

  return (float)total / samples;
}


// =====================================================
// TÍNH pH
// =====================================================

float calculatePH(float voltage) {

  float phValue =
    7.0 + ((PH_NEUTRAL_VOLTAGE - voltage) / PH_SLOPE);

  return phValue;
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  maintainWiFiConnection();

  unsigned long now = millis();
  if (now - lastSensorReadAt < SENSOR_INTERVAL_MS) {
    return;
  }
  lastSensorReadAt = now;

  // ---------------------------------------------------
  // 1. CẢM BIẾN ÁNH SÁNG
  // ---------------------------------------------------

  float lightADC = readAverageADC(LIGHT_PIN, 20);

  float lightVoltage =
    (lightADC * ADC_VOLTAGE) / ADC_MAX;


  // ---------------------------------------------------
  // 2. CẢM BIẾN TDS
  // ---------------------------------------------------

  float tdsADC = readAverageADC(TDS_PIN, 20);

  float tdsVoltage =
    (tdsADC * ADC_VOLTAGE) / ADC_MAX;


  // ---------------------------------------------------
  // 3. CẢM BIẾN pH PH4502C
  // ---------------------------------------------------

  float phADC = readAverageADC(PH_PIN, 20);

  float phVoltage =
    (phADC * ADC_VOLTAGE) / ADC_MAX;

  float phValue =
    calculatePH(phVoltage);


  // ---------------------------------------------------
  // 4. CẢM BIẾN MQ GAS
  // ---------------------------------------------------

  float gasADC = readAverageADC(MQ_PIN, 20);

  float gasVoltage =
    (gasADC * ADC_VOLTAGE) / ADC_MAX;


  // ---------------------------------------------------
  // 5. CẢM BIẾN DHT11
  // ---------------------------------------------------

  float humidity = dht.readHumidity();

  float temperature = dht.readTemperature();


  // ---------------------------------------------------
  // 6. KIỂM TRA DHT11
  // ---------------------------------------------------

  if (isnan(humidity) || isnan(temperature)) {

    Serial.println("Loi: Khong doc duoc du lieu DHT11");

  }


  // ---------------------------------------------------
  // 7. HIỂN THỊ KẾT QUẢ
  // ---------------------------------------------------

  Serial.println("------------- KET QUA DO -------------");


  Serial.print("ANH SANG | ADC: ");
  Serial.print(lightADC, 0);
  Serial.print(" | Dien ap: ");
  Serial.print(lightVoltage, 3);
  Serial.println(" V");


  Serial.print("TDS      | ADC: ");
  Serial.print(tdsADC, 0);
  Serial.print(" | Dien ap: ");
  Serial.print(tdsVoltage, 3);
  Serial.println(" V");


  Serial.print("pH       | ADC: ");
  Serial.print(phADC, 0);
  Serial.print(" | Dien ap: ");
  Serial.print(phVoltage, 3);
  Serial.print(" V | pH: ");
  Serial.println(phValue, 2);


  Serial.print("GAS      | ADC: ");
  Serial.print(gasADC, 0);
  Serial.print(" | Dien ap: ");
  Serial.print(gasVoltage, 3);
  Serial.println(" V");


  if (!isnan(humidity) && !isnan(temperature)) {

    Serial.print("NHIET DO  | ");
    Serial.print(temperature, 1);
    Serial.println(" °C");

    Serial.print("DO AM     | ");
    Serial.print(humidity, 1);
    Serial.println(" %");

  }


  Serial.println("---------------------------------------");


  // ---------------------------------------------------
  // 8. DỮ LIỆU JSON CHO RASPBERRY PI
  // ---------------------------------------------------

  Serial.println("{");

  Serial.print("  \"light_adc\": ");
  Serial.print(lightADC, 0);
  Serial.println(",");

  Serial.print("  \"light_voltage\": ");
  Serial.print(lightVoltage, 3);
  Serial.println(",");

  Serial.print("  \"tds_adc\": ");
  Serial.print(tdsADC, 0);
  Serial.println(",");

  Serial.print("  \"tds_voltage\": ");
  Serial.print(tdsVoltage, 3);
  Serial.println(",");

  Serial.print("  \"ph_adc\": ");
  Serial.print(phADC, 0);
  Serial.println(",");

  Serial.print("  \"ph_voltage\": ");
  Serial.print(phVoltage, 3);
  Serial.println(",");

  Serial.print("  \"ph\": ");
  Serial.print(phValue, 2);
  Serial.println(",");

  Serial.print("  \"gas_adc\": ");
  Serial.print(gasADC, 0);
  Serial.println(",");

  Serial.print("  \"gas_voltage\": ");
  Serial.print(gasVoltage, 3);
  Serial.println(",");

  if (!isnan(temperature)) {

    Serial.print("  \"temperature\": ");
    Serial.print(temperature, 1);
    Serial.println(",");

  }

  if (!isnan(humidity)) {

    Serial.print("  \"humidity\": ");
    Serial.print(humidity, 1);

  }

  Serial.println();

  Serial.println("}");

  Serial.println();

  postSensorData(
    lightADC,
    lightVoltage,
    tdsADC,
    tdsVoltage,
    phADC,
    phVoltage,
    phValue,
    gasADC,
    gasVoltage,
    temperature,
    humidity
  );
}