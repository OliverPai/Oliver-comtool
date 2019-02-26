#ifndef DATA_THREAD_H
#define DATA_THREAD_H

#include "mainwindow.h"

#define TIME_CYC 1

//专用于采集数据的线程类
class data_thread : public QThread
{
public:
    explicit data_thread(MainWindow *in);
    ~data_thread();
    QTimer *timer;
private:
    void run();
    MainWindow *parent;
};

#endif // DATA_THREAD_H
