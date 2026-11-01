/*
 * SMP - Swag Music Player
 * Entry point and application loop.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/audio.h"
#include "smp/common.h"
#include "smp/config.h"
#include "smp/frame.h"
#include "smp/fs.h"
#include "smp/pcm.h"
#include "smp/player.h"
#include "smp/playlist.h"
#include "smp/str.h"
#include "smp/term.h"
#include "smp/ui.h"

#define SMP_VERSION "2.2.0"
#define SMP_FRAME_INTERVAL_MS 80

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv);
#endif

typedef struct {
    const char *directory;
    const char *backend;
    const char *config_path;
    bool list_devices;
    bool render_once;
    bool no_config;
    bool init_config;
} Options;

static void print_usage(const char *program)
{
    printf("SMP (Swag Music Player) %s\n\n", SMP_VERSION);
    printf("Usage: %s [options] <music-directory>\n\n", program);
    printf("Options:\n");
    printf("  -d, --directory <path>  directory with WAV/MP3 files (also positional)\n");
    printf("                          on Windows, running without a directory opens a\n");
    printf("                          folder chooser\n");
    printf("      --audio <backend>   audio backend: auto, wasapi, alsa, null\n");
    printf("      --config <path>     read colours and options from this file\n");
    printf("      --no-config         ignore the configuration file\n");
    printf("      --init-config       write a commented default config and exit\n");
    printf("      --list-devices      print available output devices and exit\n");
    printf("      --render-once       render one frame to stdout and exit (debug)\n");
    printf("  -h, --help              show this help\n");
    printf("  -v, --version           show the version\n");
}

static bool parse_options(int argc, char **argv, Options *options)
{
    int index;

    memset(options, 0, sizeof(*options));
    options->backend = "auto";

    for (index = 1; index < argc; index++) {
        const char *argument = argv[index];

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        if (strcmp(argument, "-v") == 0 || strcmp(argument, "--version") == 0) {
            printf("SMP %s\n", SMP_VERSION);
            exit(0);
        }
        if (strcmp(argument, "--list-devices") == 0) {
            options->list_devices = true;
            continue;
        }
        if (strcmp(argument, "--render-once") == 0) {
            options->render_once = true;
            continue;
        }
        if (strcmp(argument, "--no-config") == 0) {
            options->no_config = true;
            continue;
        }
        if (strcmp(argument, "--init-config") == 0) {
            options->init_config = true;
            continue;
        }
        if (strcmp(argument, "--config") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "smp: %s requires a path\n", argument);
                return false;
            }
            options->config_path = argv[++index];
            continue;
        }
        if (strcmp(argument, "-d") == 0 || strcmp(argument, "--directory") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "smp: %s requires a path\n", argument);
                return false;
            }
            options->directory = argv[++index];
            continue;
        }
        if (strcmp(argument, "--audio") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "smp: %s requires a backend name\n", argument);
                return false;
            }
            options->backend = argv[++index];
            continue;
        }
        if (argument[0] == '-') {
            fprintf(stderr, "smp: unknown option '%s'\n", argument);
            return false;
        }

        if (options->directory != NULL) {
            fprintf(stderr, "smp: only one directory can be given\n");
            return false;
        }
        options->directory = argument;
    }

    return true;
}

/*
 * Moves the selection by `delta`. When something is already playing the newly
 * selected track starts at once and the previous one is stopped, which is what
 * the arrow keys are expected to do in a player.
 */
static void select_relative(SmpUiState *ui,
                            Playlist *playlist,
                            SmpPlayer *player,
                            const SmpPlayerSnapshot *snapshot,
                            int delta)
{
    if (playlist->count == 0) {
        return;
    }

    smp_playlist_move(playlist, delta);
    ui->selection = playlist->current;

    if (snapshot->state == SMP_PLAYER_PLAYING || snapshot->state == SMP_PLAYER_PAUSED) {
        smp_player_play(player, ui->selection);
    }
}

/*
 * Maps the Cyrillic letters that sit on the same physical keys as the Latin
 * hotkeys, so the player is usable with a Russian keyboard layout.
 */
