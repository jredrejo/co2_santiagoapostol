# co2_santiagoapostol
Medidor de CO2 y condiciones ambientales de las aulas del IES Santiago Apóstol





## Construcción:
* Sensor de humedad y temperatura ambiente DHT11
* Sensor de CO2 MQ135
* Placa Nodemcu versión 2



## Compilación del software:

Es necesario usar el IDE de Arduino, preparado para compilar software con el esp8266. Hay instrucciones en español en https://www.naylampmechatronics.com/blog/56_usando-esp8266-con-el-ide-de-arduino.html

Una vez preparado el IDE de Arduino es necesario añadir un archivo llamado credenciales.h con las contraseñas necesarias para conectar a ThingSpeak o al broker mqtt, así como las credenciales del punto de acceso Wifi al que se va a conectar si no se usa la opción (predeterminada) de WiFiManager



Librerías de Arduino que hay que instalar :

* https://github.com/arduino-libraries/NTPClient
* https://www.arduino.cc/en/Reference/WiFi
* https://github.com/adafruit/DHT-sensor-library
* https://github.com/tzapu/WiFiManager
* https://pubsubclient.knolleary.net/
* https://thingspeak.com/



## Prototipo construido

![img](prototipo.jpg)

