#define calT -2.2  //calibración sensor T
#define calH 12.0    // calibración sensor H

/*  Nivel CO2 en la atmósfera para calibrar,
 *  Nivel medio en diciembre según https://es.greenpeace.org/es/sala-de-prensa/comunicados/maximo-historico-de-concentraciones-de-co2-en-la-atmosfera/
#define ATMOCO2 414.0 
*/

#define ATMOCO2 435  // Medido en atmósfera en Mérida a 20º usando un medidor calibrado 

/// Resistencia en KOhmios que lleva la tarjeta del MQ135:
#define RLOAD 10

/// Factor de calibración obtenido mediante obtenerCalibracion()
// #define RZERO 547 // calculado a 12ºC
//#define RZERO 375 // calculado a 15ºC
#define RZERO 492 // calculado a 17ºC
//#define RZERO 511 // calculado a 18ºC
//#define RZERO 515 // calculado a 20ºC
//#define RZERO 485 // 21ºC
//#define RZERO 414 //24º


// Numero de muestras para hacer la media
#define MQ_Samples  30

// pin analógico para conectar el sensor de CO2:
const int MQ_PIN = 0;

// pin digital para conectar el DHT11
uint8_t DHTPin = 5;

// Cada cuando se envían los datos a la nube
#define intervalo 30000 // 30 seg
