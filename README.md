# pixie
Plugin-based video filtering library focused on simplicity, modularity and performance

## How it works
pixie:
1. takes in an input video or image file
2. decodes and demuxes it using FFmpeg's C api (libav*)
3. applies user-defined filters loaded from DLL/SO files on each frame
4. encodes, muxes and outputs the resulting video or image file

## Motivation
The primary goal of the project is to provide a simple (easy to pick up and understand) interface for creating your own and using others' video filters while having minimal external dependencies. Existing projects such as [VapourSynth](https://vapoursynth.com) and [AviSynth](https://avisynth.nl) already provide functionality similar to pixie but have much broader scope w.r.t how complex filtergraphs can get, which in turn hurts their ease of use. FFmpeg's libavfilter also has an interface for writing your own filters but it does not support loading them at runtime, making it not suitable for most people's needs as compiling FFmpeg is a decent effort.

The secondary goals of pixie are:
* modularity in keeping the library and frontend separate to make it easy to add your own frontends (e.g. GUI),
* keeping the decode-filter-encode pipeline as performant as possible.

## A word of warning
As pixie is still relatively early in development, the behavior documented here is all **subject to change** and **API/ABI may be broken without warning**. Until stated otherwise, you should not rely on any pixie interface staying compatible in newer versions. The current goal is to achieve stability by release 1.0.

## CLI usage
**Note**: the following only applies to the default `pixie` executable (found in [app/](app/)), 3rd-party frontend implementations may be different.

`--input`/`-i` `<file> [<file2> ...]`:
* Specify input file paths, separated by space
* Required (at least 1)
* Example: `-i cats.mp4 secrets/shh.mp4`

`--output`/`-o` `<path>`:
* Specify output path. If there is more than than one input file, this will be treated as a directory and every output file will be placed in that directory under their original names
* Required
* Example 1: `-o filtered_cat.mp4`
* Example 2: `-i shh.mp4 owo.mp4 -o secrets` (will result in `secrets/shh.mp4` and `secrets/owo.mp4`)

`--video-enc`/`-e` `<encoder>[:opt=val[:opt2=val2:...]]`:
* Specify the video encoder to use and optionally settings for it. The FFmpeg default settings will be used if none are specified.
* Default: `libx264`
* Example 1: `-e h264_nvenc`
* Example 2: `-e libx264:crf=18:preset=slow`

`--video-filters`/`-f` `<filter>[:opt=val[:opt2=val2]] [...]`:
* Specify the video filters to apply and optionally settings for each, filters separated by space.
* Example 1: `-f meowify` (assuming `meowify` requires no settings)
* Example 2: `-f meowify:amount=20 purrify:amount=5`
* Example 3: `-f hide_secrets:num=2:where=./secrets`

`--filter-dir`/`-d` `<path>`:
* Specify the directory to look for filter DLLs in
* Default: `.` (current working directory)
* Example 1: `-d ./cat_filters`

`--log-level`/`-l` `<level>`:
* Specify how verbose pixie will be with printing log messages, both from pixie itself and FFmpeg. More verbose levels inherit from less verbose ones, so e.g. `warn` will still print errors and progress info. The level may also be specified by ordinal, starting from 0 (`quiet`) and ending in 5 (`verbose`)
* Choices:
    * `quiet`: no logging output at all
    * `error`: log errors
    * `progress`: log progress info
    * `warn`: log warnings
    * `info`: log miscellaneous info
    * `verbose`: log everything (mostly for debugging)
* Default: `progress`
* Example 1: `-l warn`
* Example 2: `-l 3` (equivalent to `-l warn`)

`--help`/`-h`:
* Print a help message listing these options and exit
* Example 1: `-h`

## Building
**Dependencies**: a C compiler that (mostly) supports C23, GNU Make, FFmpeg, pkg-config (Unix-likes only)

Currently the realistic compiler requirement is either GCC 13.2 or Clang 16, but I will not stop you from using other compilers with similar language feature support. 

The pixie CLI app, library and other build artifacts will be placed in a dedicated build directory (defaults to `build`), which has to be created before running `make`.

