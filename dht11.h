#ifndef DHT11_MOD_H
#define DHT11_MOD_H

#include <DHT.h>

// Definición del pin y tipo de sensor
#define DHTPIN 4      // Pin GPIO 4 del ESP32
#define DHTTYPE DHT11

// Declaración de funciones
void setupDHT();
float medicionTemp();
float medicionHum();

#endif