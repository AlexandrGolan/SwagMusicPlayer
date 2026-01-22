#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "player.h"

void load_playlist(const char* directory);
void get_audio_info(const char* filename, Track* track);
int get_audio_duration(const char* filename);
void next_track();
void prev_track();
void toggle_pause();
void start_playback();
void auto_next_track();

#endif
