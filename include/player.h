#ifndef PLAYER_H
#define PLAYER_H

#define _POSIX_C_SOURCE 200112L
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <dirent.h>
#include <mpg123.h>
#include <pthread.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <taglib/tag_c.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

extern int TERM_WIDTH;
extern int TERM_HEIGHT;
extern int LEFT_WIDTH;
extern int RIGHT_WIDTH;
extern int IMAGE_WIDTH;
extern int IMAGE_HEIGHT;
extern int IMAGE_X;
extern int IMAGE_Y;
extern int current_volume;
extern int playlist_offset;

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
    Track* tracks;
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
extern snd_pcm_t* pcm_handle;
extern mpg123_handle* mp3_handle;

void msleep(int milliseconds);
void enable_raw_mode();
void disable_raw_mode();
void clear_screen();
void set_cursor_position(int row, int col);
void update_terminal_size();
void change_volume(int delta);
void set_alsa_volume();

#endif
