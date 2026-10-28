#include <Arduino.h>
#include <math.h>
#include "dht11.h"

// Modulos 
#include "imu.h"
#include "rtc.h"
#include "gps.h"

// Definición de pines para actuadores en ESP32-S3-DevKitC-1
#define BUZZER_PIN 5

// LED RGB 1: Temperatura (Pines R y G)
#define LED_TEMP_R 6
#define LED_TEMP_G 7

// LED RGB 2: IMU / Impactos (Pines R y G)
#define LED_IMU_R 10
#define LED_IMU_G 11

// LED RGB 3: Humedad (Pines R y G)
#define LED_HUM_R 12
#define LED_HUM_G 13

// Estados de alarma para señalización visual y sonora
enum EstadoAlarma { VERDE = 0, AMARILLO = 1, ROJO = 2 };

// Variables de estado con enclavamiento (latching)
EstadoAlarma estadoTemp = VERDE;
EstadoAlarma estadoIMU = VERDE;
EstadoAlarma estadoHum = VERDE;

// Variables globales de configuración (inician con valores por defecto)
int confRotacion = 0;
int confAceleracion = 0;
int confHumedad = 0;
float confTempMin = 2.0;
float confTempMax = 30.0;
float confMargen = 1.5;

unsigned long ultimoTiempoRegistro = 0;
const unsigned long INTERVALO_REGISTRO = 1000; // 1 segundo de delay entre muestras

void actualizarLedRGB(int pinRojo, int pinVerde, EstadoAlarma estado) {
  if (estado == VERDE) {
    analogWrite(pinRojo, 0);
    analogWrite(pinVerde, 255);
  } else if (estado == AMARILLO) {
    analogWrite(pinRojo, 255);
    analogWrite(pinVerde, 35);
  } else if (estado == ROJO) {
    analogWrite(pinRojo, 255);
    analogWrite(pinVerde, 0);
  }
}

void setup() {
  Serial.begin(115200); // Comunicación serie con la PC
  
  // Inicializar pines de actuadores
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_TEMP_R, OUTPUT);
  pinMode(LED_TEMP_G, OUTPUT);
  pinMode(LED_IMU_R, OUTPUT);
  pinMode(LED_IMU_G, OUTPUT);
  pinMode(LED_HUM_R, OUTPUT);
  pinMode(LED_HUM_G, OUTPUT);
  
  // Estado inicial: LEDs en VERDE y buzzer apagado
  actualizarLedRGB(LED_TEMP_R, LED_TEMP_G, estadoTemp);
  actualizarLedRGB(LED_IMU_R, LED_IMU_G, estadoIMU);
  actualizarLedRGB(LED_HUM_R, LED_HUM_G, estadoHum);
  digitalWrite(BUZZER_PIN, LOW);

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
    
    // Resetear variables de enclavamiento a VERDE con la nueva configuración
    estadoTemp = VERDE;
    estadoIMU = VERDE;
    estadoHum = VERDE;
    actualizarLedRGB(LED_TEMP_R, LED_TEMP_G, estadoTemp);
    actualizarLedRGB(LED_IMU_R, LED_IMU_G, estadoIMU);
    actualizarLedRGB(LED_HUM_R, LED_HUM_G, estadoHum);

    Serial.println("--- CONFIGURACION ACTUALIZADA DESDE LA PC ---");
    Serial.print("Temp Min: "); Serial.print(confTempMin);
    Serial.print(" | Temp Max: "); Serial.println(confTempMax);
    Serial.print("Perfil Humedad: "); Serial.print(confHumedad);
    Serial.print(" | Perfil Aceleracion: "); Serial.println(confAceleracion);
  }

  unsigned long tiempoActual = millis();

  // 2. Lectura periódica y registro de datos (cada 1 segundo)
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

    String timestamp = obtenerFechaHora();
    Serial.print("[" + timestamp + "] ");

    float ax = 0.0, ay = 0.0, az = 0.0;
    bool imuOk = leerIMU(ax, ay, az);
    if (imuOk) {
      Serial.print("Accel(X,Y,Z): ");
      Serial.print(ax); Serial.print(", ");
      Serial.print(ay); Serial.print(", ");
      Serial.print(az); Serial.println(" m/s^2");
    }

    // A. Evaluación de Temperatura con enclavamiento (latching)
    if (temperatura != -999.0) {
      if (temperatura < confTempMin || temperatura > confTempMax) {
        estadoTemp = ROJO;
      } else if (temperatura > (confTempMax - confMargen) || temperatura < (confTempMin + confMargen)) {
        if (estadoTemp < AMARILLO) {
          estadoTemp = AMARILLO;
        }
      }
    }

    // B. Evaluación de Humedad con enclavamiento (latching)
    if (humedad != -999.0) {
      float humMin = 20.0;
      float humMax = 50.0;
      if (confHumedad == 1) {
        humMin = 60.0;
        humMax = 80.0;
      } else if (confHumedad == 2) {
        humMin = 20.0;
        humMax = 90.0;
      }

      const float MARGEN_HUM = 5.0;
      if (humedad < humMin || humedad > humMax) {
        estadoHum = ROJO;
      } else if (humedad < (humMin + MARGEN_HUM) || humedad > (humMax - MARGEN_HUM)) {
        if (estadoHum < AMARILLO) {
          estadoHum = AMARILLO;
        }
      }
    }

    // C. Evaluación de IMU / Impactos con enclavamiento (latching)
    if (imuOk) {
      float acelTotal = sqrt(ax * ax + ay * ay + az * az);
      float impacto = fabs(acelTotal - 9.8);

      float umbralAmarillo = 1.5;
      float umbralRojo = 3.0;
      if (confAceleracion == 1) {
        umbralAmarillo = 3.0;
        umbralRojo = 6.0;
      } else if (confAceleracion == 2) {
        umbralAmarillo = 6.0;
        umbralRojo = 10.0;
      }

      if (impacto >= umbralRojo) {
        estadoIMU = ROJO;
      } else if (impacto >= umbralAmarillo) {
        if (estadoIMU < AMARILLO) {
          estadoIMU = AMARILLO;
        }
      }
    }

    // Actualizar visualización en los 3 LEDs RGB
    actualizarLedRGB(LED_TEMP_R, LED_TEMP_G, estadoTemp);
    actualizarLedRGB(LED_IMU_R, LED_IMU_G, estadoIMU);
    actualizarLedRGB(LED_HUM_R, LED_HUM_G, estadoHum);
  }

  // 3. Control no bloqueante del Buzzer
  if (estadoTemp == ROJO || estadoIMU == ROJO || estadoHum == ROJO) {
    digitalWrite(BUZZER_PIN, HIGH); // Pitido continuo en estado crítico
  } else if (estadoTemp == AMARILLO || estadoIMU == AMARILLO || estadoHum == AMARILLO) {
    bool tonoActivo = ((millis() / 300) % 2 == 0); // Pitido intermitente cada 300 ms
    digitalWrite(BUZZER_PIN, tonoActivo ? HIGH : LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW); // Apagado en estado seguro
  }

  // 4. Manejo del servidor Web (WiFi)
  // handleWebClient();
}
