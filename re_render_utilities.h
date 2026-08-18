#ifndef WINDOW_UTILS_H
#define WINDOW_UTILS_H

#include <windows.h>
#include <stdlib.h>
#include <string.h>

HWND FindWindowByPartialTitle(const char* partial);
wchar_t Utf8ToWchar(const char* utf8str);

#endif