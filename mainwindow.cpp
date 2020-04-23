#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QDateTime>
// 回调函数

MainWindow* MainWindow::s_this = nullptr;

static qrcb_arg cb_arg = { 0 };

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->rbFile->setChecked(true);
    ui->edInput->setText("../chrome.mp4");

    ui->lbImage->clear();
    QPalette palette;
    palette.setColor(QPalette::Background, QColor(0, 0, 0));
    ui->lbImage->setAutoFillBackground(true);  //一定要这句，否则不行
    ui->lbImage->setPalette(palette);

    s_this = this;
    connect(this, &MainWindow::playEnded, this, &MainWindow::on_playEnded);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_playEnded()
{
    qDebug("on_playEnded\n");
    video = NULL;
    ui->pbPalyOrPause->setText("播放");
}

void MainWindow::on_pbStop_clicked()
{
    on_playEnded();
}


void MainWindow::on_rbFile_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件");
    ui->edInput->setText(fileName);
}

void MainWindow::on_rbUrl_clicked()
{
    //ui->edInput->setText("rtsp://192.168.2.129:8554/slamtv60.264");
    //ui->edInput->setText("rtsp://192.168.2.129:8554/fsly");
    //ui->edInput->setText("rtsp://10.11.2.75/slamtv60.264");
    //ui->edInput->setText("rtsp://10.11.2.8:8554/fsly");
    ui->edInput->setText("rtsp://10.6.1.59/slamtv60.264");


    //FFmpeg丢包花屏处理 https://www.jianshu.com/p/f3d2569d0d82

}

int MainWindow::on_pbNextFrame_clicked()
{

    return 0;
}

void MainWindow::frame_handle_func(AVFrame *yuv, uint64_t rts, uint8_t *rgb)
{
    if (yuv)
    {
        qDebug("frame_handle_func rts %ld, %d x %d\n", rts, yuv->width, yuv->height);
        
        s_this->ui->lbTimestramp->setText(QString::number(rts));
        //把这个RGB数据 用QImage加载
        QImage tmpImg(rgb, yuv->width, yuv->height, QImage::Format_RGB888);
        s_this->ui->lbImage->setPixmap(QPixmap::fromImage(tmpImg));
    }
    else
    {
        emit s_this->playEnded();
    }
}

void MainWindow::on_pbPalyOrPause_clicked()
{
    if (NULL == video)

        video = std::make_shared<ffmvideo>(ui->edInput->text().toStdString().c_str());

    if (false == video->status())
    {
        //建立播放线程
        video->play(frame_handle_func);
        ui->pbPalyOrPause->setText("暂停");
    }
    else
    {
        //停止播放线程
        video->pause();
        ui->pbPalyOrPause->setText("播放");
    }

    return;
}


