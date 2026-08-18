#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WIDTH  80
#define HEIGHT 25
#define TOTAL_FRAMES 10000

typedef struct {
    unsigned char r, g, b;
} Pixel;

Pixel current_frame[HEIGHT][WIDTH];
Pixel prev_frame[HEIGHT][WIDTH];

char out_buf[128 * 1024];
size_t pos = 0;
int g_bits_per_channel = 8;

void init_terminal() {
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    printf("\x1b[?25l");
}

void update_dynamic_scene(int frame_num) 
{
    for (int y = 0; y < HEIGHT; y++) 
    {
        for (int x = 0; x < WIDTH; x++) {
            current_frame[y][x].r = (unsigned char)((x * 3 + y * 7 + frame_num * 13) % 256);
            current_frame[y][x].g = (unsigned char)((y * 5 + x * 11 + frame_num * 17) % 256);
            current_frame[y][x].b = (unsigned char)((x * y + frame_num * 19) % 256);
        }
    }
}

int SnprintfCustomBitColor(char* buf, size_t max_len, BYTE r, BYTE g, BYTE b, int bits_per_channel) {
    if (bits_per_channel >= 8) 
    {
        return snprintf(buf, max_len, "\x1b[38;2;%d;%d;%dm", r, g, b);
    }

    BYTE mask = (BYTE)(0xFF << (8 - bits_per_channel));
    BYTE compressed_r = r & mask;
    BYTE compressed_g = g & mask;
    BYTE compressed_b = b & mask;
    compressed_r |= (compressed_r >> bits_per_channel);
    compressed_g |= (compressed_g >> bits_per_channel);
    compressed_b |= (compressed_b >> bits_per_channel);

    return snprintf(buf, max_len, "\x1b[38;2;%d;%d;%dm", compressed_r, compressed_g, compressed_b);
}

static inline size_t FastAppendTrueColor(char* buf, BYTE r, BYTE g, BYTE b, int bits_per_channel) {
    if (bits_per_channel < 8) {
        BYTE mask = (BYTE)(0xFF << (8 - bits_per_channel));
        r &= mask; g &= mask; b &= mask;
        r |= (r >> bits_per_channel);
        g |= (g >> bits_per_channel);
        b |= (b >> bits_per_channel);
    }
    char* p = buf;
    *p++ = '\x1b'; *p++ = '['; *p++ = '3'; *p++ = '8'; *p++ = ';'; *p++ = '2'; *p++ = ';';
    if (r >= 100) { *p++ = '0' + (r / 100); r %= 100; *p++ = '0' + (r / 10); }
    else if (r >= 10) { *p++ = '0' + (r / 10); }
    *p++ = '0' + (r % 10); *p++ = ';';
    if (g >= 100) { *p++ = '0' + (g / 100); g %= 100; *p++ = '0' + (g / 10); }
    else if (g >= 10) { *p++ = '0' + (g / 10); }
    *p++ = '0' + (g % 10); *p++ = ';';
    if (b >= 100) { *p++ = '0' + (b / 100); b %= 100; *p++ = '0' + (b / 10); }
    else if (b >= 10) { *p++ = '0' + (b / 10); }
    *p++ = '0' + (b % 10); *p++ = 'm';
    return p - buf;
}

void render_with_snprintf() {
    pos = 0;
    pos += sprintf(out_buf + pos, "\x1b[H");
    int skip_accumulator = 0;

    for (int y = 0; y < HEIGHT; y++) {
        int x = 0;
        while (x < WIDTH) {
            Pixel curr = current_frame[y][x];
            Pixel prev = prev_frame[y][x];

            if (curr.r == prev.r && curr.g == prev.g && curr.b == prev.b) {
                skip_accumulator++;
                x++;
                continue;
            }

            if (skip_accumulator > 0) {
                int n_skip = snprintf(out_buf + pos, 1024, "\x1b[%dG", x + 1);
                pos += n_skip;
                skip_accumulator = 0;
            }

            int n = SnprintfCustomBitColor(out_buf + pos, 1024, curr.r, curr.g, curr.b, g_bits_per_channel);
            pos += n;
            out_buf[pos++] = '#';
            x++;
        }
        pos += sprintf(out_buf + pos, "\x1b[0m\n");
    }
    fwrite(out_buf, 1, pos, stdout);
    fflush(stdout);
    memcpy(prev_frame, current_frame, sizeof(current_frame));
}

