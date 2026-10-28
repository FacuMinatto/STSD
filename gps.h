#ifndef GPS_MOD_H
#define GPS_MOD_H

#include <Arduino.h>
#include <TinyGPSPlus.h>

// Definición de pines UART2 en ESP32 y velocidad del GPS GY-NEO6MV2
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_BAUD   9600

void setupGPS();
void procesarGPS();
bool tieneUbicacion();
double obtenerLatitud();
double obtenerLongitud();

#endif