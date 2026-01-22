#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "audio.h"
#include "config.h"
#include "playlist.h"

void draw_interface() {
    pthread_mutex_lock(&draw_mutex);

    clear_screen();

    for (int i = 0; i < HEIGHT; i++) {
        set_cursor_position(i, LEFT_WIDTH);
        printf("│");
    }

    display_left_panel();

    if (playlist.count > 0 && playlist.current < playlist.count) {
        Track* current = &playlist.tracks[playlist.current];

        set_cursor_position(2, LEFT_WIDTH + 2);
        printf("Author: %s", current->author);
        set_cursor_position(3, LEFT_WIDTH + 2);
        printf("Name: %s", current->title);
        set_cursor_position(4, LEFT_WIDTH + 2);
        printf("Album: %s", current->album);

        char remaining_str[16];
        format_time(current->time_remaining, remaining_str, sizeof(remaining_str));
        set_cursor_position(6, LEFT_WIDTH + 2);
        printf("Time: %s", remaining_str);

        set_cursor_position(8, LEFT_WIDTH + 2);
        printf(" ");

        int max_tracks = (playlist.count < 20) ? playlist.count : 20;
        for (int i = 0; i < max_tracks; i++) {
            set_cursor_position(9 + i, LEFT_WIDTH + 2);
            if (i == playlist.current) {
                printf("> %d. %s", i + 1, playlist.tracks[i].title);
            } else {
                printf("  %d. %s", i + 1, playlist.tracks[i].title);
            }
        }
    }

    set_cursor_position(HEIGHT - 3, LEFT_WIDTH + 2);
    printf("Status: %s", playlist.playing ? "Playing" : "Paused");

    set_cursor_position(HEIGHT - 2, LEFT_WIDTH + 2);
    printf("←/→: Navigate  SPACE: Play/Pause  q: Quit");

    fflush(stdout);
    last_draw_time = time(NULL);
    last_timer_update = time(NULL);

    pthread_mutex_unlock(&draw_mutex);
}

void display_left_panel() {
    for (int y = 0; y < HEIGHT; y++) {
        set_cursor_position(y, 0);
        for (int x = 0; x < LEFT_WIDTH; x++) {
            printf(" ");
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        set_cursor_position(y, 0);
        printf("│");
        set_cursor_position(y, LEFT_WIDTH - 1);
        printf("│");
    }

    if (!check_tiv_available()) {
        set_cursor_position(HEIGHT / 2 - 1, LEFT_WIDTH / 2 - 19);
        printf("terminal image viewer not installed please install tiv");
        return;
    }

    char* cover_path = find_cover_image(music_directory);
    if (cover_path == NULL) {
        set_cursor_position(HEIGHT / 2, LEFT_WIDTH / 2 - 8);
        printf("COVER ART NOT FOUND");
        return;
    }

    char* ansi_art = render_image_to_ansi(cover_path);
    if (ansi_art == NULL) {
        set_cursor_position(HEIGHT / 2, LEFT_WIDTH / 2 - 10);
        printf("ERROR RENDERING IMAGE");
        return;
    }

    char* line = strtok(ansi_art, "\n");
    int current_y = IMAGE_Y;
    while (line != NULL && current_y < HEIGHT) {
        set_cursor_position(current_y, IMAGE_X);
        printf("%s", line);
        line = strtok(NULL, "\n");
        current_y++;
    }
}

void draw_timer_only() {
    pthread_mutex_lock(&draw_mutex);

    if (playlist.count > 0 && playlist.current < playlist.count) {
        Track* current = &playlist.tracks[playlist.current];

        char remaining_str[16];
        format_time(current->time_remaining, remaining_str, sizeof(remaining_str));

        set_cursor_position(6, LEFT_WIDTH + 2);
        printf("Time: %s", remaining_str);

        fflush(stdout);
        last_timer_update = time(NULL);
    }

    pthread_mutex_unlock(&draw_mutex);
}

void draw_status_only() {
    pthread_mutex_lock(&draw_mutex);

    set_cursor_position(HEIGHT - 3, LEFT_WIDTH + 2);
    printf("Status: %s", playlist.playing ? "Playing" : "Paused");

    fflush(stdout);

    pthread_mutex_unlock(&draw_mutex);
}

void draw_track_list_only() {
    pthread_mutex_lock(&draw_mutex);

    if (playlist.count > 0) {
        int max_tracks = (playlist.count < 20) ? playlist.count : 20;
        for (int i = 0; i < max_tracks; i++) {
            set_cursor_position(9 + i, LEFT_WIDTH + 2);
            if (i == playlist.current) {
                printf("> %d. %s", i + 1, playlist.tracks[i].title);
            } else {
                printf("  %d. %s", i + 1, playlist.tracks[i].title);
            }
        }

        Track* current = &playlist.tracks[playlist.current];
        set_cursor_position(2, LEFT_WIDTH + 2);
        printf("Author: %s", current->author);
        set_cursor_position(3, LEFT_WIDTH + 2);
        printf("Name: %s", current->title);
        set_cursor_position(4, LEFT_WIDTH + 2);
        printf("Album: %s", current->album);

        fflush(stdout);
    }

    pthread_mutex_unlock(&draw_mutex);
}

void format_time(int seconds, char* buffer, size_t size) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    snprintf(buffer, size, "%d:%02d", minutes, secs);
}

int read_key() {
    struct timeval tv;
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 50000;

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        char c;
        int nread = read(STDIN_FILENO, &c, 1);
        if (nread == 1) {
            if (c == '\033') {
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) != 1) {
                    return '\033';
                }
                if (read(STDIN_FILENO, &seq[1], 1) != 1) {
                    return '\033';
                }

                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'C':
                            return 'R';
                        case 'D':
                            return 'L';
                    }
                }
            }
            return c;
        }
    }

    return -1;
}

void handle_input() {
    int key = read_key();

    switch (key) {
        case 'R':
            next_track();
            break;

        case 'L':
            prev_track();
            break;

        case ' ':
            toggle_pause();
            break;

        case 'q':
            audio_thread_running = 0;
            playlist.playing = 0;
            msleep(200);
            stop_playback();
            break;
    }
}

void msleep(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void enable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &original_termios);
    raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void set_cursor_position(int row, int col) {
    printf("\033[%d;%dH", row, col);
}
