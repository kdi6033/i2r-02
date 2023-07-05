#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPIFFS.h> 
#include <FS.h>
#include <ESP.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

int type=31; // 기기 인식번호
#define TRIGGER_PIN 34 

// Output pin numbers
const int outputPins[4] = {26, 27, 32, 33};
// Input pin numbers
const int inputPins[4] = {16, 17, 18, 19};

const char* ssid = "i2r";
const char* password = "00000000";
String mqtt_server = "***.amazonaws.com";
const char* outTopic = "/i2r/outTopic"; 
const char* inTopic = "/i2r/inTopic"; 
const char* clientName = "";

unsigned long currentMillis=0;
unsigned long previousMillis = 0;     
const long interval = 1000;  
String sMac;
int value;
char msg[200];
int mqttConnected=0; // 1=연결 0=끊김

int Out[8]={0},In[10]={0};  // plc 입력과 출력 저장 
// pls return 메세지 처리
String sIn="";  // 받은 문자열
int serial2InTime=0;
String strIn="0000",strInPre="0001"; // In[] 을 string으로 저장


WiFiClientSecure espClient;
//PubSubClient client(AWS_IOT_ENDPOINT, 8883, espClient);
PubSubClient client(mqtt_server.c_str(), 8883, espClient);

//json을 위한 설정
StaticJsonDocument<200> doc;
DeserializationError error;
JsonObject root;

void bootWifiStation();
void callback(char* topic, byte* payload, unsigned int length);
void doTick();
void publishMqtt();
void reconnect();
void readConfig();
void saveConfig();
void setup();


const int ledPin = 2;

void setup() {
  // Set each output pin as an output
  for (int i = 0; i < 4; i++) {
    pinMode(outputPins[i], OUTPUT);
  }
  // Set each input pin as an input
  for (int i = 0; i < 4; i++) {
    pinMode(inputPins[i], INPUT);
  }
  pinMode(ledPin, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  Serial.begin(115200);

  //이름 자동으로 생성
  uint8_t chipid[6]="";
  WiFi.macAddress(chipid);
  char* cChipID = new char[13];
  sprintf(cChipID,"%02x%02x%02x%02x%02x%02x%c",chipid[5], chipid[4], chipid[3], chipid[2], chipid[1], chipid[0],0);
  sMac=String(cChipID);
  clientName = sMac.c_str();
  Serial.println("");
  Serial.println(clientName);

  //readConfig();
  bootWifiStation();

  client.setCallback(callback);
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  espClient.setCACert(AWS_CERT_CA);
  espClient.setCertificate(AWS_CERT_CRT);
  espClient.setPrivateKey(AWS_CERT_PRIVATE);
}

void bootWifiStation() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //공장리셋
    if ( digitalRead(TRIGGER_PIN) == LOW ) 
      factoryDefault();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  String s;
  deserializeJson(doc,payload);
  root = doc.as<JsonObject>();
  const char* macIn = root["mac"];
  if( sMac.equals(String(macIn))) {
    int no = root["outNo"];
    int value = root["value"];
    Out[no]=value;
    Serial.print(" ");
    Serial.print(no);
    Serial.print(" ");
    Serial.print(outputPins[no]);
    Serial.print(" ");
    Serial.println(Out[no]);
    digitalWrite(outputPins[no], Out[no]);
  }
}

// mqtt 통신에 지속적으로 접속한다.
void reconnect() {
  if(WiFi.status() != WL_CONNECTED)
    return;
  // Loop until we're reconnected
  //while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientName)) {
      Serial.println("connected");
      client.subscribe(inTopic);
      mqttConnected=1;
      digitalWrite(ledPin, mqttConnected);  //mqtt 상태표시
      publishMqtt();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      mqttConnected=0;
      digitalWrite(ledPin, mqttConnected);  //mqtt 상태표시
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  //}
}

void loop() {
  doTick();
  //공장리셋
  if ( digitalRead(TRIGGER_PIN) == LOW ) 
    factoryDefault();
}

//1초 마다 실행되는 시간함수
void doTick() {
  if(mqttConnected==1)
    client.loop();

  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    if (!client.connected())
      reconnect();
    previousMillis = currentMillis;
    for (int i = 0; i < 4; i++) {
      // Read the state of the current input pin
      In[i] = digitalRead(inputPins[i]);
    }

    strInPre=strIn;
    strIn=String(In[0])+String(In[1])+String(In[2])+String(In[3]);
    if( !strIn.equals(strInPre)) {
      publishMqtt();
      //Serial.println(strInPre);
      //Serial.println(strIn);
    }
  }

}

void publishMqtt()
{ 
  if(mqttConnected != 1)
    return;

  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  String json;
  root["mac"] = sMac;
  root["type"] = type;
  root["in"] = strIn;
  serializeJson(root, msg);
  client.publish(outTopic, msg);
  Serial.println(msg);  
}
