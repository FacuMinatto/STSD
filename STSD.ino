#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>
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

// Servidor Web en puerto 80 y variables de caché de sensores
WebServer server(80);
float ultimaTemp = 0.0;
float ultimaHum = 0.0;
float ultimoImpacto = 0.0;

String textoEstado(EstadoAlarma e) {
  if (e == VERDE) return "NORMAL (Verde)";
  if (e == AMARILLO) return "ADVERTENCIA (Amarillo)";
  return "CRITICO (Rojo)";
}

String colorEstado(EstadoAlarma e) {
  if (e == VERDE) return "#2ecc71";
  if (e == AMARILLO) return "#f1c40f";
  return "#e74c3c";
}

void enviarPaginaWeb() {
  String strTemp = (ultimaTemp == -999.0f) ? "Error sensor" : (String(ultimaTemp, 1) + " &deg;C");
  String strHum  = (ultimaHum == -999.0f)  ? "Error sensor" : (String(ultimaHum, 1) + " %");
  String strImp  = String(ultimoImpacto, 2) + " m/s&sup2;";

  String html = "<!DOCTYPE html><html lang='es'><head>"
                "<meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<meta http-equiv='refresh' content='2'>"
                "<title>STSD Monitoreo</title>"
                "<style>"
                "body{font-family:Arial,sans-serif;background:#1e1e24;color:#f0f0f0;margin:0;padding:20px;text-align:center;}"
                "h1{font-size:1.4rem;margin-bottom:15px;color:#ffffff;}"
                ".card{background:#2a2b36;border-radius:10px;padding:15px;margin:12px auto;max-width:360px;box-shadow:0 4px 6px rgba(0,0,0,0.3);text-align:left;}"
                ".card h2{margin:0 0 8px 0;font-size:1.1rem;color:#b0b3c1;}"
                ".val{font-size:1.6rem;font-weight:bold;margin-bottom:8px;}"
                ".badge{display:inline-block;padding:4px 10px;border-radius:5px;font-size:0.85rem;font-weight:bold;color:#000;}"
                ".footer{margin-top:20px;font-size:0.8rem;color:#7a7c88;}"
                "</style></head><body>"
                "<h1>STSD - Monitoreo en Vivo</h1>"
                "<div class='card'>"
                "<h2>Temperatura</h2>"
                "<div class='val'>" + strTemp + "</div>"
                "<span class='badge' style='background:" + colorEstado(estadoTemp) + ";'>" + textoEstado(estadoTemp) + "</span>"
                "</div>"
                "<div class='card'>"
                "<h2>Humedad Relativa</h2>"
                "<div class='val'>" + strHum + "</div>"
                "<span class='badge' style='background:" + colorEstado(estadoHum) + ";'>" + textoEstado(estadoHum) + "</span>"
                "</div>"
                "<div class='card'>"
                "<h2>Impacto / Aceleraci&oacute;n</h2>"
                "<div class='val'>" + strImp + "</div>"
                "<span class='badge' style='background:" + colorEstado(estadoIMU) + ";'>" + textoEstado(estadoIMU) + "</span>"
                "</div>"
                "<p class='footer'>Actualizaci&oacute;n autom&aacute;tica cada 2s &bull; IP: 192.168.4.1</p>"
                "</body></html>";

  server.send(200, "text/html", html);
}

void setupWiFi() {
  WiFi.softAP("STSD_Monitoreo", "12345678");
  Serial.println("Red WiFi AP creada: STSD_Monitoreo");
  Serial.print("IP del Servidor Web: ");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", enviarPaginaWeb);
  server.begin();
  Serial.println("Servidor Web HTTP iniciado");
}

void guardarAnomaliaCSV(String fechaHora, float temp, float hum, float impacto) {
  File archivo = LittleFS.open("/registro.csv", FILE_APPEND);
  if (!archivo) {
    Serial.println("Error abriendo /registro.csv para escritura");
    return;
  }

  String strLat = "Sin_GPS";
  String strLon = "Sin_GPS";
  if (tieneUbicacion()) {
    strLat = String(obtenerLatitud(), 6);
    strLon = String(obtenerLongitud(), 6);
  }

  String linea = fechaHora + "," +
                 String(temp, 1) + "," +
                 String(hum, 1) + "," +
                 String(impacto, 2) + "," +
                 strLat + "," +
                 strLon + "," +
                 textoEstado(estadoTemp) + "," +
                 textoEstado(estadoHum) + "," +
                 textoEstado(estadoIMU);

  archivo.println(linea);
  archivo.close();
  Serial.println("Anomalia registrada en LittleFS: " + linea);
}

void enviarRegistroPorSerie() {
  Serial.println("---INICIO_REGISTRO---");
  Serial.println("FechaHora,Temperatura_C,Humedad_Porc,Impacto_ms2,Latitud,Longitud,Estado_Temp,Estado_Hum,Estado_IMU");
  if (LittleFS.exists("/registro.csv")) {
    File archivo = LittleFS.open("/registro.csv", "r");
    if (archivo) {
      while (archivo.available()) {
        String l = archivo.readStringUntil('\n');
        l.trim();
        if (l.length() > 0) {
          Serial.println(l);
        }
      }
      archivo.close();
    }
  }
  Serial.println("---FIN_REGISTRO---");
}

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

  // Inicializar LittleFS (Flash interna para datalogger de anomalías)
  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS");
  } else {
    Serial.println("LittleFS montado correctamente");
  }

  // Inicializar módulos
  setupDHT();
  setupRTC();
  setupIMU();
  setupGPS();
  setupWiFi(); // Servidor Web HTTP y Punto de Acceso WiFi
  
  Serial.println("Sistema de Transporte Iniciado");
}

