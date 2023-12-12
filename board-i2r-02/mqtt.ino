/* 와이파이 MQTT 함수 ===============================================*/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // JSON 파싱
  parseJSONPayload(payload, length);
}

void publishMqtt()
{ 
  char msg[200];

  //if(wifi.isConnected != 1)
  //  return;
  client.publish(wifi.outTopic, dev.sendData.c_str());
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(wifi.ssid, wifi.password);
  
  int wCount = 0;
  wifi.isConnected = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    checkFactoryDefault();
    wCount++;
    if(wCount > 20) {
      wifi.isConnected = false;
      wifi.use = false;
      returnMsg="와이파이 정보가 잘못되었습니다.";
      writeToBle(101);
      break; // while 루프를 벗어납니다.
    }
  }
  if(wifi.isConnected == true)
    Serial.println("\nConnected to Wi-Fi");
  else
    Serial.println("\nCan not find Wi-Fi");
  
}

void reconnectMQTT() {
  while (!client.connected()) {
    //checkFactoryDefault();
    Serial.println("Connecting to MQTT...");
    if (client.connect(dev.mac.c_str())) {
      Serial.println("Connected to MQTT");
      client.subscribe(wifi.inTopic); // MQTT 토픽 구독
      wifi.isConnectedMqtt=true;
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      wifi.isConnectedMqtt=false;
      delay(5000);
    }
  }
  digitalWrite(ledPin, wifi.isConnectedMqtt);
}
/* 와이파이 MQTT 함수 ===============================================*/
