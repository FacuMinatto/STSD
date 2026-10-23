#include "imu.h"

Adafruit_MPU6050 mpu;

void setupIMU() {
  if (!mpu.begin()) {
    Serial.println("No se encontró el sensor MPU6050");
    return;
  }
  
  // Configuración básica del rango del acelerómetro
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  Serial.println("IMU inicializada correctamente");
}

bool leerIMU(float &ax, float &ay, float &az) {
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    return true;
  }
  return false;
}