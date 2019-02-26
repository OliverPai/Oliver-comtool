#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QThread>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <qdebug.h>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QPixmap>
#include <QPainter>
#include <QStringListModel>
#include <QFileDialog>
#include <QTimer>
#include <QDateTime>
#include <QButtonGroup>
#include <QDesktopWidget>
#include <vector>
#include <limits.h>

#define TIME_START  0
#define TIME_END    1000
#define BUFFERSIZE  TIME_END-TIME_START

using namespace QT_CHARTS_NAMESPACE;

extern int BaudRate;
extern QSerialPort::DataBits databit;
extern QSerialPort::StopBits stopbit;
extern QSerialPort::Parity parityset;

namespace Ui {
class MainWindow;
class data_thread;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //串口
    QSerialPort *serial;

    //ui
    Ui::MainWindow *ui;
private:

    //绘制文字和图片
    void paintEvent(QPaintEvent *event);

    //RadioButton
    QButtonGroup *BtnGroup;

    //图像
    QChart *drawing;
    QChartView *drawview;
    QLineSeries *series;//数据源
    QValueAxis *axisX;
    QValueAxis *axisY;
public slots:
    void Get_data();
    void Clear_Receiver();
    void Send_Command();

private slots:
    void func_start();
    void func_shut();
    void BaudRate_select();
    void data_select();
    void stop_select();
    void odd_select();
    void show_port();
//    void save_chart();
    void save_chart_default();
    void save_txt_default();
    void dataORtext();
    void storeMessage();
    void clear_image();
};

#endif // MAINWINDOW_H
