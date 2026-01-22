#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include "audio.h"
#include "config.h"
#include "playlist.h"

void draw_center_line() {
    for (int i = 0; i < TERM_HEIGHT; i++) {
        set_cursor_position(i, LEFT_WIDTH);
        printf("│");
    }
}

void draw_header() {
    if (TERM_HEIGHT < 3) {
        return;
    }

    set_cursor_position(0, LEFT_WIDTH + 1);
    printf(" NGSMP Media Player ");

    set_cursor_position(1, LEFT_WIDTH + 1);
    for (int i = 0; i < RIGHT_WIDTH - 2; i++) {
        printf("─");
    }
}

void update_playlist_offset() {
    if (playlist.count == 0) {
        return;
    }

    int max_visible = TERM_HEIGHT - 14;
    if (max_visible < 1) {
        max_visible = 1;
    }

    if (playlist_offset > playlist.current) {
        playlist_offset = playlist.current;
    }

    if (playlist.current >= playlist_offset + max_visible) {
        playlist_offset = playlist.current - max_visible + 1;
    }

    if (playlist_offset < 0) {
        playlist_offset = 0;
    }

    if (playlist_offset > playlist.count - max_visible) {
        playlist_offset = playlist.count - max_visible;
        if (playlist_offset < 0) {
            playlist_offset = 0;
        }
    }
}

void draw_volume_display() {
    if (TERM_HEIGHT < 8) {
        return;
    }

    int bar_width = 20;
    if (RIGHT_WIDTH < 30) {
        bar_width = RIGHT_WIDTH - 10;
    }

    set_cursor_position(2, LEFT_WIDTH + 1);
    printf(" Volume: %3d%% ", current_volume);

    int filled = (current_volume * bar_width) / MAX_VOLUME;

    set_cursor_position(2, LEFT_WIDTH + 15);
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    printf("]");
}

void draw_footer() {
    if (TERM_HEIGHT < 5) {
        return;
    }

    int footer_y = TERM_HEIGHT - 3;

    set_cursor_position(footer_y, LEFT_WIDTH + 1);
    for (int i = 0; i < RIGHT_WIDTH - 2; i++) {
        printf("─");
    }

    set_cursor_position(footer_y + 1, LEFT_WIDTH + 1);
    printf(" %s | Tracks: %d/%d | Vol: %d%% ",
           playlist.playing ? "▶" : "⏸",
           playlist.current + 1,
           playlist.count,
           current_volume);

    set_cursor_position(footer_y + 2, LEFT_WIDTH + 1);
    printf(" ←/→:Nav ↑/↓:Vol SPACE:Play/Pause q:Quit ");
}

void draw_progress_bar() {
    if (playlist.count == 0 || playlist.current >= playlist.count) {
        return;
    }
    if (TERM_HEIGHT < 10) {
        return;
    }

    Track* current = &playlist.tracks[playlist.current];
    int bar_width = RIGHT_WIDTH - 20;
    if (bar_width < 20) {
        bar_width = 20;
    }

    set_cursor_position(7, LEFT_WIDTH + 1);

    if (current->duration > 0) {
        float progress = (float)playlist.position / current->duration;
        if (progress > 1.0f) {
            progress = 1.0f;
        }
        if (progress < 0.0f) {
            progress = 0.0f;
        }

        int filled = (int)(progress * bar_width);

        printf(" [");

        for (int i = 0; i < bar_width; i++) {
            if (i < filled) {
                printf("█");
            } else {
                printf("░");
            }
        }

        printf("] ");

        char elapsed[16], total[16];
        format_time(playlist.position, elapsed, sizeof(elapsed));
        format_time(current->duration, total, sizeof(total));

        printf("%s / %s", elapsed, total);
    } else {
        printf(" [");
        for (int i = 0; i < bar_width; i++) {
            printf("░");
        }
        printf("] --:-- / --:--");
    }
}

