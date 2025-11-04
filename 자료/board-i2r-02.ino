//i2r-02
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <FS.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <algorithm> 
#include <math.h> // fabs
#include "esp_system.h"  // 메모리 체크

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <vector>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Output pin numbers
const int outputPins[4] = {26, 27, 32, 33};
// Input pin numbers
const int inputPins[4] = {16, 17, 18, 19};
const int numberOfPins = sizeof(outputPins) / sizeof(outputPins[0]);

#define TRIGGER_PIN 34 // Factory Reset trigger pin GPIO36:i2r-04 GPIO34:i2r-03
const int ledPin = 2;
bool ledState = LOW; // ledPin = 2 LED의 현재 상태를 기록할 변수
unsigned int counter = 0;
String received = "";

void parseJSONPayload(byte* payload, unsigned int length);  

// Define the Data structure
struct PinStateChange {
  //bool portState; // 상태 변경 여부 (true: ON, false: OFF)
  String mac;
  int port;
  bool value;
  unsigned long timestamp; // 상태 변경 시간
};


// 입력에 따라 출력의 설정 : 배열로 형성됨
struct PendingOutput {
  int port;           // 출력 포트
  bool trigger;       // 트리거 (true: ON일때, false: OFF일때)
  int slotIndex;      // 고유 인덱스
  int delay;          // 지연 시간
  unsigned long executeTime; // 실행 시간
  bool exec = false;  // 실행 여부
  PinStateChange change; // 상태 변경 정보
} pendingOutput;

struct Device {
  std::vector<PendingOutput> pendingOutputs;
  String type = "2";
  String mac=""; // Bluetooth mac address 를 기기 인식 id로 사용한다.
  unsigned long lastTime = 0;  // 마지막으로 코드가 실행된 시간을 기록할 변수bio list sent
  const long interval = 100;  // 실행 간격을 밀리초 단위로 설정 (3초)
  int out[numberOfPins];
  int in[numberOfPins];
  String sendData="",sendDataPre=""; // 보드의 입력,출려,전압 데이터를 json 형태로 저장

  void addPendingOutput(PendingOutput output);
  void removePendingOutput(int slotIndex);
  void setPendingExec(int port, bool currentState);
  void printPendingOutputs();
  void processPendingExecutions();
  
  void checkFactoryDefault();
  void loop();
  void sendStatusCheckChange(bool dataChange); // 현재 상태를 전송하는 함수
  void sendOut(int port, bool portState); // 핀 상태를 MQTT로 전송하는 함수
  std::vector<PinStateChange> pinStateChanges[numberOfPins][2];  // 포트당 2개의 상태 변경 내역 저장 (0=false,1=true)
  void loadPinStatesFromSPIFFS();
  void digitalWriteUpdateData(int pin, bool value);
} dev;


struct Ble {
public:
  char *service_uuid="4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  char *characteristic_uuid="beb5483e-36e1-4688-b7f5-ea07361b26a8";
  bool boot=false;
  bool isConnected=false;
  void setup();
  void readBleMacAddress();
  void writeToBle(int order);
} ble;
BLECharacteristic *pCharacteristic;

WiFiClient espClient;
PubSubClient client(espClient);

struct WifiMqtt {
public:
  bool isConnected=false;
  bool isConnectedMqtt=false;
  String ssid="";
  String password="";
  String email="";
  String mqttBroker = ""; // 브로커 
  char outTopic[50]="i2r/"; 
  char inTopic[50]="i2r/";  
  unsigned long lastBlinkTime = 0; // 마지막으로 LED가 점멸된 시간을 기록할 변수
  unsigned long statusSendCounter = 0; // MQTT 연결 시 상태 전송 횟수를 기록할 변수
  unsigned long startupTime; // 프로그램 시작 시간
  const unsigned long ignoreDuration = 5000; // 무시할 시간 (밀리초 단위), 예: 5000ms = 5초

  unsigned long lastMqttRetryTime = 0;
  const unsigned long mqttRetryInterval = 60000;  // 1분
  unsigned long lastWifiRetryTime = 0;
  const unsigned long wifiRetryInterval = 60000;  // 1분

  void loop();
  void connectToWiFi();
  void publishMqtt();
  void reconnectMQTT();
  void readWifiMacAddress();
};

// Create an instance of the Data structure
WifiMqtt wifi,wifiSave;

struct Config {
  bool initializeSPIFFS();
  void loadConfigFromSPIFFS(); //wifi
  void saveConfigToSPIFFS();  //wifi
  void savePendingOutputsToFile(int portNo);    //pendingOutput
  void loadPendingOutputsFromFile(int portNo);  //pendingOutput
} config;

struct Tool {
public:
  // 업데이트 상태를 저장할 경로
  const char* updateStateFile = "/updateState.txt";
  const char* firmwareFileNameFile = "/firmwareFileName.txt";  // 파일 이름 저장 경로
  String firmwareFileName = "";  // 다운로드할 파일 이름 저장

  void download_program(String fileName);
  void blinkLed(int iteration);
} tool;

void setup();
void startDownloadFeedback();
void stopDownloadFeedback();
void blinkLEDTask(void * parameter);

//=========================================================
// NTP 서버 설정
const long utcOffsetInSeconds = 3600 * 9;  // 한국 표준시(KST) UTC+9
const unsigned long ntpUpdateInterval = 3600000;  // 1시간(3600000ms)마다 NTP 서버 업데이트

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// 핀 설정
const int controlPins[] = {26, 27, 32, 33};
bool pinStates[] = {false, false, false, false};  // 각 핀의 현재 상태 저장
bool previousPinStates[] = {false, false, false, false};  // 각 핀의 이전 상태 저장

// TimeSlot 클래스 정의
class TimeSlot {
public:
  int startHour;
  int startMinute;
  int endHour;
  int endMinute;
  String repeatMode;  // "daily" 또는 "weekly"
  int dayOfWeek;  // 요일 (0 = 일요일, 1 = 월요일, ..., 6 = 토요일)
  //time schedule 위한 프로그램
  unsigned long lastMsgTime = 0; // 마지막 메시지 전송 시간
  unsigned long lastIntTime = 0; // 마지막 메시지 전송 시간
  int slotIndexToSend = -1; // 전송할 타임슬롯 인덱스
  int currentPinIndex = 0; // 전송할 핀 인덱스
  time_t lastNtpTime;
  unsigned long lastMillis;
  unsigned long lastNtpUpdateMillis;

    void setup();
    void loop();
    TimeSlot(int sh, int sm, int eh, int em, String rm, int dow = -1)
      : startHour(sh), startMinute(sm), endHour(eh), endMinute(em), repeatMode(rm), dayOfWeek(dow) {}

    TimeSlot() // 기본 생성자 추가
      : startHour(0), startMinute(0), endHour(0), endMinute(0), repeatMode("daily"), dayOfWeek(-1) {}

