#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <stddef.h>
#include <sys/select.h>
#include <sys/time.h>
#include <alloca.h>
#include <strings.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <mpg123.h>

#define WIDTH 168
#define HEIGHT 39
#define LEFT_WIDTH 84
#define RIGHT_WIDTH 84
#define IMAGE_WIDTH 80
#define IMAGE_HEIGHT 25

int image_x = 6;
int image_y = 6;
int image_width = 80;
int image_height = 25;

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

struct termios original_termios;
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

snd_pcm_t *pcm_handle = NULL;
mpg123_handle *mp3_handle = NULL;

int check_tiv_available();
char* find_cover_image(const char *directory);
char* render_image_to_ansi(const char *image_path);
void draw_timer_only();
void format_time(int seconds, char *buffer, size_t size);
void draw_track_list_only();
void draw_status_only();
int init_audio_system();
void cleanup_audio_system();
void list_audio_devices();

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

int check_tiv_available() {
    return system("which tiv > /dev/null 2>&1") == 0;
}

char* find_cover_image(const char *directory) {
    static char cover_path[512];
    const char *extensions[] = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", NULL};
    
    for (int i = 0; extensions[i] != NULL; i++) {
        snprintf(cover_path, sizeof(cover_path), "%s/cover%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/Cover%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/COVER%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/album%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/Album%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/folder%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/Folder%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
        
        snprintf(cover_path, sizeof(cover_path), "%s/front%s", directory, extensions[i]);
        if (access(cover_path, F_OK) == 0) {
            return cover_path;
        }
    }
    
    return NULL;
}

char* render_image_to_ansi(const char *image_path) {
    static char buffer[30000];
    char command[512];
    FILE *fp;
    
    if (image_path == NULL) {
        return NULL;
    }
    
    snprintf(command, sizeof(command),
             "tiv \"%s\" -w %d -h %d 2>/dev/null",
             image_path, image_width, image_height);
    
    fp = popen(command, "r");
    if (fp == NULL) {
        return NULL;
    }
    
    size_t total_read = 0;
    buffer[0] = '\0';
    
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t line_len = strlen(line);
        if (total_read + line_len < sizeof(buffer) - 1) {
            strcat(buffer, line);
            total_read += line_len;
        } else {
            break;
        }
    }
    
    pclose(fp);
    
    if (strlen(buffer) == 0) {
        return NULL;
    }
    
    return buffer;
}

void format_time(int seconds, char *buffer, size_t size) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    snprintf(buffer, size, "%d:%02d", minutes, secs);
}

void draw_timer_only() {
    pthread_mutex_lock(&draw_mutex);
    
    if (playlist.count > 0 && playlist.current < playlist.count) {
        Track *current = &playlist.tracks[playlist.current];
        
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
        
        Track *current = &playlist.tracks[playlist.current];
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
        set_cursor_position(HEIGHT/2 - 1, LEFT_WIDTH/2 - 19);
        printf("terminal image viewer not installed please install tiv");
        return;
    }
    
    char *cover_path = find_cover_image(music_directory);
    if (cover_path == NULL) {
        set_cursor_position(HEIGHT/2, LEFT_WIDTH/2 - 8);
        printf("COVER ART NOT FOUND");
        return;
    }
    
    char *ansi_art = render_image_to_ansi(cover_path);
    if (ansi_art == NULL) {
        set_cursor_position(HEIGHT/2, LEFT_WIDTH/2 - 10);
        printf("ERROR RENDERING IMAGE");
        return;
    }
    
    char *line = strtok(ansi_art, "\n");
    int current_y = image_y;
    while (line != NULL && current_y < HEIGHT) {
        set_cursor_position(current_y, image_x);
        printf("%s", line);
        line = strtok(NULL, "\n");
        current_y++;
    }
}

