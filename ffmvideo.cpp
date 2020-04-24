#include "ffmvideo.h"
#include "ryprint.h"
#include "rymacros.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <time.h>
#else
    #include <sys/time.h>
#endif

unsigned long long GetCurrentTimeMsec()
{
#ifdef _WIN32
        struct timeval tv;
        time_t clock;
        struct tm tm;
        SYSTEMTIME wtm;

        GetLocalTime(&wtm);
        tm.tm_year = wtm.wYear - 1900;
        tm.tm_mon = wtm.wMonth - 1;
        tm.tm_mday = wtm.wDay;
        tm.tm_hour = wtm.wHour;
        tm.tm_min = wtm.wMinute;
        tm.tm_sec = wtm.wSecond;
        tm.tm_isdst = -1;
        clock = mktime(&tm);
        tv.tv_sec = clock;
        tv.tv_usec = wtm.wMilliseconds * 1000;
        return ((unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#else
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return ((unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#endif
}

ffmvideo::ffmvideo(const char *in)
{
    strncpy(url, in, sizeof (url));
    // 简单判断是不是文件流
    if(strstr(url, ":"))
    {
        input_type = 1;
    }
}

ffmvideo::~ffmvideo()
{
    stop();
    ryDbg("~ffmvideo exit.\n");
}

int ffmvideo::init()
{

    if (true == isVideoInit) return 0;

    int ret = -1;
    AVCodecParameters * avcodecParameters = NULL;
    AVCodec * codec = NULL;
    enum AVPixelFormat srcVideoFormat = AV_PIX_FMT_NONE;

    AVDictionary *options = NULL;
    //av_dict_set(&options, "rtsp_transport", "tcp", 0);

    //分配一个AVFormatContext，FFMPEG所有的操作都要通过这个AVFormatContext来进行

    pFormatCtx = avformat_alloc_context();
    assert_param_do(pFormatCtx, goto err0);

    pFormatCtx->interrupt_callback.opaque = &cb_arg;
    pFormatCtx->interrupt_callback.callback = [](void *cb_arg)->int{
        qrcb_arg *arg = (qrcb_arg *)cb_arg;
        if (arg->lasttime > 0) {
            if (time(NULL) - arg->lasttime > 1) {
                // 等待超过1s则中断
                ryErr("timeout.");
                return 1;   //只要返回非0值，就会退出阻塞
            }
        }
        return 0;
    };


    //打开file or url
    cb_arg.lasttime = time(NULL);
    ret = avformat_open_input(&pFormatCtx, url, NULL, &options);
    if (0 != ret)
    {
        char tmp[256];
        av_strerror(ret, tmp, 256);
        ryErr("avformat_open_input %s failed %d: %s\n", url, ret, tmp);
        goto err1;
    }
    cb_arg.lasttime = 0;

    ryErr("avformat_open_input %s %s %s %s\n", pFormatCtx->iformat->name,pFormatCtx->iformat->long_name,pFormatCtx->iformat->mime_type,pFormatCtx->iformat->extensions);


    //先从流中读取一部分包，以获得流媒体的格式，否则下面可能会出现获取视频宽度高度为0的情况(rtsp流情况下)，因为avformat_open_input函数只能解析出一些基本的码流信息
    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (0 != ret)
    {
        ryErr("Couldn't find stream information.\n");
        goto err2;
    }

    //打印视频信息 在win10 QT里面打印输出异常，程序结束后才打印出来
    av_dump_format(pFormatCtx, 0, url, 0);

    //循环查找媒体流中包含的流信息，直到找到视频类型的流,返回其index
    videoStreamIdx = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoStreamIdx < 0)
    {
        ryErr("Couldn't find video stream.\n");
        goto err2;
    }

    //根据视频流索引，获取解码器参数
    avcodecParameters = pFormatCtx->streams[videoStreamIdx]->codecpar;
    //根据解码器ID，然后查找解码器
    codec = avcodec_find_decoder(avcodecParameters->codec_id);
    if (NULL == codec)
    {
        ryErr("Couldn't find decoder.\n");
        goto err2;
    }

    //创建解码器上下文
    pVDecCtx = avcodec_alloc_context3(NULL);
    if(NULL == pVDecCtx)
    {
        ryErr("create condectext failed.\n");
        goto err2;
    }

    // 把解码参数复制到上下文
    ret = avcodec_parameters_to_context(pVDecCtx, avcodecParameters);
    if(ret < 0)
    {
        ryErr("avcodec_parameters_to_context failed.\n");
        goto err3;
    }

    //根据上下文打开解码器
    ret = avcodec_open2(pVDecCtx,codec,NULL);
    if(0 != ret)
    {
        ryErr("Could not open codec.\n");
        goto err3;
    }

    // 至此可以看出  我们可以直接根据查找到的视频流信息获取到解码器。 而且我们并不知道他实际用的是什么编码器。

    /// AVPacket用于存储压缩的数据
    /// AVFrame 用于存储解码后的数据
    /// av_read_frame得到压缩的数据包AVPacket，一般有三种压缩的数据包(视频、音频和字幕)，然后调用avcodec_decode_video2对AVPacket进行解码得到AVFrame。
    //根据视频的大小创建AVPacket并分配空间

    ryDbg("width %d height %d.\n", pVDecCtx->width, pVDecCtx->height);

    // 为YUV420和RGB24分配内存
    pFrameYUV420 = av_frame_alloc();
    if(NULL == pFrameYUV420)
    {
        ryErr("pFrameYUV420 alloc failed.\n");
        goto err4;
    }

    pFrameRGB24 = av_frame_alloc();
    if(NULL == pFrameRGB24)
    {
        ryErr("pFrameRGB24 alloc failed.\n");
        goto err5;
    }

    // 为pRGB24Buffer分配内存
    pRGB24Buffer = (uint8_t *) av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pVDecCtx->width, pVDecCtx->height, 1) * sizeof(uint8_t));


    if(NULL == pRGB24Buffer)
    {
        ryErr("pRGB24Buffer alloc failed.\n");
        goto err6;
    }

    // avpicture_fill会将pFrameRGB24的数据按RGB24格式自动"关联"到pRGB24Buffer。

    av_image_fill_arrays(pFrameRGB24->data, pFrameRGB24->linesize,
                             pRGB24Buffer, AV_PIX_FMT_RGB24,
                             pVDecCtx->width, pVDecCtx->height, 1);

    //创建视频格式转换器 用于把解码出来的数据从YUV转换成RGB格式，这样才能在QT上显示出来
    srcVideoFormat = pVDecCtx->pix_fmt < 0 ? AV_PIX_FMT_YUV420P : pVDecCtx->pix_fmt;
    pImgConvertCtx = sws_getContext(avcodecParameters->width, avcodecParameters->height, srcVideoFormat,
                                     avcodecParameters->width, avcodecParameters->height, AV_PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    if(NULL == pImgConvertCtx)
    {
        ryErr("pRGB24Buffer alloc failed.\n");
        goto err7;
    }

    isVideoInit = true;

    return 0;

    //err8:
        sws_freeContext(pImgConvertCtx);
    err7:
        av_free(pRGB24Buffer);
    err6:
        av_frame_free(&pFrameRGB24);
    err5:
        av_frame_free(&pFrameYUV420);
    err4:
        avcodec_close(pVDecCtx);
    err3:
        avcodec_free_context(&pVDecCtx);
    err2:
        avformat_close_input(&pFormatCtx);
    err1:
        avformat_free_context(pFormatCtx);
    err0:

        return 0 == ret ? -1 : ret;
}

void ffmvideo::exit()
{
    if (isVideoInit)
    {
        sws_freeContext(pImgConvertCtx);
        av_free(pRGB24Buffer);
        av_free(pFrameRGB24);
        av_free(pFrameYUV420);
        avcodec_close(pVDecCtx);
        avcodec_free_context(&pVDecCtx);
        avformat_close_input(&pFormatCtx);
        avformat_free_context(pFormatCtx);
        isVideoInit = false;
        ryErr("exit\n");
    }
}

void ffmvideo::pause()
{
    runFlag = false;
    if (runFlag || playThread)
    {
        runFlag = false;
        if(playThread->joinable())
        {
            playThread->join();
            playThread = nullptr;
        }
    }
}

int ffmvideo::read_nalu()
{

    assert_param_return(0 == init(), NULL);

    do{
        // 使用FFmpeg要注意内存泄漏的问题，av_read_frame中会申请内存，需要在外面进行释放
        // 所以每读完一个packet，需要调用av_packet_unref进行内存释放。
        if (av_read_frame(pFormatCtx, &packet) < 0)  //读取的是一帧视频，并存入一个AVPacket的结构中
        {
            ryDbg("file end.\n");
            return -1; //这里认为视频读取完了
        }

        if (packet.stream_index != videoStreamIdx)
        {
            av_packet_unref(&packet);//不为视频时释放pkt
            continue;
        }

    } while(packet.stream_index != videoStreamIdx);

    return 0;
}

int ffmvideo::frame(ffmvideo::frame_handle_func cb)
{
    int ret = -1;

    pause();

    ret = init();
    assert_param_do(0 == ret, goto err0);

    //即使流结束read不到nalu了，也要建议发送几个NULL过去，这样可以在avcodec_receive_frame把解码器上面的东西榨干！
    ret = avcodec_send_packet(pVDecCtx, read_nalu() >= 0 ? & packet : NULL);
    assert_param_do(0 == ret, goto err0);

    ret = avcodec_receive_frame(pVDecCtx, pFrameYUV420);
    assert_param_do(0 == ret, goto err1);

    ret = sws_scale(pImgConvertCtx,
                    (uint8_t const * const *)pFrameYUV420->data, pFrameYUV420->linesize,
                    0, pFrameYUV420->height,
                    pFrameRGB24->data, pFrameRGB24->linesize);
    assert_param_do(ret > 0, goto err1);

    cb(pFrameYUV420, packet.rts, pRGB24Buffer, false);

err1:
    av_packet_unref(&packet);
err0:
    return ret;
}

int ffmvideo::play(frame_handle_func cb)
{
    assert_param_return(0 == init(), -1);

    runFlag = true;

    playThread = std::make_shared<std::thread>([this, cb]() {

        int seq = 0;
        bool stop_flag = false;
        uint64_t lasttime;
        int duration;

        while (runFlag)
        {
            lasttime = GetCurrentTimeMsec();

            //即使流结束read不到nalu了，也要建议发送几个NULL过去，这样可以在avcodec_receive_frame把解码器上面的东西榨干！

            if(avcodec_send_packet(pVDecCtx, read_nalu() >= 0 ? & packet : NULL))
            {
                stop_flag = true;
                break;
            }

            //ryDbg("avcodec_send_packet seq:%d.", seq++);

            while (0 == avcodec_receive_frame(pVDecCtx, pFrameYUV420))
            {
                sws_scale(pImgConvertCtx,
                              (uint8_t const * const *)pFrameYUV420->data, pFrameYUV420->linesize,
                              0, pFrameYUV420->height,
                              pFrameRGB24->data, pFrameRGB24->linesize);

                if (input_type <= 0)
                {
                    duration = packet.duration * av_q2d(pFormatCtx->streams[videoStreamIdx]->time_base) * 1000;
                    duration = duration - (GetCurrentTimeMsec() - lasttime);
                    duration = duration > 0 ? duration : 0;
                    //ryDbg("duration %d  .", duration);
                    IF_TRUE_DO(duration > 0, std::this_thread::sleep_for(std::chrono::milliseconds(duration)));
                }
                cb(pFrameYUV420, packet.rts, pRGB24Buffer, stop_flag);
            }

            av_packet_unref(&packet);
        }

        cb(NULL, 0, NULL, stop_flag);
        ryDbg("playThread exit.\n");
    });

    return 0;
}

void ffmvideo::stop()
{
    pause();
    exit();
    return;
}


int ffmvideo::status()
{
    return runFlag;
}

