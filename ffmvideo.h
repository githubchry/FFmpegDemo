#ifndef FFMVIDEO_H
#define FFMVIDEO_H

#include <thread>
#include <atomic>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}


class ffmvideo
{
private:
    int init();
    void exit();

    typedef struct {
        time_t lasttime;
    } qrcb_arg;
    // 回调函数
    typedef void (*frame_handle_func)(AVFrame *yuv, uint64_t rts, uint8_t *rgb, bool stop_flag);

    int read_nalu();
    /// init -> read_nalu -> decode -> read_frame

public:
    ffmvideo(const char *url,int outw = 0, int outh = 0);
    ~ffmvideo();

    int frame(frame_handle_func cb);
    int play(frame_handle_func cb);
    void pause();
    void stop();
    int status();

private:
    char url[256];
    int input_type = -1;     //-1未知 0文件流 1网络流
    int videoStreamIdx = -1;            // 视频流索引
    int outWidth = 0;
    int outHeight = 0;
    std::atomic<bool> isVideoInit{false};

    AVPacket packet;  // 用于存储压缩的数据 h264
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pVDecCtx    = nullptr;  // 视频解码器
    AVFrame *pFrameYUV420       = nullptr;  // 用于存储解码后的yuv420格式数据
    AVFrame *pFrameRGB24        = nullptr;  // 用于存储RGB24格式数据
    SwsContext *pImgConvertCtx  = nullptr;  // 图片格式转换器 yuv420 => RGB24
    uint8_t *pRGB24Buffer       = nullptr;

    std::shared_ptr<std::thread> playThread = nullptr;
    std::atomic<bool> runFlag{false};


    qrcb_arg cb_arg = { 0 };
};

#endif // FFMVIDEO_H