void render_with_fast_bytes() {
    pos = 0;
    pos += sprintf(out_buf + pos, "\x1b[H");
    int skip_accumulator = 0;

    for (int y = 0; y < HEIGHT; y++) {
        int x = 0;
        while (x < WIDTH) {
            Pixel curr = current_frame[y][x];
            Pixel prev = prev_frame[y][x];

            if (curr.r == prev.r && curr.g == prev.g && curr.b == prev.b) {
                skip_accumulator++;
                x++;
                continue;
            }

            if (skip_accumulator > 0) {
                int target_x = x + 1;
                char* p_skip = out_buf + pos;
                *p_skip++ = '\x1b'; *p_skip++ = '[';
                if (target_x >= 100) { *p_skip++ = '0' + (target_x / 100); target_x %= 100; *p_skip++ = '0' + (target_x / 10); }
                else if (target_x >= 10) { *p_skip++ = '0' + (target_x / 10); }
                *p_skip++ = '0' + (target_x % 10); *p_skip++ = 'G';
                pos += (p_skip - (out_buf + pos));
                skip_accumulator = 0;
            }

            pos += FastAppendTrueColor(out_buf + pos, curr.r, curr.g, curr.b, g_bits_per_channel);
            out_buf[pos++] = '#';
            x++;
        }
        pos += sprintf(out_buf + pos, "\x1b[0m\n");
    }
    fwrite(out_buf, 1, pos, stdout);
    fflush(stdout);
    memcpy(prev_frame, current_frame, sizeof(current_frame));
}

int main() {
    init_terminal();
    LARGE_INTEGER frequency, start_time, end_time;
    QueryPerformanceFrequency(&frequency);

    double time_snprintf = 0.0;
    double time_fast_bytes = 0.0;

    printf("\x1b[2J\x1b[H--- RUNNING BENCHMARK: SNPRINTF METHOD ---\n");
    Sleep(1000);
    for (int f = 0; f < TOTAL_FRAMES; f++) {
        update_dynamic_scene(f);
        QueryPerformanceCounter(&start_time);
        render_with_snprintf();
        QueryPerformanceCounter(&end_time);
        time_snprintf += (double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart;
    }

    memset(prev_frame, 0, sizeof(prev_frame));

    printf("\x1b[2J\x1b[H--- RUNNING BENCHMARK: FAST BYTE METHOD ---\n");
    Sleep(1000);
    for (int f = 0; f < TOTAL_FRAMES; f++) {
        update_dynamic_scene(f);
        QueryPerformanceCounter(&start_time);
        render_with_fast_bytes();
        QueryPerformanceCounter(&end_time);
        time_fast_bytes += (double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart;
    }

    printf("\x1b[2J\x1b[H");
    printf("==================================================\n");
    printf("         TRUE COLOR OPTIMIZATION REPORT           \n");
    printf("==================================================\n");
    printf("Total Frames Rendered: %d\n\n", TOTAL_FRAMES);
    printf(" 1. OLD SNPRINTF METHOD:\n");
    printf("    Pure processing time: %.4f seconds\n\n", time_snprintf);
    printf(" 2. NEW FAST BYTE APPEND METHOD:\n");
    printf("    Pure processing time: %.4f seconds\n\n", time_fast_bytes);
    printf("==================================================\n");

    double speedup = (time_snprintf / time_fast_bytes) * 100.0;
    printf("PERFORMANCE GAIN (CPU SIDE): %.1f%%\n", speedup);
    printf("==================================================\n");

    printf("\x1b[?25h\x1b[0m\n");
    return 0;
}
