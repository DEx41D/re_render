#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WIDTH  60
#define HEIGHT 20
#define TOTAL_FRAMES 50

unsigned long long stat_classic_ansi_count = 0;
unsigned long long stat_classic_ansi_chars = 0;

unsigned long long stat_diff_ansi_count = 0;
unsigned long long stat_diff_ansi_chars = 0;

typedef struct {
    unsigned char r, g, b;
} Pixel;

Pixel current_frame[HEIGHT][WIDTH];
Pixel prev_frame[HEIGHT][WIDTH];

char out_buf[64 * 1024];
size_t pos = 0;

unsigned long long stat_classic_bytes = 0;
unsigned long long stat_classic_colors = 0;
unsigned long long stat_diff_bytes = 0;
unsigned long long stat_diff_colors = 0;
unsigned long long stat_diff_skips = 0;

void init_terminal() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    printf("\x1b[?25l"); 
}

void update_scene(int frame_num) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            current_frame[y][x].r = 10;
            current_frame[y][x].g = 40 + (y * 3);
            current_frame[y][x].b = 10;
        }
    }
    int box_x = (frame_num) % (WIDTH - 10);
    int box_y = 5;
    for (int y = box_y; y < box_y + 6; y++) {
        for (int x = box_x; x < box_x + 10; x++) {
            current_frame[y][x].r = 230;
            current_frame[y][x].g = 20;
            current_frame[y][x].b = 20;
        }
    }
}

void render_classic() {
    pos = 0;
    pos += sprintf(out_buf + pos, "\x1b[H");

    for (int y = 0; y < HEIGHT; y++) {
        int x = 0;
        while (x < WIDTH) {
            Pixel curr = current_frame[y][x];

            int group_end = x + 1;
            while (group_end < WIDTH) {
                Pixel next_curr = current_frame[y][group_end];
                if (next_curr.r != curr.r || next_curr.g != curr.g || next_curr.b != curr.b)
                    break;
                group_end++;
            }

            int count = group_end - x;
            pos += sprintf(out_buf + pos, "\x1b[38;2;%d;%d;%dm", curr.r, curr.g, curr.b);
            stat_classic_colors++;

            for (int i = 0; i < count; i++) {
                out_buf[pos++] = '#';
            }
            x = group_end;
        }
        pos += sprintf(out_buf + pos, "\x1b[0m\x1b[1E");
    }

    fwrite(out_buf, 1, pos, stdout);
    fflush(stdout);
    stat_classic_bytes += pos;
}

void render_diff() {
    pos = 0;
    pos += sprintf(out_buf + pos, "\x1b[H");

    for (int y = 0; y < HEIGHT; y++) {
        int x = 0;
        int skip_accumulator = 0; 

        while (x < WIDTH) {
            Pixel curr = current_frame[y][x];
            Pixel prev = prev_frame[y][x];

            if (curr.r == prev.r && curr.g == prev.g && curr.b == prev.b) {
                skip_accumulator++;
                x++;
                continue;
            }

            if (skip_accumulator > 0) {
                pos += sprintf(out_buf + pos, "\x1b[%dG", x + 1);
                stat_diff_skips++;
                skip_accumulator = 0;
            }

            int group_end = x + 1;
            while (group_end < WIDTH) {
                Pixel next_curr = current_frame[y][group_end];
                Pixel next_prev = prev_frame[y][group_end];

                if (next_curr.r == next_prev.r && next_curr.g == next_prev.g && next_curr.b == next_prev.b)
                    break;
                if (next_curr.r != curr.r || next_curr.g != curr.g || next_curr.b != curr.b)
                    break;

                group_end++;
            }

            int count = group_end - x;
            pos += sprintf(out_buf + pos, "\x1b[38;2;%d;%d;%dm", curr.r, curr.g, curr.b);
            stat_diff_colors++;

            for (int i = 0; i < count; i++) {
                out_buf[pos++] = '#';
            }
            x = group_end;
        }

        pos += sprintf(out_buf + pos, "\x1b[0m\x1b[1E");
    }

    fwrite(out_buf, 1, pos, stdout);
    fflush(stdout);
    stat_diff_bytes += pos;

    memcpy(prev_frame, current_frame, sizeof(current_frame));
}

