#!/bin/sh

gcc test.c ../src/frame.c ../src/log.c ../src/util/utils.c -lavutil -lm -fsanitize=address -g3 -o test.out && \
./test.out && \
ffmpeg -hide_banner -loglevel error -i test.y4m -c:v libx264 -preset superfast -crf 15 -y test.mp4