pkg-config --cflags --libs libavcodec libavformat libavutil libswscale
if [ "$?" -ne "0" ]; then
    echo "could not find ffmpeg, please ensure it is installed and pkg-config is configured correctly" 1>&2
    exit 1
fi