void draw_track_info() {
    if (TERM_HEIGHT < 8) {
        return;
    }

    if (playlist.count > 0 && playlist.current < playlist.count) {
        Track* current = &playlist.tracks[playlist.current];

        char title_display[RIGHT_WIDTH - 2];
        char artist_display[RIGHT_WIDTH - 2];

        snprintf(title_display, sizeof(title_display), "Title: %s", current->title);
        snprintf(artist_display, sizeof(artist_display), "Author: %s", current->author);

        if (strlen(title_display) > (size_t)(RIGHT_WIDTH - 10)) {
            title_display[RIGHT_WIDTH - 13] = '.';
            title_display[RIGHT_WIDTH - 12] = '.';
            title_display[RIGHT_WIDTH - 11] = '.';
            title_display[RIGHT_WIDTH - 10] = '\0';
        }

        if (strlen(artist_display) > (size_t)(RIGHT_WIDTH - 10)) {
            artist_display[RIGHT_WIDTH - 13] = '.';
            artist_display[RIGHT_WIDTH - 12] = '.';
            artist_display[RIGHT_WIDTH - 11] = '.';
            artist_display[RIGHT_WIDTH - 10] = '\0';
        }

        set_cursor_position(3, LEFT_WIDTH + 1);
        printf(" %s", artist_display);

        set_cursor_position(4, LEFT_WIDTH + 1);
        printf(" %s", title_display);

        set_cursor_position(5, LEFT_WIDTH + 1);
        printf(" Album: %s | %s", current->album, current->file_type == 1 ? "MP3" : "WAV");
    } else {
        set_cursor_position(3, LEFT_WIDTH + 1);
        printf(" No track loaded");
    }
}

void draw_playlist() {
    if (TERM_HEIGHT < 12) {
        return;
    }

    int start_y = 9;
    int available_height = TERM_HEIGHT - start_y - 4;
    if (available_height < 3) {
        return;
    }

    set_cursor_position(start_y - 1, LEFT_WIDTH + 1);
    for (int i = 0; i < RIGHT_WIDTH - 2; i++) {
        printf("─");
    }

    set_cursor_position(start_y, LEFT_WIDTH + 1);
    printf(" Playlist (%d-%d of %d)",
           playlist_offset + 1,
           (playlist_offset + available_height - 1 < playlist.count) ? playlist_offset + available_height - 1
                                                                     : playlist.count,
           playlist.count);

    update_playlist_offset();

    int max_tracks = available_height - 1;
    if (max_tracks > playlist.count - playlist_offset) {
        max_tracks = playlist.count - playlist_offset;
    }

    for (int i = 0; i < max_tracks; i++) {
        int y = start_y + 1 + i;
        if (y >= TERM_HEIGHT - 3) {
            break;
        }

        set_cursor_position(y, LEFT_WIDTH + 1);

        int track_index = playlist_offset + i;
        if (track_index >= playlist.count) {
            break;
        }

        char track_display[RIGHT_WIDTH - 6];
        snprintf(track_display, sizeof(track_display), "%s", playlist.tracks[track_index].title);

        if (strlen(track_display) > (size_t)(RIGHT_WIDTH - 8)) {
            track_display[RIGHT_WIDTH - 11] = '.';
            track_display[RIGHT_WIDTH - 10] = '.';
            track_display[RIGHT_WIDTH - 9] = '.';
            track_display[RIGHT_WIDTH - 8] = '\0';
        }

        if (track_index == playlist.current) {
            printf(" ▶ %2d. %s", track_index + 1, track_display);
        } else {
            printf("   %2d. %s", track_index + 1, track_display);
        }
    }

    for (int i = max_tracks; i < available_height - 1; i++) {
        int y = start_y + 1 + i;
        if (y < TERM_HEIGHT - 3) {
            set_cursor_position(y, LEFT_WIDTH + 1);
            for (int x = 0; x < RIGHT_WIDTH - 2; x++) {
                printf(" ");
            }
        }
    }
}

void draw_interface() {
    pthread_mutex_lock(&draw_mutex);

    update_terminal_size();

    if (TERM_WIDTH < MIN_WIDTH || TERM_HEIGHT < MIN_HEIGHT) {
        clear_screen();
        printf("Terminal too small. Minimum: %dx%d\nCurrent: %dx%d\n",
               MIN_WIDTH,
               MIN_HEIGHT,
               TERM_WIDTH,
               TERM_HEIGHT);
        fflush(stdout);
        pthread_mutex_unlock(&draw_mutex);
        return;
    }

    clear_screen();

    draw_center_line();
    display_left_panel();

    draw_header();
    draw_volume_display();
    draw_track_info();
    draw_progress_bar();
    draw_playlist();
    draw_footer();

    fflush(stdout);
    last_draw_time = time(NULL);
    last_timer_update = time(NULL);

    pthread_mutex_unlock(&draw_mutex);
}