int get_audio_duration(const char *filename) {
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE *sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (sndfile) {
        double duration = (double)sfinfo.frames / sfinfo.samplerate;
        sf_close(sndfile);
        return (int)(duration + 0.5);
    }
    
    mpg123_handle *mh = mpg123_new(NULL, NULL);
    if (mh && mpg123_open(mh, filename) == MPG123_OK) {
        off_t length;
        mpg123_seek(mh, 0, SEEK_END);
        length = mpg123_tell(mh);
        mpg123_seek(mh, 0, SEEK_SET);
        
        long rate;
        int channels, encoding;
        mpg123_getformat(mh, &rate, &channels, &encoding);
        
        double duration = 0.0;
        if (rate > 0) {
            duration = (double)length / rate;
        }
        
        mpg123_close(mh);
        mpg123_delete(mh);
        
        return (int)(duration + 0.5);
    }
    
    if (mh) {
        mpg123_delete(mh);
    }
    
    return 180;
}

void get_audio_info(const char *filename, Track *track) {
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    
    strncpy(track->title, basename, sizeof(track->title)-1);
    track->title[sizeof(track->title)-1] = '\0';
    
    char *dot = strrchr(track->title, '.');
    if (dot) *dot = '\0';
    
    strncpy(track->author, "Unknown", sizeof(track->author)-1);
    strncpy(track->album, "Unknown", sizeof(track->album)-1);
    
    const char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcasecmp(ext, ".mp3") == 0) {
            track->file_type = 1;
        } else {
            track->file_type = 0;
        }
    }
    
    track->duration = get_audio_duration(filename);
    track->time_remaining = track->duration;
}

void load_playlist(const char *directory) {
    DIR *dir;
    struct dirent *entry;
    int track_count = 0;
    
    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".mp3") == 0 || 
                   strcasecmp(ext, ".wav") == 0)) {
            track_count++;
        }
    }
    closedir(dir);
    
    if (track_count == 0) {
        printf("No audio files found in %s\n", directory);
        exit(1);
    }
    
    playlist.tracks = malloc(track_count * sizeof(Track));
    if (playlist.tracks == NULL) {
        perror("Failed to allocate memory for tracks");
        exit(1);
    }
    playlist.count = 0;
    
    dir = opendir(directory);
    while ((entry = readdir(dir)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".mp3") == 0 || 
                   strcasecmp(ext, ".wav") == 0)) {
            
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, entry->d_name);
            
            if (playlist.count < track_count) {
                Track *track = &playlist.tracks[playlist.count];
                
                strncpy(track->filename, fullpath, sizeof(track->filename)-1);
                track->filename[sizeof(track->filename)-1] = '\0';
                
                get_audio_info(fullpath, track);
                playlist.count++;
            }
        }
    }
    closedir(dir);
    
    playlist.current = 0;
    playlist.playing = 0;
    playlist.position = 0;
}

