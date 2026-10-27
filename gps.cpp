#include "gps.h"

// En el Arduino UNO, conectamos el GPS a estos pines:
// TX del módulo GPS -> Pin 2 del Arduino (RX)
// RX del módulo GPS -> Pin 3 del Arduino (TX)
TinyGPSPlus gps;
SoftwareSerial ss(GPS_RX_PIN, GPS_TX_PIN);

void setupGPS() {
  ss.begin(GPS_BAUD);
}

void procesarGPS() {
  // Esta función debe llamarse constantemente para no perder datos NMEA
  while (ss.available() > 0) {
    gps.encode(ss.read());
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