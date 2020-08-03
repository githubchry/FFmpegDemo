#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QDateTime>
// 回调函数

MainWindow* MainWindow::s_this = nullptr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->rbFile->setChecked(true);
    ui->edInput->setText("../1080P.mp4");

    ui->lbImage->clear();
    QPalette palette;
    palette.setColor(QPalette::Background, QColor(0, 0, 0));
    ui->lbImage->setAutoFillBackground(true);  //一定要这句，否则不行
    ui->lbImage->setPalette(palette);
    ui->lbImage->setAlignment(Qt::AlignCenter); //居中
    s_this = this;
    connect(this, &MainWindow::playEnded, this, &MainWindow::on_playEnded);
}

MainWindow::~MainWindow()
{
    video = nullptr;
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

void MainWindow::frame_handle_func(AVFrame *yuv, uint64_t rts, uint8_t *rgb, bool stop_flag)
{
    if (yuv)
    {
        //qDebug("frame_handle_func rts %lu, %d x %d, stop_flag:%d\n", rts, yuv->width, yuv->height, stop_flag);

        s_this->ui->lbTimestramp->setText(QString::number(rts));
        //把这个RGB数据 用QImage加载
        QImage tmpImg(rgb, yuv->width, yuv->height, QImage::Format_RGB888);
        //scaled:缩放显示
        s_this->ui->lbImage->setPixmap(QPixmap::fromImage(tmpImg).scaled(480, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else if (stop_flag)
    {
        emit s_this->playEnded();
    }
}

int MainWindow::on_pbNextFrame_clicked()
{
    if (NULL == video)
        video = std::make_shared<ffmvideo>(ui->edInput->text().toStdString().c_str());

    video->frame(frame_handle_func);
    ui->pbPalyOrPause->setText("播放");
    return 0;
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


