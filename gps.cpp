#include "gps.h"

// En el ESP32 usamos el puerto serie por hardware Serial2 (UART2):
// TX del módulo GPS -> Pin GPIO 16 del ESP32 (RX2)
// RX del módulo GPS -> Pin GPIO 17 del ESP32 (TX2)
TinyGPSPlus gps;

void setupGPS() {
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void procesarGPS() {
  // Esta función debe llamarse constantemente para no perder datos NMEA
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }
}

bool tieneUbicacion() {
  return gps.location.isValid();
}

double obtenerLatitud() {
  return gps.location.lat();
}

double obtenerLongitud() {
  return gps.location.lng();
}