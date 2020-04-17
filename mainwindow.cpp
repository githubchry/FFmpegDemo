#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    int ret = -1;
    char *file_path = "../chrome.mp4";

    //初始化FFMPEG  调用了这个才能正常适用编码器和解码器
    av_register_all();

    //分配一个AVFormatContext，FFMPEG所有的操作都要通过这个AVFormatContext来进行
    pFormatCtx = avformat_alloc_context();

    //打开视频文件
    ret = avformat_open_input(&pFormatCtx, file_path, NULL, NULL);
    if(0 != ret)
    {
        char tmp[256];
        av_strerror(ret, tmp, 256);
        qDebug("avformat_open_input %s failed %d: %s\n", file_path, ret,tmp);
    }
    av_dump_format(pFormatCtx, 0, file_path, 0); //打印视频信息

    //文件打开成功后循环查找文件中包含的流信息，直到找到视频类型的流,将其记录下来 保存到videoStream变量中
    ///这里我们现在只处理视频流  音频流先不管他
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
        }
    }

    if (videoStreamIdx >= 0) {
        //根据视频流索引，获取解码器参数
        AVCodecParameters * avcodecParameters = pFormatCtx->streams[videoStreamIdx]->codecpar;

        //根据解码器ID，然后查找解码器
        AVCodec * codec = avcodec_find_decoder(avcodecParameters->codec_id);

        //创建解码器上下文
        pCodecCtx = avcodec_alloc_context3(NULL);
        if(pCodecCtx == NULL)
        {
            printf("create condectext failed.\n");
            exit(-6);
        }

        // 把解码参数复制到上下文
        avcodec_parameters_to_context(pCodecCtx, avcodecParameters);

        //根据上下文打开解码器
        int ret = avcodec_open2(pCodecCtx,codec,NULL);
        if (ret < 0) {
            printf("Could not open codec.\n");
            exit(-2);
        }

        ///可以看出  我们可以直接根据查找到的视频流信息获取到解码器。 而且我们并不知道他实际用的是什么编码器。


        /// AVPacket用于存储压缩的数据
        /// AVFrame 用于存储解码后的数据
        /// av_read_frame得到压缩的数据包AVPacket，一般有三种压缩的数据包(视频、音频和字幕)，然后调用avcodec_decode_video2对AVPacket进行解码得到AVFrame。
        //根据视频的大小创建AVPacket并分配空间
        int y_size = pCodecCtx->width * pCodecCtx->height;
        packet = (AVPacket *) malloc(sizeof(AVPacket));
        av_new_packet(packet, y_size);

        /// 要想在QT显示一帧图像，
        /// 1.从文件获取一帧h264数据 - AVPacket
        /// 2.使用ffmpeg解码成YUV420数据 - AVFrame
        /// 3.将YUV420数据转成RGB24 - AVFrame
        /// 4.把RGB24 AVFrame里面的数据拿出来放到pRGB24Buffer - uint8_t[]
        /// 5.根据pRGB24Buffer创建QImage并显示
        // 为YUV420和RGB24分配内存
        pFrameYUV420 = av_frame_alloc();
        pFrameRGB24 = av_frame_alloc();

        // 为pRGB24Buffer分配内存
        int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
        pRGB24Buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

        // avpicture_fill会将pFrameRGB24的数据按RGB24格式自动"关联"到pRGB24Buffer。
        avpicture_fill((AVPicture *) pFrameRGB24, pRGB24Buffer, AV_PIX_FMT_RGB24,
                    pCodecCtx->width, pCodecCtx->height);

        //创建视频格式转换器 用于把解码出来的数据从YUV转换成RGB格式，这样才能在QT上显示出来
        enum AVPixelFormat srcVideoFormat = pCodecCtx->pix_fmt < 0 ? AV_PIX_FMT_YUV420P : pCodecCtx->pix_fmt;
        img_convert_ctx = sws_getContext(avcodecParameters->width, avcodecParameters->height, srcVideoFormat,
                                         avcodecParameters->width, avcodecParameters->height, AV_PIX_FMT_RGB24,
                                         SWS_BICUBIC, NULL, NULL, NULL);
    }

}

MainWindow::~MainWindow()
{
    av_free_packet(packet);
    av_free(pFrameYUV420);
    av_free(pFrameRGB24);
    av_free(pRGB24Buffer);

    sws_freeContext(img_convert_ctx);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    delete ui;
}


void MainWindow::on_pbNextFrame_clicked()
{
    if(nullptr == packet) return;

    do{
        if (av_read_frame(pFormatCtx, packet) < 0)  //读取的是一帧视频，并存入一个AVPacket的结构中
        {
            printf("file end.\n");
            return; //这里认为视频读取完了
        }
    }while(packet->stream_index != videoStreamIdx);

    int got_picture = 0;
    int ret = avcodec_decode_video2(pCodecCtx, pFrameYUV420, &got_picture, packet);

    if (ret < 0)
    {
        printf("decode error.\n");
        return;
    }

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
}
