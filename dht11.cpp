#include "dht11.h"

// Inicialización de la instancia del sensor
DHT dht(DHTPIN, DHTTYPE);

void setupDHT() {
  dht.begin();
}

float medicionTemp() {
  float temp = dht.readTemperature();
  // Verificar si la lectura falló
  if (isnan(temp)) {
    return -999.0; // Código de error
  }
  return temp;
}

float medicionHum() {
  float hum = dht.readHumidity();
  // Verificar si la lectura falló
  if (isnan(hum)) {
    return -999.0; // Código de error
  }
  return hum;
}