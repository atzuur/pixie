#pragma once

typedef void* PXDllHandle;

PXDllHandle px_dll_open(const char* path);
void* px_dll_get_sym(PXDllHandle handle, const char* sym);
int px_dll_close(PXDllHandle handle);