void draw_interface() {
    pthread_mutex_lock(&draw_mutex);
    
    clear_screen();
    
    for (int i = 0; i < HEIGHT; i++) {
        set_cursor_position(i, LEFT_WIDTH);
        printf("│");
    }
    
    display_left_panel();
    
    if (playlist.count > 0 && playlist.current < playlist.count) {
        Track *current = &playlist.tracks[playlist.current];
        
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

void list_audio_devices() {
    printf("Available audio devices:\n");
    
    const char *devices[] = {
        "default",
        "plughw:0,0", 
        "hw:0,0",
        "pulse",
        NULL
    };
    
    for (int i = 0; devices[i] != NULL; i++) {
        snd_pcm_t *test_handle;
        int err = snd_pcm_open(&test_handle, devices[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err == 0) {
            printf("  ✓ %s\n", devices[i]);
            snd_pcm_close(test_handle);
        } else {
            printf("  ✗ %s (%s)\n", devices[i], snd_strerror(err));
        }
    }
}

int init_audio_system() {
    int err;
    
    mpg123_init();
    
    const char *devices[] = {
        "default",
        "plughw:0,0",
        "hw:0,0", 
        "pulse",
        NULL
    };
    
    const char *used_device = NULL;
    
    for (int i = 0; devices[i] != NULL; i++) {
        printf("Trying audio device: %s\n", devices[i]);
        err = snd_pcm_open(&pcm_handle, devices[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err == 0) {
            used_device = devices[i];
            printf("Successfully opened: %s\n", used_device);
            break;
        } else {
            printf("Failed to open %s: %s\n", devices[i], snd_strerror(err));
        }
    }
    
    if (pcm_handle == NULL) {
        fprintf(stderr, "Could not open any audio device\n");
        list_audio_devices();
        return -1;
    }
    
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    
    err = snd_pcm_hw_params_any(pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot initialize hardware parameter structure: %s\n", snd_strerror(err));
        return -1;
    }
    
    err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "Cannot set access type: %s\n", snd_strerror(err));
        return -1;
    }
    
    err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, FORMAT);
    if (err < 0) {
        fprintf(stderr, "Cannot set sample format: %s\n", snd_strerror(err));
        return -1;
    }
    
    unsigned int sample_rate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot set sample rate: %s\n", snd_strerror(err));
        return -1;
    }
    
    err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, CHANNELS);
    if (err < 0) {
        fprintf(stderr, "Cannot set channel count: %s\n", snd_strerror(err));
        return -1;
    }
    
    snd_pcm_uframes_t buffer_size = ALSA_BUFFER_SIZE;
    snd_pcm_uframes_t period_size = ALSA_PERIOD_SIZE;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);
    if (err < 0) {
        fprintf(stderr, "Cannot set buffer size: %s\n", snd_strerror(err));
        return -1;
    }
    
    err = snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period_size, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot set period size: %s\n", snd_strerror(err));
        return -1;
    }
    
    err = snd_pcm_hw_params(pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot set parameters: %s\n", snd_strerror(err));
        return -1;
    }
    
    printf("Audio system initialized successfully with device: %s\n", used_device);
    return 0;
}

