#include "audio.h"

#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

int init_audio_system() {
    int err;
    mpg123_init();

    const char* devices[] = {"default", "plughw:0,0", "hw:0,0", "pulse", NULL};

    for (int i = 0; devices[i] != NULL; i++) {
        err = snd_pcm_open(&pcm_handle, devices[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err == 0) {
            break;
        }
    }

    if (pcm_handle == NULL) {
        list_audio_devices();
        return -1;
    }

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(pcm_handle, hw_params);
    if (err < 0) {
        return -1;
    }

    err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        return -1;
    }

    err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, FORMAT);
    if (err < 0) {
        return -1;
    }

    unsigned int sample_rate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0);
    if (err < 0) {
        return -1;
    }

    err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, CHANNELS);
    if (err < 0) {
        return -1;
    }

    snd_pcm_uframes_t buffer_size = ALSA_BUFFER_SIZE;
    snd_pcm_uframes_t period_size = ALSA_PERIOD_SIZE;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);
    if (err < 0) {
        return -1;
    }

    err = snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period_size, 0);
    if (err < 0) {
        return -1;
    }

    err = snd_pcm_hw_params(pcm_handle, hw_params);
    if (err < 0) {
        return -1;
    }

    set_alsa_volume();

    return 0;
}

void set_alsa_volume() {
    if (pcm_handle == NULL) {
        return;
    }

    snd_pcm_sw_params_t* sw_params;
    snd_pcm_sw_params_alloca(&sw_params);

    int err = snd_pcm_sw_params_current(pcm_handle, sw_params);
    if (err < 0) {
        return;
    }

    long min, max;
    snd_pcm_sw_params_get_avail_min(sw_params, &min);

    err = snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, ALSA_BUFFER_SIZE);
    if (err < 0) {
        return;
    }

    err = snd_pcm_sw_params(pcm_handle, sw_params);
    if (err < 0) {
        return;
    }
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

void list_audio_devices() {
    printf("Available audio devices:\n");
    const char* devices[] = {"default", "plughw:0,0", "hw:0,0", "pulse", NULL};

    for (int i = 0; devices[i] != NULL; i++) {
        snd_pcm_t* test_handle;
        int err = snd_pcm_open(&test_handle, devices[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err == 0) {
            printf("  ✓ %s\n", devices[i]);
            snd_pcm_close(test_handle);
        } else {
            printf("  ✗ %s (%s)\n", devices[i], snd_strerror(err));
        }
    }
}

int apply_volume_to_buffer(short* buffer, int frames, int channels) {
    if (current_volume == MAX_VOLUME) {
        return 0;
    }

    float volume_factor = (float)current_volume / MAX_VOLUME;

    for (int i = 0; i < frames * channels; i++) {
        int sample = (int)(buffer[i] * volume_factor);
        if (sample > 32767) {
            sample = 32767;
        }
        if (sample < -32768) {
            sample = -32768;
        }
        buffer[i] = (short)sample;
    }

    return 0;
}

int play_wav_file(const char* filename) {
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE* sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (!sndfile) {
        return -1;
    }

    if (sfinfo.channels != CHANNELS) {
        sf_close(sndfile);
        return -1;
    }

    int buffer_size = ALSA_PERIOD_SIZE * sizeof(short) * CHANNELS;
    short* buffer = malloc(buffer_size);
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

        apply_volume_to_buffer(buffer, frames_read, CHANNELS);

        int err = snd_pcm_writei(pcm_handle, buffer, frames_read);
        if (err == -EPIPE) {
            snd_pcm_prepare(pcm_handle);
        } else if (err < 0) {
            break;
        } else {
            frames_played += frames_read;
            pthread_mutex_lock(&audio_mutex);
            playlist.position = (frames_played * playlist.tracks[playlist.current].duration) / total_frames;
            if (playlist.current < playlist.count) {
                Track* current_track = &playlist.tracks[playlist.current];
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

int play_mp3_file(const char* filename) {
    mpg123_handle* mh = mpg123_new(NULL, NULL);
    if (!mh) {
        return -1;
    }

    if (mpg123_open(mh, filename) != MPG123_OK) {
        mpg123_delete(mh);
        return -1;
    }

    long rate;
    int channels, encoding;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        mpg123_close(mh);
        mpg123_delete(mh);
        return -1;
    }

    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, MPG123_ENC_SIGNED_16);

    size_t buffer_size = mpg123_outblock(mh);
    unsigned char* buffer = malloc(buffer_size);
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
            break;
        }

        if (done > 0) {
            int samples = done / (channels * 2);
            apply_volume_to_buffer((short*)buffer, samples, channels);

            int pcm_err = snd_pcm_writei(pcm_handle, buffer, samples);
            if (pcm_err == -EPIPE) {
                snd_pcm_prepare(pcm_handle);
            } else if (pcm_err < 0) {
                break;
            }

            frames_played = mpg123_tell(mh);
            pthread_mutex_lock(&audio_mutex);
            if (total_frames > 0) {
                playlist.position =
                    (frames_played * playlist.tracks[playlist.current].duration) / total_frames;
            }
            if (playlist.current < playlist.count) {
                Track* current_track = &playlist.tracks[playlist.current];
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

void stop_playback() {
    playback_thread_running = 0;
    playlist.playing = 0;

    if (pcm_handle) {
        snd_pcm_drop(pcm_handle);
        snd_pcm_prepare(pcm_handle);
    }
}

void* playback_thread_function(void* arg) {
    char* filename = (char*)arg;

    if (playlist.current < playlist.count) {
        Track* current_track = &playlist.tracks[playlist.current];

        if (current_track->file_type == 1) {
            play_mp3_file(filename);
        } else {
            play_wav_file(filename);
        }
    }

    playback_thread_running = 0;
    return NULL;
}