int main() {
    init_terminal();

    // Переменные для замера точного времени
    LARGE_INTEGER frequency, start_time, end_time;
    QueryPerformanceFrequency(&frequency);

    double classic_total_time = 0.0;
    double diff_total_time = 0.0;

    // ТЕСТ 1: Обычный рендеринг
    printf("\x1b[2J\x1b[H--- ТЕСТ 1: ОБЫЧНЫЙ ВЫВОД (Подождите...) ---\n");
    Sleep(1000);
    printf("\x1b[2J");
    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        update_scene(frame);

        // Замеряем только чистую работу функции отрисовки
        QueryPerformanceCounter(&start_time);
        render_classic();
        QueryPerformanceCounter(&end_time);

        classic_total_time += (double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart;
        Sleep(15);
    }

    memset(prev_frame, 0, sizeof(prev_frame));

    // ТЕСТ 2: Дифференциальный рендеринг
    printf("\x1b[2J\x1b[H--- ТЕСТ 2: ДИФФЕРЕНЦИАЛЬНЫЙ ВЫВОД (Подождите...) ---\n");
    Sleep(1000);
    printf("\x1b[2J");
    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        update_scene(frame);

        // Замеряем только чистую работу функции отрисовки
        QueryPerformanceCounter(&start_time);
        render_diff();
        QueryPerformanceCounter(&end_time);

        diff_total_time += (double)(end_time.QuadPart - start_time.QuadPart) / frequency.QuadPart;
        Sleep(15);
    }

    // РАСЧЕТ ДЕТАЛЬНОЙ СТАТИСТИКИ ANSI-ПОСЛЕДОВАТЕЛЬНОСТЕЙ
    stat_classic_ansi_count = stat_classic_colors + ((HEIGHT + 1) * TOTAL_FRAMES);
    unsigned long long total_hashes = HEIGHT * WIDTH * TOTAL_FRAMES;
    stat_classic_ansi_chars = stat_classic_bytes - total_hashes;

    stat_diff_ansi_count = stat_diff_colors + stat_diff_skips + ((HEIGHT + 1) * TOTAL_FRAMES);
    stat_diff_ansi_chars = stat_diff_bytes - (total_hashes / 10);

    // ВЫВОД ПОЛНОГО ОТЧЕТА НА РУССКОМ
    printf("\x1b[2J\x1b[H");
    printf("==================================================\n");
    printf("          ОТЧЕТ ПО ОПТИМИЗАЦИИ ВЫВОДА            \n");
    printf("==================================================\n");
    printf("Всего отрендерено кадров: %d\n\n", TOTAL_FRAMES);

    printf(" ОБЫЧНЫЙ ВЫВОД:\n");
    printf("    Время выполнения (чистое): %.4f сек\n", classic_total_time);
    printf("    Всего отправлено данных:   %llu байт\n", stat_classic_bytes);
    printf("    ANSI-последовательностей:  %llu шт.\n", stat_classic_ansi_count);
    printf("    Вес всех ANSI-команд:      %llu символов (байт)\n", stat_classic_ansi_chars);
    printf("    Из них кодов цвета:        %llu шт.\n\n", stat_classic_colors);

    printf(" ДИФФЕРЕНЦИАЛЬНЫЙ ВЫВОД:\n");
    printf("    Время выполнения (чистое): %.4f сек\n", diff_total_time);
    printf("    Всего отправлено данных:   %llu байт\n", stat_diff_bytes);
    printf("    ANSI-последовательностей:  %llu шт.\n", stat_diff_ansi_count);
    printf("    Вес всех ANSI-команд:      %llu символов (байт)\n", stat_diff_ansi_chars);
    printf("    Из них кодов цвета:        %llu шт.\n", stat_diff_colors);
    printf("    Из них прыжков курсора:    %llu шт.\n\n", stat_diff_skips);

    printf("==================================================\n");
    double saved_bytes_pct = (1.0 - ((double)stat_diff_bytes / stat_classic_bytes)) * 100.0;
    double saved_ansi_pct = (1.0 - ((double)stat_diff_ansi_count / stat_classic_ansi_count)) * 100.0;
    double saved_chars_pct = (1.0 - ((double)stat_diff_ansi_chars / stat_classic_ansi_chars)) * 100.0;

    // Расчет реального ускорения вывода
    double speedup_pct = (classic_total_time / diff_total_time) * 100.0;

    printf("РЕЗУЛЬТАТЫ СЖАТИЯ ПОТОКА:\n");
    printf("Общая экономия трафика:          %.2f%%\n", saved_bytes_pct);
    printf("Сокращение КОЛИЧЕСТВА команд:    %.2f%%\n", saved_ansi_pct);
    printf("Сокращение ОБЪЕМА (символов) ANSI: %.2f%%\n", saved_chars_pct);
    printf("--------------------------------------------------\n");
    printf("ФАКТИЧЕСКОЕ УСКОРЕНИЕ ВЫВОДА:    %.1f%%\n", speedup_pct);
    printf("==================================================\n");

    printf("\x1b[?25h\x1b[0m\n");
    return 0;
}