void cleanup_audio_system() {
    if (pcm_handle) {
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
    
    if (mp3_handle) {
        mpg123_close(mp3_handle);
        mpg123_delete(mp3_handle);
        mp3_handle = NULL;
    }
    
    mpg123_exit();
}

int play_wav_file(const char *filename) {
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE *sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (!sndfile) {
        fprintf(stderr, "Cannot open WAV file: %s\n", sf_strerror(NULL));
        return -1;
    }
    
    if (sfinfo.channels != CHANNELS) {
        fprintf(stderr, "Unsupported audio format: %d channels (expected %d)\n", 
                sfinfo.channels, CHANNELS);
        sf_close(sndfile);
        return -1;
    }
    
    int buffer_size = ALSA_PERIOD_SIZE * sizeof(short) * CHANNELS;
    short *buffer = malloc(buffer_size);
    if (!buffer) {
        sf_close(sndfile);
        return -1;
    }
    
    int total_frames = sfinfo.frames;
    int frames_played = 0;
    
    while (playback_thread_running && frames_played < total_frames) {
        if (!playlist.playing) {
            msleep(50);
            continue;
        }
        
        int frames_to_read = ALSA_PERIOD_SIZE;
        if (frames_played + frames_to_read > total_frames) {
            frames_to_read = total_frames - frames_played;
        }
        
        sf_count_t frames_read = sf_readf_short(sndfile, buffer, frames_to_read);
        if (frames_read <= 0) {
            break;
        }
        
        int err = snd_pcm_writei(pcm_handle, buffer, frames_read);
        if (err == -EPIPE) {
            printf("Audio buffer underrun, recovering...\n");
            snd_pcm_prepare(pcm_handle);
        } else if (err < 0) {
            fprintf(stderr, "Write error: %s\n", snd_strerror(err));
            break;
        } else {
            frames_played += frames_read;
            
            pthread_mutex_lock(&audio_mutex);
            playlist.position = (frames_played * playlist.tracks[playlist.current].duration) / total_frames;
            if (playlist.current < playlist.count) {
                Track *current_track = &playlist.tracks[playlist.current];
                current_track->time_remaining = current_track->duration - playlist.position;
            }
            pthread_mutex_unlock(&audio_mutex);
        }
        
        msleep(10);
    }
    
    free(buffer);
    sf_close(sndfile);
    return 0;
}

int play_mp3_file(const char *filename) {
    mpg123_handle *mh = mpg123_new(NULL, NULL);
    if (!mh) {
        fprintf(stderr, "Cannot create mpg123 handle\n");
        return -1;
    }
    
    if (mpg123_open(mh, filename) != MPG123_OK) {
        fprintf(stderr, "Cannot open MP3 file: %s\n", mpg123_strerror(mh));
        mpg123_delete(mh);
        return -1;
    }
    
    long rate;
    int channels, encoding;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        fprintf(stderr, "Cannot get MP3 format\n");
        mpg123_close(mh);
        mpg123_delete(mh);
        return -1;
    }
    
    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, MPG123_ENC_SIGNED_16);
    
    size_t buffer_size = mpg123_outblock(mh);
    unsigned char *buffer = malloc(buffer_size);
    if (!buffer) {
        mpg123_close(mh);
        mpg123_delete(mh);
        return -1;
    }
    
    off_t total_frames = mpg123_length(mh);
    off_t frames_played = 0;
    
    while (playback_thread_running) {
        if (!playlist.playing) {
            msleep(50);
            continue;
        }
        
        size_t done;
        int err = mpg123_read(mh, buffer, buffer_size, &done);
        
        if (err == MPG123_DONE) {
            break;
        } else if (err != MPG123_OK) {
            fprintf(stderr, "MP3 read error: %s\n", mpg123_strerror(mh));
            break;
        }
        
        if (done > 0) {
            int pcm_err = snd_pcm_writei(pcm_handle, buffer, done / (channels * 2));
            if (pcm_err == -EPIPE) {
                printf("Audio buffer underrun, recovering...\n");
                snd_pcm_prepare(pcm_handle);
            } else if (pcm_err < 0) {
                fprintf(stderr, "PCM write error: %s\n", snd_strerror(pcm_err));
                break;
            }
            
            frames_played = mpg123_tell(mh);
            
            pthread_mutex_lock(&audio_mutex);
            if (total_frames > 0) {
                playlist.position = (frames_played * playlist.tracks[playlist.current].duration) / total_frames;
            }
            if (playlist.current < playlist.count) {
                Track *current_track = &playlist.tracks[playlist.current];
                current_track->time_remaining = current_track->duration - playlist.position;
            }
            pthread_mutex_unlock(&audio_mutex);
        }
        
        msleep(10);
    }
    
    free(buffer);
    mpg123_close(mh);
    mpg123_delete(mh);
    return 0;
}

void* playback_thread_function(void* arg) {
    char *filename = (char*)arg;
    
    if (playlist.current < playlist.count) {
        Track *current_track = &playlist.tracks[playlist.current];
        
        if (current_track->file_type == 1) {
            play_mp3_file(filename);
        } else {
            play_wav_file(filename);
        }
    }
    
    playback_thread_running = 0;
    return NULL;
}

void stop_playback() {
    playback_thread_running = 0;
    playlist.playing = 0;
    
    if (pcm_handle) {
        snd_pcm_drop(pcm_handle);
        snd_pcm_prepare(pcm_handle);
    }
}

void start_playback() {
    if (playlist.count == 0 || playlist.current >= playlist.count) return;
    
    stop_playback();
    
    Track *current_track = &playlist.tracks[playlist.current];
    
    playback_thread_running = 1;
    
    if (pthread_create(&playback_thread, NULL, playback_thread_function, 
                      (void*)current_track->filename) != 0) {
        printf("Failed to create playback thread\n");
        playback_thread_running = 0;
        return;
    }
    
    playlist.playing = 1;
    playlist.position = 0;
    playback_start_time = time(NULL);
    current_track->time_remaining = current_track->duration;
}

void toggle_pause() {
    if (playback_thread_running) {
        playlist.playing = !playlist.playing;
        draw_status_only();
    } else if (playlist.count > 0) {
        playlist.playing = 1;
        start_playback();
        draw_interface();
    }
}

