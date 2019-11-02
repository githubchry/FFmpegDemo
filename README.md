# install ffmpeg
```
./configure  --disable-x86asm --prefix=output --enable-pthreads --enable-shared --disable-static
make
make install
strip output/lib/*
mv output ~/codes/FFmpegDemo/ffmpeg
```

# create qt project
Peoject - build settings - build directory
/home/chry/codes/FFmpegDemo/%{CurrentBuild:Type}

# edit FFmpegDemo.pro
```
FFMPEG_PATH = /home/chry/codes/FFmpegDemo/ffmpeg

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
