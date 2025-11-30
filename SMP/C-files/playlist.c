#include "playlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <strings.h>
#include "audio.h"
#include "ui.h"

void load_playlist(const char *directory) {
    DIR *dir;
    struct dirent *entry;
    int track_count = 0;
    
    dir = opendir(directory);
    if (dir == NULL) return;
    
    while ((entry = readdir(dir)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0)) {
            track_count++;
        }
    }
    closedir(dir);
    
    if (track_count == 0) {
        printf("No audio files found\n");
        exit(1);
    }
    
    playlist.tracks = malloc(track_count * sizeof(Track));
    if (playlist.tracks == NULL) {
        exit(1);
    }
    playlist.count = 0;
    
    dir = opendir(directory);
    while ((entry = readdir(dir)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0)) {
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

void start_playback() {
    if (playlist.count == 0 || playlist.current >= playlist.count) return;
    
    stop_playback();
    
    Track *current_track = &playlist.tracks[playlist.current];
    
    playback_thread_running = 1;
    
    if (pthread_create(&playback_thread, NULL, playback_thread_function, 
                      (void*)current_track->filename) != 0) {
        playback_thread_running = 0;
        return;
    }
    
    playlist.playing = 1;
    playlist.position = 0;
    playback_start_time = time(NULL);
    current_track->time_remaining = current_track->duration;
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
