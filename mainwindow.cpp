#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "threads.h"
#include <typeinfo>

static int Started = 0;     //标志位，用来标志是否在采集
static data_thread* Getting;

#define IsText 1            //1:文本，0；数据（默认文本）
#define IsData 0
static int TextOrData = IsText;  //标志位，用来判断当前数据是文本还是数据

static QStringList text_content;//记录文本的变量
static QStringListModel *model_text = new QStringListModel(text_content);

//不同类型数据缓冲区
static std::vector<int> buffer_int(BUFFERSIZE);
static std::vector<float> buffer_float(BUFFERSIZE);
static std::vector<double> buffer_double(BUFFERSIZE);
static int buffer_index = -1;

static int del = 0;//标志位,判断是否清楚曲线

//时间间隔，用来改变横轴
static double interval;

//泛型，将不同类型数据送入对应缓冲区
template<typename T>
inline void data_to_storage(QByteArray data,std::vector<T> &buffer)
{
    T temp_data = (typeid (T) == typeid (int))?data.toInt()://int默认十进制
                 ((typeid (T) == typeid (float))?data.toFloat():data.toDouble());
    //qDebug()<<temp_data;
    if(buffer_index == BUFFERSIZE)
        buffer.pop_back();
    buffer.insert(buffer.begin(),temp_data);
    if(buffer_index<BUFFERSIZE)buffer_index++;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    //开启UI
    ui->setupUi(this);

    //设置标题和图标
    setWindowTitle("Oliver-comtool");
    setWindowIcon(QIcon(":/temp.ico"));

    //做分辨率适配
    /*
    QDesktopWidget* desktopWidget = QApplication::desktop();
    QRect screenRect = desktopWidget->screenGeometry();
    int g_nActScreenW = screenRect.width();
    int g_nActScreenH = screenRect.height();
    resize(g_nActScreenW,g_nActScreenH);
    */

    //关闭串口
    serial = new QSerialPort();
    serial->close();

    //设置显示窗口
    ui->listView->setFlow(QListView::TopToBottom);

    //设置radiobutton
    BtnGroup = new QButtonGroup(this);
    BtnGroup->addButton(ui->radioButton, 0);
    BtnGroup->addButton(ui->radioButton_2, 1);
    ui->radioButton_2->setChecked(true);

    //连接槽函数
    connect(this->ui->pushButton_start,SIGNAL(clicked()),this,SLOT(func_start()));
    connect(this->ui->pushButton_shut,SIGNAL(clicked()),this,SLOT(func_shut()));
    connect(this->ui->comboBox,SIGNAL(currentIndexChanged(QString)),this,SLOT(BaudRate_select()));
    connect(this->ui->comboBox_2,SIGNAL(currentIndexChanged(QString)),this,SLOT(data_select()));
    connect(this->ui->comboBox_3,SIGNAL(currentIndexChanged(QString)),this,SLOT(stop_select()));
    connect(this->ui->comboBox_4,SIGNAL(currentIndexChanged(QString)),this,SLOT(odd_select()));
    connect(this->ui->pushRefresh,SIGNAL(clicked()),this,SLOT(show_port()));
//    connect(this->ui->save,SIGNAL(triggered()),this,SLOT(save_chart()));
    connect(this->ui->save_default,SIGNAL(triggered()),this,SLOT(save_chart_default()));
    connect(this->ui->save_txt_default,SIGNAL(triggered()),this,SLOT(save_txt_default()));
    connect(this->ui->pushSend,SIGNAL(clicked()),this,SLOT(Send_Command()));
    connect(this->ui->pushClear,SIGNAL(clicked()),this,SLOT(Clear_Receiver()));
    connect(this->ui->radioButton, SIGNAL(clicked()), this, SLOT(dataORtext()));
    connect(this->ui->radioButton_2, SIGNAL(clicked()), this, SLOT(dataORtext()));
    connect(this->ui->pushButton_clear,SIGNAL(clicked()),this,SLOT(clear_image()));

    //画图准备
    drawing = new QChart;
    drawing->setTitle("串口数据曲线");
    drawview = new QChartView(drawing);
    axisX = new QValueAxis;
    axisY = new QValueAxis;
    //建立数据源队列
    series = new QLineSeries;

    //建立坐标轴
    QBrush AxisColor;
    AxisColor.setColor(Qt::black);
    axisX->setRange(0-TIME_END, TIME_START);                // 设置范围
    axisX->setLabelFormat("%d");                            // 设置刻度的格式
    axisX->setGridLineVisible(true);                        // 网格线可见
    axisX->setTickCount(6);                                 // 设置多少个大格
    axisX->setMinorTickCount(1);                            // 设置每个大格里面小刻度线的数目
    axisX->setTitleText("时间");                             // 设置描述
    axisX->setLabelsVisible(true);                          // 设置刻度是否可见
    axisX->setLabelsColor(QColor(47, 79, 79));
    axisX->setTitleBrush(AxisColor);
    axisY->setRange(0, 14);
    axisY->setLabelFormat("%.1f");
    axisY->setGridLineVisible(true);
    axisY->setTickCount(8);
    axisY->setMinorTickCount(1);
    axisY->setTitleText("数据");
    axisY->setLabelsColor(QColor(47, 79, 79));
    axisY->setTitleBrush(AxisColor);
    //为曲线添加坐标轴
    drawing->addAxis(axisX, Qt::AlignBottom);               // 下：Qt::AlignBottom  上：Qt::AlignTop
    drawing->addAxis(axisY, Qt::AlignLeft);                 // 左：Qt::AlignLeft    右：Qt::AlignRight
    drawing->legend()->hide();                              //隐藏图例
    //chart放入chartview内
    drawview->setRenderHint(QPainter::Antialiasing);        //防止图形走样
    //将chartview放入layout内
    ui->verticalLayout->addWidget(drawview);
}