void next_track() {
    if (playlist.current < playlist.count - 1) {
        playlist.current++;
        playlist.position = 0;
        stop_playback();
        
        if (playlist.current < playlist.count) {
            Track *current_track = &playlist.tracks[playlist.current];
            current_track->time_remaining = current_track->duration;
        }
        playlist.playing = 0;
        
        draw_track_list_only();
        draw_status_only();
    }
}

void prev_track() {
    if (playlist.current > 0) {
        playlist.current--;
        playlist.position = 0;
        stop_playback();
        
        if (playlist.current < playlist.count) {
            Track *current_track = &playlist.tracks[playlist.current];
            current_track->time_remaining = current_track->duration;
        }
        playlist.playing = 0;
        
        draw_track_list_only();
        draw_status_only();
    }
}

void auto_next_track() {
    if (playlist.current < playlist.count - 1) {
        playlist.current++;
        playlist.position = 0;
        if (playlist.playing) {
            start_playback();
        }
        draw_track_list_only();
        draw_status_only();
    } else {
        playlist.playing = 0;
        playlist.position = 0;
        stop_playback();
        draw_status_only();
    }
}

void* audio_thread_function(void* arg) {
    (void)arg;
    
    time_t last_update_time = time(NULL);
    
    while (audio_thread_running) {
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
                
                Track *current_track = &playlist.tracks[playlist.current];
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
                if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
                if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
                
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'C': return 'R';
                        case 'D': return 'L';
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
            disable_raw_mode();
            
            if (playlist.tracks) {
                free(playlist.tracks);
            }
            
            cleanup_audio_system();
            pthread_mutex_destroy(&draw_mutex);
            pthread_mutex_destroy(&audio_mutex);
            clear_screen();
            printf("SMP Media Player closed. Thank you for using!\n");
            exit(0);
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <music_directory>\n", argv[0]);
        return 1;
    }
    
    if (pthread_mutex_init(&draw_mutex, NULL) != 0 ||
        pthread_mutex_init(&audio_mutex, NULL) != 0) {
        printf("Mutex init failed\n");
        return 1;
    }
    
    strncpy(music_directory, argv[1], sizeof(music_directory)-1);
    music_directory[sizeof(music_directory)-1] = '\0';
    
    if (system("which tiv > /dev/null 2>&1") != 0) {
        printf("Warning: tiv not installed. Install with: sudo pacman -S tiv\n");
    }
    
    printf("Initializing audio system...\n");
    if (init_audio_system() != 0) {
        printf("Failed to initialize audio system\n");
        printf("\nTroubleshooting tips:\n");
        printf("1. Run: sudo pacman -S alsa-utils\n");
        printf("2. Run: alsamixer (unmute channels)\n");
        printf("3. Run: speaker-test -t wav -c 2\n");
        printf("4. Check if PulseAudio is running: pulseaudio --check\n");
        return 1;
    }
    
    enable_raw_mode();
    
    printf("Loading playlist and analyzing audio files...\n");
    load_playlist(argv[1]);
    
    if (playlist.count == 0) {
        printf("No tracks loaded. Exiting.\n");
        return 1;
    }
    
    if (pthread_create(&audio_thread, NULL, audio_thread_function, NULL) != 0) {
        printf("Failed to create audio thread\n");
        return 1;
    }
    
    printf("SMP Media Player started. Loaded %d tracks.\n", playlist.count);
    printf("Press any key to continue...");
    getchar();
    
    draw_interface();
    
    while (audio_thread_running) {
        handle_input();
        msleep(50);
    }
    
    pthread_join(audio_thread, NULL);
    if (playback_thread_running) {
        playback_thread_running = 0;
        pthread_join(playback_thread, NULL);
    }
    
    disable_raw_mode();
    cleanup_audio_system();
    pthread_mutex_destroy(&draw_mutex);
    pthread_mutex_destroy(&audio_mutex);
    
    return 0;
}


    
   