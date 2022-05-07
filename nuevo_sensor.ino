#include <MHZ19.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define CO2_MEASURE_PERIOD_MS 1000
#define CO2_CALIBRATION_OFFSET 20
#define SERIAL_BAUD_RATE 9600

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";

MHZ19 co2;
int last_co2_value = 0;
float last_temperature_value = 0.0;
unsigned int last_raw_co2_value = 0;
int last_background_co2_value = 0;

WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];

void setup() {

  Serial.begin(SERIAL_BAUD_RATE);
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

    // Obtención de datos
    last_co2_value = co2.getCO2();
    last_co2_value = last_co2_value + CO2_CALIBRATION_OFFSET;
    last_temperature_value = co2.getTemperature();
    last_raw_co2_value = co2.getCO2Raw();
    last_background_co2_value = co2.getBackgroundCO2();
    
    // Serial.println(last_co2_value);

    // Publicación de datos mediante MQTT
    snprintf (msg, 50, "%d", last_co2_value);
    client.publish("instituto/clase/co2", msg);
    snprintf (msg, 50, "%f", last_temperature_value);
    client.publish("instituto/clase/temperatura", msg);
    snprintf (msg, 50, "%d", last_raw_co2_value);
    client.publish("instituto/clase/rawco2", msg);
    snprintf (msg, 50, "%d", last_background_co2_value);
    client.publish("instituto/clase/backgroundco2", msg);

    
    lastCo2Measure = millis();
  }

}