/*
    绘制背景
    绘制文字
*/
void MainWindow::paintEvent(QPaintEvent *event) {           // paintEvent不能随便改名，这是一个虚函数
    // 画字
    QPainter painter(this);
    QFont font;

    /*
        文本的位置：
            QPointF:第一个参数：与left的距离
                    第二个参数：与top的距离
            QSizeF: 第一个参数：width
                    第二个参数：height
        文本的内容：最后一个参数
    */
    QPixmap pix;
    pix.load(":/background.jpg");
    painter.drawPixmap(0, 0, width(), height(), pix);
    painter.drawText(QRectF(QPointF(1090, 50), QSizeF(100, 40)), Qt::AlignCenter, "波特率/bps");
    painter.drawText(QRectF(QPointF(1090, 90), QSizeF(60, 40)), Qt::AlignCenter, "端口");
    painter.drawText(QRectF(QPointF(1090, 130), QSizeF(72, 40)), Qt::AlignCenter, "数据位");
    painter.drawText(QRectF(QPointF(1090, 170), QSizeF(72, 40)), Qt::AlignCenter, "停止位");
    painter.drawText(QRectF(QPointF(1090, 210), QSizeF(60, 40)), Qt::AlignCenter, "校验");
    painter.drawText(QRectF(QPointF(840, 125), QSizeF(100, 50)), Qt::AlignCenter, "实时数据显示");
    painter.drawText(QRectF(QPointF(840, 40), QSizeF(100, 50)), Qt::AlignCenter, "在线COM端口");
    painter.drawText(QRectF(QPointF(380, 50), QSizeF(80, 40)), Qt::AlignCenter, "数据类型");
    painter.drawText(QRectF(QPointF(535, 50), QSizeF(100, 40)), Qt::AlignCenter, "发送时间间隔");
    painter.drawText(QRectF(QPointF(720, 40), QSizeF(40, 40)), Qt::AlignCenter, "Min");
    painter.drawText(QRectF(QPointF(720, 67), QSizeF(40, 40)), Qt::AlignCenter, "Max");
}

MainWindow::~MainWindow()
{
    delete serial;
    delete ui;      //删除ui
}

