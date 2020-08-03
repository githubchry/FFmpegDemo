# 环境搭建

## 安装QT

`Qt 5.14.2`下根据需求勾选`MinGW `32位或64位`MinGW 7.3.0 32-bit`和`MinGW 7.3.0 64-bit`

`Developer and Desiger Tools`仅选勾选`Qt Creator 4.11.1 CDB `，这里面的MinGW不需要勾！



## 安装FFmpeg
### Linux下编译

#### 编译x264依赖库(可选)

[下载地址](https://code.videolan.org/videolan/x264)

```
 sudo apt install nasm
./configure --enable-shared  --enable-pic --prefix=../new-root 
make
make install
ls -l ../new-root
```

#### 编译FFmpeg

[Download FFmpeg](http://ffmpeg.org/download.html#releases)

```
默认不安装x264:
./configure --prefix=../new-root --enable-pthreads --enable-shared --disable-static --enable-pic

链接x264:
./configure --prefix=../new-root --enable-pthreads --enable-shared --disable-static  --enable-pic \
--extra-cflags=-I../new-root/include \
--extra-ldflags=-L../new-root/lib \
--enable-gpl --enable-libx264 
 
 
make
make install
strip ../new-root/lib/*
mv ../new-root ~/codes/FFmpegDemo/ffmpeg
```

### Windows下直接下载开发包

[Download FFmpeg Builds](https://ffmpeg.zeranoe.com/builds/)

`Architecture`选择`Windows 64-bit`，在`Linking`分别选择`Shared`和`Dev`下载两个压缩包。

解压 `Shared`压缩包，把里面的`bin`目录添加到系统环境变量

解压`Dev`压缩包，把文件夹命名为`ffmpeg`后放在工程目录

### Windows下编译

[windows下编译FFMPEG](https://blog.csdn.net/listener51/article/details/81605472)

[windows下编译ffmpeg](https://blog.csdn.net/mvp_Dawn/article/details/91352773)

[Qt5开发：使用windeployqt打包发布](https://blog.csdn.net/Stone_Wang_MZ/article/details/94591363)

```
mingw64.exe

export PATH="/mingw64/bin:$PATH"
./configure --disable-static --enable-shared --disable-doc --prefix=output64 --arch=x86_64

liblzma-5.dll
libwinpthread-1.dll
libbz2-1.dll
zlib1.dll
libiconv-2.dll




=====32位==========================================================
export PATH="/mingw32/bin:$PATH"
./configure --disable-static --enable-shared --disable-doc --prefix=output32 --arch=x86_32


libgcc_s_dw2-1.dll
liblzma-5.dll
libwinpthread-1.dll
libbz2-1.dll
zlib1.dll
libiconv-2.dll



```





## 配置工程

`Peoject - build settings - build directory`

项目-构建设置-构建目录

linux配置：`%{CurrentProject:Path}/%{CurrentBuild:Type}`

windows配置：`%{CurrentProject:Path}\build`



## pro工程配置引用ffmpeg
```
# ffmpeg
FFMPEG_PATH = $$PWD/ffmpeg

INCLUDEPATH += $$FFMPEG_PATH/include

LIBS += -L$$FFMPEG_PATH/lib/ \
    -lavcodec \
    -lavdevice \
    -lavfilter \
    -lavformat \
    -lavutil \
    -lswresample \
    -lswscale
```

