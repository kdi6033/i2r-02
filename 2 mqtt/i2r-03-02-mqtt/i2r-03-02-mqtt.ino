#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// Output pin numbers
const int outputPins[4] = {26, 27, 32, 33};
// Input pin numbers
const int inputPins[4] = {16, 17, 18, 19};

const char* ssid = "405902-2.4G";
const char* password = "k01033887147";
//const char* mqtt_server = "192.168.0.2";
//IPAddress mqtt_server(192, 168, 0, 2);
const char* mqtt_server = "broker.mqtt-dashboard.com";
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
String strIn="00000000",strInPre="00000000"; // In[] 을 string으로 저장

WiFiClient espClient;
PubSubClient client(espClient);

//json을 위한 설정
StaticJsonDocument<200> doc;
DeserializationError error;
JsonObject root;

void bootWifiStation();
void callback(char* topic, byte* payload, unsigned int length);
void doTick();
void reconnect();
void setup();
void publishMqtt();

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
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    // Turn the current output pin on
    digitalWrite(outputPins[i], HIGH);

    // Wait for a second
    delay(1000);

    // Turn the current output pin off
    digitalWrite(outputPins[i], LOW);
  }

  //이름 자동으로 생성
  uint8_t chipid[6]="";
  WiFi.macAddress(chipid);
  char* cChipID = new char[13];
  sprintf(cChipID,"%02x%02x%02x%02x%02x%02x%c",chipid[5], chipid[4], chipid[3], chipid[2], chipid[1], chipid[0],0);
  sMac=String(cChipID);
  clientName = sMac.c_str();
  Serial.println("");
  Serial.println(clientName);

  bootWifiStation();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
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
  //if( sMac.equals(String(macIn))) {
    int no = root["no"];
    int value = root["value"];
    Out[no]=value;
    Serial.print(" ");
    Serial.print(no);
    Serial.print(" ");
    Serial.print(outputPins[no]);
    Serial.print(" ");
    Serial.println(Out[no]);
    digitalWrite(outputPins[no], Out[no]);
  //}

}

// mqtt 통신에 지속적으로 접속한다.
void reconnect() {
  if(WiFi.status() != WL_CONNECTED)
    return;
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientName)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish(outTopic, "Reconnected");
      // ... and resubscribe
      client.subscribe(inTopic);
      mqttConnected=1;
      digitalWrite(ledPin, mqttConnected);  //mqtt 상태표시
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      mqttConnected=0;
      digitalWrite(ledPin, mqttConnected);  //mqtt 상태표시
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  doTick();
}

//1초 마다 실행되는 시간함수
void doTick() {
  if (!client.connected())
    reconnect();
  if(mqttConnected==1)
    client.loop();

  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    for (int i = 0; i < 4; i++) {
      // Read the state of the current input pin
      In[i] = digitalRead(inputPins[i]);
    }

    strInPre=strIn;
    strIn=String(In[0])+String(In[1])+String(In[2])+String(In[3])+String(In[4])+String(In[5])+String(In[6])+String(In[7]);
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
  root["in0"] = In[0];
  root["in1"] = In[1];
  root["in2"] = In[2];
  root["in3"] = In[3];
  serializeJson(root, msg);
  client.publish(outTopic, msg);
  Serial.println(msg);  
}
