#include "credenciales.h"
#include "opciones.h"
#include <ThingSpeak.h>

#include <WiFiManager.h>

#include <strings_en.h>  // English strings for  WiFiManager

#include <DHT.h>
#include <DHT_U.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTTYPE DHT11

WiFiManager wifiManager;

WiFiClient  client;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

ESP8266WebServer server(80);

// Inicializamos el sensor DHT11
DHT dht(DHTPin, DHTTYPE);

float t;
float h;
unsigned long tiempoInicial;

/// Parámetros para calcular los ppm de CO2 mediante
/// la resistencia del sensor (según gráficas en datasheet
/// https://www.electronicoscaldas.com/datasheet/MQ-135_Hanwei.pdf )

#define PARA 116.6020682
#define PARB 2.769034857

/// Paramétros para ajustar la dependencia de humedad y temperatura
/// según la gráfica del datasheet
#define CORA 0.00035
#define CORB 0.02718
#define CORC 1.39538
#define CORD 0.0018
#define CORE -0.003333333
#define CORF -0.001923077
#define CORG 1.130128205
#define MQ135_MAXRSRO 2.428 //for CO2
#define MQ135_MINRSRO 0.358 //for CO2
/*
  NOTA IMPORTANTE: La precisión del sensor viene afectada por la temperatura y la humedad
  según las gráficas del datasheet. Por simplicidad (Haría falta añadir un medidor Tª y de humedad ambiente, como el DHT11)
  no se han introducido en la fórmula por lo que la calibración y la medición
  deben hacerse en las mismas condiciones de Tª y H

  Las medidas no serán correctas hasta que no se estabilice el número en bruto (aprox 5 minutos)

*/





float factorCorreccion(float t, float h) {
  // Calcula el factor de corrección según la temperatura ambiente y la humedad relativa

  // Basado en la linearización de la curva la dependencia es distinta por encima y debajo de
  // 20º C, siendo lineal por encima de 20.
  // Provided by Balk77 https://github.com/GeorgK/MQ135/pull/6/files

  float factor;
  if (isnan(t) || isnan(h) || t > 50.0 || h > 100.0)
    factor = 1.0;
  else if (t < 20)
    factor = CORA * t * t - CORB * t + CORC - (h - 33.) * CORD;
  else
    factor = CORE * t + CORF * h + CORG;
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.print("\tHumedad: ");
  Serial.println(h);
  Serial.print("corrección: ");
  Serial.println(factor);

  return factor;

}


float medidaResistencia() {

  float resistenciaMedia = 0.0;
  float maximo = 0;
  float minimo = 9999;
  for (int s = 0; s < MQ_Samples; s++)
  {
    int val = analogRead(MQ_PIN);
    //float resistenciaSensor = ((1023. / (float) val) * 5. - 1.) * RLOAD;
    float resistenciaSensor = ((1023. / (float) val)  - 1.) * RLOAD;
    if (resistenciaSensor > maximo)
      maximo = resistenciaSensor;
    if (resistenciaSensor < minimo)
      minimo = resistenciaSensor;
    resistenciaMedia = resistenciaMedia + resistenciaSensor;
    delay(100);
  }
  // quitamos el minimo y el máximo:
  resistenciaMedia = resistenciaMedia - maximo - minimo;
    
  return resistenciaMedia / (MQ_Samples - 2);
}

float medidaResistenciaCorregida(float t, float h) {
  return medidaResistencia() / factorCorreccion(t, h);
}

float obtenerCalibracion() {
  float resistencia = medidaResistencia();
  return resistencia * pow((ATMOCO2 / PARA), (1. / PARB));
}

float obtenerCalibracionCorregida(float t, float h) {
  return medidaResistenciaCorregida(t, h) * pow((ATMOCO2 / PARA), (1. / PARB));
}

float ppm() {
  float resistencia = medidaResistencia();
  float ratio = (resistencia / (double)RZERO);

  if (ratio < MQ135_MAXRSRO && ratio > MQ135_MINRSRO) {
    return PARA * pow(ratio, -PARB);
  }
  else
    return 0.0;
}

