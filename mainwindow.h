#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pbNextFrame_clicked();

private:
    Ui::MainWindow *ui;
    AVFormatContext *pFormatCtx = nullptr;
    AVPacket *packet = nullptr;     // 用于存储压缩的数据
    AVCodecContext *pCodecCtx = nullptr;

    int videoStreamIdx = -1;

    AVFrame *pFrameYUV420 = nullptr;    // 用于存储解码后的数据
    AVFrame *pFrameRGB24 = nullptr;
    SwsContext *img_convert_ctx = nullptr;
    uint8_t *pRGB24Buffer = nullptr;
};
#endif // MAINWINDOW_H
