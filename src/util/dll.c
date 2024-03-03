#include "../internals.h"

#include <pixie/util/dll.h>
#include <pixie/util/platform.h>

#ifdef PX_PLATFORM_WINDOWS
#include <libloaderapi.h>
#elif defined(PX_PLATFORM_UNIX)
#include <dlfcn.h>
#endif

PXDllHandle px_dll_open(const char* path) {
#ifdef PX_PLATFORM_WINDOWS
    HMODULE lib_handle = LoadLibraryA(path);
    if (!lib_handle)
        OS_THROW_MSG("LoadLibraryA", PX_LAST_OS_ERR());
#elif defined(PX_PLATFORM_UNIX)
    void* lib_handle = dlopen(path, RTLD_NOW);
    if (!lib_handle)
        px_log(PX_LOG_ERROR, "dlopen() failed: %s", dlerror());
#endif
    return lib_handle;
}

void* px_dll_get_sym(PXDllHandle handle, const char* sym) {
#ifdef PX_PLATFORM_WINDOWS
    FARPROC sym_fn = GetProcAddress(handle, sym);
    if (!sym_fn)
        OS_THROW_MSG("GetProcAddress", PX_LAST_OS_ERR());
    void* sym_addr = *(void**)&sym_fn;
#elif defined(PX_PLATFORM_UNIX)
    void* sym_addr = dlsym(handle, sym);
    if (!sym_addr)
        px_log(PX_LOG_ERROR, "dlsym() failed: %s", dlerror());
#endif
    return sym_addr;
}

int px_dll_close(PXDllHandle handle) {
#ifdef PX_PLATFORM_WINDOWS
    int ret = FreeLibrary(handle);
    if (!ret)
        OS_THROW_MSG("FreeLibrary", PX_LAST_OS_ERR());
#elif defined(PX_PLATFORM_UNIX)
    int ret = dlclose(handle);
    if (ret)
        px_log(PX_LOG_ERROR, "dlclose() failed: %s", dlerror());
#endif
    return ret;
}