    bool isActive(struct tm * timeinfo) {
        int currentHour = timeinfo->tm_hour;
        int currentMinute = timeinfo->tm_min;
        int currentDayOfWeek = timeinfo->tm_wday;

        //Serial.printf("Checking if active: Current time %02d:%02d, Current day %d\n", currentHour, currentMinute, currentDayOfWeek);
        //Serial.println(repeatMode);
        if (repeatMode == "daily") {
            return isTimeInRange(currentHour, currentMinute);
        } else if (repeatMode == "weekly") {
            if (currentDayOfWeek == dayOfWeek) {
                return isTimeInRange(currentHour, currentMinute);
            }
        }
        return false;
    }

private:
    bool isTimeInRange(int currentHour, int currentMinute) {
        int startTotalMinutes = startHour * 60 + startMinute;
        int endTotalMinutes = endHour * 60 + endMinute;
        int currentTotalMinutes = currentHour * 60 + currentMinute;
        if (startTotalMinutes < endTotalMinutes) {
            return currentTotalMinutes >= startTotalMinutes && currentTotalMinutes < endTotalMinutes;
        } else {
            return currentTotalMinutes >= startTotalMinutes || currentTotalMinutes < endTotalMinutes;
        }
    }
} timeManager;  // 변수 이름을 time에서 timeManager로 변경


// 각 핀에 대한 동적 시간대 관리
std::vector<TimeSlot> timeSlots[numberOfPins]; // 배열 크기를 numberOfPins로 수정

// 함수 선언
void addTimeSlot(int pinIndex, int startHour, int startMinute, int endHour, int endMinute, String repeatMode, int dayOfWeek = -1);
void removeTimeSlot(int pinIndex, int slotIndex);
void removeAllTimeSlots(int pinIndex); // 새로운 함수 선언
void loadTimeSlotsFromSPIFFS(int pinIndex);
void saveTimeSlotsToSPIFFS(int pinIndex);
String getTimeSlotJson(int pinIndex, int slotIndex);
void startSendingTimeSlots(int pinIndex);
void printCurrentTime();
void printSchedules();
void sendNextTimeSlot();
void TimeSlot::setup() {
  timeClient.begin();
  timeClient.update();
  this->lastNtpTime = timeClient.getEpochTime();
  this->lastMillis = millis();
  this->lastNtpUpdateMillis = millis();

  // 현재 시간 출력
  printCurrentTime();

  // SPIFFS에서 시간 슬롯 로드
  for (int i = 0; i < numberOfPins; i++) { // 배열 크기를 numberOfPins로 수정
    loadTimeSlotsFromSPIFFS(i);
  }

  // 현재 스케줄 출력
  //printSchedules();
}
void TimeSlot::loop() {
  unsigned long currentMillis = millis();
  // 타임슬롯 전송 로직
  if (this->slotIndexToSend >= 0) {
    if (currentMillis - this->lastMsgTime >= 1000) { // 1초 간격으로 전송
      sendNextTimeSlot();
      this->lastMsgTime = currentMillis;
    }
  }

  if (currentMillis - this->lastIntTime < 1000) { // 1초 loop 실행
    return;
  }
  this->lastIntTime = currentMillis;

  if (WiFi.status() == WL_CONNECTED) {
    wifi.isConnected = true;
    if (millis() - this->lastNtpUpdateMillis > ntpUpdateInterval) {
      timeClient.update();
      this->lastNtpTime = timeClient.getEpochTime();
      this->lastMillis = millis();
      this->lastNtpUpdateMillis = millis();
      Serial.println("NTP time updated");
    }
  } else {
    wifi.isConnected = false;
  }

  time_t currentTime = this->lastNtpTime + ((millis() - this->lastMillis) / 1000);
  struct tm* timeinfo = localtime(&currentTime);
  for (int i = 0; i < numberOfPins; i++) { // 배열 크기를 numberOfPins로 수정
    bool pinOn = false;
    for (TimeSlot slot : timeSlots[i]) {
      if (slot.isActive(timeinfo)) {
        pinOn = true;
        break;
      }
    }

    pinStates[i] = pinOn;

    if (pinStates[i] != previousPinStates[i]) {
      //digitalWrite(controlPins[i], pinOn ? HIGH : LOW);
      dev.digitalWriteUpdateData(i, pinOn);
      Serial.printf("%02d:%02d:%02d - Pin %d is %s\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, controlPins[i], pinOn ? "ON" : "OFF");
      previousPinStates[i] = pinStates[i];
    }
  }
}
//=========================================================
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // 프로그램 시작 후 일정 시간 동안 메시지 무시
  unsigned long currentMillis = millis();
  if (currentMillis - wifi.startupTime < wifi.ignoreDuration) {
    Serial.println("프로그램 시작 후 초기 메시지 무시 중...");
    return;
  }

  // JSON 파싱
  parseJSONPayload(payload, length);
}

/* 블루투스 함수 ===============================================*/
// 받은 order의 리턴정보
void Ble::writeToBle(int order) {
  // Create a JSON object
  DynamicJsonDocument responseDoc(1024);
  // Fill the JSON object based on the order
  if (order == 1) {
    responseDoc["order"] = order;
    responseDoc["ssid"] = wifi.ssid;
    responseDoc["password"] = wifi.password;
    responseDoc["email"] = wifi.email;
  }

  // Serialize JSON object to string
  String responseString;
  serializeJson(responseDoc, responseString);

  if (order == 0) {
    responseString = "프로그램 다운로드";
  } else if (order == 2) {
    responseString = dev.sendData;
  } else if (order == 101) {
    responseString = "와이파이 정보가 잘못되었습니다.";
  } else if (order == 102) {
    responseString = "와이파이 정보가 저장되었습니다.";
  }

  // String 타입으로 변경 후 전송
  if (pCharacteristic) {
    pCharacteristic->setValue(responseString); // String 타입으로 전달
    pCharacteristic->notify(); // If notification enabled
  }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Ble Device connected");
      ble.isConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
      wifi.isConnected = false; // Set the isConnected flag to false on disconnection
      Serial.println("Device disconnected");
      ble.isConnected = false;
      BLEDevice::startAdvertising();  // Start advertising again after disconnect
    }
};

// 전송된 문자를 받는다.
class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue().c_str(); // std::string 대신 String 사용
      if (value.length() > 0) {
        Serial.println("Received on BLE:");
        for (int i = 0; i < value.length(); i++) {
          Serial.print(value[i]);
        }
        Serial.println();

        // `std::string` 대신 `String`을 사용
        parseJSONPayload((byte*)value.c_str(), value.length());
      }
    }
};

