#pragma once

#ifdef _WIN32
#define PX_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#define PX_PLATFORM_UNIX
#else
#error Unsupported platform
#endif
