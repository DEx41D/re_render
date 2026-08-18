#include "re_render_utilities.h"

void PrintIntroMessage()
{

}

// Function for transform UTF8 format string to WideChar format (for C work)
// This function just using MultiByteToWideChar for transform
wchar_t Utf8ToWchar(const char* utf8str)
{
    if (!utf8str || !*utf8str)
        return 0;

    int len = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, NULL, 0);

    if (len <= 0)
        return 0;

    wchar_t* wbuffer = malloc(len * sizeof(wchar_t));

    if (!wbuffer)
        return 0;

    MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, wbuffer, len);
    wchar_t wch = wbuffer[0];

    free(wbuffer);

    return wch;
}

// Function for find window by part by title
HWND FindWindowByPartialTitle(const char* partial)
{
    // Startup value for predicabillity by C-side
    HWND hwnd = NULL;

    // Code will be just each even window for find needed
    do
    {
        // Subsequent window
        // The second (2) argument is hwnd - cuz its window from which we must start find (its argument mean previous founded window)
        hwnd = FindWindowExA(NULL, hwnd, NULL, NULL);
        if (!hwnd)
            break;

        // Get window title
        char title[256];
        GetWindowTextA(hwnd, title, sizeof(title));

        // If, its thos who us need, we return him
        if (strstr(title, partial))
            return hwnd;
    } 
    while (hwnd);

    return NULL;
}