/*
    槽函数func_start
    功能：提示用户是否开始采集
            是：打开串口，开始显示
            否：do nothing
    输入：无
    输出：无
*/
void MainWindow::func_start(){
    switch(QMessageBox::question(this, "提示", "确认开始采集？", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes))
    {
    case QMessageBox::Yes:
        if(Started == 1);//do nothing
        else
        {
            int match = 0;
            //设置串口信息
            foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()){
                if(QString::localeAwareCompare(info.portName(),ui->lineEdit->text())==0){
                    delete serial;
                    serial = new QSerialPort();              //初始化串口（USB）
                    //设置串口名字
                    serial->setPortName(info.portName()); //端口名

                    match = 1;
                }
            }
            if(!match)QMessageBox::warning(this,"warning","请输入正确端口");
            else
            {
                if(!serial->open(QIODevice::ReadWrite))
                //用ReadWrite 的模式尝试打开串口
                {
                        //qDebug()<<ui->lineEdit->text()<<"打开失败!";
                        QMessageBox::warning(this,"Error","打开失败");
                        return;
                }
                serial->setDataTerminalReady(true);                 //为了能进入槽函数
                connect(this->serial,SIGNAL(readyRead()),this,SLOT(storeMessage()));
                serial->setBaudRate(BaudRate);                      //设置波特率
                serial->setDataBits(databit);                       //设置数据位数
                serial->setParity(parityset);                       //设置奇偶校验
                serial->setStopBits(stopbit);                       //设置停止位
                serial->setFlowControl(QSerialPort::NoFlowControl); //设置流控制

                //为坐标轴改范围
                double max_num = ui->maximum->text().toDouble();
                double min_num = ui->minimum->text().toDouble();
                interval = ui->time_interval->text().toDouble();
                axisX->setRange(0-TIME_END*interval, TIME_START);
                axisY->setRange(min_num, max_num);

                Started = 1;
                Getting = new data_thread(this);//创建采集线程
                Getting->start();
            }
        }
        break;
    case QMessageBox::No:
        //do nothing
        break;
    default:
        break;
    }
}

/*
    槽函数func_shut
    功能：提示用户是否停止采集
            是：关闭数码管进程
               关闭曲线线程
            否：do nothing
    输入：无
    输出：无
*/
void MainWindow::func_shut(){
    switch(QMessageBox::question(this, "提示", "确认停止采集？", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes))
    {
    case QMessageBox::Yes:
        if(Started == 0);//do nothing
        else {
            //关闭串口
            //serial->close();
            Started  = 0;
            Getting->timer->stop();
            Getting->quit();//终止线程
            while(!Getting->wait());//等待线程退出
            delete Getting;//删除采集线程
            Getting = NULL;

            serial->close();
        }
        break;
    case QMessageBox::No:
        //do nothing
        break;
    default:
        break;
    }
}

/*
    槽函数BaudRate_select
    功能：根据ComboBox改变串口波特率
    输入：无
    输出：无
*/
void MainWindow::BaudRate_select(){
    switch(ui->comboBox->currentIndex())
    {
    case 0:
        BaudRate = 115200;break;
    case 1:
        BaudRate = 57600;break;
    case 2:
        BaudRate = 56000;break;
    case 3:
        BaudRate = 43000;break;
    case 4:
        BaudRate = 38400;break;
    case 5:
        BaudRate = 19200;break;
    case 6:
        BaudRate = 9600;break;
    case 7:
        BaudRate = 4800;break;
    case 8:
        BaudRate = 2400;break;
    case 9:
        BaudRate = 1200;break;
    case 10:
        BaudRate = 600;break;
    case 11:
        BaudRate = 300;break;
    default:
        break;
    }
    //qDebug()<<"BaudRate:"<<BaudRate<<endl;//调试
}

/*
    槽函数data_select
    功能：根据ComboBox改变数据位位数
    输入：无
    输出：无
*/
void MainWindow::data_select(){
    switch(ui->comboBox_2->currentIndex())
    {
    case 0:
        databit = QSerialPort::Data8;break;
    case 1:
        databit = QSerialPort::Data5;break;
    case 2:
        databit = QSerialPort::Data6;break;
    case 3:
        databit = QSerialPort::Data7;break;
    default:
        break;
    }
    //qDebug()<<"BaudRate:"<<BaudRate<<endl;//调试
}

/*
    槽函数stop_select
    功能：根据ComboBox改变停止位位数
    输入：无
    输出：无
*/
void MainWindow::stop_select(){
    switch(ui->comboBox_3->currentIndex())
    {
    case 0:
        stopbit = QSerialPort::OneStop;break;
    case 1:
        stopbit = QSerialPort::OneAndHalfStop;break;
    case 2:
        stopbit = QSerialPort::TwoStop;break;
    default:
        break;
    }
    //qDebug()<<"BaudRate:"<<BaudRate<<endl;//调试
}

/*
    槽函数odd_select
    功能：根据ComboBox改变奇偶校验位选项
    输入：无
    输出：无
*/
void MainWindow::odd_select(){
    switch(ui->comboBox_4->currentIndex())
    {
    case 0:
        parityset = QSerialPort::NoParity;break;
    case 1:
        parityset = QSerialPort::EvenParity;break;
    case 2:
        parityset = QSerialPort::OddParity;break;
    default:
        break;
    }
    //qDebug()<<"BaudRate:"<<BaudRate<<endl;//调试
}

