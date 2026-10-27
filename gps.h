#ifndef GPS_MOD_H
#define GPS_MOD_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// Definición de pines y velocidad del GPS
#define GPS_RX_PIN 2
#define GPS_TX_PIN 3
#define GPS_BAUD   9600

void setupGPS();
void procesarGPS();
bool tieneUbicacion();
double obtenerLatitud();
double obtenerLongitud();

#endif