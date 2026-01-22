#ifndef CONFIG_H
#define CONFIG_H

#define MIN_WIDTH 80
#define MIN_HEIGHT 24
#define DEFAULT_LEFT_RATIO 0.45f
#define ALSA_BUFFER_SIZE 4096
#define ALSA_PERIOD_SIZE 1024
#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FORMAT SND_PCM_FORMAT_S16_LE

#define MAX_ANSI_BUFFER 50000
#define MAX_LINE_LENGTH 16384

#define VOLUME_STEP 5
#define MIN_VOLUME 0
#define MAX_VOLUME 100
#define DEFAULT_VOLUME 70

#define PLAYLIST_VISIBLE_ROWS 15

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

void update_terminal_size();

#endif
