#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMainWindow>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void enviarDatos();
    void pedirRegistro();
    void desbloquearAdmin();
    void leerDatosSerie();

private:
    void configurarUI();
    void aplicarEstilos();

    QComboBox *comboRotacion;
    QComboBox *comboAceleracion;
    QComboBox *comboHumedad;
    QDoubleSpinBox *spinTempMin;
    QDoubleSpinBox *spinTempMax;
    QDoubleSpinBox *spinMargen;
    
    QPushButton *btnAdmin;
    QPushButton *btnEnviar;
    QPushButton *btnDescargar;
    QSerialPort *puertoSerie;
    QString bufferSerie;
    bool descargandoRegistro = false;
    QFile archivoCSV;
};

#endif
