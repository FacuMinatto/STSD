#include "rtc.h"

RTC_DS3231 rtc;

void setupRTC() {
  if (!rtc.begin()) {
    Serial.println("No se encontró el RTC");
    return;
  }

  if (rtc.lostPower()) {
    Serial.println("El RTC perdió energía, ajustando la hora...");
    // Fija la fecha y hora a la del momento de compilación
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

String obtenerFechaHora() {
  DateTime now = rtc.now();
  char buffer[20];
  // Formato: DD/MM/YYYY HH:MM:SS
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          now.day(), now.month(), now.year(), 
          now.hour(), now.minute(), now.second());
  return String(buffer);
}