void Ble::setup() {
  ble.boot = true;
  String namePlc = "i2r-" + String(dev.type) + "-IoT PLC";
  BLEDevice::init(namePlc.c_str());
  BLEServer *pServer = BLEDevice::createServer();

  // Set server callbacks
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(ble.service_uuid);
  pCharacteristic = pService->createCharacteristic(
                                         ble.characteristic_uuid,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pCharacteristic->setValue(""); // 초기 값 설정
  pCharacteristic->setValue(String(200, ' ')); // 최대 길이를 200으로 설정 (String 타입으로 변경)

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(ble.service_uuid);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE service started");
  // 이제 BLE MAC 주소를 읽어 봅니다.
  ble.readBleMacAddress();
}


void Ble::readBleMacAddress() {
  // BLE 디바이스에서 MAC 주소를 가져옵니다.
  BLEAddress bleAddress = BLEDevice::getAddress();
  // MAC 주소를 String 타입으로 변환합니다.
  String mac = bleAddress.toString().c_str();
  // MAC 주소를 모두 대문자로 변환합니다.
  mac.toUpperCase();
  // 시리얼 모니터에 BLE MAC 주소를 출력합니다.
  Serial.print("BLE MAC Address: ");
  Serial.println(mac);
}
/* 블루투스 함수 ===============================================*/


/* 와이파이 MQTT 함수 ===============================================*/
void WifiMqtt::publishMqtt() { 
  // dev.sendData에 email과 mac을 추가
  String message = dev.sendData;

  // dev.sendData 끝에 '}'가 없으면 추가
  if (message.charAt(message.length() - 1) != '}') {
    message += "}";
  }

  // email, type, from 추가
  message = message.substring(0, message.length() - 1) + ", \"e\": \"" + wifi.email + 
          "\", \"t\": \"" + dev.type + "\"" + ", \"fr\": \"" + dev.mac + "\"}";

  Serial.println("publishMqtt: " + message);

  // JSON 파싱해서 "m" 값 확인
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, message);
  
  if (!error) {
    String msgMac = doc["m"] | "";  // 메시지 안의 "m" 값
    if (msgMac != "" && msgMac != dev.mac) {
      // "m"과 현재 보드 mac이 다르면 inTopic으로 전송
      client.publish(wifi.inTopic, message.c_str());
      Serial.println("➡ Redirected to inTopic (MAC mismatch)");
    } else {
      // 같으면 outTopic으로 전송
      client.publish(wifi.outTopic, message.c_str());
      Serial.println("➡ Published to outTopic (MAC match)");
    }
  } else {
    // JSON 파싱 실패하면 기본적으로 outTopic으로 보냄
    client.publish(wifi.outTopic, message.c_str());
    Serial.println("⚠ JSON parse error, sent to outTopic");
  }

  delay(100);
}

/*
void WifiMqtt::publishMqtt() { 
  // dev.sendData에 email과 mac을 추가
  String message = dev.sendData;
  // dev.sendData 끝에 '}'가 없으면 추가
  if (message.charAt(message.length() - 1) != '}') {
    message += "}";
  }
  // email,type,from을 추가
  message = message.substring(0, message.length() - 1) + 
            ", \"e\": \"" + wifi.email + "\", \"from\": \"" + dev.mac + "\"}";
  Serial.println("publishMqtt: " + message);
  client.publish(wifi.outTopic, message.c_str());
  delay(100);
}
*/
void WifiMqtt::connectToWiFi() {
  if (wifi.ssid == NULL) {
    Serial.println("SSID is NULL or empty, returning...");
    return; // SSID가 null이거나 빈 문자열이면 함수를 빠져나갑니다.
  }
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(wifi.ssid, wifi.password);

  int wCount = 0;
  wifi.isConnected = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    dev.checkFactoryDefault();
    wCount++;
    if(wCount > 10) {
      wifi.isConnected = false;
      break; // while 루프를 벗어납니다.
    }
  }

  this->readWifiMacAddress();

  if (WiFi.status() == WL_CONNECTED) {
    this->isConnected = true;  // ✅ 여기에서 연결되었음을 명시적으로 설정
    Serial.println("\nConnected to Wi-Fi");
    // ✅ MAC 주소 출력
    Serial.print("Device MAC Address: ");
    Serial.println(dev.mac);

    // 이메일 기반으로 MQTT 토픽 이름 설정
    String outTopicBase = "i2r/" + this->email + "/out";
    String inTopicBase = "i2r/" + this->email + "/in";
    strncpy(this->outTopic, outTopicBase.c_str(), sizeof(this->outTopic) - 1);
    this->outTopic[sizeof(this->outTopic) - 1] = '\0'; // 널 종료 보장
    strncpy(this->inTopic, inTopicBase.c_str(), sizeof(this->inTopic) - 1);
    this->inTopic[sizeof(this->inTopic) - 1] = '\0'; // 널 종료 보장

  } else {
    this->isConnected = false;  // ✅ 연결 실패 시 false 설정
    Serial.println("\nWi-Fi를 찾을 수 없습니다.");
  }
}

void WifiMqtt::loop() {
  // Wi-Fi 연결 상태 점검 및 재시도
  if (WiFi.status() != WL_CONNECTED) { 
    this->isConnected = false;  // 연결 실패 상태 반영

    if (millis() - lastWifiRetryTime > wifiRetryInterval) { // 30초마다 재시도
      Serial.println("와이파이 재시도...");
      WiFi.disconnect(true);  // true: erase old credentials (optional)
      delay(100);             // ✴️ 내부 상태 초기화 대기 (중요)
      WiFi.begin(ssid.c_str(), password.c_str());
      lastWifiRetryTime = millis();
    }
  } else {
    this->isConnected = true; // 연결 성공 시 true로 설정
  }

  // WiFi가 연결되어 있을 때만 동작
  if (wifi.isConnected) {
    if (!client.connected() && (millis() - lastMqttRetryTime > mqttRetryInterval)) {
      Serial.println("mqtt 재접속 시도");
      wifi.reconnectMQTT();
      lastMqttRetryTime = millis();
    }
    if (client.connected()) {
      client.loop();  // MQTT 연결된 경우에만 실행
    }
  }

  // LED 점멸 로직 mqtt가 연결되면 2초간격으로 점멸한다.
  if (this->isConnectedMqtt) {
    unsigned long currentMillis = millis();
    if (currentMillis - this->lastBlinkTime >= 1000) { // 2초 간격으로 점멸
      this->lastBlinkTime = currentMillis;
      ledState = !ledState; // LED 상태를 반전시킴
      digitalWrite(ledPin, ledState); // LED 상태를 설정
    }
    // dev.sendStatusCheckChange(false)를 3번 보냅니다.
    if (statusSendCounter < 3) {
      dev.sendStatusCheckChange(false);
      statusSendCounter++;
      delay(1000); // 잠시 대기 후 전송 (필요 시 조정)
    }
  } else {
    digitalWrite(ledPin, LOW); // MQTT 연결이 안된 경우 LED를 끔
  }
}

void WifiMqtt::reconnectMQTT() {
  if (!wifi.isConnected) return;

  if (client.connected()) {
    this->isConnectedMqtt = true;
    return;
  }

  unsigned long now = millis();
  // ✅ 부팅 직후에는 지연 없이 바로 시도 (lastMqttRetryTime == 0일 경우)
  if (wifi.lastMqttRetryTime != 0 && (now - wifi.lastMqttRetryTime < wifi.mqttRetryInterval)) {
    return;
  }
  wifi.lastMqttRetryTime = now;  // 시도 시간 업데이트

  Serial.println("Trying to connect to MQTT...");

  if (client.connect(dev.mac.c_str())) {
    Serial.println("MQTT connected.");
    client.subscribe(wifi.inTopic);
    this->isConnectedMqtt = true;
  } else {
    Serial.print("MQTT connect failed, rc=");
    Serial.println(client.state());
    this->isConnectedMqtt = false;
  }
}

void WifiMqtt::readWifiMacAddress() {
  // Wi-Fi 디바이스에서 MAC 주소를 가져옵니다.
  String macAddress = WiFi.macAddress();
  // MAC 주소를 String 타입으로 변환합니다.
  dev.mac = macAddress;
  // MAC 주소를 모두 대문자로 변환합니다.
  dev.mac.toUpperCase();
  // 시리얼 모니터에 Wi-Fi MAC 주소를 출력합니다.
  Serial.print("Wi-Fi MAC Address: ");
  Serial.println(dev.mac);
}
/* 와이파이 MQTT 함수 ===============================*/
/* Time Schedule =====================================================*/
void sendNextTimeSlot() {
  if (timeManager.slotIndexToSend < timeSlots[timeManager.currentPinIndex].size()) {
    dev.sendData=getTimeSlotJson(timeManager.currentPinIndex, timeManager.slotIndexToSend);
    // ✅ 메시지를 Serial 모니터에 출력
    Serial.println("📤 Sending schedule message:");
    Serial.println(dev.sendData);  // 추가된 부분
    wifi.publishMqtt();
    timeManager.slotIndexToSend++;
  } else {
    timeManager.slotIndexToSend = -1; // 전송 완료
    Serial.println("All time slots sent for pin " + String(timeManager.currentPinIndex));
  }
}

