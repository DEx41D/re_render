#include "re_render_utilities.h"

void PrintHelp(const char* prog_name) 
{
    printf("    ****     *****       ****     *****    *   *    ****     *****    ****  \n");
    printf("   *   *    *           *   *    *        **  *    *   *    *        *   *  \n");
    printf("  ****     ****        ****     ****     * * *    *   *    ****     ****    \n");
    printf(" *  *     *           *  *     *        *  **    *   *    *        *  *     \n");
    printf("*   *    *****       *   *    *****    *   *    ****     *****    *   *  \n");

    printf("Usage: %s [target_window_title] [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help           Show this help message and exit\n");
    printf("  --color-threshold=INT Color tolerance for diff rendering (0-255, default: 0)\n");
    printf("                         0  = Perfect match (Slower for video streams)\n");
    printf("                         15 = Recommended for video/games (Massive FPS boost)\n");
    printf("  --fps=INT            Set maximum frames per second limit\n");
    printf("  --width=INT          Set target render buffer width\n");
    printf("  --height=INT         Set target render buffer height\n");
    printf("  --color-depth=INT    Color depth per channel from 1 to 8 (default: 8)\n");
    printf("                         8 = 24-bit True Color (High quality, slower)\n");
    printf("                         6 = 18-bit color      (Balanced)\n");
    printf("                         4 = 12-bit color      (Retro style, maximum FPS)\n");
    printf("  --char=CHAR          Set a custom render block character\n");
    printf("  --auto-size          Automatically fit frame buffer to terminal window\n");
    printf("  --scroll-disable     Disable terminal scrolling\n");
    printf("  --interlacing        Enable interlacing (skips every second line for 2x performance)\n");
    printf("  --verbose            Enable verbose output / debug stats\n");
    printf("\nExample:\n");
    printf("  %s \"My Game\" --width=120 --height=40 --fps=60 --interlacing\n", prog_name); // Обновлен пример
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

