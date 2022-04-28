#include <MHZ19.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define CO2_MEASURE_PERIOD_MS 1000
#define CO2_CALIBRATION_OFFSET 20

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";

MHZ19 co2;
int last_co2_value = 0;
WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];

void setup() {

  Serial.begin(9600);
  co2.begin(Serial);
  // co2.autoCalibration(true);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266Client")) {
      client.publish("instituto/clase/co2", "Enviando el primer dato");
    } else {
      delay(5000);
    }
  }
}

void loop() {

  static unsigned long lastCo2Measure = millis();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if( millis() - lastCo2Measure > CO2_MEASURE_PERIOD_MS){
    last_co2_value = co2.getCO2();
    last_co2_value = last_co2_value + CO2_CALIBRATION_OFFSET;
    // Serial.println(last_co2_value);
    snprintf (msg, 50, "%d", last_co2_value);
    client.publish("instituto/clase/co2", msg);
    lastCo2Measure = millis();
  }

}