/*
    槽函数show_port
    功能：在采集阶段从串口获取数据
    输入：无
    输出：无
*/
void MainWindow::show_port(){
    //qDebug()<<"刷新端口"<<endl;//调试

    //容器
    QStringList port;

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        //qDebug()<<info.portName()<<endl;//调试
        //检测各个串口
        port<<info.portName()<<"\n";//测试
    }

    QStringListModel *model = new QStringListModel(port);
    ui->listView_2->setModel(model);
}

/*
    槽函数ClearReceiver
    功能：清空接受窗口
    输入：无
    输出：无
*/
void MainWindow::Clear_Receiver(){
    model_text->removeRows(0,model_text->rowCount());//清楚model
    text_content.clear(); //清除字节流
}

/*
    槽函数SendCommand
    功能：发送命令
    输入：无
    输出：无
*/
void MainWindow::Send_Command(){
    int ready = 0;//标志位，标志端口名是否有效
    int need_delete = 0;//标志位，判断发送完是否应该删除对象

    //如果此刻没有串口打开，就重新定义一遍
    if(!serial->isOpen()){

        //设置串口信息
        foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()){
            if(QString::localeAwareCompare(info.portName(),ui->lineEdit->text())==0){
                delete serial;
                serial = new QSerialPort();              //初始化串口（USB）
                //设置串口名字
                serial->setPortName(info.portName()); //端口名
                ready = 1;
            }
        }
        if(ready){
            serial->setBaudRate(BaudRate);                      //设置波特率
            serial->setDataBits(databit);                       //设置数据位数
            serial->setParity(parityset);                       //设置奇偶校验
            serial->setStopBits(stopbit);                       //设置停止位
            serial->setFlowControl(QSerialPort::NoFlowControl); //设置流控制
            need_delete = 1;

            if(!serial->open(QIODevice::ReadWrite))
            //用ReadWrite 的模式尝试打开串口
            {
                    //qDebug()<<ui->lineEdit->text()<<"打开失败!";
                    QMessageBox::warning(this,"Error","打开失败");
                    return;
            }

        }
        else
        {
            QMessageBox::warning(this,"warning","请输入正确端口");
            return;
        }
    }
    serial->write(ui->Command->text().toStdString().data());
    if(need_delete)serial->close();
}

/*
    槽函数SaveChart
    功能：保存曲线为图片,可以让用户选择路径
    输入：无
    输出：无
*/
/*
void MainWindow::save_chart(){
    QPixmap save_P = QPixmap::grabWidget(drawview);
    QString save_fileName;
    save_fileName = QFileDialog::getSaveFileName(this, QString::fromLocal8Bit("Save"),QDir::currentPath()+ "/untitled.png",tr("Images (*.png *.xpm *.jpg)"));
    save_P.save(save_fileName);
}
*/

/*
    槽函数clear_image
    功能：保存曲线为图片,可以让用户选择路径
    输入：无
    输出：无
*/
void MainWindow::clear_image(){
    if(Started == 0){
        delete series;
        series = new QLineSeries;
        serial->clear();
        drawing->addSeries(series);
        buffer_double.clear();
        buffer_int.clear();
        buffer_float.clear();
        buffer_index = -1;
    }
    else del = 1;
}

/*
    槽函数SaveChartDefault
    功能：保存曲线到默认路径
    输入：无
    输出：无
*/
void MainWindow::save_chart_default(){
    QPixmap save_P = QPixmap::grabWidget(drawview);
    QString save_fileName;
    //qDebug()<<QDir::currentPath()<<endl;
    //文件以时间命名
    QDateTime current_date_time =QDateTime::currentDateTime();
    QString current_date =current_date_time.toString("yyyy-MM.dd-hh:mm:ss:zzz");
    save_fileName = "Image/"+current_date+".png";//默认路径还可以再修改

    //创建保存文件夹
    QDir *Text = new QDir;
    bool exist = Text->exists("Image");
    if(!exist)
        bool ok = Text->mkdir("Image");

    save_P.save(save_fileName);
}