static uint32_t normalize_hotkey(uint32_t character)
{
    switch (character) {
    case 0x0439:
    case 0x0419:
        return 'q'; /* Р В Р’В Р вЂ™Р’В Р В Р вЂ Р Р†Р вЂљРЎвЂєР Р†Р вЂљРІР‚Сљ */
    case 0x0444:
    case 0x0424:
        return 'a'; /* Р В Р’В Р В Р вЂ№Р В Р вЂ Р В РІР‚С™Р РЋРІР‚С” */
    case 0x043c:
    case 0x041c:
        return 'v'; /* Р В Р’В Р вЂ™Р’В Р В Р Р‹Р вЂ™Р’В */
    case 0x0442:
    case 0x0422:
        return 'n'; /* Р В Р’В Р В Р вЂ№Р В Р вЂ Р В РІР‚С™Р РЋРІвЂћСћ */
    case 0x0437:
    case 0x0417:
        return 'p'; /* Р В Р’В Р вЂ™Р’В Р В РІР‚в„ўР вЂ™Р’В· */
    case 0x043a:
    case 0x041a:
        return 'r'; /* Р В Р’В Р вЂ™Р’В Р В Р Р‹Р Р†Р вЂљРЎСљ */
    case 0x0440:
    case 0x0420:
        return 'h'; /* Р В Р’В Р В Р вЂ№Р В Р’В Р Р†Р вЂљРЎв„ў */
    default:
        return character;
    }
}

static void handle_key(SmpKey key,
                       SmpUiState *ui,
                       Playlist *playlist,
                       SmpPlayer *player,
                       bool *running,
                       const Options *options)
{
    SmpPlayerSnapshot snapshot;

    smp_player_snapshot(player, &snapshot);

    switch (key.code) {
    case SMP_KEY_UP:
        select_relative(ui, playlist, player, &snapshot, -1);
        break;

    case SMP_KEY_DOWN:
        select_relative(ui, playlist, player, &snapshot, 1);
        break;

    case SMP_KEY_LEFT:
        if (snapshot.state == SMP_PLAYER_PLAYING || snapshot.state == SMP_PLAYER_PAUSED) {
            smp_player_seek_by(player, -5.0);
        }
        break;

    case SMP_KEY_RIGHT:
        if (snapshot.state == SMP_PLAYER_PLAYING || snapshot.state == SMP_PLAYER_PAUSED) {
            smp_player_seek_by(player, 5.0);
        }
        break;

    case SMP_KEY_PAGE_UP:
        smp_player_set_volume(player, snapshot.volume + 5);
        break;

    case SMP_KEY_PAGE_DOWN:
        smp_player_set_volume(player, snapshot.volume - 5);
        break;

    case SMP_KEY_ENTER:
        if (playlist->count > 0 && ui->selection < playlist->count) {
            smp_playlist_select(playlist, ui->selection);
            smp_player_play(player, ui->selection);
        }
        break;

    case SMP_KEY_SPACE:
        if (playlist->count > 0 && snapshot.state != SMP_PLAYER_PLAYING
            && snapshot.state != SMP_PLAYER_PAUSED)
        {
            smp_playlist_select(playlist, ui->selection);
            smp_player_play(player, ui->selection);
        } else {
            smp_player_toggle(player);
        }
        break;

    case SMP_KEY_CHAR:
        switch (normalize_hotkey(key.character)) {
        case 'q':
        case 'Q':
            *running = false;
            break;
        case 'n':
        case 'N':
            smp_player_next(player);
            break;
        case 'p':
        case 'P':
            smp_player_previous(player);
            break;
        case '+':
        case '=':
            smp_player_set_volume(player, snapshot.volume + 5);
            break;
        case '-':
        case '_':
            smp_player_set_volume(player, snapshot.volume - 5);
            break;
        case 'h':
        case 'H':
        case '?':
            ui->show_help = !ui->show_help;
            break;
        case 'v':
        case 'V':
            ui->theme.show_spectrum = !ui->theme.show_spectrum;
            smp_ui_set_theme(ui, &ui->theme);
            break;
        case 'a':
        case 'A':
            ui->theme.cover_mode
                = ui->theme.cover_mode == SMP_COVER_ASCII ? SMP_COVER_BLOCKS : SMP_COVER_ASCII;
            smp_ui_set_theme(ui, &ui->theme);
            break;
        case 'r':
        case 'R': {
            SmpStatus status = smp_playlist_load(playlist, options->directory);

            if (status == SMP_OK) {
                smp_ui_set_message(ui, "library reloaded", smp_now_ms());
            } else {
                smp_ui_set_message(ui, "reload failed", smp_now_ms());
            }
            ui->selection = 0;
            ui->scroll = 0;
            break;
        }
        default:
            break;
        }
        break;

    case SMP_KEY_ESCAPE:
    case SMP_KEY_CTRL_C:
        *running = false;
        break;

    default:
        break;
    }
}

static void render_frame(SmpFrame *frame,
                         SmpUiState *ui,
                         const Playlist *playlist,
                         SmpPlayer *player,
                         const char *backend,
                         StrBuf *output)
{
    SmpPlayerSnapshot snapshot;

    smp_player_snapshot(player, &snapshot);

    if (snapshot.track_index < playlist->count) {
        ui->selection = SMP_MIN(ui->selection, playlist->count - 1u);
    }

    sb_clear(output);
    smp_ui_render(ui, frame, playlist, &snapshot, backend, smp_now_ms());
    smp_frame_render(frame, output);
}

