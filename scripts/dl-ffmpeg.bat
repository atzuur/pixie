@echo off

if not exist "build" mkdir build
if exist "build\ffmpeg" rmdir /s /q "build\ffmpeg"

cd build

echo getting latest ffmpeg version 1>&2
for /f "tokens=*" %%v in ('curl -L https://www.gyan.dev/ffmpeg/builds/release-version') do (
    set ff_ver=%%v
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