void printSchedules() {
  Serial.println("Current Schedules:");
  for (int i = 0; i < numberOfPins; i++) { // 배열 크기를 numberOfPins로 수정
    Serial.printf("Pin %d:\n", controlPins[i]);
    for (const TimeSlot& slot : timeSlots[i]) {
      if (slot.repeatMode == "weekly") {
        Serial.printf("  %02d:%02d - %02d:%02d on day %d (%s)\n",
          slot.startHour, slot.startMinute, slot.endHour, slot.endMinute, slot.dayOfWeek, slot.repeatMode.c_str());
      }
      else {
        Serial.printf("  %02d:%02d - %02d:%02d (%s)\n",
          slot.startHour, slot.startMinute, slot.endHour, slot.endMinute, slot.repeatMode.c_str());
      }
    }
  }
}

void printCurrentTime() {
  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();
  struct tm* timeinfo = localtime(&currentTime);

  Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

void addTimeSlot(int pinIndex, int startHour, int startMinute, int endHour, int endMinute, String repeatMode, int dayOfWeek) {
  if (pinIndex >= 0 && pinIndex < numberOfPins) {
    timeSlots[pinIndex].push_back(TimeSlot(startHour, startMinute, endHour, endMinute, repeatMode, dayOfWeek));
    Serial.println("Time slot added.");
  } else {
    Serial.println("Invalid index.");
  }
}

void removeTimeSlot(int pinIndex, int slotIndex) {
  if (pinIndex >= 0 && pinIndex < numberOfPins && slotIndex >= 0 && slotIndex < timeSlots[pinIndex].size()) { // 배열 크기를 numberOfPins로 수정
    timeSlots[pinIndex].erase(timeSlots[pinIndex].begin() + slotIndex);
    Serial.println("Time slot removed.");
  } else {
    Serial.println("Invalid index or slot index.");
  }
}

void removeAllTimeSlots(int pinIndex) {
  if (pinIndex >= 0 && pinIndex < numberOfPins) { // 배열 크기를 numberOfPins로 수정
    timeSlots[pinIndex].clear();
    Serial.println("All time slots removed.");
  } else {
    Serial.println("Invalid index.");
  }
}

void saveTimeSlotsToSPIFFS(int pinIndex) {
  String fileName = "/timeslots_" + String(pinIndex) + ".json";
  File file = SPIFFS.open(fileName, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  DynamicJsonDocument doc(512);  // Adjust size according to expected payload
  JsonArray pinArray = doc.to<JsonArray>();
  for (const TimeSlot& slot : timeSlots[pinIndex]) {
    JsonObject slotObj = pinArray.createNestedObject();
    slotObj["sH"] = slot.startHour;
    slotObj["sM"] = slot.startMinute;
    slotObj["eH"] = slot.endHour;
    slotObj["eM"] = slot.endMinute;
    slotObj["rm"] = slot.repeatMode;
    slotObj["dw"] = slot.dayOfWeek;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
  }
  file.close();
}

void loadTimeSlotsFromSPIFFS(int pinIndex) {
  String fileName = "/timeslots_" + String(pinIndex) + ".json";
  File file = SPIFFS.open(fileName, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  size_t size = file.size();
  if (size > 512) {  // Adjust size according to expected payload
    Serial.println("File size is too large");
    file.close();
    return;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  file.readBytes(buf.get(), size);

  DynamicJsonDocument doc(512);  // Adjust size according to expected payload
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error) {
    //Serial.println("Failed to parse file");
    file.close();
    return;
  }

  JsonArray pinArray = doc.as<JsonArray>();
  for (JsonObject slotObj : pinArray) {
    int startHour = slotObj["sH"];
    int startMinute = slotObj["sM"];
    int endHour = slotObj["eH"];
    int endMinute = slotObj["eM"];
    String repeatMode = slotObj["rm"].as<String>();
    int dayOfWeek = slotObj["dw"];

    timeSlots[pinIndex].push_back(TimeSlot(startHour, startMinute, endHour, endMinute, repeatMode, dayOfWeek));
  }

  file.close();
}

String getTimeSlotJson(int pinIndex, int slotIndex) {
  DynamicJsonDocument doc(256);
  JsonObject slotObj = doc.to<JsonObject>();
  const TimeSlot& slot = timeSlots[pinIndex][slotIndex];
  slotObj["c"] = "sch";
  slotObj["o"] = "list";
  slotObj["pi"] = pinIndex;
  slotObj["index"] = slotIndex;
  slotObj["start"] = slot.startHour * 60 + slot.startMinute;  // 분 단위로 변환
  slotObj["end"] = slot.endHour * 60 + slot.endMinute;        // 분 단위로 변환
  slotObj["rm"] = slot.repeatMode;
  slotObj["dw"] = slot.dayOfWeek;
  // 전체 타임슬롯 중 현재 타임슬롯의 인덱스 + 1 / 전체 타임슬롯 수
  slotObj["pn"] = String(slotIndex + 1) + "/" + String(timeSlots[pinIndex].size());

  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}


void startSendingTimeSlots(int pinIndex) {
  timeManager.slotIndexToSend = 0;
  timeManager.currentPinIndex = pinIndex;
}
/* Time Schedule =====================================================*/

/* Tools ===========================================================*/
void Device::loop() {
  unsigned long currentTime = millis();  // 현재 시간을 가져옵니다

  if (currentTime - this->lastTime >= this->interval) {
    this->lastTime = currentTime;

    // 모든 입력 포트를 확인
    for (int i = 0; i < numberOfPins; i++) {
      bool currentState = digitalRead(inputPins[i]) == HIGH;  // HIGH일 경우 true, LOW일 경우 false로 변환

      // 입력 값이 바뀌었을 때 처리
      if (dev.in[i] != currentState) {
        dev.in[i] = currentState;

        // 트리거 발생 조건 수정 (currentState가 HIGH 때)
        if (currentState == HIGH) {
          //Serial.printf("⚡ 트리거 발생: 포트 %d (HIGH 상태)\n", i);
          this->setPendingExec(i,currentState);
          this->printPendingOutputs();
        }
        // currentState가 LOW일 때도 트리거 발생
        if (currentState == LOW) {
          //Serial.printf("⚡ 트리거 발생: 포트 %d (LOW 상태)\n", i);
          this->setPendingExec(i,currentState);
          this->printPendingOutputs();
        }
      }
    }

    // 상태 변경 여부와 관계없이 상태 전송
    this->sendStatusCheckChange(true);
  }

  // ✅ 실행 조건 확인 및 수행
  this->processPendingExecutions();
}

void Device::setPendingExec(int port, bool currentState) {
  for (auto& po : pendingOutputs) {
    if (po.port == port && po.trigger == currentState) {  // trigger가 currentState와 일치할 때만 실행
      po.exec = true;
      po.executeTime = millis() + po.delay * 1000;
    }
  }
}

void Device::printPendingOutputs() {
  if (pendingOutputs.empty()) {
    Serial.println("📭 pendingOutputs 비어 있음");
    return;
  }

  Serial.println("📤 [MQTT 형식 PendingOutput 목록]");
  for (const auto& po : pendingOutputs) {
    Serial.printf(
      "{\"slotIndex\":%d,\"port\":%d,\"trigger\":%s,\"delay\":%d,\"exec\":%s,"
      "\"mac\":\"%s\",\"n\":%d,\"v\":%d}\n",
      po.slotIndex,
      po.port,
      po.trigger ? "true" : "false",
      po.delay,
      po.exec ? "true" : "false",
      po.change.mac.c_str(),
      po.change.port,
      po.change.value ? 1 : 0
    );
  }
}

void Device::addPendingOutput(PendingOutput output) {
  pendingOutputs.push_back(output);
}

void Device::removePendingOutput(int slotIndex) {
  for (auto it = pendingOutputs.begin(); it != pendingOutputs.end(); ++it) {
    if (it->slotIndex == slotIndex) {
        pendingOutputs.erase(it);
        break;
    }
  }
}


void Device::processPendingExecutions() {
  unsigned long now = millis();

  for (int i = 0; i < pendingOutputs.size(); ) {
    PendingOutput& po = pendingOutputs[i];
    if (po.exec && now >= po.executeTime) {
      // ✅ 각 PendingOutput 내용 출력
      //Serial.printf("⚙️ 실행됨 → slotIndex=%d | port=%d → value=%d\n", po.slotIndex, po.change.port, po.change.value ? 1 : 0);
      if (po.change.mac == this->mac) {
        this->digitalWriteUpdateData(po.change.port, po.change.value);
      } else {
        // MQTT로 전송
        DynamicJsonDocument doc(256);
        doc["c"] = "so";
        doc["m"] = po.change.mac;
        doc["n"] = po.change.port;
        doc["v"] = po.change.value ? 1 : 0;
        doc["sI"] = po.slotIndex;
        serializeJson(doc, this->sendData);
        wifi.publishMqtt();
      }
      po.exec = false; // ✅ 다시 대기 상태로 되돌림
      //this->printPendingOutputs();
    } else {
      ++i;  // 조건이 안 되면 다음 항목으로
    }
  }
}

void Device::digitalWriteUpdateData(int pin, bool value) {
  // dev.out 업데이트
  dev.out[pin] = value;
  // 출력 포트로 값 설정
  digitalWrite(outputPins[pin], value ? HIGH : LOW);
  this->sendStatusCheckChange(false);
}

void Device::loadPinStatesFromSPIFFS() {
  for (int port = 0; port < numberOfPins; ++port) {
    String fileName = "/pinState_" + String(port) + ".json";

    // === 파일이 없으면 빈 배열 구조로 초기화하고 저장 ===
    if (!SPIFFS.exists(fileName)) {
      dev.pinStateChanges[port][0].clear();
      dev.pinStateChanges[port][1].clear();

      File file = SPIFFS.open(fileName, FILE_WRITE);
      if (file) {
        DynamicJsonDocument doc(256);
        doc.createNestedArray("changes");  // 빈 changes 배열
        serializeJson(doc, file);
        file.close();
        Serial.println("📄 Created empty config: " + fileName);
      } else {
        Serial.println("⚠️ Failed to create file: " + fileName);
      }
      continue;  // 다음 포트로 넘어감
    }

    // === 파일 존재할 경우 로딩 ===
    File file = SPIFFS.open(fileName, FILE_READ);
    if (!file) {
      Serial.println("❌ Failed to open file: " + fileName);
      continue;
    }

    size_t size = file.size();
    if (size > 1024) {
      Serial.println("❌ File too large: " + fileName);
      file.close();
      continue;
    }

    std::unique_ptr<char[]> buf(new char[size + 1]);
    file.readBytes(buf.get(), size);
    buf[size] = '\0';  // Null terminate for safety
    file.close();

    //Serial.println("📥 Loading config: " + fileName);
    //Serial.println(buf.get());

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, buf.get());

    if (error) {
      Serial.println("❌ Failed to parse JSON: " + String(error.c_str()));
      continue;
    }

    JsonArray changesArray = doc["changes"];
    dev.pinStateChanges[port][0].clear();
    dev.pinStateChanges[port][1].clear();

    for (JsonObject changeObj : changesArray) {
      PinStateChange change;
      change.mac = changeObj["mac"] | "";
      change.port = changeObj["port"] | -1;
      change.value = changeObj["value"] | false;
      change.timestamp = changeObj["timestamp"] | 0;

      int stateIndex = change.value ? 1 : 0;  // true=1, false=0
      dev.pinStateChanges[port][stateIndex].push_back(change);
    }

    //Serial.printf("✅ Loaded %d change(s) for port %d\n", changesArray.size(), port);
  }
}

// 핀 상태를 다이렉트 또는 MQTT로 전송하는 함수 정의
//입력 포트가 ON 또는 OFF될 때 연결된 출력 동작을 실행하기 위해 호출되는 함수입니다.
//sendStatusCheckChange() 함수에서 입력 상태가 변경되었을 때 실행됩니다.
void Device::sendOut(int port, bool portState) {
  // 설정된 출력이 없으면 아무 작업도 하지 않음
  if (dev.pinStateChanges[port][int(portState)].empty()) {
    Serial.println("❌ No output binding set for input port " + String(port) + " with state " + String(portState));
    return;
  }

  PinStateChange change = dev.pinStateChanges[port][int(portState)][0];  // pinStateChanges[port][1]에서 첫 번째 항목 가져오기
  if (change.mac == dev.mac) {
    // MAC 주소가 현재 장치의 MAC 주소와 동일하면 직접 포트로 출력
    this->digitalWriteUpdateData(change.port,change.value);
    Serial.println("Direct output to port " + String(change.port) + " with value " + String(change.value));
  } else {
    // MAC 주소가 다르면 MQTT 메시지 전송
    DynamicJsonDocument doc(256);
    doc["c"] = "so";
    doc["m"] = change.mac;
    doc["n"] = change.port;
    doc["v"] = change.value ? 1 : 0;  // ✅ true → 1, false → 0
    String output;
    serializeJson(doc, output);
    client.publish(wifi.inTopic, output.c_str());
    Serial.println("MQTT message sent: " + output);
  }
}

//dataChange=true이전값과 비교하여 값이 변했으면 데이터 보낸다.
//dataChange=false 무조건 데이터 보낸다.
void Device::sendStatusCheckChange(bool dataChange) {
  DynamicJsonDocument responseDoc(1024);
  responseDoc["m"] = dev.mac;

  JsonArray inArray = responseDoc.createNestedArray("in");
  for (int i = 0; i < numberOfPins; i++) {
    inArray.add(dev.in[i]); // mqtt보내기위한 문장 작성
    //in 포트 입력 변화시 여기 설정된 출력값 실행
    int currentState = digitalRead(inputPins[i]);
    if (dev.in[i] != currentState) {
      dev.in[i] = currentState;
      sendOut(i, currentState); // 상태 변화를 MQTT로 전송
    }
  }

  JsonArray outArray = responseDoc.createNestedArray("out");
  for (int i = 0; i < numberOfPins; i++) {
    outArray.add(dev.out[i]);
  }
  dev.sendData="";
  serializeJson(responseDoc, dev.sendData);

  if(dataChange == false && wifi.isConnectedMqtt == true) {
    wifi.publishMqtt();
  }

  if( !dev.sendData.equals(dev.sendDataPre)) {
    dev.sendDataPre = dev.sendData;
    if(wifi.isConnectedMqtt == true) {
      Serial.println(dev.sendData);
      wifi.publishMqtt();
    }
  }
}

void Config::loadPendingOutputsFromFile(int portNo) {
  String fileName = "/bio_" + String(portNo) + ".json";
  if (!SPIFFS.exists(fileName)) {
    Serial.println("ℹ️ 스킵됨 (파일 없음): " + fileName);
    return;
  }

  File file = SPIFFS.open(fileName, FILE_READ);
  if (!file) { Serial.println("❌ 파일 열기 실패: " + fileName); return; }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) { Serial.println("❌ JSON 파싱 실패: " + String(error.c_str())); return; }

  if (!doc.containsKey("ps") || !doc["ps"].is<JsonArray>()) {
    Serial.println("❌ 'ps' 키가 없거나 배열이 아닙니다.");
    return;
  }

  JsonArray psArray = doc["ps"];
  for (JsonObject obj : psArray) {
    PendingOutput po;
    po.port = portNo;

    // ✅ 'tr' robust 파싱 (bool 또는 0/1 모두 허용)
    if (obj.containsKey("tr")) {
      if (obj["tr"].is<bool>()) {
        po.trigger = obj["tr"].as<bool>();
      } else {
        po.trigger = (obj["tr"].as<int>() != 0);
      }
    } else {
      // 과거 파일 호환: 기본 false로 두고 경고
      po.trigger = false;
      Serial.println("⚠️ 'tr' 없음 → 기본 false로 로드");
    }

    po.slotIndex    = obj["sI"] | 0;
    po.delay        = obj["d"]  | 0;
    po.executeTime  = obj["et"] | 0;
    po.exec         = false;

    po.change.mac       = obj["m"] | "";
    po.change.port      = obj["n"] | -1;
    po.change.value     = (obj["v"] | 0) == 1;
    po.change.timestamp = millis();

    dev.addPendingOutput(po);

    Serial.printf("📥 로드됨: slotIndex=%d | port=%d | tr=%s → mac=%s, n=%d, v=%d, d=%d\n",
      po.slotIndex, po.port, po.trigger ? "true" : "false",
      po.change.mac.c_str(), po.change.port, po.change.value ? 1 : 0, po.delay);
  }
}


void Config::savePendingOutputsToFile(int portNo) {
  String fileName = "/bio_" + String(portNo) + ".json";
  DynamicJsonDocument doc(4096);
  JsonArray ps = doc.createNestedArray("ps");

  for (const auto& po : dev.pendingOutputs) {
    if (po.port != portNo) continue;

    JsonObject o = ps.createNestedObject();
    o["tr"] = po.trigger ? 1 : 0;       // ✅ 반드시 기록
    o["sI"] = po.slotIndex;
    o["d"]  = po.delay;
    o["et"] = po.executeTime;
    o["m"]  = po.change.mac;
    o["n"]  = po.change.port;
    o["v"]  = po.change.value ? 1 : 0;
  }

  File f = SPIFFS.open(fileName, FILE_WRITE);
  if (!f) {
    Serial.println("❌ 파일 쓰기 실패: " + fileName);
    return;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println("💾 저장 완료: " + fileName);
}


// Config 파일을 SPIFFS에서 읽어오는 함수
void Config::loadConfigFromSPIFFS() {
  Serial.println("파일 읽기");

  if (!config.initializeSPIFFS()) {
    Serial.println("Failed to initialize SPIFFS.");
    return;
  }

  if (!SPIFFS.exists("/config.txt")) {
    Serial.println("Config file does not exist.");
    return;
  }

  File configFile = SPIFFS.open("/config.txt", FILE_READ);
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  wifi.ssid = doc["ssid"] | "";
  wifi.password = doc["password"] | "";
  wifi.email = doc["email"] | "";
  wifi.mqttBroker = doc["mqttBroker"] | "";

  Serial.print("wifi.ssid: "); Serial.println(wifi.ssid);
  Serial.print("wifi.password: "); Serial.println(wifi.password);
  Serial.print("wifi.email: "); Serial.println(wifi.email);
  Serial.print("wifi.mqttBroker: "); Serial.println(wifi.mqttBroker);
  configFile.close();
}

void Config::saveConfigToSPIFFS() {
  Serial.println("config.txt 저장");

  if (!config.initializeSPIFFS()) {
    Serial.println("SPIFFS 초기화 실패.");
    return;
  }

  // SPIFFS 초기화를 시도합니다.
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS failed to initialize. Formatting...");
    // 초기화 실패 시 포맷을 시도합니다.
    if (!SPIFFS.format()) {
      Serial.println("SPIFFS format failed.");
      return;
    }
    // 포맷 후에 다시 초기화를 시도합니다.
    if (!SPIFFS.begin()) {
      Serial.println("SPIFFS failed to initialize after format.");
      return;
    }
  }

  File configFile = SPIFFS.open("/config.txt", FILE_WRITE);
  
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  DynamicJsonDocument doc(1024);

  // 데이터를 구조체에서 가져온다고 가정합니다.
  doc["ssid"] = wifiSave.ssid;
  doc["password"] = wifiSave.password;
  doc["email"] = wifiSave.email;
  doc["mqttBroker"] = wifiSave.mqttBroker;

  Serial.print("wifi.ssid: "); Serial.println(wifiSave.ssid);
  Serial.print("wifi.password: "); Serial.println(wifiSave.password);
  Serial.print("wifi.email: "); Serial.println(wifiSave.email);
  Serial.print("wifi.mqttBroker: "); Serial.println(wifiSave.mqttBroker);

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write to file");
    configFile.close();
    return;
  }

  configFile.close();
  // 파일이 제대로 닫혔는지 확인합니다.
  if (configFile) {
    Serial.println("파일이 여전히 열려있습니다.");
  } else {
    Serial.println("파일이 성공적으로 닫혔습니다.");
  }
  Serial.println("파일 저장 끝");

  // 파일이 제대로 저장되었는지 확인합니다.
  if (SPIFFS.exists("/config.txt")) {
    Serial.println("Config file saved successfully.");
    // 저장이 확인된 후 재부팅을 진행합니다.
    Serial.println("Rebooting...");
    delay(1000); // 재부팅 전에 짧은 지연을 줍니다.
    ESP.restart();
  } else {
    Serial.println("Config file was not saved properly.");
  }
  
  // ESP32 재부팅
  delay(1000);
  ESP.restart();
}

