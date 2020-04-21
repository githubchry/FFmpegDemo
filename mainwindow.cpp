#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>

// 回调函数
typedef struct {
    time_t lasttime;
} qrcb_arg;

static qrcb_arg cb_arg = { 0 };

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->rbFile->setChecked(true);
    ui->edInput->setText("../chrome.mp4");

    //分配一个AVFormatContext，FFMPEG所有的操作都要通过这个AVFormatContext来进行
    pFormatCtx = avformat_alloc_context();

    pFormatCtx->interrupt_callback.opaque = &cb_arg;
    pFormatCtx->interrupt_callback.callback = [](void *cb_arg)->int{
        qrcb_arg *arg = (qrcb_arg *)cb_arg;
        if (arg->lasttime > 0) {
            if (time(NULL) - arg->lasttime > 1) {
                // 等待超过1s则中断
                qDebug("timeout.");
                return 1;   //只要返回非0值，就会退出阻塞
            }
        }
        return 0;
    };
}

MainWindow::~MainWindow()
{
    VideoDeinit();
    avformat_free_context(pFormatCtx);

    if(playThread->joinable())
    {
        playThread->join();
        playThread = nullptr;
    }


    delete ui;
}

int MainWindow::VideoInit(const char *url)
{
    if (true == isVideoInit) return 0;

    int ret = -1;
    AVCodecParameters * avcodecParameters = NULL;
    AVCodec * codec = NULL;
    enum AVPixelFormat srcVideoFormat = AV_PIX_FMT_NONE;
    //打开file or url

    cb_arg.lasttime = time(NULL);
    ret = avformat_open_input(&pFormatCtx, url, NULL, NULL);
    if (0 != ret)
    {
        char tmp[256];
        av_strerror(ret, tmp, 256);
        qDebug("avformat_open_input %s failed %d: %s\n", ui->edInput->text().toStdString().c_str(), ret, tmp);
        goto err0;
    }
    cb_arg.lasttime = 0;

    //先从流中读取一部分包，以获得流媒体的格式，否则下面可能会出现获取视频宽度高度为0的情况(rtsp流情况下)，因为avformat_open_input函数只能解析出一些基本的码流信息
    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (0 != ret)
    {
        qDebug("Couldn't find stream information.\n");
        goto err1;
    }

    //打印视频信息 在win10 QT里面打印输出异常，程序结束后才打印出来
    av_dump_format(pFormatCtx, 0, ui->edInput->text().toStdString().c_str(), 0);

    //循环查找媒体流中包含的流信息，直到找到视频类型的流,返回其index
    videoStreamIdx = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoStreamIdx < 0)
    {
        qDebug("Couldn't find video stream.\n");
        goto err1;
    }

    //根据视频流索引，获取解码器参数
    avcodecParameters = pFormatCtx->streams[videoStreamIdx]->codecpar;
    //根据解码器ID，然后查找解码器
    codec = avcodec_find_decoder(avcodecParameters->codec_id);

    //创建解码器上下文
    pCodecCtx = avcodec_alloc_context3(NULL);
    if(pCodecCtx == NULL)
    {
        qDebug("create condectext failed.\n");
        goto err1;
    }

    // 把解码参数复制到上下文
    ret = avcodec_parameters_to_context(pCodecCtx, avcodecParameters);
    if(ret < 0)
    {
        qDebug("avcodec_parameters_to_context failed.\n");
        goto err2;
    }

    //根据上下文打开解码器
    ret = avcodec_open2(pCodecCtx,codec,NULL);
    if(0 != ret)
    {
        qDebug("Could not open codec.\n");
        goto err2;
    }

    // 至此可以看出  我们可以直接根据查找到的视频流信息获取到解码器。 而且我们并不知道他实际用的是什么编码器。

    /// AVPacket用于存储压缩的数据
    /// AVFrame 用于存储解码后的数据
    /// av_read_frame得到压缩的数据包AVPacket，一般有三种压缩的数据包(视频、音频和字幕)，然后调用avcodec_decode_video2对AVPacket进行解码得到AVFrame。
    //根据视频的大小创建AVPacket并分配空间

    packet = (AVPacket *) malloc(sizeof(AVPacket));
    if(packet == NULL)
    {
        qDebug("Could not malloc packet.\n");
        goto err3;
    }

    ret = av_new_packet(packet, pCodecCtx->width * pCodecCtx->height);
    if(0 != ret)
    {
        qDebug("av_new_packet failed.\n");
        goto err4;
    }


    qDebug("width %d height %d.\n", pCodecCtx->width, pCodecCtx->height);

    /// 要想在QT显示一帧图像，
    /// 1.从文件获取一帧h264数据 - AVPacket
    /// 2.使用ffmpeg解码成YUV420数据 - AVFrame
    /// 3.将YUV420数据转成RGB24 - AVFrame
    /// 4.把RGB24 AVFrame里面的数据拿出来放到pRGB24Buffer - uint8_t[]
    /// 5.根据pRGB24Buffer创建QImage并显示
    // 为YUV420和RGB24分配内存
    pFrameYUV420 = av_frame_alloc();
    if(NULL == pFrameYUV420)
    {
        qDebug("pFrameYUV420 alloc failed.\n");
        goto err5;
    }

    pFrameRGB24 = av_frame_alloc();
    if(NULL == pFrameRGB24)
    {
        qDebug("pFrameRGB24 alloc failed.\n");
        goto err6;
    }

    // 为pRGB24Buffer分配内存
    pRGB24Buffer = (uint8_t *) av_malloc(avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height) * sizeof(uint8_t));
    if(NULL == pRGB24Buffer)
    {
        qDebug("pRGB24Buffer alloc failed.\n");
        goto err7;
    }

    // avpicture_fill会将pFrameRGB24的数据按RGB24格式自动"关联"到pRGB24Buffer。
    avpicture_fill((AVPicture *) pFrameRGB24, pRGB24Buffer, AV_PIX_FMT_RGB24,
                pCodecCtx->width, pCodecCtx->height);

    //创建视频格式转换器 用于把解码出来的数据从YUV转换成RGB格式，这样才能在QT上显示出来
    srcVideoFormat = pCodecCtx->pix_fmt < 0 ? AV_PIX_FMT_YUV420P : pCodecCtx->pix_fmt;
    img_convert_ctx = sws_getContext(avcodecParameters->width, avcodecParameters->height, srcVideoFormat,
                                     avcodecParameters->width, avcodecParameters->height, AV_PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    if(NULL == img_convert_ctx)
    {
        qDebug("pRGB24Buffer alloc failed.\n");
        goto err8;
    }

    isVideoInit = true;
    return 0;

    //err9:
        sws_freeContext(img_convert_ctx);
    err8:
        av_free(pRGB24Buffer);
    err7:
        av_frame_free(&pFrameRGB24);
    err6:
        av_frame_free(&pFrameYUV420);
    err5:
        av_free_packet(packet);
    err4:
        free(packet);
    err3:
        avcodec_close(pCodecCtx);
    err2:
        avcodec_free_context(&pCodecCtx);
    err1:
        avformat_close_input(&pFormatCtx);
    err0:
        return 0 == ret ? -1 : ret;
}

