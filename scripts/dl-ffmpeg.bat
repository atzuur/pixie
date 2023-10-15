@echo off

if exist build (
    rmdir /s /q build/ffmpeg 2> nul
) else (
    mkdir build
)
cd build

if "%1" == "" (
    echo getting latest ffmpeg version 1>&2
    for /f "tokens=*" %%v in ('curl -L https://www.gyan.dev/ffmpeg/builds/release-version') do (
        set ff_ver=%%v
    )
) else (
    set ff_ver=%1
)

echo downloading ffmpeg %ff_ver% 1>&2
curl -L https://github.com/GyanD/codexffmpeg/releases/download/%ff_ver%/ffmpeg-%ff_ver%-full_build-shared.zip -o ffmpeg.zip
powershell Expand-Archive ffmpeg.zip -DestinationPath .
del ffmpeg.zip
ren ffmpeg-%ff_ver%-full_build-shared ffmpeg

echo copying ffmpeg dlls 1>&2
copy ffmpeg\bin\avcodec*.dll .
copy ffmpeg\bin\avformat*.dll .
copy ffmpeg\bin\avutil*.dll .
copy ffmpeg\bin\swresample*.dll .

cd ..
