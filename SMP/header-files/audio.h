#ifndef AUDIO_H
#define AUDIO_H

#include "player.h"

int init_audio_system();
void cleanup_audio_system();
void list_audio_devices();
int play_wav_file(const char *filename);
int play_mp3_file(const char *filename);
void stop_playback();
void* playback_thread_function(void* arg);

#endif
