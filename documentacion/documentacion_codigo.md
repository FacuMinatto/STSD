# Documentación Técnica del Proyecto STSD

Este documento explica en detalle el funcionamiento del código fuente del **Sistema de Transporte de Sustancias Delicadas (STSD)**, dividido en dos grandes bloques:
1. **Parte 1: Funcionamiento del Sistema Embebido (Arduino y Módulos de Sensores)**
2. **Parte 2: Aplicación de Escritorio en C++ / Qt (`interfaz/`)**

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
En la parte superior se definen mediante directivas del preprocesador (`#define`) los pines de los actuadores y las variables globales que almacenan la configuración vigente:

```cpp
#define BUZZER_PIN 6
#define VERDE 9
#define ROJO 10

int confRotacion = 0;
int confAceleracion = 0;
int confHumedad = 0;
float confTempMin = 2.0;
float confTempMax = 30.0;
float confMargen = 1.5;
```
* Estas variables inician con valores seguros por defecto para que el sistema pueda operar de forma autónoma incluso si todavía no se conectó a la PC.

### B. Inicialización (`setup()`)
Se ejecuta una única vez al encender o resetear el microcontrolador:
1. Inicia la comunicación serie UART a **115200 baudios** (`Serial.begin(115200)`).
2. Configura los pines de los LEDs y el buzzer como salidas digitales (`pinMode(..., OUTPUT)`), encendiendo el LED verde para indicar que el sistema está activo.
3. Llama a las funciones de inicialización de cada módulo (`setupDHT()`, `setupIMU()`, `setupRTC()`, `setupGPS()`).

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
}
```
* **Cómo funciona:** Cuando la aplicación Qt envía una trama como `0,1,0,2.0,8.0,1.5\n`, `Serial.available()` detecta bytes en el buffer de recepción.
* `Serial.parseInt()` y `Serial.parseFloat()` leen secuencialmente cada número salteando automáticamente las comas separadoras (`,`).
* `Serial.readStringUntil('\n')` consume el salto de línea final para dejar limpio el buffer serie.

#### 3. Muestreo no bloqueante con `millis()`
```cpp
unsigned long tiempoActual = millis();
if (tiempoActual - ultimoTiempoRegistro >= INTERVALO_REGISTRO) {
  ultimoTiempoRegistro = tiempoActual;
  // Lectura de sensores y reporte...
}
```
* **Importancia técnica:** En lugar de frenar el microcontrolador con `delay(1000)`, se compara el tiempo transcurrido mediante `millis()`. Esto permite que durante ese segundo el microcontrolador siga leyendo el GPS (`procesarGPS()`) y escuchando comandos de la PC de forma instantánea.

#### 4. Lógica de Alarmas (3 estados)
```cpp
if (temperatura > confTempMax || temperatura < confTempMin) {
  activarAlarma();
} else if (temperatura > (confTempMax - confMargen) || temperatura < (confTempMin + confMargen)) {
  alertaNaranja();    
} else {
  desactivarAlarma();
}
```
* **Estado Normal (`desactivarAlarma`):** Temperatura dentro del rango seguro -> LED Verde encendido, LED Rojo y Buzzer apagados.
* **Estado de Alerta Preventiva (`alertaNaranja`):** La temperatura entró en la zona de margen (`confTempMax - confMargen` o `confTempMin + confMargen`). Combina ambos LEDs usando modulación por ancho de pulso (**PWM** con `analogWrite(ROJO, 255)` y `analogWrite(VERDE, 40)`) y activa el buzzer.
* **Estado Crítico (`activarAlarma`):** Temperatura fuera de los límites permitidos -> LED Rojo encendido al 100%, LED Verde apagado y Buzzer encendido.

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
* Crea un puerto serie por software (`SoftwareSerial ss(GPS_RX_PIN, GPS_TX_PIN)`) en los pines 2 y 3 a 9600 baudios para dejar libre el puerto serie principal (`Serial`) que se comunica con la PC.
* A medida que llegan caracteres desde el GPS, `gps.encode(ss.read())` decodifica las tramas NMEA y actualiza la latitud y longitud.

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


```
git add .
git commit -m "Escribe aquí brevemente qué cambiaste"
git push
```
