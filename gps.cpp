#include "gps.h"

// En el Arduino UNO, conectaremos el GPS a estos pines:
// TX del módulo GPS -> Pin 2 del Arduino (RX)
// RX del módulo GPS -> Pin 3 del Arduino (TX)
static const int RXPin = 2;
static const int TXPin = 3;
static const uint32_t GPSBaud = 9600; // Velocidad estándar para módulos como el NEO-6M

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

void setupGPS() {
  ss.begin(GPSBaud);
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