On Windows you need to have a shared build of FFmpeg (like the ones at [gyan.dev](https://www.gyan.dev/ffmpeg/builds/)) in `build/ffmpeg` (by default). You can download one there automatically by using [`scripts/dl-ffmpeg.bat`](scripts/dl-ffmpeg.bat). On Unix-likes your FFmpeg installation is detected automatically with `pkg-config`.

```bash
git clone https://github.com/atzuur/pixie
cd pixie
mkdir build
./scripts/dl-ffmpeg.bat # Windows only, optional
make
```

<details>
  <summary>Addional arguments to the Makefile</summary>
  
  * `cc`: C compiler to build with, must support the same basic argument syntax as GCC and Clang (default: `gcc`)
  * `build_dir`: Directory to place build artifacts in (default: `build`)
  * `ff_dir`: Directory to look for FFmpeg in on Windows (default: `$(build_dir)/ffmpeg`)
  * `debug`: Build in debug mode (`0` = release, `1` = debug) (default: `0`)
  * `sanitize`: Enable sanitizers (passed directly after `-fsanitize=`)
  * `extra_cflags`: Additional flags to compile C files with
  * `extra_ldflags`: Additional flags to link object files with (passed to `cc`, not linker directly)

  ### Examples
  ```bash
  # Clang build with all warnings (pls don't)
  make cc=clang extra_cflags=-Weverything

  # Debug build with all the bells and whistles
  make debug=1 sanitize=address,undefined

  # Not sure what to call this one but you get the idea
  make build_dir=target ff_dir=../my_cool_custom_ffmpeg
  ```
</details>

## Writing your own filters
There is a template for a filter named `test` in [`tests/test_filter.c`](tests/test_filter.c) that should get you started (it doesn't do anything in particular, mostly just a color invert).

### Architecture
#### Loading filters
For each filter, pixie will look for a DLL within the specified filter directory whose filename corresponds to the specified filter name. For example, given a filter name `"foo"`, pixie will look for `<filter directory>/<filter name>.so` (or `.dll` on Windows). After the DLL is found, pixie will call its `pixie_export_filter` function (see below) and initialize the filter via `PXFilter::init()`, passing the specified options to it as the `args` parameter. 

#### Applying filters
After a frame is decoded, pixie will call each filter's `PXFilter::apply()` function on it in the order that the filters were specified in. If the input file is detected to have a pixel format not included in the enum `PXPixelFormat`, pixie will automatically convert the frames to a compatible format (if possible without data loss).

#### Cleanup and errors
When the transcode ends, each filter's `PXFilter::free()` function will be called automatically by pixie, regardless of whether the transcode was successful or not. A filter's `PXFilter::init()` or `PXFilter::apply()` function failing (returning a negative error code) will also end the transcode.

### Limitations
Filters can currently only take one frame as input at a time (`PXFilter::in_frame`) and output a modified version of the same frame (`PXFilter::out_frame`). Modifying any part of the output frame other than the `data` member of each plane (the actual pixel data) is currently disallowed.

### Exporting your filter
Filters may be written in any language, but they must be compiled into shared libraries with at least the `pixie_export_filter` function exported (GNU `ld` exports symbols by default). The signature of `pixie_export_filter` must be equivalent to `PXFilter* pixie_export_filter(void)` in C. The `pixie_export_filter` function must set at least `PXFilter::name` and `PXFilter::apply()`, `PXFilter::init()` and `PXFilter::free()` are optional.

### Windows oddities
On Windows, filters need to be linked with either an import library for the pixie library DLL (`pixie.dll.a`) orth the DLL itself (`pixie.dll`). This is because filters need to be able to call functions from the pixie DLL, which they will only have access to after being loaded in by pixie (i.e. at runtime). As Windows doesn't support [exporting symbols that are visible at runtime of the DLL](https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_node/ld_3.html#:~:text=%2DE-,%2D%2Dexport%2Ddynamic), the locations need to be known statically at link time.

### Example commands to build [`tests/test_filter.c`](tests/test_filter.c):
On Windows:
```pwsh
# Assuming ./pixie/incl is the include directory and either ./pixie/pixie.dll or ./pixie/pixie.dll.a exist
gcc test_filter.c -shared -Ipixie/incl -Lpixie -lpixie -o test.dll
```

On Unix-likes:
```bash
gcc test_filter.c -shared -o test.so
```

## Extending the frontend
The reference implementation (CLI app) can be found in [`app/`](`app/`), but more documentation regarding creating your own frontend will be coming later as the API is still being refined.

## Contact
For any issues or questions, you can DM me on Discord (`@atzur`) or [create an issue on GitHub](https://github.com/atzuur/pixie/issues/new).
