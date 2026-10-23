#include "mainwindow.h"
#include <QDebug>
#include <QThread>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), modoOscuro(true) {
    configurarUI();
    aplicarEstilos();
    setWindowTitle("Configuración de Transporte ESP32");
    resize(400, 500);
    // Inicializar y mantener abierto el puerto serie
    puertoSerie = new QSerialPort(this);
    puertoSerie->setPortName("/dev/ttyUSB0");
    puertoSerie->setBaudRate(QSerialPort::Baud115200);

    if (puertoSerie->open(QIODevice::WriteOnly)) {
        qDebug() << "Puerto abierto. Esperando que el Arduino termine de reiniciarse...";
        // Le damos 2 segundos al Arduino para que pase su bootloader
        QThread::sleep(2); 
        qDebug() << "Listo para enviar datos.";
    } else {
        qDebug() << "Error: No se pudo abrir el puerto serie al iniciar.";
    }
}

MainWindow::~MainWindow() {
    // Cerramos el puerto limpiamente al cerrar la ventana
    if (puertoSerie->isOpen()) {
        puertoSerie->close();
    }
}


void MainWindow::configurarUI() {
    QWidget *widgetCentral = new QWidget(this);
    QVBoxLayout *layoutPrincipal = new QVBoxLayout(widgetCentral);

    // Grupo de Sensores (IMU y Humedad)
    QGroupBox *grupoSensores = new QGroupBox("Perfiles de Entorno");
    QFormLayout *layoutSensores = new QFormLayout();

    comboRotacion = new QComboBox();
    comboRotacion->addItems({"Mínima", "Media", "Libre"});

    comboAceleracion = new QComboBox();
    comboAceleracion->addItems({"Muy Frágil", "Frágil", "Resistente"});

    comboHumedad = new QComboBox();
    comboHumedad->addItems({"Seco (20-50%)", "Húmedo (60-80%)", "Indistinto (20-90%)"});

    layoutSensores->addRow("Límite de Rotación:", comboRotacion);
    layoutSensores->addRow("Sensibilidad de Impacto:", comboAceleracion);
    layoutSensores->addRow("Perfil de Humedad:", comboHumedad);
    grupoSensores->setLayout(layoutSensores);

    // Grupo de Temperatura
    QGroupBox *grupoTemperatura = new QGroupBox("Rango Térmico (°C)");
    QFormLayout *layoutTemperatura = new QFormLayout();

    spinTempMin = new QDoubleSpinBox();
    spinTempMin->setRange(-50.0, 100.0);
    spinTempMin->setValue(2.0);

    spinTempMax = new QDoubleSpinBox();
    spinTempMax->setRange(-50.0, 100.0);
    spinTempMax->setValue(8.0);

    spinMargen = new QDoubleSpinBox();
    spinMargen->setRange(0.0, 10.0);
    spinMargen->setValue(1.5); // Margen riesgoso amarillo

    layoutTemperatura->addRow("Temperatura Mínima:", spinTempMin);
    layoutTemperatura->addRow("Temperatura Máxima:", spinTempMax);
    layoutTemperatura->addRow("Margen Riesgoso (±):", spinMargen);
    grupoTemperatura->setLayout(layoutTemperatura);

    // Botones de acción
    btnEnviar = new QPushButton("Cargar Parámetros al Sistema");
    btnTema = new QPushButton("Alternar Modo Claro/Oscuro");

    // CONEXIÓN DE SEÑALES Y SLOTS
    // Aquí vinculamos el clic del botón con la función a ejecutar
    connect(btnEnviar, &QPushButton::clicked, this, &MainWindow::enviarDatos);
    connect(btnTema, &QPushButton::clicked, this, &MainWindow::cambiarTema);

    layoutPrincipal->addWidget(grupoSensores);
    layoutPrincipal->addWidget(grupoTemperatura);
    layoutPrincipal->addWidget(btnEnviar);
    layoutPrincipal->addWidget(btnTema);

    setCentralWidget(widgetCentral);
}

void MainWindow::cambiarTema() {
    modoOscuro = !modoOscuro;
    aplicarEstilos();
}

void MainWindow::aplicarEstilos() {
    QString qss;
    if (modoOscuro) {
        // Paleta: Fondo Negro, Detalles Morados y Azules
        qss = R"(
            QWidget { background-color: #121212; color: #E0E0E0; font-family: sans-serif; }
            QGroupBox { border: 1px solid #6A0DAD; border-radius: 5px; margin-top: 10px; font-weight: bold; }
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #8A2BE2; }
            QComboBox, QDoubleSpinBox { background-color: #1E1E1E; border: 1px solid #4169E1; border-radius: 3px; padding: 4px; color: white; }
            QPushButton { background-color: #4B0082; color: white; border: 1px solid #8A2BE2; padding: 8px; border-radius: 4px; font-weight: bold; }
            QPushButton:hover { background-color: #6A0DAD; border: 1px solid #4169E1; }
        )";
    } else {
        // Paleta Clara
        qss = R"(
            QWidget { background-color: #F8F9FA; color: #212529; font-family: sans-serif; }
            QGroupBox { border: 1px solid #4169E1; border-radius: 5px; margin-top: 10px; font-weight: bold; }
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #4169E1; }
            QComboBox, QDoubleSpinBox { background-color: white; border: 1px solid #B0C4DE; border-radius: 3px; padding: 4px; color: black; }
            QPushButton { background-color: #4169E1; color: white; border: none; padding: 8px; border-radius: 4px; font-weight: bold; }
            QPushButton:hover { background-color: #6A0DAD; }
        )";
    }
    this->setStyleSheet(qss);
}

void MainWindow::enviarDatos() {
    int idRotacion = comboRotacion->currentIndex();
    int idAceleracion = comboAceleracion->currentIndex();
    int idHumedad = comboHumedad->currentIndex();
    
    double tMin = spinTempMin->value();
    double tMax = spinTempMax->value();
    double margen = spinMargen->value();

    // Crear la trama separada por comas
    QString trama = QString("%1,%2,%3,%4,%5,%6\n")
                    .arg(idRotacion).arg(idAceleracion).arg(idHumedad)
                    .arg(tMin).arg(tMax).arg(margen);

    // Configurar y abrir el puerto serie
    

   // Solo enviamos la trama, sin abrir ni cerrar el puerto aquí
    if (puertoSerie->isOpen()) {
        puertoSerie->write(trama.toUtf8());
        puertoSerie->waitForBytesWritten(100);
        qDebug() << "Datos enviados correctamente:" << trama;
    } else {
        qDebug() << "Error: El puerto no está abierto.";
    }
 

}
