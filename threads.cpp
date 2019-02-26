#include "threads.h"

/*
    线程构造函数data_thread
    功能：继承父类
    输入：无
    输出：无
*/
data_thread::data_thread(MainWindow *in)
{
    //继承mainwindow
    parent = in;

    //构造定时器使进入事件循环
    timer = new QTimer;
    connect(timer,SIGNAL(timeout()),this->parent,SLOT(Get_data()));
    timer->start(TIME_CYC);
}

data_thread::~data_thread()
{
    delete timer;
}

/*
    线程函数run
    功能：在采集阶段从串口获取数据
    输入：无
    输出：无
*/
void data_thread::run()
{
    exec();//开启线程
}
