#include "mainwindow.h"
#include <QApplication>
#include <QPalette>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    configurarUI();
    aplicarEstilos();
    setWindowTitle("Configurador de Transporte");
    setMinimumSize(380, 420);

    // Inicializar y configurar el puerto serie según lo visto en cátedra
    puertoSerie = new QSerialPort(this);
    puertoSerie->setPortName("/dev/ttyUSB0");
    puertoSerie->setBaudRate(QSerialPort::Baud115200);
    puertoSerie->setDataBits(QSerialPort::Data8);
    puertoSerie->setParity(QSerialPort::NoParity);
    puertoSerie->setStopBits(QSerialPort::OneStop);
    puertoSerie->setFlowControl(QSerialPort::NoFlowControl);

    if (puertoSerie->open(QIODevice::ReadWrite)) {
        qDebug() << "Puerto serie abierto correctamente.";
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
    layoutPrincipal->setContentsMargins(16, 16, 16, 16);
    layoutPrincipal->setSpacing(14);

    // Grupo de Sensores (IMU y Humedad)
    QGroupBox *grupoSensores = new QGroupBox("Perfiles de Entorno");
    QFormLayout *layoutSensores = new QFormLayout();
    layoutSensores->setContentsMargins(12, 16, 12, 12);
    layoutSensores->setSpacing(10);

    comboRotacion = new QComboBox();
    comboRotacion->addItem("Mínima");
    comboRotacion->addItem("Media");
    comboRotacion->addItem("Libre");

    comboAceleracion = new QComboBox();
    comboAceleracion->addItem("Muy Frágil");
    comboAceleracion->addItem("Frágil");
    comboAceleracion->addItem("Resistente");

    comboHumedad = new QComboBox();
    comboHumedad->addItem("Seco (20-50%)");
    comboHumedad->addItem("Húmedo (60-80%)");
    comboHumedad->addItem("Indistinto (20-90%)");

    layoutSensores->addRow("Límite de Rotación:", comboRotacion);
    layoutSensores->addRow("Sensibilidad de Impacto:", comboAceleracion);
    layoutSensores->addRow("Perfil de Humedad:", comboHumedad);
    grupoSensores->setLayout(layoutSensores);

    // Grupo de Temperatura
    QGroupBox *grupoTemperatura = new QGroupBox("Rango Térmico (°C)");
    QFormLayout *layoutTemperatura = new QFormLayout();
    layoutTemperatura->setContentsMargins(12, 16, 12, 12);
    layoutTemperatura->setSpacing(10);

    spinTempMin = new QDoubleSpinBox();
    spinTempMin->setRange(-50.0, 100.0);
    spinTempMin->setValue(2.0);

    spinTempMax = new QDoubleSpinBox();
    spinTempMax->setRange(-50.0, 100.0);
    spinTempMax->setValue(8.0);

    spinMargen = new QDoubleSpinBox();
    spinMargen->setRange(0.0, 10.0);
    spinMargen->setValue(1.5);

    layoutTemperatura->addRow("Temperatura Mínima:", spinTempMin);
    layoutTemperatura->addRow("Temperatura Máxima:", spinTempMax);
    layoutTemperatura->addRow("Margen Riesgoso (±):", spinMargen);
    grupoTemperatura->setLayout(layoutTemperatura);

    // Botón de acción
    btnEnviar = new QPushButton("Cargar Parámetros");
    btnEnviar->setMinimumHeight(38);

    // Conectar señal con ranura (signals & slots)
    QObject::connect(btnEnviar, &QPushButton::clicked, this, &MainWindow::enviarDatos);

    layoutPrincipal->addWidget(grupoSensores);
    layoutPrincipal->addWidget(grupoTemperatura);
    layoutPrincipal->addWidget(btnEnviar);

    setCentralWidget(widgetCentral);
}

void MainWindow::aplicarEstilos() {
    // Paleta oscura para que el borde y la barra de título de la ventana también sean oscuros
    QPalette paletaOscura = qApp->palette();
    paletaOscura.setColor(QPalette::Window, QColor("#121212"));
    paletaOscura.setColor(QPalette::WindowText, QColor("#E0E0E0"));
    paletaOscura.setColor(QPalette::Base, QColor("#1A1A1A"));
    paletaOscura.setColor(QPalette::Button, QColor("#1F1F1F"));
    paletaOscura.setColor(QPalette::ButtonText, QColor("#E0E0E0"));
    qApp->setPalette(paletaOscura);

    // Tema oscuro (negros y grises) con etiquetas transparentes y detalles en morado (#7C3AED)
    QString qss =
        "QMainWindow, QWidget { background-color: #121212; color: #E0E0E0; font-size: 13px; }"
        "QLabel { background-color: transparent; }"
        "QGroupBox { background-color: #1A1A1A; border: 1px solid #2E2E2E; border-radius: 6px; margin-top: 8px; font-weight: bold; color: #A78BFA; }"
        "QComboBox, QDoubleSpinBox { background-color: #242424; color: #F0F0F0; border: 1px solid #3A3A3A; border-radius: 4px; padding: 5px; }"
        "QComboBox:focus, QDoubleSpinBox:focus, QComboBox:hover, QDoubleSpinBox:hover { border: 1px solid #7C3AED; }"
        "QPushButton { background-color: #1F1F1F; color: #FFFFFF; border: 1px solid #7C3AED; border-radius: 5px; padding: 8px; font-weight: bold; }"
        "QPushButton:hover { background-color: #7C3AED; }"
        "QPushButton:pressed { background-color: #5B21B6; }";

    this->setStyleSheet(qss);
}

void MainWindow::enviarDatos() {
    int idRotacion = comboRotacion->currentIndex();
    int idAceleracion = comboAceleracion->currentIndex();
    int idHumedad = comboHumedad->currentIndex();

    double tMin = spinTempMin->value();
    double tMax = spinTempMax->value();
    double margen = spinMargen->value();

    // Armar la trama separada por comas usando QString::number
    QString trama = QString::number(idRotacion) + "," +
                    QString::number(idAceleracion) + "," +
                    QString::number(idHumedad) + "," +
                    QString::number(tMin) + "," +
                    QString::number(tMax) + "," +
                    QString::number(margen) + "\n";

    if (puertoSerie->isOpen()) {
        puertoSerie->write(trama.toUtf8());
        qDebug() << "Datos enviados correctamente:" << trama;
    } else {
        qDebug() << "Error: El puerto no está abierto.";
    }
}
