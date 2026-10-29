#include "mainwindow.h"
#include <QApplication>
#include <QPalette>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    configurarUI();
    aplicarEstilos();
    setWindowTitle("Configurador de Transporte");
    setMinimumSize(380, 520);

    // Inicializar y configurar el puerto serie según lo visto en cátedra
    puertoSerie = new QSerialPort(this);
    puertoSerie->setPortName("/dev/ttyUSB0");
    puertoSerie->setBaudRate(QSerialPort::Baud115200);
    puertoSerie->setDataBits(QSerialPort::Data8);
    puertoSerie->setParity(QSerialPort::NoParity);
    puertoSerie->setStopBits(QSerialPort::OneStop);
    puertoSerie->setFlowControl(QSerialPort::NoFlowControl);

    connect(puertoSerie, &QSerialPort::readyRead, this, &MainWindow::leerDatosSerie);

    if (puertoSerie->open(QIODevice::ReadWrite)) {
        qDebug() << "Puerto serie abierto correctamente.";
        puertoSerie->write("OBTENER_CONFIG\n");
    } else {
        qDebug() << "Error: No se pudo abrir el puerto serie al iniciar.";
    }
}

MainWindow::~MainWindow() {
    if (archivoCSV.isOpen()) {
        archivoCSV.close();
    }
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

    QLabel *lblTitulo = new QLabel("Configurador de Transporte", this);
    lblTitulo->setAlignment(Qt::AlignCenter);
    lblTitulo->setStyleSheet("font-size: 16px; font-weight: bold; color: #FFFFFF; padding: 4px 0px;");
    layoutPrincipal->addWidget(lblTitulo);

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

    // Botones de acción
    btnAdmin = new QPushButton("Modo Administrador (Desbloquear)");
    btnAdmin->setMinimumHeight(38);

    btnEnviar = new QPushButton("Cargar Parámetros");
    btnEnviar->setMinimumHeight(38);

    btnDescargar = new QPushButton("Descargar Registro");
    btnDescargar->setMinimumHeight(38);

    // Estado inicial Read-Only (Operador / Receptor)
    comboRotacion->setEnabled(false);
    comboAceleracion->setEnabled(false);
    comboHumedad->setEnabled(false);
    spinTempMin->setEnabled(false);
    spinTempMax->setEnabled(false);
    spinMargen->setEnabled(false);
    btnEnviar->setEnabled(false);

    // Conectar señales con ranuras (signals & slots)
    connect(btnAdmin, &QPushButton::clicked, this, &MainWindow::desbloquearAdmin);
    connect(btnEnviar, &QPushButton::clicked, this, &MainWindow::enviarDatos);
    connect(btnDescargar, &QPushButton::clicked, this, &MainWindow::pedirRegistro);

    layoutPrincipal->addWidget(btnAdmin);
    layoutPrincipal->addWidget(grupoSensores);
    layoutPrincipal->addWidget(grupoTemperatura);
    layoutPrincipal->addWidget(btnEnviar);
    layoutPrincipal->addWidget(btnDescargar);

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
        "QComboBox:disabled, QDoubleSpinBox:disabled { background-color: #1A1A1A; color: #9E9E9E; border: 1px solid #2A2A2A; }"
        "QComboBox:focus, QDoubleSpinBox:focus, QComboBox:hover, QDoubleSpinBox:hover { border: 1px solid #7C3AED; }"
        "QPushButton { background-color: #1F1F1F; color: #FFFFFF; border: 1px solid #7C3AED; border-radius: 5px; padding: 8px; font-weight: bold; }"
        "QPushButton:disabled { background-color: #181818; color: #555555; border: 1px solid #2E2E2E; }"
        "QPushButton:hover { background-color: #7C3AED; }"
        "QPushButton:pressed { background-color: #5B21B6; }";

    this->setStyleSheet(qss);
}

void MainWindow::desbloquearAdmin() {
    QInputDialog dialog(this);
    dialog.setWindowTitle("Acceso Administrador");
    dialog.setLabelText("Ingrese la contraseña de administrador:");
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Quitar iconos de los botones OK / Cancel
    QDialogButtonBox *box = dialog.findChild<QDialogButtonBox *>();
    if (box) {
        if (QAbstractButton *btnOk = box->button(QDialogButtonBox::Ok)) {
            btnOk->setIcon(QIcon());
        }
        if (QAbstractButton *btnCancel = box->button(QDialogButtonBox::Cancel)) {
            btnCancel->setIcon(QIcon());
        }
    }

    if (dialog.exec() == QDialog::Accepted) {
        if (dialog.textValue() == "12345678") {
            comboRotacion->setEnabled(true);
            comboAceleracion->setEnabled(true);
            comboHumedad->setEnabled(true);
            spinTempMin->setEnabled(true);
            spinTempMax->setEnabled(true);
            spinMargen->setEnabled(true);
            btnEnviar->setEnabled(true);
            btnAdmin->setText("Modo Administrador (Activo)");
            btnAdmin->setEnabled(false);
        } else {
            QMessageBox msg(QMessageBox::NoIcon, "Acceso Denegado", "Contraseña incorrecta.", QMessageBox::Ok, this);
            if (QAbstractButton *btnOk = msg.button(QMessageBox::Ok)) {
                btnOk->setIcon(QIcon());
            }
            msg.exec();
        }
    }
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

void MainWindow::pedirRegistro() {
    if (puertoSerie->isOpen()) {
        puertoSerie->write("OBTENER_CONFIG\n");
        puertoSerie->write("DESCARGAR\n");
        qDebug() << "Comando DESCARGAR enviado a la ESP32.";
    } else {
        QMessageBox::warning(this, "Puerto Desconectado", "No se puede descargar el registro porque el puerto serie no está conectado.");
    }
}

void MainWindow::leerDatosSerie() {
    bufferSerie.append(QString::fromUtf8(puertoSerie->readAll()));

    while (bufferSerie.contains('\n')) {
        int idx = bufferSerie.indexOf('\n');
        QString linea = bufferSerie.left(idx).trimmed();
        bufferSerie.remove(0, idx + 1);

        if (linea.startsWith("CONFIG:")) {
            QStringList partes = linea.mid(7).split(',');
            if (partes.size() == 6) {
                comboRotacion->setCurrentIndex(partes[0].toInt());
                comboAceleracion->setCurrentIndex(partes[1].toInt());
                comboHumedad->setCurrentIndex(partes[2].toInt());
                spinTempMin->setValue(partes[3].toDouble());
                spinTempMax->setValue(partes[4].toDouble());
                spinMargen->setValue(partes[5].toDouble());
            }
        } else if (linea == "---INICIO_REGISTRO---") {
            archivoCSV.setFileName("registro_viaje.csv");
            if (archivoCSV.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                descargandoRegistro = true;
                qDebug() << "Iniciando descarga de registro_viaje.csv...";
            }
        } else if (linea == "---FIN_REGISTRO---") {
            if (archivoCSV.isOpen()) {
                archivoCSV.close();
            }
            descargandoRegistro = false;
            qDebug() << "Descarga de registro finalizada.";
            QMessageBox::information(this, "Registro Descargado", "El historial de anomalías se descargó exitosamente en registro_viaje.csv");
        } else if (descargandoRegistro && archivoCSV.isOpen()) {
            if (!linea.isEmpty()) {
                QTextStream out(&archivoCSV);
                out << linea << "\n";
            }
        }
    }
}
