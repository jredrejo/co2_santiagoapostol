#include "credenciales.h"
#include "opciones.h"

#if (use_thingspeak)
#include <ThingSpeak.h>
#endif

#if (use_mqtt)
#include <PubSubClient.h>
#endif
// #include <WiFiManager.h>

#include <strings_en.h>  // English strings for  WiFiManager

#include <DHT.h>
#include <DHT_U.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <EEPROM.h>


//WiFiManager wifiManager;

WiFiClient  cliente_wifi;

#if (use_mqtt)
PubSubClient mqtt_client(cliente_wifi);
char msg[50];
#endif

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



unsigned int leerWordEEPROM(int direccion) {
  EEPROM.begin(2);
  byte hiByte = EEPROM.read(direccion);
  byte loByte = EEPROM.read(direccion + 1);
  EEPROM.end();
  return word(hiByte, loByte); // usa dos bytes para generar un integer
}

void escribirWordEEPROM(int direccion, int valor) {
  EEPROM.begin(2);
  byte hiByte = highByte(valor);
  byte loByte = lowByte(valor);
  EEPROM.write(direccion, hiByte);
  EEPROM.write(direccion + 1, loByte);
  EEPROM.end();
  Serial.println("Escribiendo el valor " + (String)valor + " en " + (String)direccion);
}


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
    //Serial.print("val: ");
    //Serial.println(val);
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
  delay(300);
  Serial.println("Conectando ");

  // wifiManager.autoConnect("CO2");

  /*
     Si no se usa wifiManager porque el essid y passwd de la wifi son fijos:
  */
  Serial.println(WiFi.macAddress());

  WiFi.begin(ssid, password); // Intentamos la conexion a la WIFI

  while (WiFi.status() != WL_CONNECTED)
  { delay(1000);

    Serial.print(".");      // Escribiendo puntitos hasta que conecte
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Nuestra IP: ");
  Serial.println(WiFi.localIP());   // Imprimir nuestra IP al conectar

  Serial.println("conectado a la wifi");
  server.on("/", handle_OnConnect);       // De esto tenemos que hablar
  server.on("/SetCal", HTTP_POST, handle_Parametros);
  server.onNotFound(handle_NotFound);
  server.begin();
#if (use_thingspeak )
  ThingSpeak.begin(cliente_wifi);
#endif

  Serial.println("Iniciado el servidor HTTP");
  timeClient.begin();
  timeClient.setTimeOffset(3600); // GMT + 1
  tiempoInicial = millis() + intervalo;  //para forzar una lectura en el primer loop

#if (use_mqtt)
  mqtt_client.setServer(MQTT_Broker, MQTT_port);
  mqtt_reconnect();
#endif

  RZERO = leerWordEEPROM(RZERO_ADDR);

  if (RZERO == 0 || RZERO > 1000)  // no se ha grabado nada antes
  {
    RZERO = default_RZERO;
    escribirWordEEPROM(RZERO_ADDR, RZERO);
  }

}


void handle_Parametros()
{

  if (server.hasArg("plain") == false) { //Check if body received

    server.send(200, "text/plain", "No se ha recibido nada");
    return;

  }

  Serial.println("post:");
  Serial.println(server.arg("plain"));

  // mostrar por puerto serie
  Serial.println(server.argName(0));


  String webPIN = server.arg(String("pin"));
  String rzero = server.arg(String("rzero"));
  String atm = server.arg(String("atm"));
  Serial.print("PIN:\t");
  Serial.println(webPIN);
  Serial.print("Atm:\t");
  Serial.println(atm);
  Serial.print("rzero:\t");
  Serial.println(rzero);

  if (webPIN != (String)PIN)
    server.send(403, "text/plain", "PIN incorrecto");

  if (rzero != "") {
    RZERO = rzero.toFloat();
    escribirWordEEPROM(RZERO_ADDR, RZERO);
  }
  
  server.sendHeader("Location", String("/"), true);
  server.send( 302, "text/plain", "");

}

void handle_NotFound()
{
  server.send(404, "text/plain", "Lo que has intentado no existe");
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
  ptr += "<h2>Nodo " + (String)DISPOSITIVO + "</h2>\n";
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

  ptr += "<form action=\"/SetCal\" method=\"post\"> <fieldset>\n";
  ptr += "<h2>Recalibración (necesita PIN):</h2>\n";
  ptr += "Factor:<input type=\"text\" name=\"rzero\" value=\"\">\n";
  ptr += "Atmósfera:<input type=\"text\" name=\"atm\" value=\"\"><br>\n";
  ptr += "PIN: <input type=\"password\" name=\"pin\" value=\"0000\">\n";
  ptr += "<button type=\"submit\">Enviar</button>\n";
  ptr += "</fieldset></form>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

#if (use_mqtt)
void mqtt_reconnect() {
  int intentos = 0;
  const int max_intentos = 2; // no intentarlo más de estas veces
  // bucle intentando conectar:
  while (!mqtt_client.connected() && intentos <= max_intentos) {
    Serial.print("Intentando conexión MQTT...");
    // Attempt to connect
    if (mqtt_client.connect("arduinoClient")) {
      Serial.println("Conectado al Broker");

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Se intentará de nuevo en 5 segundos");
      delay(5000);
      intentos++;
    }
  }
}
#endif

void loop() {
  server.handleClient();
  if (millis() - tiempoInicial > intervalo) {

    tiempoInicial = millis();
    t = dht.readTemperature() + (float)calT;         // Leemos la temperatura
    h = dht.readHumidity() + (float)calH;               // Leemos la humedad
    float cal = obtenerCalibracionCorregida(t, h);
    float ppmcorregido = ppmCorregido(t, h);
    if (ppmcorregido < ATMOCO2) { //autocalibrado a ppm mínimo
      RZERO = cal;
      escribirWordEEPROM(RZERO_ADDR, RZERO);
      ppmcorregido = ppmCorregido(t, h);
    }
    float ppmnormal = ppm();
    Serial.print("ppm: ");
    Serial.print(ppmcorregido);
    Serial.print(" - ");
    Serial.println(ppmnormal);


    Serial.print("Calibracion: ");
    Serial.println(cal);

#if (use_thingspeak)
    // set the fields with the values
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);
    ThingSpeak.setField(3, ppmcorregido);
    ThingSpeak.setField(4, ppmnormal);
    ThingSpeak.setField(5, RZERO);
    ThingSpeak.setField(6, cal);

    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
#endif

#if (use_mqtt)
    if (!mqtt_client.connected()) {
      mqtt_reconnect();
    }
    mqtt_client.loop();
    snprintf (msg, 50, "t:%.2f;h:%.2f;ppm:%.0f", t, h, ppmcorregido);
    Serial.print("MQTT topic: ");
    Serial.println(tema);
    Serial.print("MQTT msg: ");
    Serial.println(msg);
    mqtt_client.publish(tema, msg);
#endif


  }

}
