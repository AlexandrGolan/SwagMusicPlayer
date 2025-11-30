#ifndef PLAYER_H
#define PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <time.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <mpg123.h>
#include <alloca.h>

#define WIDTH 168
#define HEIGHT 39
#define LEFT_WIDTH 84
#define IMAGE_X 6
#define IMAGE_Y 6
#define ALSA_BUFFER_SIZE 4096
#define ALSA_PERIOD_SIZE 1024
#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FORMAT SND_PCM_FORMAT_S16_LE

typedef struct {
    char filename[256];
    char title[256];
    char author[256];
    char album[256];
    int duration;
    int time_remaining;
    int file_type;
} Track;

typedef struct {
    Track *tracks;
    int count;
    int current;
    int playing;
    int position;
} Playlist;

extern struct termios original_termios;
extern Playlist playlist;
extern pthread_t audio_thread;
extern pthread_t playback_thread;
extern int audio_thread_running;
extern int playback_thread_running;
extern time_t playback_start_time;
extern time_t last_draw_time;
extern time_t last_timer_update;
extern char music_directory[512];
extern pthread_mutex_t draw_mutex;
extern pthread_mutex_t audio_mutex;
extern snd_pcm_t *pcm_handle;
extern mpg123_handle *mp3_handle;

void msleep(int milliseconds);
void enable_raw_mode();
void disable_raw_mode();
void clear_screen();
void set_cursor_position(int row, int col);

#endif