void MainWindow::VideoDeinit()
{
    sws_freeContext(img_convert_ctx);
    av_free(pRGB24Buffer);
    av_free(pFrameRGB24);
    av_free(pFrameYUV420);
    av_free_packet(packet);
    free(packet);
    avcodec_close(pCodecCtx);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    isVideoInit = false;
}

void MainWindow::on_pbInitFFmpeg_clicked()
{
    VideoInit(ui->edInput->text().toStdString().c_str());
}

void MainWindow::on_pbNextFrame_clicked()
{
    if(0 != VideoInit(ui->edInput->text().toStdString().c_str())) return;

    do{
        //使用FFmpeg要注意内存泄漏的问题，av_read_frame中会申请内存，需要在外面进行释放，所以每读完一个packet，需要调用av_packet_unref进行内存释放。
        if (av_read_frame(pFormatCtx, packet) < 0)  //读取的是一帧视频，并存入一个AVPacket的结构中
        {
            qDebug("file end.\n");
            return; //这里认为视频读取完了
        }

        if (packet->stream_index != videoStreamIdx)
        {
            av_packet_unref(packet);//不为视频时释放pkt
            continue;
        }

    }while(packet->stream_index != videoStreamIdx);

    // 时间戳 https://www.cnblogs.com/gr-nick/p/10993363.html
    // FFmpeg:AVStream结构体分析 https://blog.csdn.net/qq_25333681/article/details/80486212
    // 深入理解pts，dts，time_base https://blog.csdn.net/bixinwei22/article/details/78770090
    qDebug("packet dts[%lld].",packet->dts);
    qDebug("packet pts[%lld].",packet->pts);
    //qDebug("packet rts[%lld].\n",packet->rts);
    AVStream * avStream = pFormatCtx->streams[0];
    qDebug("time_base[%d/%d].\n", avStream->time_base.num, avStream->time_base.den);
    //根据pts来计算一桢在整个视频中的时间位置：timestamp(秒) = packet->pts * av_q2d(avStream->time_base)
    qDebug("show time [%f].\n", packet->pts * av_q2d(avStream->time_base));
    qDebug("len [%f].\n", packet->duration * av_q2d(avStream->time_base));
    ;



    int got_picture = 0;
    int ret = avcodec_decode_video2(pCodecCtx, pFrameYUV420, &got_picture, packet);

    qDebug("%d", __LINE__);
    if (ret < 0)
    {
        qDebug("decode error.\n");
        return;
    }

    qDebug("%d", __LINE__);
    if (got_picture)
    {
        sws_scale(img_convert_ctx,
                  (uint8_t const * const *)pFrameYUV420->data, pFrameYUV420->linesize,
                  0, pCodecCtx->height,
                  pFrameRGB24->data, pFrameRGB24->linesize);
        //把这个RGB数据 用QImage加载
        QImage tmpImg(pRGB24Buffer, pCodecCtx->width, pCodecCtx->height, QImage::Format_RGB888);
        ui->lbImage->setPixmap(QPixmap::fromImage(tmpImg));
    }

    qDebug("%d", __LINE__);
    av_packet_unref(packet);//不为视频时释放pkt
    qDebug("%d", __LINE__);
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
    ui->edInput->setText("rtsp://10.11.2.10:8554/fsly");

}