void display_left_panel() {
    if (LEFT_WIDTH < 10 || TERM_HEIGHT < 10) {
        return;
    }

    for (int y = 0; y < TERM_HEIGHT; y++) {
        set_cursor_position(y, 0);
        for (int x = 0; x < LEFT_WIDTH; x++) {
            printf(" ");
        }
    }

    for (int y = 0; y < TERM_HEIGHT; y++) {
        set_cursor_position(y, 0);
        printf("│");
    }

    if (!check_tiv_available()) {
        int msg_y = TERM_HEIGHT / 2;
        int msg_x = (LEFT_WIDTH - 26) / 2;
        if (msg_y >= 0 && msg_y < TERM_HEIGHT && msg_x >= 0 && msg_x + 26 < LEFT_WIDTH) {
            set_cursor_position(msg_y, msg_x);
            printf("Install 'tiv' for album art");
        }
        return;
    }

    char* cover_path = find_cover_image(music_directory);
    if (cover_path == NULL) {
        int msg_y = TERM_HEIGHT / 2;
        int msg_x = (LEFT_WIDTH - 11) / 2;
        if (msg_y >= 0 && msg_y < TERM_HEIGHT && msg_x >= 0 && msg_x + 11 < LEFT_WIDTH) {
            set_cursor_position(msg_y, msg_x);
            printf("No album art");
        }
        return;
    }

    char* ansi_art = render_image_to_ansi(cover_path);
    if (ansi_art == NULL) {
        int msg_y = TERM_HEIGHT / 2;
        int msg_x = (LEFT_WIDTH - 12) / 2;
        if (msg_y >= 0 && msg_y < TERM_HEIGHT && msg_x >= 0 && msg_x + 12 < LEFT_WIDTH) {
            set_cursor_position(msg_y, msg_x);
            printf("Error loading");
        }
        return;
    }

    int start_y = (TERM_HEIGHT - IMAGE_HEIGHT) / 2;
    if (start_y < 1) {
        start_y = 1;
    }

    int start_x = (LEFT_WIDTH - IMAGE_WIDTH) / 2;
    if (start_x < 1) {
        start_x = 1;
    }

    printf("\033[0m");

    char* line_start = ansi_art;
    int line_num = 0;

    while (*line_start && line_num < IMAGE_HEIGHT && (start_y + line_num) < TERM_HEIGHT) {
        char* line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }

        int line_len = strlen(line_start);
        if (line_len > 0) {
            set_cursor_position(start_y + line_num, start_x);
            printf("%s", line_start);
        }

        line_num++;

        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    printf("\033[0m");
}

void draw_timer_only() {
    pthread_mutex_lock(&draw_mutex);

    if (playlist.count > 0 && playlist.current < playlist.count && TERM_HEIGHT >= 10) {
        draw_progress_bar();
        fflush(stdout);
        last_timer_update = time(NULL);
    }

    pthread_mutex_unlock(&draw_mutex);
}

void draw_status_only() {
    pthread_mutex_lock(&draw_mutex);

    if (TERM_HEIGHT >= 5) {
        int footer_y = TERM_HEIGHT - 3;
        set_cursor_position(footer_y + 1, LEFT_WIDTH + 1);
        printf(" %s | Tracks: %d/%d | Vol: %d%% ",
               playlist.playing ? "▶" : "⏸",
               playlist.current + 1,
               playlist.count,
               current_volume);
        fflush(stdout);
    }

    pthread_mutex_unlock(&draw_mutex);
}

void draw_volume_only() {
    pthread_mutex_lock(&draw_mutex);

    if (TERM_HEIGHT >= 8) {
        draw_volume_display();
        fflush(stdout);
    }

    pthread_mutex_unlock(&draw_mutex);
}

void draw_track_list_only() {
    pthread_mutex_lock(&draw_mutex);

    if (playlist.count > 0 && TERM_HEIGHT >= 12) {
        update_playlist_offset();
        draw_playlist();
        fflush(stdout);
    }

    pthread_mutex_unlock(&draw_mutex);
}

void format_time(int seconds, char* buffer, size_t size) {
    if (seconds < 0) {
        seconds = 0;
    }
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
                        case 'A':
                            return 'U';
                        case 'B':
                            return 'D';
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

void change_volume(int delta) {
    int new_volume = current_volume + delta;
    if (new_volume < MIN_VOLUME) {
        new_volume = MIN_VOLUME;
    }
    if (new_volume > MAX_VOLUME) {
        new_volume = MAX_VOLUME;
    }

    if (new_volume != current_volume) {
        current_volume = new_volume;
        draw_volume_only();
        draw_status_only();
    }
}

void handle_input() {
    int key = read_key();

    switch (key) {
        case 'R':
        case 'n':
            next_track();
            break;

        case 'L':
        case 'p':
            prev_track();
            break;

        case 'U':
            change_volume(VOLUME_STEP);
            break;

        case 'D':
            change_volume(-VOLUME_STEP);
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

        case 'r':
            update_terminal_size();
            draw_interface();
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
    printf("\033[%d;%dH", row + 1, col + 1);
}
