> **Última actualización:** 27/10/2026 - 22:58 hs

# Documentación Técnica del Proyecto STSD

Este documento explica en detalle el funcionamiento del código fuente del **Sistema de Transporte de Sustancias Delicadas (STSD)**.

---

## 📑 Índice de Contenidos

- [PARTE 1: Funcionamiento del Sistema Embebido (Arduino)](#parte-1-funcionamiento-del-sistema-embebido-arduino)
  - [1.1 Archivo Principal (`STSD.ino`)](#11-archivo-principal-stsdino)
    - [A. Definición de pines y variables de estado](#a-definición-de-pines-y-variables-de-estado)
    - [B. Inicialización (`setup()`)](#b-inicialización-setup)
    - [C. Bucle Principal (`loop()`) y Partes Clave](#c-bucle-principal-loop-y-partes-clave)
      - [1. Lectura constante del GPS](#1-lectura-constante-del-gps)
      - [2. Recepción de configuración desde la PC (Qt)](#2-recepción-de-configuración-desde-la-pc-qt)
      - [3. Muestreo no bloqueante con `millis()`](#3-muestreo-no-bloqueante-con-millis)
      - [4. Lógica de Alarmas con Enclavamiento (3 Canales Independientes)](#4-lógica-de-alarmas-con-enclavamiento-3-canales-independientes)
      - [5. Control No Bloqueante del Buzzer (Continuo / Intermitente)](#5-control-no-bloqueante-del-buzzer-continuo--intermitente)
  - [1.2 Módulos de Sensores (`.h` y `.cpp`)](#12-módulos-de-sensores-h-y-cpp)
    - [A. Sensor de Temperatura y Humedad (`dht11`)](#a-sensor-de-temperatura-y-humedad-dht11h-y-dht11cpp)
    - [B. Acelerómetro y Giroscopio (`imu`)](#b-acelerómetro-y-giroscopio-imuh-y-imucpp)
    - [C. Reloj en Tiempo Real (`rtc`)](#c-reloj-en-tiempo-real-rtch-y-rtccpp)
    - [D. Módulo GPS (`gps`)](#d-módulo-gps-gpsh-y-gpscpp)
- [PARTE 2: Aplicación de Escritorio en Qt (`interfaz/`)](#parte-2-aplicación-de-escritorio-en-qt-interfaz)
  - [2.1 Configuración de Compilación (`CMakeLists.txt`)](#21-configuración-de-compilación-cmakeliststxt)
  - [2.2 Punto de Entrada (`main.cpp`)](#22-punto-de-entrada-maincpp)
  - [2.3 Declaración de la Clase (`mainwindow.h`)](#23-declaración-de-la-clase-mainwindowh)
  - [2.4 Implementación de la Ventana (`mainwindow.cpp`)](#24-implementación-de-la-ventana-mainwindowcpp)
    - [A. Configuración del Puerto Serie en el Constructor y Destructor](#a-configuración-del-puerto-serie-en-el-constructor-y-destructor)
    - [B. Construcción de los Widgets y Layouts (`configurarUI()`)](#b-construcción-de-los-widgets-y-layouts-configurarui)
    - [C. Conexión de Señales y Ranuras (`Signals & Slots`)](#c-conexión-de-señales-y-ranuras-signals--slots)
    - [D. Armado y Envío de la Trama Serie (`enviarDatos()`)](#d-armado-y-envío-de-la-trama-serie-enviardatos)
    - [E. Paleta Oscura y Hoja de Estilos (`aplicarEstilos()`)](#e-paleta-oscura-y-hoja-de-estilos-aplicarestilos)
- [PARTE 3: Esquema de Conexiones y Arquitectura del Sistema](#parte-3-esquema-de-conexiones-y-arquitectura-del-sistema)
  - [3.1 Lienzo Interactivo en Obsidian Canvas](#31-lienzo-interactivo-en-obsidian-canvas)
  - [3.2 Diagrama de Bloques y Conexiones](#32-diagrama-de-bloques-y-conexiones)
  - [3.3 Tabla Detallada de Cableado Hardware](#33-tabla-detallada-de-cableado-hardware)

---

# PARTE 1: Funcionamiento del Sistema Embebido (Arduino)

El firmware está organizado de forma **modular**, separando las declaraciones (interfaz) en archivos de cabecera (`.h`) y las definiciones (implementación) en archivos fuente (`.cpp`), mientras que la lógica central y el control de actuadores se encuentran en `STSD.ino`.

```text
STSD/
├── STSD.ino          # Lógica principal, recepción serie, temporización y alarmas
├── dht11.h / .cpp    # Módulo de temperatura y humedad (DHT11)
├── imu.h   / .cpp    # Módulo acelerómetro y giroscopio (MPU6050)
├── rtc.h   / .cpp    # Módulo de reloj en tiempo real (DS3231)
└── gps.h   / .cpp    # Módulo de posicionamiento satelital (NEO-6M)
```

---

## 1.1 Archivo Principal (`STSD.ino`)

### A. Definición de pines y variables de estado
En la parte superior se definen mediante directivas del preprocesador (`#define`) los pines de los actuadores (Buzzer y 3 LEDs RGB) y las variables globales de configuración y estado:

```cpp
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

int confRotacion = 0;
int confAceleracion = 0;
int confHumedad = 0;
float confTempMin = 2.0;
float confTempMax = 30.0;
float confMargen = 1.5;
```
* **Variables de enclavamiento (*latching*):** `estadoTemp`, `estadoIMU` y `estadoHum` registran individualmente la severidad de las infracciones. Una vez que un canal pasa a `AMARILLO` o `ROJO`, **nunca desciende solo**, asegurando trazabilidad de cualquier anomalía ocurrida durante el viaje.
* **Función de visualización RGB con PWM (`actualizarLedRGB`):**
  ```cpp
  void actualizarLedRGB(int pinRojo, int pinVerde, EstadoAlarma estado) {
    if (estado == VERDE) {
      analogWrite(pinRojo, 0);
      analogWrite(pinVerde, 255); // R=0, G=255, B=0
    } else if (estado == AMARILLO) {
      analogWrite(pinRojo, 255);
      analogWrite(pinVerde, 35);  // R=255, G=35, B=0 (Amarillo calibrado por PWM)
    } else if (estado == ROJO) {
      analogWrite(pinRojo, 255);
      analogWrite(pinVerde, 0);   // R=255, G=0, B=0
    }
  }
  ```

### B. Inicialización (`setup()`)
Se ejecuta una única vez al encender o resetear el microcontrolador:
1. Inicia la comunicación serie UART a **115200 baudios** (`Serial.begin(115200)`).
2. Configura los 7 pines de salida (Buzzer y los pines Rojo/Verde de los 3 LEDs RGB) como `OUTPUT`.
3. Pone los 3 LEDs RGB en `VERDE` y el buzzer en `LOW` como estado seguro inicial.
4. Llama a las funciones de inicialización de cada módulo (`setupDHT()`, `setupRTC()`, `setupIMU()`, `setupGPS()`).

### C. Bucle Principal (`loop()`) y Partes Clave

#### 1. Lectura constante del GPS
```cpp
procesarGPS();
```
Se llama al inicio de cada iteración del `loop()` sin bloqueos para vaciar el buffer serie del módulo GPS y alimentar el decodificador NMEA.

#### 2. Recepción de configuración desde la PC (Qt)
```cpp
if (Serial.available() > 0) {
  confRotacion = Serial.parseInt();
  confAceleracion = Serial.parseInt();
  confHumedad = Serial.parseInt();
  confTempMin = Serial.parseFloat();
  confTempMax = Serial.parseFloat();
  confMargen = Serial.parseFloat();
  
  Serial.readStringUntil('\n'); 
  
  // Resetear variables de enclavamiento a VERDE con la nueva configuración
  estadoTemp = VERDE;
  estadoIMU = VERDE;
  estadoHum = VERDE;
  actualizarLedRGB(LED_TEMP_R, LED_TEMP_G, estadoTemp);
  actualizarLedRGB(LED_IMU_R, LED_IMU_G, estadoIMU);
  actualizarLedRGB(LED_HUM_R, LED_HUM_G, estadoHum);
}
```
* **Reinicio de enclavamiento:** Al cargar una nueva configuración para un nuevo despacho o viaje, las tres alarmas se restablecen a `VERDE` y se actualizan de inmediato en los LEDs.

#### 3. Muestreo no bloqueante con `millis()`
```cpp
unsigned long tiempoActual = millis();
if (tiempoActual - ultimoTiempoRegistro >= INTERVALO_REGISTRO) {
  ultimoTiempoRegistro = tiempoActual;
  // Lectura de sensores y evaluación de alarmas...
}
```
* Permite que durante el intervalo de muestreo (1 segundo) el microcontrolador continúe decodificando el GPS, recibiendo tramas de la PC y ejecutando la oscilación del buzzer intermitente de forma fluida.

#### 4. Lógica de Alarmas con Enclavamiento (3 Canales Independientes)
Cada variable física cuenta con su propio LED RGB dedicado y lógica de severidad con retención:
1. **Canal de Temperatura (`estadoTemp`):**
   - Si `temperatura < confTempMin || temperatura > confTempMax`: sube a `ROJO`.
   - Si está dentro del margen de aviso (`temperatura > confTempMax - confMargen || temperatura < confTempMin + confMargen`): si estaba en `VERDE`, sube a `AMARILLO`.
2. **Canal de Humedad (`estadoHum`):**
   - Evalúa límites según el perfil configurado:
     - `0` (Seco): 20% a 50%.
     - `1` (Húmedo): 60% a 80%.
     - `2` (Indistinto): 20% a 90%.
   - Con margen de advertencia de `5.0%`: fuera de rango sube a `ROJO`; dentro de los 5 puntos porcentuales respecto a los bordes sube a `AMARILLO`.
3. **Canal de IMU / Impactos (`estadoIMU`):**
   - Calcula la aceleración total $a = \sqrt{a_x^2 + a_y^2 + a_z^2}$ y la desviación respecto a la gravedad terrestre ($9.8 \text{ m/s}^2$): $\text{impacto} = |a - 9.8|$.
   - Aplica umbrales según el perfil de fragilidad:
     - `0` (Muy Frágil): Amarillo = 1.5 m/s², Rojo = 3.0 m/s².
     - `1` (Frágil): Amarillo = 3.0 m/s², Rojo = 6.0 m/s².
     - `2` (Resistente): Amarillo = 6.0 m/s², Rojo = 10.0 m/s².

#### 5. Control No Bloqueante del Buzzer (Continuo / Intermitente)
El buzzer acústico único se gestiona en el `loop()` principal de forma no bloqueante:
```cpp
if (estadoTemp == ROJO || estadoIMU == ROJO || estadoHum == ROJO) {
  digitalWrite(BUZZER_PIN, HIGH); // Pitido continuo (Crítico)
} else if (estadoTemp == AMARILLO || estadoIMU == AMARILLO || estadoHum == AMARILLO) {
  bool tonoActivo = ((millis() / 300) % 2 == 0); // Pitido intermitente cada 300 ms
  digitalWrite(BUZZER_PIN, tonoActivo ? HIGH : LOW);
} else {
  digitalWrite(BUZZER_PIN, LOW); // Silenciado (Normal)
}
```
* **Prioridad:** Si cualquiera de las tres variables sufrió una violación crítica (`ROJO`), el buzzer suena de manera **continua**. Si no hay críticos pero sí advertencias (`AMARILLO`), emite un **pitido intermitente** (300 ms encendido / 300 ms apagado) calculado matemáticamente sin usar delays. Si todo está en `VERDE`, permanece **apagado**.

---

## 1.2 Módulos de Sensores (`.h` y `.cpp`)

Todos los archivos `.h` utilizan **guardas de inclusión** (`#ifndef ... #define ... #endif`) para evitar que el preprocesador incluya dos veces el mismo encabezado durante la compilación.

### A. Sensor de Temperatura y Humedad (`dht11.h` y `dht11.cpp`)
* Utiliza la biblioteca `DHT.h` en el pin definido por `#define DHTPIN 4`.
* **Validación de fallos:**
  ```cpp
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    return -999.0; // Código de error
  }
  ```
  Usa `isnan()` (*Is Not a Number*) para detectar si el sensor se desconectó o falló en la lectura, devolviendo `-999.0` como valor centinela.

### B. Acelerómetro y Giroscopio (`imu.h` y `imu.cpp`)
* Se comunica por el bus **I2C** (`Wire.h`) con el sensor `Adafruit_MPU6050`.
* **Uso de referencias de C++ (Punto clave):**
  ```cpp
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
  ```
  Como en C/C++ una función solo puede devolver un valor con `return`, se utiliza **paso por referencia** (`float &ax, float &ay, float &az`). De esta forma, la función devuelve un `bool` indicando si la lectura fue exitosa y modifica directamente las variables `ax`, `ay` y `az` declaradas en `STSD.ino` sin necesidad de usar punteros.

### C. Reloj en Tiempo Real (`rtc.h` y `rtc.cpp`)
* Controla el módulo `RTC_DS3231` por I2C.
* Si detecta que se quedó sin batería (`rtc.lostPower()`), carga automáticamente la fecha y hora del momento exacto en que se compiló el programa usando las macros del preprocesador `__DATE__` y `__TIME__`.
* **Formateo de fecha y hora:**
  ```cpp
  char buffer[20];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          now.day(), now.month(), now.year(), 
          now.hour(), now.minute(), now.second());
  return String(buffer);
  ```
  Utiliza `sprintf` con el especificador `%02d` para asegurar que los números de un dígito se rellenen con un cero a la izquierda (por ejemplo, `05` en vez de `5`).

### D. Módulo GPS (`gps.h` y `gps.cpp`)
* Utiliza el segundo puerto serie por hardware del ESP32 (`Serial2`) en los pines **GPIO 16 (`GPS_RX_PIN`)** y **GPIO 17 (`GPS_TX_PIN`)** a 9600 baudios (`Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN)`), dejando libre el puerto serie principal (`Serial`) que se comunica con la PC.
* A medida que llegan caracteres desde el GPS, `gps.encode(Serial2.read())` decodifica las tramas NMEA y actualiza la latitud y longitud.

---
---

# PARTE 2: Aplicación de Escritorio en Qt (`interfaz/`)

La interfaz gráfica permite al operador seleccionar los perfiles del transporte y los rangos térmicos y enviarlos por puerto serie al microcontrolador.

```text
interfaz/
├── CMakeLists.txt    # Configuración de compilación con CMake (C++17)
├── main.cpp          # Punto de entrada e inicio del bucle de eventos
├── mainwindow.h      # Declaración de la clase MainWindow, widgets y slots
└── mainwindow.cpp    # Construcción de la interfaz, puerto serie y estilos
```

---

## 2.1 Configuración de Compilación (`CMakeLists.txt`)

```cmake
cmake_minimum_required(VERSION 3.10)
project(ConfiguradorTransporte VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets SerialPort REQUIRED)
add_executable(ConfiguradorTransporte
    main.cpp
    mainwindow.cpp
    mainwindow.h
)
target_link_libraries(ConfiguradorTransporte PRIVATE Qt6::Widgets Qt6::SerialPort)
```
* **`CMAKE_CXX_STANDARD 17`:** Obliga al compilador `g++` a utilizar el estándar **ISO C++17**.
* **`CMAKE_AUTOMOC ON`:** Activa automáticamente el *Meta-Object Compiler* (`moc`) de Qt, indispensable para que funcionen las señales y ranuras (`signals & slots`) en clases que contienen la macro `Q_OBJECT`.
* **`find_package(... Widgets SerialPort)`:** Busca e incluye los módulos de interfaz gráfica (`Widgets`) y de comunicación serie (`SerialPort`), siendo compatible tanto con **Qt6** como con **Qt5**.

---

## 2.2 Punto de Entrada (`main.cpp`)

```cpp
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
```
* `QApplication` administra los recursos de la aplicación gráfica.
* `MainWindow w;` instancia la ventana principal y `w.show()` la hace visible en pantalla.
* `a.exec()` inicia el **bucle de eventos** (*event loop*), quedando a la espera de acciones del usuario (clics, teclado, etc.) hasta que se cierre la ventana.

---

## 2.3 Declaración de la Clase (`mainwindow.h`)

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void enviarDatos();

private:
    void configurarUI();
    void aplicarEstilos();
    // Punteros a widgets y QSerialPort...
};
```
* **Herencia:** `MainWindow` hereda de `QMainWindow`.
* **Encapsulamiento:** Los punteros a los controles gráficos (`QComboBox`, `QDoubleSpinBox`, `QPushButton`) y al puerto serie (`QSerialPort *puertoSerie`) son atributos `private` de la clase, junto con los métodos auxiliares `configurarUI()` y `aplicarEstilos()`.
* **Parámetro por defecto y `nullptr`:** El constructor recibe `QWidget *parent = nullptr` (estándar C++11/17) indicando que por defecto esta ventana no tiene un widget padre (es la ventana superior).
* **`private slots:`:** Declara `enviarDatos()` como una ranura (*slot*) que responderá cuando se presione el botón.

---

## 2.4 Implementación de la Ventana (`mainwindow.cpp`)

### A. Configuración del Puerto Serie en el Constructor y Destructor
```cpp
puertoSerie = new QSerialPort(this);
puertoSerie->setPortName("/dev/ttyUSB0");
puertoSerie->setBaudRate(QSerialPort::Baud115200);
puertoSerie->setDataBits(QSerialPort::Data8);
puertoSerie->setParity(QSerialPort::NoParity);
puertoSerie->setStopBits(QSerialPort::OneStop);
puertoSerie->setFlowControl(QSerialPort::NoFlowControl);

if (puertoSerie->open(QIODevice::ReadWrite)) { ... }
```
* Reserva memoria dinámicamente con `new QSerialPort(this)`. Al pasarle el puntero `this` como padre, el **sistema de parentesco de Qt** se encarga de liberar su memoria automáticamente cuando se destruye `MainWindow`.
* Configura la trama **115200 8N1** sin control de flujo (exactamente igual que el `Serial.begin(115200)` de Arduino).
* En el **destructor** (`~MainWindow()`), verifica si el puerto quedó abierto (`puertoSerie->isOpen()`) y lo cierra limpiamente con `puertoSerie->close()`.

### B. Construcción de los Widgets y Layouts (`configurarUI()`)
La interfaz se construye mediante código C++ organizando los controles en contenedores:
* **`QVBoxLayout *layoutPrincipal`:** Acomoda los bloques uno debajo del otro verticalmente.
* **`QGroupBox` + `QFormLayout`:** Crea los dos recuadros agrupadores (*"Perfiles de Entorno"* y *"Rango Térmico"*) y organiza cada etiqueta a la izquierda con su control a la derecha mediante `addRow("Etiqueta:", widget)`.
* **`QComboBox`:** Listas desplegables cargadas con `addItem(...)`.
* **`QDoubleSpinBox`:** Selectores numéricos decimales configurados con rango (`setRange`) y valor inicial (`setValue`).

### C. Conexión de Señales y Ranuras (`Signals & Slots`)
```cpp
QObject::connect(btnEnviar, &QPushButton::clicked, this, &MainWindow::enviarDatos);
```
* Vincula la señal `clicked` del botón `btnEnviar` con el método `enviarDatos` del objeto actual (`this`). Cada vez que el usuario hace clic en **"Cargar Parámetros"**, Qt invoca automáticamente a `enviarDatos()`.

### D. Armado y Envío de la Trama Serie (`enviarDatos()`)
```cpp
int idRotacion = comboRotacion->currentIndex();
int idAceleracion = comboAceleracion->currentIndex();
int idHumedad = comboHumedad->currentIndex();

double tMin = spinTempMin->value();
double tMax = spinTempMax->value();
double margen = spinMargen->value();

QString trama = QString::number(idRotacion) + "," +
                QString::number(idAceleracion) + "," +
                QString::number(idHumedad) + "," +
                QString::number(tMin) + "," +
                QString::number(tMax) + "," +
                QString::number(margen) + "\n";

if (puertoSerie->isOpen()) {
    puertoSerie->write(trama.toUtf8());
}
```
1. Obtiene el índice seleccionado de cada desplegable (`0`, `1` o `2`) mediante `currentIndex()` y los valores numéricos de temperatura mediante `value()`.
2. Convierte cada número a cadena con `QString::number(...)` y los concatena separados por comas, cerrando con `\n`.
3. Convierte el `QString` a un arreglo de bytes UTF-8 (`trama.toUtf8()`) y lo transmite por el puerto serie hacia el Arduino usando `puertoSerie->write(...)`.

### E. Paleta Oscura y Hoja de Estilos (`aplicarEstilos()`)
1. **`QPalette` (`qApp->setPalette(...)`):** Define los colores base oscuros (`#121212` y `#E0E0E0`) a nivel de aplicación para que el gestor de ventanas de Linux pinte también la barra de título y los bordes en modo oscuro.
2. **Hoja de estilos (`this->setStyleSheet(qss)`):** Aplica reglas de estilo tipo CSS a cada clase de widget:
   * Fondo negro/gris oscuro (`#121212` y `#1A1A1A`) con etiquetas transparentes (`QLabel { background-color: transparent; }`).
   * Bordes morados (`#7C3AED`) al interactuar con los controles gracias a los estados `:hover` (pasar el mouse por encima), `:focus` (control seleccionado) y `:pressed` (botón presionado).
---

# PARTE 3: Esquema de Conexiones y Arquitectura del Sistema

Esta sección resume la arquitectura de interconexión física entre la placa **ESP32-S3-DevKitC-1**, los módulos de sensores, los actuadores y la aplicación de control en la PC.

---

## 3.1 Lienzo Interactivo en Obsidian Canvas

Si estás leyendo esta documentación dentro de **Obsidian**, puedes abrir y navegar el lienzo visual interactivo con zoom, nodos de colores y flechas de conexión haciendo clic aquí:

👉 **[[esquema_sistema.canvas|Abrir lienzo interactivo del sistema (esquema_sistema.canvas)]]**

*(O accediendo directamente al archivo [`esquema_sistema.canvas`](esquema_sistema.canvas) dentro de la carpeta `documentacion/`)*.

---

## 3.2 Diagrama de Bloques y Conexiones

```mermaid
graph TD
    subgraph SENSORES ["Sensores (Entrada)"]
        IMU["📐 MPU-6050<br/>(Acelerómetro / Giroscopio)<br/>I2C: 0x68 / 0x69"]
        GPS["🛰️ GY-NEO6MV2<br/>(Receptor GPS + Antena)<br/>UART2: 9600 bps"]
        DHT["🌡️ DHT11<br/>(Temperatura / Humedad)<br/>1-Wire Digital"]
        RTC["⏰ DS3231<br/>(Reloj RTC)<br/>I2C: 0x68"]
    end

    subgraph ESP ["Microcontrolador Central"]
        ESP32["🧠 ESP32-S3-DevKitC-1<br/>(Dual Core / 240 MHz)<br/>Alimentación: 3.3V / 5V"]
    end

    subgraph ACTUADORES ["Actuadores (Salida / Alarmas con Enclavamiento)"]
        BUZ["🔊 Buzzer Activo (GPIO 5)<br/>Continuo (Rojo) / Intermitente (Amarillo)"]
        LEDT["💡 LED RGB 1: Temperatura<br/>R: GPIO 6 | G: GPIO 7"]
        LEDI["💡 LED RGB 2: IMU / Impactos<br/>R: GPIO 10 | G: GPIO 11"]
        LEDH["💡 LED RGB 3: Humedad<br/>R: GPIO 12 | G: GPIO 13"]
    end

    subgraph PC ["Estación de Monitoreo (PC)"]
        QT["💻 App de Escritorio Qt6 / C++17<br/>(ConfiguradorTransporte)<br/>Puerto: /dev/ttyUSB0 (115200 bps)"]
    end

    %% Conexiones Sensores -> ESP32
    IMU -->|"SDA ➔ GPIO 8<br/>SCL ➔ GPIO 9"| ESP32
    RTC -->|"SDA ➔ GPIO 8<br/>SCL ➔ GPIO 9"| ESP32
    GPS -->|"TX ➔ GPIO 16 (RX2)<br/>RX ➔ GPIO 17 (TX2)"| ESP32
    DHT -->|"DATA ➔ GPIO 4"| ESP32

    %% Conexiones ESP32 -> Actuadores
    ESP32 -->|"GPIO 5"| BUZ
    ESP32 -->|"GPIO 6 (R), GPIO 7 (G)"| LEDT
    ESP32 -->|"GPIO 10 (R), GPIO 11 (G)"| LEDI
    ESP32 -->|"GPIO 12 (R), GPIO 13 (G)"| LEDH

    %% Conexión Bidireccional USB con la PC
    ESP32 <==|"Cable USB-C (Puerto UART)<br/>Trama: idRot,idAcc,idHum,tMin,tMax,margen"|==> QT
```

---

## 3.3 Tabla Detallada de Cableado Hardware

Todos los pines asignados en la **ESP32-S3-DevKitC-1** se encuentran en la **hilera lateral izquierda (`J1`)**, permitiendo un conexionado ordenado en el protoboard:

| Dispositivo / Módulo | Pin del Módulo | Pin en ESP32-S3 | Tipo de Señal / Protocolo | Notas de Conexión |
| :--- | :--- | :--- | :--- | :--- |
| **MPU-6050** | `VCC` / `GND` | `3V3` / `GND` | Alimentación | 3.3V nominal |
| **MPU-6050** | `SDA` | **Pin `8`** | I2C Data (`Wire`) | Compartido con RTC |
| **MPU-6050** | `SCL` | **Pin `9`** | I2C Clock (`Wire`) | Compartido con RTC |
| **MPU-6050** | `AD0` | `GND` o `3V3` | Selección de dirección | `GND` = 0x68 (solo IMU) / `3V3` = 0x69 (si se conecta también RTC) |
| **GY-NEO6MV2** | `VCC` / `GND` | `3V3` / `GND` | Alimentación | Acepta 3.3V o 5V en VCC |
| **GY-NEO6MV2** | `TX` del GPS | **Pin `16`** | UART2 Rx (`Serial2`) | El GPS transmite datos NMEA a la ESP32 |
| **GY-NEO6MV2** | `RX` del GPS | **Pin `17`** | UART2 Tx (`Serial2`) | La ESP32 envía comandos al GPS (opcional) |
| **DHT11** | `VCC` / `GND` | `3V3` / `GND` | Alimentación | Resistencia pull-up de 4.7kΩ recomendada en DATA |
| **DHT11** | `DATA` | **Pin `4`** | Digital 1-Wire | Lectura de temperatura y humedad |
| **RTC DS3231** | `SDA` / `SCL` | **Pines `8` y `9`** | I2C (`Wire`) | Bus I2C compartido con MPU-6050 |
| **Buzzer Activo** | `+` (Positivo) | **Pin `5`** | Salida Digital | Continuo si hay algún Rojo; intermitente (300 ms) si hay Amarillo |
| **LED RGB 1 (Temp)** | Ánodos Rojo / Verde | **Pines `6` y `7`** | Salida PWM (`analogWrite`) | Pin 6=R, Pin 7=G (Verde: R=0, G=255 \| Amarillo: R=255, G=35 \| Rojo: R=255, G=0) |
| **LED RGB 2 (IMU)** | Ánodos Rojo / Verde | **Pines `10` y `11`** | Salida PWM (`analogWrite`) | Pin 10=R, Pin 11=G (Verde: R=0, G=255 \| Amarillo: R=255, G=35 \| Rojo: R=255, G=0) |
| **LED RGB 3 (Humedad)** | Ánodos Rojo / Verde | **Pines `12` y `13`** | Salida PWM (`analogWrite`) | Pin 12=R, Pin 13=G (Verde: R=0, G=255 \| Amarillo: R=255, G=35 \| Rojo: R=255, G=0) |
| **PC (Qt6)** | USB-C UART | Puerto `UART` | Serial UART (115200 bps) | Detectado en Linux como `/dev/ttyUSB0` o `/dev/ttyACM0` |

