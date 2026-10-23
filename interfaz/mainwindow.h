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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
~MainWindow();
private slots:
    void cambiarTema();
    void enviarDatos();

private:
    void configurarUI();
    void aplicarEstilos();

    bool modoOscuro;

    QComboBox *comboRotacion;
    QComboBox *comboAceleracion;
    QComboBox *comboHumedad;
    QDoubleSpinBox *spinTempMin;
    QDoubleSpinBox *spinTempMax;
    QDoubleSpinBox *spinMargen;
    
    QPushButton *btnEnviar;
    QPushButton *btnTema;
    QSerialPort *puertoSerie;
};

#endif
