#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <FS.h>
#include <Wire.h>
#include <HTTPUpdate.h>

// Output pin numbers
const int outputPins[4] = {26, 27, 32, 33};
// Input pin numbers
const int inputPins[4] = {16,17,18,19};
#define TRIGGER_PIN 34 // trigger pin GPIO36:i2r-04 GPIO34:i2r-03
const int ledPin = 2;

// Define the Data structure
struct DataDevice {
  String mac=""; // Bluetooth mac address 를 기기 인식 id로 사용한다.
  int out[4];
  int in[4];
  String strIn="0000",strInPre="0000"; // In[] 을 string으로 저장
  String sendData=""; // 보드의 입력,출려,전압 데이터를 json 형태로 저장
};

struct DataBle {
  char *service_uuid="4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  char *characteristic_uuid="beb5483e-36e1-4688-b7f5-ea07361b26a8";
  bool isConnected=false;
};

struct DataWifiMqtt {
  bool selectMqtt=false;
  bool use=false;
  bool isConnected=false;
  bool isConnectedMqtt=false;
  String ssid="";
  String password="";
  String mqttBroker = ""; // 브로커 
  String email="";
  char outTopic[50]="";  // "Ble mac address/out" 로 생성
  char inTopic[50]="";   // "Ble mac address/in" 으로 생성
};

// Create an instance of the Data structure
DataDevice dev;
DataBle ble;
DataWifiMqtt wifi,wifiSave;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned int counter = 0;
unsigned long lastTime = 0;  // 마지막으로 코드가 실행된 시간을 기록할 변수
const long interval = 3000;  // 실행 간격을 밀리초 단위로 설정 (3초)
unsigned int event = 0;
String returnMsg="";

BLECharacteristic *pCharacteristic;

void callback(char* topic, byte* payload, unsigned int length);
void checkFactoryDefault();
void connectToWiFi();
void doTick();
void loadConfigFromSPIFFS();
void parseJSONPayload(byte* payload, unsigned int length);
void publishMqtt();
void readBleMacAddress();
void reconnectMQTT();
void returnMessage();
void saveConfigToSPIFFS();
void setup();
void setupBLE();
void updateCharacteristicValue();
void writeToBle(int order);
bool initializeSPIFFS();
void saveConfigToSPIFFS01();

void download_program(String fileName);
void update_started();
void update_finished(); 
void update_progress(int cur, int total);
void update_error(int error);

void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  // Set each output pin as an output
  for (int i = 0; i <4; i++) {
    pinMode(outputPins[i], OUTPUT);
  }
  // Set each input pin as an input
  for (int i = 0; i < 4; i++) {
    pinMode(inputPins[i], INPUT);
  }

  Serial.begin(115200);
  loadConfigFromSPIFFS();

  setupBLE();
  // BLE이 제대로 초기화될 수 있도록 약간의 시간을 기다립니다.
  delay(1000);
  // 이제 BLE MAC 주소를 읽어 봅니다.
  readBleMacAddress();
  Serial.println("BLE ready!");

  if (wifi.use) {
    // Wi-Fi 연결 설정
    connectToWiFi();
    // MQTT 설정
    client.setServer(wifi.mqttBroker.c_str(), 1883);
    client.setCallback(callback);
  } else {
    Serial.println("Wi-Fi and MQTT setup skipped as wifi.use is false.");
  }

}

void loop() {
  doTick();
  
  if(event != 0) {
    writeToBle(event);
    delay(1000);
    writeToBle(event);
      event = 0;
  }


  if (wifi.use) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
  }
 
  checkFactoryDefault();

}
