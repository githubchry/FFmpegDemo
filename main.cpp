#include "mainwindow.h"

#include <QApplication>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavfilter/avfilter.h>
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    av_register_all();
    unsigned version = avcodec_version();
    printf("version is: %d \n", version);
    w.show();
    return a.exec();
}
