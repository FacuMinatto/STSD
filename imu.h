#ifndef IMU_MOD_H
#define IMU_MOD_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// Pines I2C por defecto en ESP32-S3-DevKitC-1
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

void setupIMU();
// Usamos paso por referencia para obtener los 3 valores de aceleración a la vez
bool leerIMU(float &ax, float &ay, float &az);

#endif