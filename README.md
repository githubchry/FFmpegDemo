
# 环境搭建
## 安装FFmpeg
### Linux下编译

[Download FFmpeg](http://ffmpeg.org/download.html#releases)

```
./configure  --disable-x86asm --prefix=output --enable-pthreads --enable-shared --disable-static
make
make install
strip output/lib/*
mv output ~/codes/FFmpegDemo/ffmpeg
```

### Windows下直接下载开发包

[Download FFmpeg Builds](https://ffmpeg.zeranoe.com/builds/)

`Architecture`选择`Windows 64-bit`，在`Linking`分别选择`Shared`和`Dev`下载两个压缩包。

解压 `Shared`压缩包，把里面的`bin`目录添加到系统环境变量

解压`Dev`压缩包，把文件夹命名为`ffmpeg`后放在工程目录

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

