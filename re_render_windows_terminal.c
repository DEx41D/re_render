#define _WIN32_WINNT 0x0600

#include <stdio.h>
#include "re_render_utilities.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

int g_max_fps = 60;
int g_buffer_width = 160;
int g_buffer_height = 60;
int g_auto_size = 0;
const char* target_window_title;
wchar_t g_block_char = L'█';

int ParseCmdArguments(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <window title or partial> [--fps=N] [--width=N] [--height=N] [--char=X]\n", argv[0]);

        return 1;
    }

    target_window_title = argv[1];

    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "--fps=", 6) == 0)
            g_max_fps = atoi(argv[i] + 6);
        else if (strncmp(argv[i], "--width=", 8) == 0)
            g_buffer_width = atoi(argv[i] + 8);
        else if (strncmp(argv[i], "--height=", 9) == 0)
            g_buffer_height = atoi(argv[i] + 9);
        else if (strncmp(argv[i], "--auto-size", 11) == 0)
            g_auto_size = 1;
        else if (strncmp(argv[i], "--char=", 7) == 0) {
            wchar_t ch = Utf8ToWchar(argv[i] + 7);

            if (ch != 0) 
                g_block_char = ch;
        }
    }

    return 0;
}

int main(int argc, char* argv[]) 
{
    // Parse arguments from command line
    if(ParseCmdArguments(argc, argv) != 0) 
        return 1;

    // Find window, title which was transfered by command line arguments
    // That using std function from windows.h
    HWND hwnd = FindWindowA(
        NULL, target_window_title
    );

    // If this window wasnt be found
    // That using ownself function from "re_render_utilities.h"
    if (!hwnd)
        hwnd = FindWindowByPartialTitle(target_window_title);

    if (!hwnd) 
    {
        fprintf(stderr, "Window not found: %s\n", target_window_title);
        return 1;
    }

    // Code select region for get pixels
    RECT rc;
    if (!GetClientRect(hwnd, &rc)) 
    {
        fprintf(stderr, "Failed to get client rect\n");
        return 1;
    }

    // Auto size detect
    if (g_auto_size) 
    {
        g_buffer_width = rc.right - rc.left;
        g_buffer_height = rc.bottom - rc.top;
    }

    // Size of window
    int src_width = rc.right - rc.left;
    int src_height = rc.bottom - rc.top;
    
    // Get context descriptor of window
    HDC hdcWindow = GetDC(
        hwnd
    );
    if (!hdcWindow) 
    {
        fprintf(stderr, "GetDC failed\n");
        return 1;
    }

    // Get DC to memory 
    HDC hdcMem = CreateCompatibleDC(
        hdcWindow
    );
    if (!hdcMem) 
    {
        fprintf(stderr, "CreateCompatibleDC failed\n");
        // For avoid leak destroy in memory
        ReleaseDC(hwnd, hdcWindow);

        return 1;
    }

    // Define bit map for initialize graphical parameters
    BITMAPINFO bmi = {
        0 
    };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = src_width;
    // For height using negative position - its need for Top-Down type map
    bmi.bmiHeader.biHeight = -src_height;
    // biPlanes - always 1 (as const), just dont giva fuck about it
    bmi.bmiHeader.biPlanes = 1;
    // biBitCount - color height
    // Default value (24) mean define RGB height (8 red + 8 green + 8 blue = 24 bits (3 byte) on color)
    bmi.bmiHeader.biBitCount = 24;
    // BI_RGB mean what colors is not compress (that is, "clear directly sequensy" in memory)
    bmi.bmiHeader.biCompression = BI_RGB;

    // Setup memory for pixels selection
    BYTE* pixels = NULL;
    HBITMAP hbm = CreateDIBSection(
        hdcMem, &bmi, DIB_RGB_COLORS, (void**) &pixels, NULL, 0
    );
    if (!hbm) 
    {
        fprintf(stderr, "CreateDIBSection failed\n");
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWindow);

        return 1;
    }

    // Put hbm to hdcMem
    HGDIOBJ oldbm = SelectObject(hdcMem, hbm);

    // --- Console ---
    // Get std output handle (console output, place from printf)
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    // Get all console settings
    DWORD consoleMode = 0;
    if (!GetConsoleMode(hStdOut, &consoleMode)) 
    {
        fprintf(stderr, "GetConsoleMode(hStdOut, &consoleMode) failed\n");
        return 1;
    }

    // Enable ANSI-escape sequens in console (for colorize, clear and etc.)
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hStdOut, consoleMode)) 
    {
        fprintf(stderr, "SetConsoleMode ENABLE_VIRTUAL_TERMINAL_PROCESSING failed\n");
        return 1;
    }

    // Set console output format to UTF-8
    // Its need for graphical
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Hide text cursor
    CONSOLE_CURSOR_INFO cci = {
        sizeof(cci), FALSE
    };
    SetConsoleCursorInfo(hStdOut, &cci);

    // Clear console
    printf("\x1b[2J");
    fflush(stdout);

    // Convert utf-8 char to wide
    char utf8char[5] = {
        0
    };
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, &g_block_char, 1, utf8char, sizeof(utf8char), NULL, NULL);

    // Full render frame buffer (+48 on each symbol)
    size_t max_buf_size = (size_t) g_buffer_width * g_buffer_height * 48;
    char* out_buf = malloc(max_buf_size);
    if (!out_buf) 
    {
        fprintf(stderr, "Failed to allocate output buffer\n");
        return 1;
    }

    // ANSI-sequenst for clear
    const char* clear_seq = "\x1b[H";
    const int clear_seq_lgt = sizeof(clear_seq) - 1;
    // ANSI-sequenst for reset
    const char reset_nl[] = "\x1b[0m\n";

    // For optimize in resampling (idk what is it)
    int stride = (src_width * 3 + 3) & ~3;

    // ----------------------- NEW CODE -------------------------------
    static BYTE* prev_pixels = NULL;
    static int is_first_frame = 1;
    static int prev_w = 0, prev_h = 0;
    if (!prev_pixels || prev_w != g_buffer_width || prev_h != g_buffer_height) 
    {
        free(prev_pixels);
        prev_pixels = calloc(1, g_buffer_width * g_buffer_height * 3);
        prev_w = g_buffer_width; prev_h = g_buffer_height;
        is_first_frame = 1;
    }

    const char move_next_line[] = "\x1b[0m\x1b[1E";
    const size_t move_next_line_lgt = sizeof(move_next_line) - 1;

    int skip_accumulator = 0;

    // ----------------------------------------------------------------

    // Main loop for render
    while (1) 
    {
        ULONGLONG frame_start = GetTickCount64();

        // Get all "real" space (cuz PW_CLIENTONLY) of window to hdcMem
        if (!PrintWindow(hwnd, hdcMem, PW_CLIENTONLY)) 
        {
            fprintf(stderr, "PrintWindow failed: %lu\n", GetLastError());

            // If window is iconic, render cannot resize, then sleep
            if (IsIconic(hwnd))
            {
                Sleep(1000);

                continue;
            } 
            // If window closed
            else if (!IsWindow(hwnd))
            {
                fprintf(stderr, "\nWindow has been closed.\n");

                break;
            }

            break;
        }

        // Start pos
        size_t pos = 0;

        // Add clear sequense to frame buffer
        memcpy(out_buf, clear_seq, clear_seq_lgt);
        // Add sequens length to position (7)
        pos += clear_seq_lgt;

        // Resampling
        for (int y = 0; y < g_buffer_height; y++)
        {
            // Nearest-Neighbor Interpolation method for y
            int src_y = (y * src_height) / g_buffer_height;
            int x = 0;

            // Calculate the address begin of current row
            BYTE* row_ptr = pixels + (src_y * stride);

            while (x < g_buffer_width) 
            {
                // Nearest-Neighbor Interpolation method for x
                int src_x = (x * src_width) / g_buffer_width;

                // Pixel
                BYTE *px = row_ptr + (src_x * 3);
                // Red
                BYTE r = px[2];
                // Green
                BYTE g = px[1];
                // Blue
                BYTE b = px[0];

                // ----------------------- NEW CODE -------------------------------
                if (!is_first_frame)
                {
                    BYTE* prev_px = prev_pixels + (y * g_buffer_width + x) * 3;
                    if (r == prev_px[2] && g == prev_px[1] && b == prev_px[0])
                    {
                        skip_accumulator++;
                        x++;
                        continue;
                    }
                }

                if (skip_accumulator > 0)
                {
                    int n_skip = snprintf(out_buf + pos, max_buf_size - pos, "\x1b[%dG", x + 1);
                    if (n_skip < 0 || (size_t)n_skip >= max_buf_size - pos)
                        break;
                    pos += (size_t)n_skip;
                    skip_accumulator = 0;
                }
                // ---------------------------------------------------------------

                // Color optimization for render:
                // code dont print own color on each symbol,
                // he just look on next pixel-colors, and print color once to all colors in row.
                int group_end = x + 1;
                while (group_end < g_buffer_width)
                {
                    // size of x
                    int sx = (group_end * src_width) / g_buffer_width;

                    // Pixel
                    BYTE* pxt = row_ptr + (sx * 3);

                    // ----------------------- NEW CODE -------------------------------
                    if (!is_first_frame)
                    {
                        BYTE* prev_pxt = prev_pixels + (y * g_buffer_width + group_end) * 3;
                        if (pxt[2] == prev_pxt[2] && pxt[1] == prev_pxt[1] && pxt[0] == prev_pxt[0])
                            break;
                    }
                    // ---------------------------------------------------------------

                    // End of 
                    if (pxt[2] != r || pxt[1] != g || pxt[0] != b)
                        break;

                    group_end++;
                }

                // Count of pixels
                int count = group_end - x;
                int n = snprintf(out_buf + pos, max_buf_size - pos, "\x1b[38;2;%d;%d;%dm", r, g, b);

                if (n < 0 || (size_t) n >= max_buf_size - pos) 
                    break; 

                // Offset the position
                pos += (size_t) n;

                for (int i = 0; i < count; i++) 
                {
                    if ((size_t) utf8_len >= max_buf_size - pos) 
                        break;

                    memcpy(out_buf + pos, utf8char, utf8_len);
                    pos += (size_t) utf8_len;

                    // ----------------------- NEW CODE -------------------------------
                    BYTE* save_px = prev_pixels + (y * g_buffer_width + (x + i)) * 3;
                    save_px[2] = r;
                    save_px[1] = g;
                    save_px[0] = b;
                    // ---------------------------------------------------------------
                }

                x = group_end;
            }

            skip_accumulator = 0;

            if (pos + sizeof(reset_nl) - 1 > max_buf_size) 
                break;

            memcpy(out_buf + pos, reset_nl, sizeof(reset_nl) - 1);
            pos += sizeof(reset_nl) - 1;
        }

        // Render the frame
        fwrite(out_buf, 1, pos, stdout);
        fflush(stdout);

        is_first_frame = 0;

        // Sleep for fps with delta
        int target_frame_time = 1000 / g_max_fps;

        // Calculate delta of render frame time
        ULONGLONG elapsed_time = GetTickCount64() - frame_start;

        // Sweet dreams...
        if (elapsed_time < (ULONGLONG) target_frame_time)
        {
            Sleep(target_frame_time - (int)elapsed_time);
        }
        else
        {
            Sleep(0);
        }
    }

    // Clear all busy memories
    free(out_buf);

    // Show text-cursor
    cci.bVisible = TRUE;
    SetConsoleCursorInfo(hStdOut, &cci);

    SelectObject(hdcMem, oldbm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWindow);

    return 0;
}
