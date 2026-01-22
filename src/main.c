#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "audio.h"
#include "config.h"
#include "file_utils.h"
#include "player.h"
#include "playlist.h"
#include "ui.h"

Playlist playlist;
pthread_t audio_thread;
pthread_t playback_thread;
int audio_thread_running = 1;
int playback_thread_running = 0;
time_t playback_start_time = 0;
time_t last_draw_time = 0;
time_t last_timer_update = 0;
char music_directory[512];
pthread_mutex_t draw_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
snd_pcm_t* pcm_handle = NULL;
mpg123_handle* mp3_handle = NULL;
struct termios original_termios;
volatile sig_atomic_t resize_flag = 0;

void handle_resize(int sig) {
    (void)sig;
    resize_flag = 1;
}

void* audio_thread_function(void* arg) {
    (void)arg;
    time_t last_update_time = time(NULL);

    while (audio_thread_running) {
        if (resize_flag) {
            resize_flag = 0;
            draw_interface();
        }

        if (playlist.count == 0) {
            msleep(100);
            continue;
        }

        if (!playback_thread_running && playlist.playing) {
            auto_next_track();
        }

        if (playlist.playing && playback_thread_running && playlist.current < playlist.count) {
            time_t current_time = time(NULL);

            if (current_time != last_update_time) {
                playlist.position = current_time - playback_start_time;
                last_update_time = current_time;

                Track* current_track = &playlist.tracks[playlist.current];
                if (current_track->duration > playlist.position) {
                    current_track->time_remaining = current_track->duration - playlist.position;
                } else {
                    current_track->time_remaining = 0;
                }

                if (current_time != last_timer_update) {
                    draw_timer_only();
                }

                if (playlist.position >= current_track->duration - 2) {
                    auto_next_track();
                }
            }
        }

        msleep(50);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <music_directory>\n", argv[0]);
        return 1;
    }

    signal(SIGWINCH, handle_resize);

    if (pthread_mutex_init(&draw_mutex, NULL) != 0 || pthread_mutex_init(&audio_mutex, NULL) != 0) {
        printf("Mutex init failed\n");
        return 1;
    }

    strncpy(music_directory, argv[1], sizeof(music_directory) - 1);
    music_directory[sizeof(music_directory) - 1] = '\0';

    update_terminal_size();

    if (TERM_WIDTH < MIN_WIDTH || TERM_HEIGHT < MIN_HEIGHT) {
        printf("Terminal too small. Minimum: %dx%d, Current: %dx%d\n",
               MIN_WIDTH,
               MIN_HEIGHT,
               TERM_WIDTH,
               TERM_HEIGHT);
        return 1;
    }

    if (!check_tiv_available()) {
        printf("Warning: tiv not found. Install with: sudo pacman -S tiv\n");
    }

    printf("Initializing audio system...\n");
    if (init_audio_system() != 0) {
        printf("Failed to initialize audio system\n");
        return 1;
    }

    enable_raw_mode();

    printf("Loading playlist...\n");
    load_playlist(argv[1]);

    if (playlist.count == 0) {
        printf("No tracks loaded. Exiting.\n");
        disable_raw_mode();
        return 1;
    }

    if (pthread_create(&audio_thread, NULL, audio_thread_function, NULL) != 0) {
        printf("Failed to create audio thread\n");
        disable_raw_mode();
        return 1;
    }

    printf("SwagMusicPlayer started. Loaded %d tracks.\n", playlist.count);
    printf("Volume: %d%%\n", current_volume);
    printf("Press any key to continue...");
    getchar();

    draw_interface();

    while (audio_thread_running) {
        handle_input();

        if (resize_flag) {
            resize_flag = 0;
            draw_interface();
        }

        msleep(50);
    }

    pthread_join(audio_thread, NULL);
    if (playback_thread_running) {
        playback_thread_running = 0;
        pthread_join(playback_thread, NULL);
    }

    disable_raw_mode();
    cleanup_audio_system();

    if (playlist.tracks) {
        free(playlist.tracks);
    }

    pthread_mutex_destroy(&draw_mutex);
    pthread_mutex_destroy(&audio_mutex);

    clear_screen();
    printf("SMP closed. Thank you for using!\n");

    return 0;
}