// SPIFFS를 초기화하고 필요한 경우 포맷하는 함수를 정의합니다.
bool Config::initializeSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS 초기화 실패!");
    if (!SPIFFS.format()) {
      Serial.println("SPIFFS 포맷 실패!");
      return false;
    }
    if (!SPIFFS.begin()) {
      Serial.println("포맷 후 SPIFFS 초기화 실패!");
      return false;
    }
  }
  return true;
}

void Device::checkFactoryDefault() {
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    digitalWrite(ledPin, LOW);
    Serial.println("Please wait over 3 min");
    SPIFFS.format();
    delay(1000);
    ESP.restart();
    delay(1000);
  }
}

// httpsupdate()
// 다운로드 함수 
void Tool::download_program(String fileName) {
  // 다운로드 시작 전에 LED 깜박이기
  this->blinkLed(10);
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure clientSecure;
    clientSecure.setInsecure();  // 인증서 검증 무시

    // Add optional callback notifiers
    httpUpdate.onStart([]() {
      Serial.println("Update Started");
    });
    httpUpdate.onEnd([]() {
      Serial.println("Update Finished");
      tool.blinkLed(30);
    });
    httpUpdate.onProgress([](int cur, int total) {
      Serial.printf("Progress: %d%%\n", (cur * 100) / total);
    });
    httpUpdate.onError([](int error) {
      Serial.printf("Update Error: %d\n", error);
    });

    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String url = "https://github.com/kdi6033/download/raw/main/" + fileName;
    Serial.println("Downloading from: " + url);
    
    // 서버에서 HTTP 응답 코드 확인 추가
    t_httpUpdate_return ret = httpUpdate.update(clientSecure, url);
    Serial.printf("HTTP Code: %d\n", clientSecure.connected() ? clientSecure.available() : -1);
  
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
}

