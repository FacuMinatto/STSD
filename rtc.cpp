#include "rtc.h"

RTC_DS3231 rtc;
bool rtcInicializado = false;

void setupRTC() {
  Wire.begin(8, 9); // Pines SDA=8 y SCL=9 en ESP32-S3-DevKitC-1
  if (!rtc.begin()) {
    Serial.println("No se encontró el RTC");
    rtcInicializado = false;
    return;
  }

  rtcInicializado = true;
  if (rtc.lostPower()) {
    Serial.println("El RTC perdió energía, ajustando la hora...");
    // Fija la fecha y hora a la del momento de compilación
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

String obtenerFechaHora() {
  if (!rtcInicializado) {
    return "00/00/0000 00:00:00";
  }
  DateTime now = rtc.now();
  char buffer[20];
  // Formato: DD/MM/YYYY HH:MM:SS
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          now.day(), now.month(), now.year(), 
          now.hour(), now.minute(), now.second());
  return String(buffer);
}