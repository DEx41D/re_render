#ifndef WINDOW_UTILS_H
#define WINDOW_UTILS_H

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --------------------
#ifdef _WIN32
	#include <conio.h>  
#else
	#include <sys/select.h>
	#include <termios.h>
	#include <unistd.h>
#endif

void PrintHelp(const char* prog_name);
HWND FindWindowByPartialTitle(const char* partial);
wchar_t Utf8ToWchar(const char* utf8str);

#endif