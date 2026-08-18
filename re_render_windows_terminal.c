/* 
    The MIT License (MIT)

    Copyright (c) 2025-2026 The Rediska4445 Author

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#define _WIN32_WINNT 0x0600

#include "re_render_utilities.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Main data which using in program
// They is not into sturcture for optimization
int g_max_fps = 60;
int g_buffer_width = 160;
int g_buffer_height = 60;
int g_auto_size = 0;
int g_bits_per_channel = 8;
int g_color_threshold = 0;
int g_scroll = 1;
int g_interlacing = 0;
int g_verbose = 0;
const char* target_window_title;
wchar_t g_block_char = L'█';

// Data for color processing
int g_color_shift = 0;
BYTE g_color_mask = 0xFF;

// Data for render processing
unsigned int frame_counter = 0;

// Color cache
char g_ansi_color_cache[16][16][16][20];
int g_ansi_color_lengths[16][16][16];

// TODO:
// OpenMP
#define ROW_BUF_SIZE (120 * 32) 
char row_buffers[40][ROW_BUF_SIZE]; 
int row_lengths[40];

// Verbose
unsigned long long g_interlacing_skipped_frames = 0;

// Verbose functions
#define InterlacingSkippedFramesStats count_skipped_frame(g_verbose, &g_interlacing_skipped_frames)
static inline void count_skipped_frame(int verbose_mode, unsigned long long* counter)
{
    if (verbose_mode)
    {
        (*counter)++;
    }
}

// Parse the input arguments
int ParseCmdArguments(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <window title or partial> [--fps=N] [--width=N] [--height=N] [--char=X]\n", argv[0]);

        return 1;
    }

    target_window_title = argv[1];

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) 
        {
            PrintHelp(argv[0]);
            exit(0);
        }
        else if (strncmp(argv[i], "--color-threshold=", 18) == 0)
        {
            g_color_threshold = atoi(argv[i] + 18);
            if (g_color_threshold < 0)  
                g_color_threshold = 0;
            if (g_color_threshold > 255) 
                g_color_threshold = 255;
        }
        else if (strncmp(argv[i], "--color-depth=", 14) == 0)
        {
            g_bits_per_channel = atoi(argv[i] + 14);

            if (g_bits_per_channel < 1)
                g_bits_per_channel = 1;
            if (g_bits_per_channel > 8) 
                g_bits_per_channel = 8;
        }
        else if (strncmp(argv[i], "--char=", 7) == 0) 
        {
            wchar_t ch = Utf8ToWchar(argv[i] + 7);

            if (ch != 0) 
                g_block_char = ch;
        }
        if (strncmp(argv[i], "--fps=", 6) == 0)
            g_max_fps = atoi(argv[i] + 6);
        else if (strncmp(argv[i], "--width=", 8) == 0)
            g_buffer_width = atoi(argv[i] + 8);
        else if (strncmp(argv[i], "--height=", 9) == 0)
            g_buffer_height = atoi(argv[i] + 9);
        else if (strncmp(argv[i], "--auto-size", 11) == 0)
            g_auto_size = 1;
        else if (strncmp(argv[i], "--scroll-disable", 16) == 0)
            g_scroll = 0;
        else if (strncmp(argv[i], "--interlacing", 13) == 0)
                g_interlacing = 1;
        else if (strncmp(argv[i], "--verbose", 9) == 0)
            g_verbose = 1;
    }

    return 0;
}

// Function for color cache
int InitColorCache()
{
    for (int r = 0; r < 16; r++)
    {
        for (int g = 0; g < 16; g++) 
        {
            for (int b = 0; b < 16; b++) 
            {
                int true_r = (r * 255) / 15;
                int true_g = (g * 255) / 15;
                int true_b = (b * 255) / 15;

                int len = snprintf(g_ansi_color_cache[r][g][b], 20, "\x1b[38;2;%d;%d;%dm", true_r, true_g, true_b);
                g_ansi_color_lengths[r][g][b] = len;
            }
        }
    }

    return 0;
}

// Start point
int main(int argc, char* argv[]) 
{
    // Parse arguments from command line
    if(ParseCmdArguments(argc, argv) != 0)
        return 1;

    // Setup cache for color
    InitColorCache();

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
    CONSOLE_CURSOR_INFO cci = 
    {
        sizeof(cci), FALSE
    };
    SetConsoleCursorInfo(hStdOut, &cci);

    // Clear console
    printf("\x1b[2J");
    fflush(stdout);

    // Convert utf-8 char to wide
    char utf8char[5] = 
    {
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

    // Prepare memory for "prev-current translate optimization"
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

    // const char move_next_line[] = "\x1b[0m\x1b[1E";
    // const size_t move_next_line_lgt = sizeof(move_next_line) - 1;
    int skip_accumulator = 0;

    // Disable hystory in console
    if (!g_scroll) 
    {
        printf("\033[?1049h\033[2J\033[H");
    }

    // Main loop for render
    while (1) 
    {
        // Frame count, in particular for interlacing
        frame_counter++;

        // Time for fps delay
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
            // Interlacing
            if (g_interlacing & ((y ^ frame_counter) & 1)) // Detect for even (all even number have 1 on last bit position, and else)
            {
                // Verbose code
                InterlacingSkippedFramesStats;

                continue;
            }

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

                // "prev-current translate" optimization
                if (!is_first_frame)
                {
                    BYTE* prev_px = prev_pixels + (y * g_buffer_width + x) * 3;

                    int diff = abs(r - prev_px[2]) + abs(g - prev_px[1]) + abs(b - prev_px[0]);

                    if (diff <= g_color_threshold)
                    {
                        skip_accumulator++;
                        x++;
                        continue;
                    }
                }

                if (skip_accumulator > 0)
                {
                    int target_x = x + 1;
                    char* p_skip = out_buf + pos;
                    *p_skip++ = '\x1b'; *p_skip++ = '[';

                    if (target_x >= 100) 
                    { 
                        *p_skip++ = '0' + (target_x / 100);
                        target_x %= 100;
                        *p_skip++ = '0' + (target_x / 10); 
                    }
                    else if (target_x >= 10) 
                    { 
                        *p_skip++ = '0' + (target_x / 10); 
                    }

                    *p_skip++ = '0' + (target_x % 10);
                    *p_skip++ = 'G';

                    pos += (p_skip - (out_buf + pos));
                    skip_accumulator = 0;
                }

                // Color fast-runner optimization for render:
                // 1 Stage of optimization:
                // code dont print own color on each symbol,
                // he just look on next pixel-colors, and print color once to all colors in row.
                int group_end = x + 1;
                while (group_end < g_buffer_width)
                {
                    // size of x
                    int sx = (group_end * src_width) / g_buffer_width;

                    // Pixel
                    BYTE* pxt = row_ptr + (sx * 3);

                    // 2 Stage fast-runner of optimization: 
                    // Code compare current pixels with pixels from previous frame
                    if (!is_first_frame)
                    {
                        BYTE* prev_pxt = prev_pixels + (y * g_buffer_width + group_end) * 3;

                        // Apply the color zone (outta args)
                        int diff_t = abs(pxt[2] - prev_pxt[2]) + abs(pxt[1] - prev_pxt[1]) + abs(pxt[0] - prev_pxt[0]);
                        if (diff_t <= g_color_threshold)
                            break;
                    }

                    // End of 
                    if (pxt[2] != r || pxt[1] != g || pxt[0] != b)
                        break;

                    group_end++;
                }

                // Count of pixels
                int count = group_end - x;

                if (pos + 32 >= max_buf_size) 
                    break;

                // pos += FastAppendTrueColor(out_buf + pos, r, g, b, g_bits_per_channel);

                // NEW CODE _______
                int r_idx = r >> 4;
                int g_idx = g >> 4;
                int b_idx = b >> 4;

                memcpy(out_buf + pos, g_ansi_color_cache[r_idx][g_idx][b_idx], g_ansi_color_lengths[r_idx][g_idx][b_idx]);
                pos += g_ansi_color_lengths[r_idx][g_idx][b_idx];
                // ------------------

                for (int i = 0; i < count; i++) 
                {
                    if ((size_t) utf8_len >= max_buf_size - pos) 
                        break;

                    memcpy(out_buf + pos, utf8char, utf8_len);
                    pos += (size_t) utf8_len;

                    // Save previous frame
                    BYTE* save_px = prev_pixels + (y * g_buffer_width + (x + i)) * 3;
                    save_px[2] = r;
                    save_px[1] = g;
                    save_px[0] = b;
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

        // Calculate delta of render frame time for fps delay
        ULONGLONG elapsed_time = GetTickCount64() - frame_start;

        // Sweet dreams...
        if (elapsed_time < (ULONGLONG) target_frame_time)
        {
            Sleep(target_frame_time - (int) elapsed_time);
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

    if (!g_scroll) 
    {
        fprintf(stdout, "\033[?1049l");
        fflush(stdout);
    }

    if (g_verbose)
    {
        Sleep(1000);

        fprintf(stderr, "\n\n___ Verbose ___\n");
        fprintf(stderr, "Interlacing skipped rows: %llu\n", g_interlacing_skipped_frames);
        fprintf(stderr, "======================================\n");
        fflush(stderr);
    }

    return 0;
}