void MainWindow::on_pbPlay_clicked()
{

    if(0 != VideoInit(ui->edInput->text().toStdString().c_str())) return;

    if(true == runFlag) return;

    runFlag = true;
    playThread = std::make_shared<std::thread>([this]() {

        while (runFlag)
        {
            //使用FFmpeg要注意内存泄漏的问题，av_read_frame中会申请内存，需要在外面进行释放，所以每读完一个packet，需要调用av_packet_unref进行内存释放。
            if (av_read_frame(pFormatCtx, packet) < 0)  //读取的是一帧视频，并存入一个AVPacket的结构中
            {
                qDebug("file end.\n");
                break; //这里认为视频读取完了
            }
            if (packet->stream_index != videoStreamIdx)
            {
                av_packet_unref(packet);//不为视频时释放pkt
                continue;
            }


            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            // 时间戳 https://www.cnblogs.com/gr-nick/p/10993363.html
            // FFmpeg:AVStream结构体分析 https://blog.csdn.net/qq_25333681/article/details/80486212
            // 深入理解pts，dts，time_base https://blog.csdn.net/bixinwei22/article/details/78770090
            qDebug("packet dts[%lld].",packet->dts);
            qDebug("packet pts[%lld].",packet->pts);
            qDebug("packet rts[%lld].\n",packet->rts);
            AVStream * avStream = pFormatCtx->streams[0];
            qDebug("time_base[%d/%d].\n", avStream->time_base.num, avStream->time_base.den);
            //根据pts来计算一桢在整个视频中的时间位置：timestamp(秒) = packet->pts * av_q2d(avStream->time_base)
            qDebug("show time [%f].\n", packet->pts * av_q2d(avStream->time_base));
            qDebug("len [%f].\n", packet->duration * av_q2d(avStream->time_base));



            int got_picture = 0;
            int ret = avcodec_decode_video2(pCodecCtx, pFrameYUV420, &got_picture, packet);

            qDebug("%d", __LINE__);
            if (ret < 0)
            {
                qDebug("decode error.\n");
                return;
            }

            qDebug("%d", __LINE__);
            if (got_picture)
            {
                qDebug("%d", __LINE__);
                sws_scale(img_convert_ctx,
                          (uint8_t const * const *)pFrameYUV420->data, pFrameYUV420->linesize,
                          0, pCodecCtx->height,
                          pFrameRGB24->data, pFrameRGB24->linesize);
                //把这个RGB数据 用QImage加载
                QImage tmpImg(pRGB24Buffer, pCodecCtx->width, pCodecCtx->height, QImage::Format_RGB888);
                ui->lbImage->setPixmap(QPixmap::fromImage(tmpImg));
            }
            qDebug("%d", __LINE__);

            av_packet_unref(packet);
            qDebug("%d", __LINE__);
        }
        runFlag = false;
        qDebug("playThread exit.\n");
    });
}
