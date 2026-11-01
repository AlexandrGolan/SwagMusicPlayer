/*
 * SMP - Swag Music Player
 * Configuration file: interface colours and playback options.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_CONFIG_H
#define SMP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} SmpColor;

typedef struct {
    SmpColor text;          /* normal text                     */
    SmpColor accent;        /* titles, headings                */
    SmpColor dim;           /* secondary text, separators      */
    SmpColor selection_bg;  /* background of the selected row  */
    SmpColor playing;       /* currently playing track         */
    SmpColor error;         /* error messages                  */
    SmpColor progress;      /* progress bar fill               */
    SmpColor volume_color;  /* volume bar fill (file key: volume) */
    SmpColor border;        /* panel borders                   */
    SmpColor cover;         /* ASCII cover art                 */
    SmpColor spectrum;      /* visualiser bars                 */

    int volume;             /* 0..100, initial volume          */
    bool visualizer;        /* draw the spectrum analyser      */
    int cover_mode;         /* 0 = half blocks, 1 = ASCII art  */
    int frame_interval_ms;  /* UI refresh interval, 16..500    */
} SmpConfig;

void smp_config_defaults(SmpConfig *config);
/* Parses `path`. Missing file => SMP_ERR_NOT_FOUND and `config` keeps its
   previous values (defaults). Malformed lines and unknown keys are ignored. */
SmpStatus smp_config_load(SmpConfig *config, const char *path);
/* Loads the user configuration: %APPDATA%\smp\smp.conf on Windows,
   $XDG_CONFIG_HOME/smp/smp.conf or $HOME/.config/smp/smp.conf elsewhere.
   Writes the path that was used into `used_path` (may be NULL).
   Returns SMP_OK, SMP_ERR_NOT_FOUND (no file, defaults kept) or an error. */
SmpStatus smp_config_load_user(SmpConfig *config, char *used_path, size_t used_path_size);
/* Absolute path of the user configuration file. Caller frees. */
char *smp_config_user_path(void);
/* Writes a commented template with every option at its default value.
   Never overwrites an existing file: returns SMP_ERR_IO then. */
SmpStatus smp_config_write_template(const char *path);
/* "#RRGGBB", "#RGB", "white", "black", "default" (pure white) */
bool smp_color_parse(const char *text, SmpColor *out);

#endif /* SMP_CONFIG_H */