void Tool::blinkLed(int iteration) {
  for (int i = 0; i < iteration; i++) {
    digitalWrite(ledPin, HIGH); // LED 켜기
    delay(100);                 // 0.1초 대기
    digitalWrite(ledPin, LOW);  // LED 끄기
    delay(100);                 // 0.1초 대기
  }
}

/* Tools ===========================================================*/
void parseJSONPayload(byte* payload, unsigned int length) {
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';  // Null-terminate the string
  Serial.println(payloadStr);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payloadStr);

  if (error) {
    Serial.println("JSON 파싱 실패!");
    return;
  }

  String command = doc["c"] | "";

  //command == "si"  ================================================================
  if (command == "si") {
    const char *ssid = doc["ssid"] | "";
    const char *password = doc["password"] | "";
    const char *email = doc["e"] | "";
    const char *mqttBroker = doc["mqttBroker"] | "";

    wifiSave.ssid = ssid;
    wifiSave.password = password;
    wifiSave.email = email;
    wifiSave.mqttBroker = mqttBroker;

    Serial.print("wifi.ssid: "); Serial.println(wifiSave.ssid);
    Serial.print("wifi.password: "); Serial.println(wifiSave.password);
    Serial.print("wifi.email: "); Serial.println(wifiSave.email);
    Serial.print("wifi.mqttBroker: "); Serial.println(wifiSave.mqttBroker);
    config.saveConfigToSPIFFS();
  }

  // 수신된 메시지에서 mac 주소를 읽어옵니다.
  String receivedMac = doc["m"] | "";
  // 이 기기의 MAC 주소와 비교합니다.
  Serial.println(receivedMac);
    Serial.println(dev.mac);
  if (receivedMac != dev.mac) {
    Serial.println("Received MAC address does not match device MAC address. Ignoring message.");
    return;
  }

  //command == "df"  ================================================================
  if (command == "df") {
    //펌웨어 다운로드
    const char *fileName = doc["f"] | "";
    tool.download_program(fileName);
  }

  //command == "so"  ================================================================
  else if (command == "so") {
    //mqtt 로 전송된 출력을 실행한다.
    // JSON 메시지에서 "no"와 "value" 값을 읽어옵니다.
    int no = doc["n"] | -1;  // 유효하지 않은 인덱스로 초기화
    int intValue = doc["v"] | 0;
    bool value = (intValue == 1);  // 1이면 true, 아니면 false
    dev.digitalWriteUpdateData(no, value);
  }

  //command == "bio"  ================================================================
  else if (command == "bio") {
    String oper = doc["o"] | "";  // "o"는 operation (예: "save")
    int portNo = doc["n"] | -1;   // 포트 번호
    JsonArray portStates = doc["ps"].as<JsonArray>();  // 포트 상태 배열

    if (oper == "insert") {
      //Serial.println(payloadStr);
      //{"c":"bio","d":10,"m":"D4:8A:FC:B5:30:10","o":"insert","n":0,"tr":1,"ps":[{"m":"D4:8A:FC:B5:30:10","n":0,"v":1}]}
      int inputPort = doc["n"] | -1;
      bool triggerValue = (doc["tr"] | -1) == 1;
      int triggerDelay = doc["d"] | 0;

      if (inputPort < 0 || inputPort >= numberOfPins) {
        Serial.println("❌ Invalid input port.");
        return;
      }

      if (triggerValue != true && triggerValue != false) {
        Serial.println("❌ Invalid trigger value.");
        return;
      }

      //1. 포트 상태 배열 처리
      for (JsonObject portState : portStates) {
        PendingOutput po;
        po.port = inputPort;
        po.trigger = triggerValue;
        po.slotIndex = dev.pendingOutputs.size();  // 고유 index 부여
        po.delay = triggerDelay;
        po.executeTime = millis() + triggerDelay * 1000;
        po.exec = false;

        // 포트 상태 변경 정보 설정
        po.change.mac = portState["m"] | "";
        po.change.port = portState["n"] | -1;
        po.change.value = (portState["v"] | 0) == 1;
        po.change.timestamp = millis();

        dev.addPendingOutput(po);

        // 핀 상태 조건에도 등록
        int val = po.trigger ? 1 : 0;
        dev.pinStateChanges[po.port][val].push_back(po.change);

        Serial.printf("✅ PendingOutput 추가됨: in[%d]=%s → out[%d]=%s, delay=%d, slotIndex=%d\n",
          po.port, po.trigger ? "ON" : "OFF",
          po.change.port, po.change.value ? "ON" : "OFF",
          po.delay, po.slotIndex
        );
      }
      // SPIFFS에 저장
      config.savePendingOutputsToFile(inputPort);
      //dev.printPendingOutputs();
    }
    

    else if (oper == "list" && portNo >= 0 && portNo < numberOfPins) {
      Serial.printf("📋 포트 %d의 pendingOutputs 리스트 개별 전송 중...\n", portNo);

      dev.printPendingOutputs();  // 디버깅용 전체 출력

      for (const auto& po : dev.pendingOutputs) {
        if (po.port != portNo) continue;  // 현재 요청한 포트만 응답

        DynamicJsonDocument responseDoc(512);
        responseDoc["c"] = "bio";
        responseDoc["o"] = "list";
        responseDoc["n"] = portNo;

        // ✅ 배열로 생성
        JsonArray psArray = responseDoc.createNestedArray("ps");

        JsonObject obj = psArray.createNestedObject();
        obj["tr"] = po.trigger;
        obj["d"] = po.delay;
        obj["m"] = po.change.mac;
        obj["n"] = po.change.port;
        obj["v"] = po.change.value;
        obj["sI"] = po.slotIndex;

        dev.sendData = "";  // 이전 데이터 초기화
        serializeJson(responseDoc, dev.sendData);
        Serial.println(dev.sendData);
        wifi.publishMqtt();  // 개별 전송
        delay(100);  // 전송 간격 조절
      }

      Serial.println("✅ pendingOutputs 개별 bio list 전송 완료");
    }

    else if (oper == "delete" && portNo >= 0 && portNo < (numberOfPins)) {
      int targetSlot = doc["sI"] | -1;
      if (targetSlot < 0) {
        Serial.println("❌ 잘못된 slotIndex");
        return;
      }

      // 1. pendingOutputs 에서 삭제
      int beforeSize = dev.pendingOutputs.size();
      dev.removePendingOutput(targetSlot);  // ✅ 함수 호출

      // 2. 삭제 확인
      if (dev.pendingOutputs.size() < beforeSize) {
        Serial.printf("🗑️ slotIndex=%d 삭제 완료\n", targetSlot);
      } else {
        Serial.printf("⚠️ 지정된 slotIndex를 찾을 수 없음: %d\n", targetSlot);
      }

      // 3. SPIFFS에 저장
      config.savePendingOutputsToFile(portNo);
    }

    else if (oper == "deleteAll" && portNo >= 0 && portNo < (numberOfPins)) {
      Serial.printf("🧹 포트 %d의 모든 조건 삭제 시작...\n", portNo);

      // 1. 해당 포트의 핀 상태 초기화
      if (portNo < numberOfPins) {
        dev.pinStateChanges[portNo][0].clear();  // LOW 상태
        dev.pinStateChanges[portNo][1].clear();  // HIGH 상태

        // 2. 해당 포트의 SPIFFS 파일 삭제
        String fileName = "/pinState_" + String(portNo) + ".json";
        if (SPIFFS.exists(fileName)) {
          SPIFFS.remove(fileName);
          Serial.println("🗑️ 파일 삭제됨: " + fileName);
        } else {
          Serial.println("⚠️ 파일 없음: " + fileName);
        }
      }

      // 3. 해당 포트의 pendingOutputs만 삭제
      dev.pendingOutputs.erase(
        std::remove_if(dev.pendingOutputs.begin(), dev.pendingOutputs.end(),
                      [portNo](const PendingOutput& po) { return po.port == portNo; }),
        dev.pendingOutputs.end()
      );
      Serial.printf("🗑️ 포트 %d의 pendingOutputs 삭제 완료\n", portNo);

      Serial.printf("✅ 포트 %d의 deleteAll 완료\n", portNo);
    }

    else {
      Serial.println("❗ Invalid bio save request or port number");
    }
  }

  //command == "sch"  ================================================================
  else if (command == "sch") {
    Serial.println(payloadStr);
    String oper = doc["o"] | "";
    int pinIndex = doc["pi"] | -1;

    if (oper == "insert") {
      //{"c":"sch","m":"D4:8A:FC:B5:30:10","o":"insert","pi":0,"start":796,"end":796,"rm":"w","dw":[2,6]}
      int start = doc["start"] | 0;  // 분 단위
      int end = doc["end"] | 0;      // 분 단위
      String repeatMode = doc["rm"] | "daily";
      int pinIndex = doc["pi"] | 0;  // 출력 포트 인덱스

      // 분 → 시:분 변환
      int startHour = start / 60;
      int startMinute = start % 60;
      int endHour = end / 60;
      int endMinute = end % 60;

      // "rm"이 "w"일 때만 "dW" 배열 처리
      if (repeatMode == "weekly") {
        // "dW" 값이 JsonArray로 변환되는지 확인하고 출력
        if (doc["dw"].is<JsonArray>()) {
          JsonArray days = doc["dw"].as<JsonArray>();
          
          // JsonArray에서 값이 잘 추출되고 있는지 확인
          String jsonString;
          serializeJson(doc["dw"], jsonString);  // dW를 문자열로 변환하여 출력
          Serial.println("dw 배열: " + jsonString);  // dw 배열 확인 출력
          // JsonArray에서 값이 잘 추출되고 있는지 확인
          Serial.println("dw 배열 크기: " + String(days.size()));  // dw 배열 크기 확인
          for (int i = 0; i < days.size(); i++) {
              int day = days[i];
              Serial.println("########## day: " + String(day));  // day 값 확인
              addTimeSlot(pinIndex, startHour, startMinute, endHour, endMinute, repeatMode, day);
          }
        } else {
          // "dW"가 JsonArray가 아닌 경우
          Serial.println("Error: dW is not a JsonArray");
        }
      } else if (repeatMode == "daily") {
        // "rm"이 "d"일 경우 dw 배열을 무시하고 기본 처리
        Serial.println("rm is 'd', ignoring dw array");
        addTimeSlot(pinIndex, startHour, startMinute, endHour, endMinute, repeatMode, -1); // "d"일 경우 -1로 처리
      }
      saveTimeSlotsToSPIFFS(pinIndex); // SPIFFS 저장
      Serial.println("✅ Time slot added and saved to SPIFFS: " + String(pinIndex));
      startSendingTimeSlots(pinIndex);  // 목록 전송
    }

    else if (oper == "delete") {
      int slotIndex = doc["sI"] | -1;
      if (slotIndex >= 0 && slotIndex < timeSlots[pinIndex].size()) {
        removeTimeSlot(pinIndex, slotIndex);
        saveTimeSlotsToSPIFFS(pinIndex);
        Serial.println("Time slot removed and saved to SPIFFS");
        startSendingTimeSlots(pinIndex);
      } else {
        Serial.println("Invalid slot index.");
      }
    }

    else if (oper == "deleteAll") {
      removeAllTimeSlots(pinIndex);
      saveTimeSlotsToSPIFFS(pinIndex);
      Serial.println("All time slots removed for pin " + String(pinIndex));
    }

    else if (oper == "list") {
      startSendingTimeSlots(pinIndex);  // 타임 슬롯 전송
    }
  }
  return;
}


