#ifndef GPS_MOD_H
#define GPS_MOD_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

void setupGPS();
void procesarGPS();
bool tieneUbicacion();
double obtenerLatitud();
double obtenerLongitud();

#endif