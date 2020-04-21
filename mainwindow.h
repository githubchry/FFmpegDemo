#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <thread>
#include <atomic>

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

private:
    int VideoInit(const char *url);
    void VideoDeinit();
private slots:
    void on_pbNextFrame_clicked();

    void on_rbFile_clicked();

    void on_rbUrl_clicked();

    void on_pbInitFFmpeg_clicked();

    void on_pbPlay_clicked();

private:
    Ui::MainWindow *ui;
    AVFormatContext *pFormatCtx = nullptr;
    AVPacket *packet = nullptr;     // 用于存储压缩的数据 h264
    AVCodecContext *pCodecCtx = nullptr;

    int videoStreamIdx = -1;

    AVFrame *pFrameYUV420 = nullptr;    // 用于存储解码后的数据 yuv420
    AVFrame *pFrameRGB24 = nullptr;
    SwsContext *img_convert_ctx = nullptr;
    uint8_t *pRGB24Buffer = nullptr;
    bool isVideoInit = false;

    std::shared_ptr<std::thread> playThread = nullptr;
    std::atomic<bool> runFlag{false};
};
#endif // MAINWINDOW_H