void loop() {
  procesarGPS();

  // 1. Escuchar si la PC (Qt) envía una nueva configuración o solicita descarga de registro
  if (Serial.available() > 0) {
    String linea = Serial.readStringUntil('\n');
    linea.trim();
    if (linea == "DESCARGAR") {
      enviarRegistroPorSerie();
    } else if (linea == "OBTENER_CONFIG") {
      Serial.println("CONFIG:" + String(confRotacion) + "," +
                     String(confAceleracion) + "," +
                     String(confHumedad) + "," +
                     String(confTempMin, 1) + "," +
                     String(confTempMax, 1) + "," +
                     String(confMargen, 1));
    } else if (linea.length() > 0) {
      int rot = 0, acc = 0, hum = 0;
      float tMin = 0.0, tMax = 0.0, marg = 0.0;
      if (sscanf(linea.c_str(), "%d,%d,%d,%f,%f,%f", &rot, &acc, &hum, &tMin, &tMax, &marg) == 6) {
        confRotacion = rot;
        confAceleracion = acc;
        confHumedad = hum;
        confTempMin = tMin;
        confTempMax = tMax;
        confMargen = marg;

        // Resetear variables de enclavamiento a VERDE con la nueva configuración
        estadoTemp = VERDE;
        estadoIMU = VERDE;
        estadoHum = VERDE;
        ultimoImpacto = 0.0;
        actualizarLedRGB(LED_TEMP_R, LED_TEMP_G, estadoTemp);
        actualizarLedRGB(LED_IMU_R, LED_IMU_G, estadoIMU);
        actualizarLedRGB(LED_HUM_R, LED_HUM_G, estadoHum);

        // Limpiar el registro CSV en flash para el nuevo viaje
        if (LittleFS.exists("/registro.csv")) {
          LittleFS.remove("/registro.csv");
        }

        Serial.println("--- CONFIGURACION ACTUALIZADA DESDE LA PC ---");
        Serial.print("Temp Min: "); Serial.print(confTempMin);
        Serial.print(" | Temp Max: "); Serial.println(confTempMax);
        Serial.print("Perfil Humedad: "); Serial.print(confHumedad);
        Serial.print(" | Perfil Aceleracion: "); Serial.println(confAceleracion);
        Serial.println("Registro previo de viaje eliminado de LittleFS.");
      }
    }
  }

  unsigned long tiempoActual = millis();

  // 2. Lectura periódica y registro de datos (cada 1 segundo)
  if (tiempoActual - ultimoTiempoRegistro >= INTERVALO_REGISTRO) {
    ultimoTiempoRegistro = tiempoActual;
    
    float temperatura = medicionTemp();
    float humedad = medicionHum();
    ultimaTemp = temperatura;
    ultimaHum = humedad;
    
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

    bool anomaliaActual = false;

    // A. Evaluación de Temperatura con enclavamiento (latching)
    if (temperatura != -999.0) {
      if (temperatura < confTempMin || temperatura > confTempMax) {
        estadoTemp = ROJO;
        anomaliaActual = true;
      } else if (temperatura > (confTempMax - confMargen) || temperatura < (confTempMin + confMargen)) {
        if (estadoTemp < AMARILLO) {
          estadoTemp = AMARILLO;
        }
        anomaliaActual = true;
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
        anomaliaActual = true;
      } else if (humedad < (humMin + MARGEN_HUM) || humedad > (humMax - MARGEN_HUM)) {
        if (estadoHum < AMARILLO) {
          estadoHum = AMARILLO;
        }
        anomaliaActual = true;
      }
    }

    // C. Evaluación de IMU / Impactos con enclavamiento (latching)
    if (imuOk) {
      float acelTotal = sqrt(ax * ax + ay * ay + az * az);
      float impacto = fabs(acelTotal - 9.8);
      ultimoImpacto = impacto;

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
        anomaliaActual = true;
      } else if (impacto >= umbralAmarillo) {
        if (estadoIMU < AMARILLO) {
          estadoIMU = AMARILLO;
        }
        anomaliaActual = true;
      }
    }

    // Actualizar visualización en los 3 LEDs RGB
    actualizarLedRGB(LED_TEMP_R, LED_TEMP_G, estadoTemp);
    actualizarLedRGB(LED_IMU_R, LED_IMU_G, estadoIMU);
    actualizarLedRGB(LED_HUM_R, LED_HUM_G, estadoHum);

    // D. Registro en memoria Flash (LittleFS) si hubo anomalía en la lectura actual
    if (anomaliaActual) {
      guardarAnomaliaCSV(timestamp, temperatura, humedad, ultimoImpacto);
    }
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
  server.handleClient();
}