void setup() {
  Serial.begin(115200);

  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // SPIFFS 먼저
  if (!SPIFFS.begin()) {
    Serial.println("❌ SPIFFS 초기화 실패 → 포맷 후 재시도");
    if (SPIFFS.format() && SPIFFS.begin()) {
      Serial.println("✅ SPIFFS 재마운트 성공");
    } else {
      Serial.println("💥 SPIFFS 치명적 오류");
    }
  }
  
  config.loadConfigFromSPIFFS();

  if (wifi.ssid.isEmpty()) {
    Serial.println("Bluetooth 셋업");
    ble.setup();
    // BLE이 제대로 초기화될 수 있도록 약간의 시간을 기다립니다.
    delay(1000);
    Serial.println("BLE ready!");
    return;
  }
  wifi.connectToWiFi();

  // Set each output pin as an output
  for (int i = 0; i <numberOfPins; i++) {
    pinMode(outputPins[i], OUTPUT);
  }
  // Set each input pin as an input
  for (int i = 0; i < numberOfPins; i++) {
    pinMode(inputPins[i], INPUT);
  }


  // ✅ PendingOutputs 로드 추가
  for (int i = 0; i < numberOfPins; i++) {
    config.loadPendingOutputsFromFile(i);  // 이 함수를 실행하면 에러가 발생해요
  }

  // MQTT 설정
  client.setServer(wifi.mqttBroker.c_str(), 1883);
  client.setCallback(callback);
  wifi.startupTime = millis();
  wifi.reconnectMQTT();

  timeManager.setup();

  // Load pin states from SPIFFS
  dev.loadPinStatesFromSPIFFS();
  
  // setup이 끝나는 시점에서 메모리 사용량 출력
  Serial.print("Free heap memory after setup: ");
  Serial.println(esp_get_free_heap_size());
}


void loop() {
  if(!ble.boot) {
    dev.loop();
    wifi.loop();
    timeManager.loop();
  }
  dev.checkFactoryDefault();
}


