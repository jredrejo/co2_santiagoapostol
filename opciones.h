#define calT -0.7  //calibración sensor T
#define calH +6    // calibración sensor H

/*  Nivel CO2 en la atmósfera para calibrar,
    Nivel medio en diciembre según https://es.greenpeace.org/es/sala-de-prensa/comunicados/maximo-historico-de-concentraciones-de-co2-en-la-atmosfera/
  #define ATMOCO2 414.0
*/

#define ATMOCO2 415  // Medido en atmósfera en Mérida a 20º usando un medidor calibrado 

/// Resistencia en KOhmios que lleva la tarjeta del MQ135:
#define RLOAD 10

const int RZERO_ADDR = 0; // dirección EEPROM para guardar RZERO
#define default_RZERO  375 // calculado a 20ºC

/// Factor de calibración obtenido mediante obtenerCalibracion()
float RZERO;


#define DHTTYPE DHT11  //Tipo de sensor de Tª ambiente y humedad

// Usar mqtt
#define use_mqtt 1 // Comentar esta línea si no se usa MQTT
#define MQTT_Broker  "172.23.120.19"
#define MQTT_port 1883
#define DISPOSITIVO "002" // Dispositivo que identifica al publicar en MQTT
#define RAIZ "santiagoapostol/calidad_aire"  //raiz de la ruta donde va a publicar
String tema_string =  String(RAIZ) + "/" + String(DISPOSITIVO);
const char* tema = tema_string.c_str();

//Usar ThingSpeak
// #define use_thingspeak 1  // Comentar esta línea si no se usa ThingSpeak

// pin analógico para conectar el sensor de CO2:
const int MQ_PIN = 0;

// pin digital para conectar el DHT11
const uint8_t DHTPin = 5;

// Cada cuando se envían los datos a la nube
unsigned long intervalo = 30000; // 30 seg

// Numero de muestras para hacer la media
uint8_t MQ_Samples =  30;
