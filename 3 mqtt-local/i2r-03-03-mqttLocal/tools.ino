void readConfig() {
  if (!SPIFFS.begin(true)) { // SPIFFS를 시작하고 필요하면 포맷
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File f = SPIFFS.open("/config.txt", "r");
  if (!f) {
      Serial.println("file open failed");
  }  Serial.println("====== Reading from SPIFFS file =======");
  
  mqtt_server=f.readStringUntil('\n');
  f.close();
  SPIFFS.end();
  Serial.println("Mqtt Server: "+mqtt_server);
}

void saveConfig() {
  if (!SPIFFS.begin(true)) { // SPIFFS를 시작하고 필요하면 포맷
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // open file for writing
  File f = SPIFFS.open("/config.txt", "w");
  if (!f) {
      Serial.println("file open failed");
  }
  //f.println(ipMqtt);
  f.println(mqtt_server);
  f.close();
  SPIFFS.end();
  delay(2000);
  ESP.restart();
  delay(2000);
}

// trigger pin 0(D3) 2(D4)
void factoryDefault() {
    Serial.println("mode as Factory Deafault");
    Serial.println("Please wait over 3 min");
    SPIFFS.begin();
    SPIFFS.format();
    SPIFFS.end();
    delay(1000);
    ESP.restart();
    delay(1000);
}
