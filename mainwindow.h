#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <thread>
#include <atomic>

#include "ffmvideo.h"

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
    static void frame_handle_func(AVFrame *yuv, uint64_t rts, uint8_t *rgb, bool stop_flag);

private slots:
    void on_rbFile_clicked();

    void on_rbUrl_clicked();

    int on_pbNextFrame_clicked();

    void on_pbStop_clicked();

    void on_pbPalyOrPause_clicked();

    void on_playEnded();

signals:
    void playEnded();

private:
    Ui::MainWindow *ui;
    static MainWindow* s_this;        //--------静态指针--------------------------

    std::shared_ptr<ffmvideo> video = nullptr;

};
#endif // MAINWINDOW_H