float ppmCorregido(float t, float h) {
  float ratio = medidaResistenciaCorregida(t, h) / (double)RZERO;
  if (ratio < MQ135_MAXRSRO && ratio > MQ135_MINRSRO)
    return PARA * pow(ratio, -PARB);
  else
    return 0;
}


void setup() {
  Serial.begin(9600);
  pinMode(DHTPin, INPUT);
  dht.begin();
  Serial.println("Conectando ");
  wifiManager.autoConnect("CO2");

  /*
   * Si no se usa wifiManager porque el essid y passwd de la wifi son fijos:
   * 
     WiFi.begin(ssid, password); // Intentamos la conexion a la WIFI

    while (WiFi.status() != WL_CONNECTED)
       {  delay(1000);
          Serial.print(".");      // Escribiendo puntitos hasta que conecte
        }
    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Nuestra IP: ");
    Serial.println(WiFi.localIP());   // Imprimir nuestra IP al conectar
  */
  Serial.println("connected");
  server.on("/", handle_OnConnect);       // De esto tenemos que hablar
  server.onNotFound(handle_NotFound);

  server.begin();
  ThingSpeak.begin(client);
  Serial.println("Iniciado el servidor HTTP");
  timeClient.begin();
  timeClient.setTimeOffset(3600); // GMT + 1
  tiempoInicial = millis();

}

void handle_NotFound()
{
  server.send(404, "text/plain", "Ni idea de a dónde quieres ir pringao");
}

void handle_OnConnect()
{
  // Esto se va a usar sólo para obtener el valor de calibración

  t = dht.readTemperature() + (float)calT;          // Leemos la temperatura
  h = dht.readHumidity() + (float)calH;                // Leemos la humedad

  float cal = obtenerCalibracionCorregida(t, h);
  Serial.print("Calibracion: ");
  Serial.println(cal);

  float ppmcorregido = ppmCorregido(t, h);
  float ppmnormal = ppm();


  server.send(200, "text/html", SendHTML(t, h, cal, ppmnormal, ppmcorregido));

}


String SendHTML(float Temperaturestat, float Humiditystat, float Calibracion, float ppm, float correctedPpm) {

  String ptr = "<!DOCTYPE html> <html>\n";
  timeClient.update();
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<meta charset=\"UTF-8\">";
  // ptr += "<meta http-equiv=\"refresh\" content=\"10\">";
  ptr += "<title>Nodo medida CO2 IES Santiago Apóstol</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>Nodo medida CO<sub>2</sub> IES Santiago Apóstol</h1>\n";
  ptr += "<h2>Nodo 1</h2>\n";
  ptr += "<h3>" +  timeClient.getFormattedTime() + "</h3>\n";
  ptr += "<p>Temperatura: ";
  ptr += Temperaturestat;
  ptr += "°C</p>";
  ptr += "<p>Humedad: ";
  ptr += (int)Humiditystat;
  ptr += "%</p>";
  ptr += "<p>Calibración: ";
  ptr += (int)Calibracion;
  ptr += "</p>";
  ptr += "<p>Lecturas (normalizada según Tª y Humedad) - bruta: ";
  ptr += (int)correctedPpm;
  ptr += " - ";
  ptr += (int)ppm;
  ptr += "</p>";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}


void loop() {
  server.handleClient();
  if (millis() - tiempoInicial > intervalo) {

    tiempoInicial = millis();
    t = dht.readTemperature() + (float)calT;         // Leemos la temperatura
    h = dht.readHumidity() + (float)calH;               // Leemos la humedad

    float ppmcorregido = ppmCorregido(t, h);
    float ppmnormal = ppm();
    Serial.print("ppm: ");
    Serial.print(ppmcorregido);
    Serial.print(" - ");
    Serial.println(ppmnormal);

    // set the fields with the values
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);
    ThingSpeak.setField(3, ppmcorregido);
    ThingSpeak.setField(4, ppmnormal);
    ThingSpeak.setField(5, RZERO);
    ThingSpeak.setField(6, factorCorreccion(t, h));
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);


  }

}