static int run_application(const Options *options, const char *program)
{
    Playlist playlist;
    SmpAudioDevice *device = NULL;
    SmpPlayer *player = NULL;
    SmpUiState ui;
    SmpSpectrum *spectrum = NULL;
    SmpFrame frame;
    StrBuf output;
    StrBuf previous;
    SmpAudioFormat format;
    SmpStatus status;
    static const char *const screen_on = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";
    bool running = true;
    int exit_code = 0;
    int width = 0;
    int height = 0;
    uint64_t next_frame = 0;

    const char *directory = options->directory;
    SmpConfig config;
    SmpUiTheme theme;
    SmpStatus config_status;
    int frame_interval;

    smp_config_defaults(&config);

    if (!options->no_config) {
        config_status = options->config_path != NULL
                            ? smp_config_load(&config, options->config_path)
                            : smp_config_load_user(&config, NULL, 0);
        if (config_status != SMP_OK && config_status != SMP_ERR_NOT_FOUND) {
            fprintf(stderr,
                    "smp: cannot read the configuration (%s), using defaults\n",
                    smp_status_str(config_status));
        }
    }

    /*
     * The visualiser needs a higher frame rate to look fluid; keep the
     * caller's value for the rest of the interface.
     */
    frame_interval = config.frame_interval_ms;
    if (config.visualizer && frame_interval > 33) {
        frame_interval = 33;
    }

    smp_playlist_init(&playlist);
    smp_ui_init(&ui);
    smp_ui_theme_from_config(&theme, &config);
    smp_ui_set_theme(&ui, &theme);
    smp_frame_init(&frame);
    sb_init(&output);
    sb_init(&previous);

    /*
     * Started from the Start menu there is no directory argument: ask the shell
     * for one before the terminal switches to the alternate screen.
     */
    if (directory == NULL) {
        static char picked[SMP_PATH_MAX];

        if (smp_term_pick_directory(picked, sizeof(picked))) {
            directory = picked;
        } else {
            fprintf(stderr, "smp: no music directory given\n\n");
            print_usage(program);
            exit_code = 2;
            goto cleanup;
        }
    }

    status = smp_playlist_load(&playlist, directory);
    if (status == SMP_ERR_NOT_FOUND) {
        fprintf(stderr, "smp: directory not found: %s\n", directory);
        exit_code = 1;
        goto cleanup;
    }
    if (status != SMP_OK) {
        fprintf(stderr, "smp: cannot load playlist: %s\n", smp_status_str(status));
        exit_code = 1;
        goto cleanup;
    }
    if (playlist.count == 0) {
        fprintf(stderr,
                "smp: no WAV, MP3, FLAC or OGG files found in %s\n",
                directory);
        exit_code = 1;
        goto cleanup;
    }

    format.sample_rate = SMP_OUTPUT_RATE;
    format.channels = SMP_OUTPUT_CHANNELS;

    status = smp_audio_open(options->backend, &device, &format);
    if (status != SMP_OK) {
        fprintf(stderr, "smp: cannot open audio output (%s)\n", smp_status_str(status));
        exit_code = 1;
        goto cleanup;
    }

    spectrum = smp_spectrum_create(SMP_OUTPUT_RATE, 1024);

    player = smp_player_create(&playlist, device);
    if (player == NULL) {
        fprintf(stderr, "smp: cannot start the playback engine\n");
        exit_code = 1;
        goto cleanup;
    }

    smp_ui_set_spectrum(&ui, spectrum);
    smp_player_set_spectrum(player, spectrum);
    smp_player_set_volume(player, config.volume);

    smp_ui_set_directory(&ui, directory);
    smp_ui_set_message(&ui, "press space or enter to start", smp_now_ms());

    if (options->render_once) {
        smp_term_get_size(&width, &height);
        if (width < 60 || height < 16) {
            width = 100;
            height = 30;
        }
        if (!smp_frame_begin(&frame, width, height)) {
            exit_code = 1;
            goto cleanup;
        }
        render_frame(&frame, &ui, &playlist, player, smp_audio_name(device), &output);
        smp_term_write(sb_cstr(&output), output.len);
        smp_term_write("\n", 1);
        goto cleanup;
    }

    if (!smp_term_init()) {
        fprintf(stderr, "smp: standard input is not an interactive terminal\n");
        exit_code = 1;
        goto cleanup;
    }

    smp_term_get_size(&width, &height);
    if (!smp_frame_begin(&frame, width, height)) {
        fprintf(stderr, "smp: cannot allocate the render buffer\n");
        exit_code = 1;
        goto cleanup;
    }

    /* Take over the whole screen and restore the shell afterwards. */
    smp_term_write(screen_on, strlen(screen_on));

    while (running) {
        SmpKey key;
        uint64_t now = smp_now_ms();
        int timeout = frame_interval;

        if (smp_term_poll(&key, timeout)) {
            handle_key(key, &ui, &playlist, player, &running, options);
        }

        if (smp_player_take_finished(player)) {
            SmpPlayerSnapshot snapshot;

            smp_player_snapshot(player, &snapshot);
            if (snapshot.state == SMP_PLAYER_ERROR) {
                smp_ui_set_message(&ui, "cannot play this track", smp_now_ms());
            } else if (snapshot.track_index + 1u < playlist.count) {
                SmpTrack *next = &playlist.tracks[snapshot.track_index + 1u];

                SMP_UNUSED(next);
                smp_playlist_select(&playlist, snapshot.track_index + 1u);
                ui.selection = snapshot.track_index + 1u;
                smp_player_play(player, snapshot.track_index + 1u);
            } else {
                smp_ui_set_message(&ui, "end of playlist", smp_now_ms());
            }
        }

        if (smp_term_take_resize()) {
            smp_term_get_size(&width, &height);
            (void)smp_frame_begin(&frame, width, height);
            smp_term_write("\x1b[2J\x1b[H", 7);
            next_frame = 0;
        }

        /* Guard against terminals that resize without notifying us. */
        {
            int current_width = 0;
            int current_height = 0;

            smp_term_get_size(&current_width, &current_height);
            if (current_width != width || current_height != height) {
                width = current_width;
                height = current_height;
                if (smp_frame_begin(&frame, width, height)) {
                    smp_term_write("\x1b[2J\x1b[H", 7);
                    next_frame = 0;
                }
            }
        }

        if (now >= next_frame) {
            next_frame = now + (uint64_t)frame_interval;

            if (spectrum != NULL) {
                float dt = (float)frame_interval / 1000.0f;

                smp_spectrum_update(spectrum, dt);
            }

            render_frame(&frame, &ui, &playlist, player, smp_audio_name(device), &output);

            /* Only touch the terminal when something actually changed. */
            if (output.len != previous.len
                || memcmp(sb_cstr(&output), sb_cstr(&previous), output.len) != 0)
            {
                StrBuf swap = previous;

                smp_term_write(sb_cstr(&output), output.len);
                previous = output;
                output = swap;
            }
        }
    }

cleanup:
    if (player != NULL) {
        smp_player_destroy(player);
    }
    smp_spectrum_destroy(spectrum);
    smp_audio_close(device);
    smp_term_shutdown();
    smp_frame_free(&frame);
    sb_free(&output);
    sb_free(&previous);
    smp_ui_free(&ui);
    smp_playlist_free(&playlist);
    return exit_code;
}

