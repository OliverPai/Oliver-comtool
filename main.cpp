#include "mainwindow.h"
#include <QApplication>

int BaudRate = 115200;
QSerialPort::DataBits databit = QSerialPort::Data8;
QSerialPort::StopBits stopbit = QSerialPort::OneStop;
QSerialPort::Parity parityset = QSerialPort::NoParity;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
