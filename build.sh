#!/bin/bash 

# export env path
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/home/yofc/lee/ffmpeg/output/lib/pkgconfig
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda-9.1/lib64

gcc -o nvenc_dummy ./nvenc_dummy.c -L/usr/local/cuda-9.1/lib64 ` pkg-config --cflags --libs libavfilter libavcodec  libavdevice  libavfilter  libavformat  libavutil  libswresample  libswscale `