static int smp_main_entry(int argc, char **argv)
{
    Options options;

    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

    if (options.list_devices) {
        smp_audio_list_devices();
        return 0;
    }

    if (options.init_config) {
        char *path = smp_config_user_path();
        SmpStatus write_status;
        char *directory;

        if (path == NULL) {
            fprintf(stderr, "smp: cannot resolve the configuration path\n");
            return 1;
        }

        directory = smp_strdup(path);
        if (directory != NULL) {
            char *separator = strrchr(directory, '/');
#ifdef _WIN32
            {
                char *backslash = strrchr(directory, '\\');

                if (backslash != NULL && (separator == NULL || backslash > separator)) {
                    separator = backslash;
                }
            }
#endif
            if (separator != NULL) {
                *separator = '\0';
                (void)smp_dir_create(directory);
            }
            free(directory);
        }

        write_status = smp_config_write_template(path);
        if (write_status == SMP_OK) {
            printf("wrote %s\n", path);
        } else if (write_status == SMP_ERR_IO) {
            fprintf(stderr, "smp: %s already exists\n", path);
        } else {
            fprintf(stderr, "smp: cannot write %s\n", path);
        }
        free(path);
        return write_status == SMP_OK ? 0 : 1;
    }
    /*
     * Without a directory, run_application() asks the shell for one before it
     * opens the player and falls back to the usage message when the platform
     * has no folder chooser.
     */


    return run_application(&options, argv[0]);
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv)
{
    char **argv;
    int index;
    int result;

    argv = (char **)calloc((size_t)argc + 1u, sizeof(char *));
    if (argv == NULL) {
        return 1;
    }

    for (index = 0; index < argc; index++) {
        argv[index] = smp_wide_to_utf8(wide_argv[index]);
        if (argv[index] == NULL) {
            argv[index] = smp_strdup("");
        }
    }

    result = smp_main_entry(argc, argv);

    for (index = 0; index < argc; index++) {
        free(argv[index]);
    }
    free(argv);
    return result;
}
#else
int main(int argc, char **argv)
{
    return smp_main_entry(argc, argv);
}
#endif