/*
    槽函数SaveTxtDefault
    功能：保存文本到默认路径
    输入：无
    输出：无
*/
void MainWindow::save_txt_default(){
    QDateTime current_date_time =QDateTime::currentDateTime();
    QString current_date =current_date_time.toString("yyyy-MM.dd-hh:mm:ss:zzz");
    QString save_fileName = "Text/"+current_date+".txt";//默认路径还可以再修改

    //创建保存文件夹
    QDir *Text = new QDir;
    bool exist = Text->exists("Text");
    if(!exist)
        bool ok = Text->mkdir("Text");

    //对文件进行操作
    QFile file(save_fileName);
    if(!file.open(QIODevice::WriteOnly  | QIODevice::Text|QIODevice::Append))
    {
       QMessageBox::warning(this,"Error","打开失败",QMessageBox::Yes);
       return;
    }
    QTextStream in(&file);

    switch(ui->data_classset->currentIndex())
    {
    case 0:
        for (int i = buffer_index; i >= TIME_START; i--)
            in<<buffer_double[i]<<"\n";
        break;
    case 1:
        for (int i = buffer_index; i >= TIME_START; i--)
            in<<buffer_int[i]<<"\n";
        break;
    case 2:
        for (int i = buffer_index; i >= TIME_START; i--)
            in<<buffer_float[i]<<"\n";
        break;
    default:
        break;
    }

    file.close();
}

/*
    槽函数dataORtext
    功能：选择数据的显示形式，若文本，则不显示曲线
    输入：无
    输出：无
*/
void MainWindow::dataORtext(){
    switch(BtnGroup->checkedId())
   {
    case 0:
       TextOrData = IsData;
       break;
    case 1:
       TextOrData = IsText;
       break;
    default:
       break;
   }
}

/*
    storeMessage
    功能：触发事件：得到每次串口发送进来的数据
    输入：无
    输出：无
*/
void MainWindow::storeMessage(){
    QByteArray text = serial->readAll();

    if(TextOrData == IsText){   //采集到的是文本时
        //先保证不要溢出
        if(text_content.size() >= 1500)
            for (int i=0;i<500;i++)
                text_content.removeFirst();
        text_content<<text;
        QStringListModel *model_temp = model_text;
        model_text = new QStringListModel(text_content);
        delete model_temp;      //删除上一次的model
        ui->listView->setModel(model_text);
        ui->listView->scrollToBottom();
    }
    else {                      //采集到的是数据时
        //将数据存在缓冲区的操作
        switch(ui->data_classset->currentIndex())
        {
        case 0:
            if(del){
                series->clear();
                buffer_double.clear();
                buffer_index = 0;
                del = 0;
            }
            else data_to_storage<double>(text,buffer_double);
            break;
        case 1:
            if(del){
                series->clear();
                buffer_int.clear();
                buffer_index = 0;
                del = 0;
            }
            else data_to_storage<int>(text,buffer_int);
            break;
        case 2:
            if(del){
                series->clear();
                buffer_float.clear();
                buffer_index = 0;
                del = 0;
            }
            else data_to_storage<float>(text,buffer_float);
            break;
        default:
            break;
        }

        //清楚曲线操作

        //将数据直接显示的操作
        //先保证不要溢出
        if(text_content.size() >= 1500){
            for (int i=0;i<500;i++)
                text_content.removeFirst();
        }
        //qDebug()<<text_content.size();
        QDateTime current_date_time =QDateTime::currentDateTime();
        QString current_date =current_date_time.toString("MM.dd-hh:mm:ss:zzz");
        text_content<<current_date<<text;
        QStringListModel *model_temp = model_text;
        model_text = new QStringListModel(text_content);
        delete model_temp;      //删除上一次的model
        ui->listView->setModel(model_text);
        ui->listView->scrollToBottom();
    }
    //qDebug()<<text;//调试
}

/*
    线程事件循环
    画图函数，通过从缓冲区获取数据来更新曲线
    更新事件通过TIME_CYC宏控制
*/
void MainWindow::Get_data(){
    if(TextOrData == IsText){
        series->clear();
        return;
    }
    delete series;
    series = new QLineSeries;
    series->setPen(QPen(Qt::red, 1, Qt::SolidLine));  // 设置折线显示效果
    switch(ui->data_classset->currentIndex())         //让最右边是当前数值
    {
    case 0:
        for (int i = TIME_START; i < buffer_index; i++)
            series->append(TIME_START-i*interval, buffer_double[i]);
        break;
    case 1:
        for (int i = TIME_START; i < buffer_index; i++)
            series->append(TIME_START-i*interval, buffer_int[i]);
        break;
    case 2:
        for (int i = TIME_START; i < buffer_index; i++)
            series->append(TIME_START-i*interval, buffer_float[i]);
        break;
    default:
        break;
    }

    drawing->addSeries(series);                             //输入数据
    drawing->setAxisX(axisX,series);
    drawing->setAxisY(axisY,series);
}
