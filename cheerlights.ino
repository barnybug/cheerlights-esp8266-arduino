#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

// put your wifi credentials in here
#include "credentials.h"

#define RED_LED   14
#define GREEN_LED 5
#define BLUE_LED  12

const char *mqtt_server = "iot.eclipse.org";
char mqtt_id[33];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup() {
  Serial.begin(76800);
  Serial.println("Booting");

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  
  // set range to 0-0xff
  analogWriteRange(255);
  // lower the default frequency from 1khz, which makes an awful high pitch noise when modulating
  analogWriteFreq(100);

  // update the lights to show we're loading
  digitalWrite(RED_LED, HIGH);
  setupWifi();
  digitalWrite(GREEN_LED, HIGH);
  setupOTA();
  digitalWrite(BLUE_LED, HIGH);
  setupMqtt();
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
}

void setupWifi() {
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupOTA() {
  sprintf(mqtt_id, "cheerlights-%06x", ESP.getChipId());
  ArduinoOTA.setHostname(mqtt_id);
  ArduinoOTA.onStart([]() {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
    Serial.println("\nOTA start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA end\n");
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int p = (progress * 100 / total);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    digitalWrite(RED_LED, p > 25);
    digitalWrite(BLUE_LED, p > 50);
    digitalWrite(GREEN_LED, p > 75);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();  
}

void setColor(long val) {
  int r = (val >> 16) & 0xff;
  int g = (val >> 8) & 0xff;
  int b = val & 0xff;
  analogWrite(RED_LED, r);
  analogWrite(GREEN_LED, g);
  analogWrite(BLUE_LED, b);
}

void messageReceived(char* topic, unsigned char* payload, unsigned int length) {
  if (length != 7) {
    Serial.print("Bad color: ");
    Serial.println((char*)payload);
    return;
  }
  // parse the hexidecimal value
  char color[7];
  memcpy(color, &payload[1], 6);
  color[6] = '\0';
  long val = strtol(color, NULL, 16); // base 16 (hex)
  setColor(val);
  Serial.println(color);
}

void setupMqtt() {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(messageReceived);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(mqtt_id)) {
      Serial.println("connected");
      // subscribe
      mqttClient.subscribe("cheerlightsRGB");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  ArduinoOTA.handle();
  if (!mqttClient.connected())
    reconnect();
  mqttClient.loop();
}

