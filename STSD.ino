#include <Arduino.h>
#include "dht11.h"

// Modulos 
#include "imu.h"
#include "rtc.h"
#include "gps.h"

// Definición de pines para actuadores en ESP32-S3-DevKitC-1
#define BUZZER_PIN 5
#define ROJO 6
#define VERDE 7

// Variables globales de configuración (inician con valores por defecto)
int confRotacion = 0;
int confAceleracion = 0;
int confHumedad = 0;
float confTempMin = 2.0;
float confTempMax = 30.0;
float confMargen = 1.5;

unsigned long ultimoTiempoRegistro = 0;
const unsigned long INTERVALO_REGISTRO = 1000; // 1 segundo de delay entre muestras

void setup() {
  Serial.begin(115200); // Comunicación serie con la PC
  
  // Inicializar pines de salida
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VERDE, OUTPUT);
  pinMode(ROJO, OUTPUT);
  
  digitalWrite(VERDE, HIGH); // Sistema encendido

  // Inicializar módulos
  setupDHT();
  setupRTC();
  setupIMU();
  setupGPS();
  // setupWiFi(); // Para la página web
  
  Serial.println("Sistema de Transporte Iniciado");
}

void loop() {
  procesarGPS();

  // 1. Escuchar si la PC (Qt) envía una nueva configuración
  if (Serial.available() > 0) {
    // La función parse extrae los números ignorando las comas automáticamente
    confRotacion = Serial.parseInt();
    confAceleracion = Serial.parseInt();
    confHumedad = Serial.parseInt();
    confTempMin = Serial.parseFloat();
    confTempMax = Serial.parseFloat();
    confMargen = Serial.parseFloat();
    
    // Limpiar el salto de línea del final de la trama
    Serial.readStringUntil('\n'); 
    
    Serial.println("--- CONFIGURACION ACTUALIZADA DESDE LA PC ---");
    Serial.print("Temp Min: "); Serial.print(confTempMin);
    Serial.print(" | Temp Max: "); Serial.println(confTempMax);
  }
  unsigned long tiempoActual = millis();

  // 1. Lectura periódica y registro de datos (cada 5 segundos)
  if (tiempoActual - ultimoTiempoRegistro >= INTERVALO_REGISTRO) {
    ultimoTiempoRegistro = tiempoActual;
    
    float temperatura = medicionTemp();
    float humedad = medicionHum();
    
    // Enviar datos por Serial a la PC (C/C++ Qt)
    Serial.print("Temp: ");
    Serial.print(temperatura);
    Serial.print(" C, Hum: ");
    Serial.print(humedad);
    Serial.println(" %");

    // Imprimir datos del GPS
    if (tieneUbicacion()) {
      Serial.print("Ubicacion: Lat: ");
      Serial.print(obtenerLatitud(), 6);
      Serial.print(" Lon: ");
      Serial.println(obtenerLongitud(), 6);
    } else {
      Serial.println("Buscando señal GPS...");
    }

    // 2. Sistema de Alarma (solo evalúa si el sensor DHT11 devolvió una lectura válida)
    if (temperatura != -999.0) {
      if (temperatura > confTempMax || temperatura < confTempMin) {
        activarAlarma();
      } else if (temperatura > (confTempMax - confMargen) || temperatura < (confTempMin + confMargen)) {
        alertaNaranja();    
      } else {
        desactivarAlarma();
      }
    }

    String timestamp = obtenerFechaHora();
    Serial.print("[" + timestamp + "] ");

    float ax, ay, az;
    if (leerIMU(ax, ay, az)) {
      Serial.print("Accel(X,Y,Z): ");
      Serial.print(ax); Serial.print(", ");
      Serial.print(ay); Serial.print(", ");
      Serial.print(az); Serial.println(" m/s^2");
    }
  }

  // 3. Manejo del servidor Web (WiFi)
  // handleWebClient();
}

void activarAlarma() {
  digitalWrite(ROJO, HIGH);
  digitalWrite(VERDE, LOW);
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("ALARMA: Temperatura fuera de rango!");
}

void desactivarAlarma() {
  digitalWrite(ROJO, LOW);
  digitalWrite(VERDE, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
}

void alertaNaranja() {
  analogWrite(ROJO, 255);
  analogWrite(VERDE, 40);
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("ALARMA: Temperatura cerca del rango no deseado!");
}
