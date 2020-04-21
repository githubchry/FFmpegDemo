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
    #include <libavutil/imgutils.h>

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
    int video_init(const char *url);
    void video_deinit();

    int video_frame_get(AVFormatContext *s, AVPacket *pkt, int idx);
    int video_frame_decode(AVCodecContext *avctx, AVFrame *yuv, const AVPacket *pkt);
    int video_frame_to_rgb24(SwsContext *swsctx, AVFrame *yuv, AVFrame *rgb24);
    int video_frame_show(uint8_t *pRGB24Buffer, int width, int height);
    int video_frame_play();

private slots:
    int on_pbNextFrame_clicked();

    void on_rbFile_clicked();

    void on_rbUrl_clicked();

    void on_pbStop_clicked();

    void on_pbPalyOrPause_clicked();

    void on_playEnded();

signals:
    void playEnded();

private:
    Ui::MainWindow *ui;

    int videoStreamIdx = -1;            // 视频流索引
    bool isVideoInit = false;

    AVPacket packet;  // 用于存储压缩的数据 h264
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pVDecCtx    = nullptr;  // 视频解码器
    AVFrame *pFrameYUV420       = nullptr;  // 用于存储解码后的yuv420格式数据
    AVFrame *pFrameRGB24        = nullptr;  // 用于存储RGB24格式数据
    SwsContext *pImgConvertCtx  = nullptr;  // 图片格式转换器 yuv420 => RGB24
    uint8_t *pRGB24Buffer       = nullptr;

    std::shared_ptr<std::thread> playThread = nullptr;
    std::atomic<bool> runFlag{false};
};
#endif // MAINWINDOW_H
