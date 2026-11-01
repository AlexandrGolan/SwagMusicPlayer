/*
 * SMP - Swag Music Player
 * Configuration file: interface colours and playback options.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/fs.h"
#include "smp/str.h"

#define SMP_CONFIG_LINE_MAX 512u
#define SMP_CONFIG_MAX_VOLUME 100
#define SMP_CONFIG_DEFAULT_VOLUME 70
#define SMP_CONFIG_MIN_FRAME_MS 16
#define SMP_CONFIG_MAX_FRAME_MS 500
#define SMP_CONFIG_DEFAULT_FRAME_MS 80

static const SmpColor config_white = {255, 255, 255};

static const char config_template[] =
    "# SMP configuration\n"
    "#\n"
    "# Colours accept #RRGGBB, #RGB or the names white, black and default.\n"
    "# Lines starting with '#' or ';' are comments.\n"
    "#\n"
    "# Interface colours\n"
    "text         = #FFFFFF\n"
    "accent       = #FFFFFF\n"
    "dim          = #FFFFFF\n"
    "selection_bg = #FFFFFF\n"
    "playing      = #FFFFFF\n"
    "error        = #FFFFFF\n"
    "progress     = #FFFFFF\n"
    "volume_bar   = #FFFFFF\n"
    "border       = #FFFFFF\n"
    "cover        = #FFFFFF\n"
    "spectrum     = #FFFFFF\n"
    "#\n"
    "# Playback and interface options\n"
    "# Initial volume, 0..100.\n"
    "volume = 70\n"
    "# Draw the spectrum analyser (true/false).\n"
    "visualizer = true\n"
    "# Cover art style: blocks or ascii.\n"
    "cover_mode = blocks\n"
    "# Interface refresh interval in milliseconds, 16..500.\n"
    "frame_interval_ms = 80\n";

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static bool text_ieq(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (lower_ascii(*left) != lower_ascii(*right)) {
            return false;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static char *trim_left(char *text)
{
    while (*text != '\0' && is_space(*text)) {
        text++;
    }
    return text;
}

static void trim_right(char *text)
{
    size_t length = strlen(text);

    while (length > 0u && is_space(text[length - 1u])) {
        text[length - 1u] = '\0';
        length--;
    }
}

/*
 * A '#' or ';' after the first character starts an inline comment. The first
 * character is preserved so that colour values such as "#FFFFFF" survive.
 */
static void strip_inline_comment(char *text)
{
    size_t index;

    for (index = 1u; text[index] != '\0'; index++) {
        if (text[index] == '#' || text[index] == ';') {
            text[index] = '\0';
            return;
        }
    }
}

static bool parse_bool(const char *text, bool *out)
{
    if (text == NULL || out == NULL) {
        return false;
    }

    if (text_ieq(text, "true") || text_ieq(text, "yes") || text_ieq(text, "on")
        || text_ieq(text, "1"))
    {
        *out = true;
        return true;
    }
    if (text_ieq(text, "false") || text_ieq(text, "no") || text_ieq(text, "off")
        || text_ieq(text, "0"))
    {
        *out = false;
        return true;
    }
    return false;
}

static bool parse_int_clamped(const char *text, int minimum, int maximum, int *out)
{
    char *end = NULL;
    long value;

    if (text == NULL || out == NULL || text[0] == '\0') {
        return false;
    }

    value = strtol(text, &end, 10);
    if (end == text) {
        return false;
    }
    while (is_space(*end)) {
        end++;
    }
    if (*end != '\0') {
        return false;
    }

    if (value < (long)minimum) {
        value = (long)minimum;
    } else if (value > (long)maximum) {
        value = (long)maximum;
    }
    *out = (int)value;
    return true;
}

static SmpColor *color_slot(SmpConfig *config, const char *key)
{
    if (text_ieq(key, "text")) {
        return &config->text;
    }
    if (text_ieq(key, "accent")) {
        return &config->accent;
    }
    if (text_ieq(key, "dim")) {
        return &config->dim;
    }
    if (text_ieq(key, "selection_bg")) {
        return &config->selection_bg;
    }
    if (text_ieq(key, "playing")) {
        return &config->playing;
    }
    if (text_ieq(key, "error")) {
        return &config->error;
    }
    if (text_ieq(key, "progress")) {
        return &config->progress;
    }
    if (text_ieq(key, "border")) {
        return &config->border;
    }
    if (text_ieq(key, "cover")) {
        return &config->cover;
    }
    if (text_ieq(key, "spectrum")) {
        return &config->spectrum;
    }
    return NULL;
}

static void apply_setting(SmpConfig *config, const char *key, const char *value)
{
    SmpColor *slot;
    SmpColor color;
    bool flag;
    int number;

    if (text_ieq(key, "volume")) {
        if (parse_int_clamped(value, 0, SMP_CONFIG_MAX_VOLUME, &number)) {
            config->volume = number;
        }
        return;
    }

    if (text_ieq(key, "volume_bar")) {
        if (smp_color_parse(value, &color)) {
            config->volume_color = color;
        }
        return;
    }

    slot = color_slot(config, key);
    if (slot != NULL) {
        if (smp_color_parse(value, &color)) {
            *slot = color;
        }
        return;
    }

    if (text_ieq(key, "visualizer")) {
        if (parse_bool(value, &flag)) {
            config->visualizer = flag;
        }
        return;
    }

    if (text_ieq(key, "cover_mode")) {
        if (text_ieq(value, "blocks") || text_ieq(value, "0")) {
            config->cover_mode = 0;
        } else if (text_ieq(value, "ascii") || text_ieq(value, "1")) {
            config->cover_mode = 1;
        }
        return;
    }

    if (text_ieq(key, "frame_interval_ms")) {
        if (parse_int_clamped(value, SMP_CONFIG_MIN_FRAME_MS, SMP_CONFIG_MAX_FRAME_MS, &number)) {
            config->frame_interval_ms = number;
        }
        return;
    }
}

static void apply_line(SmpConfig *config, char *line)
{
    char *cursor = trim_left(line);
    char *equals;
    char *key;
    char *value;

    if (*cursor == '\0' || *cursor == '#' || *cursor == ';') {
        return;
    }

    equals = strchr(cursor, '=');
    if (equals == NULL) {
        return;
    }
    *equals = '\0';

    key = trim_left(cursor);
    trim_right(key);
    if (*key == '\0') {
        return;
    }

    value = trim_left(equals + 1);
    strip_inline_comment(value);
    trim_right(value);
    if (*value == '\0') {
        return;
    }

    apply_setting(config, key, value);
}

static bool read_config_line(FILE *file, char *line, size_t capacity, bool *usable)
{
    size_t length;
    int ch;

    if (fgets(line, (int)capacity, file) == NULL) {
        return false;
    }

    length = strlen(line);
    if (length > 0u && line[length - 1u] == '\n') {
        *usable = true;
        return true;
    }
    if (feof(file) != 0) {
        *usable = true;
        return true;
    }

    *usable = false;
    while ((ch = fgetc(file)) != EOF && ch != '\n') {
    }
    return true;
}

static FILE *open_write_file(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = smp_utf8_to_wide(path);
    FILE *file = NULL;

    if (wide != NULL) {
        file = _wfopen(wide, L"wb");
        free(wide);
    }
    return file;
#else
    return fopen(path, "wb");
#endif
}

static void create_parent_dirs(const char *path)
{
    char *copy = smp_strdup(path);
    char *last = NULL;
    char *cursor;

    if (copy == NULL) {
        return;
    }

    for (cursor = copy; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            last = cursor;
        }
    }
    if (last != NULL && last != copy) {
        *last = '\0';
        (void)smp_dir_create(copy);
    }
    free(copy);
}

static void copy_path(char *destination, size_t size, const char *source)
{
    size_t length;

    if (destination == NULL || size == 0u) {
        return;
    }
    if (source == NULL) {
        source = "";
    }

    length = strlen(source);
    if (length >= size) {
        length = size - 1u;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
}

void smp_config_defaults(SmpConfig *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->text = config_white;
    config->accent = config_white;
    config->dim = config_white;
    config->selection_bg = config_white;
    config->playing = config_white;
    config->error = config_white;
    config->progress = config_white;
    config->volume_color = config_white;
    config->border = config_white;
    config->cover = config_white;
    config->spectrum = config_white;

    config->volume = SMP_CONFIG_DEFAULT_VOLUME;
    config->visualizer = true;
    config->cover_mode = 0;
    config->frame_interval_ms = SMP_CONFIG_DEFAULT_FRAME_MS;
}

bool smp_color_parse(const char *text, SmpColor *out)
{
    char token[8];
    size_t length;
    size_t index;
    SmpColor color;

    if (text == NULL || out == NULL) {
        return false;
    }

    while (is_space(*text)) {
        text++;
    }
    length = strlen(text);
    while (length > 0u && is_space(text[length - 1u])) {
        length--;
    }
    if (length == 0u || length >= sizeof(token)) {
        return false;
    }

    for (index = 0u; index < length; index++) {
        token[index] = text[index];
    }
    token[length] = '\0';

    if (text_ieq(token, "white") || text_ieq(token, "default")) {
        color = config_white;
    } else if (text_ieq(token, "black")) {
        color.r = 0;
        color.g = 0;
        color.b = 0;
    } else if (token[0] == '#') {
        if (length == 7u) {
            int red_high = hex_value(token[1]);
            int red_low = hex_value(token[2]);
            int green_high = hex_value(token[3]);
            int green_low = hex_value(token[4]);
            int blue_high = hex_value(token[5]);
            int blue_low = hex_value(token[6]);

            if (red_high < 0 || red_low < 0 || green_high < 0 || green_low < 0
                || blue_high < 0 || blue_low < 0)
            {
                return false;
            }
            color.r = (uint8_t)(red_high * 16 + red_low);
            color.g = (uint8_t)(green_high * 16 + green_low);
            color.b = (uint8_t)(blue_high * 16 + blue_low);
        } else if (length == 4u) {
            int red = hex_value(token[1]);
            int green = hex_value(token[2]);
            int blue = hex_value(token[3]);

            if (red < 0 || green < 0 || blue < 0) {
                return false;
            }
            color.r = (uint8_t)(red * 17);
            color.g = (uint8_t)(green * 17);
            color.b = (uint8_t)(blue * 17);
        } else {
            return false;
        }
    } else {
        return false;
    }

    *out = color;
    return true;
}

SmpStatus smp_config_load(SmpConfig *config, const char *path)
{
    FILE *file;
    char line[SMP_CONFIG_LINE_MAX];
    bool usable = false;

    if (config == NULL || path == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    file = smp_file_open(path);
    if (file == NULL) {
        return smp_path_exists(path) ? SMP_ERR_IO : SMP_ERR_NOT_FOUND;
    }

    while (read_config_line(file, line, sizeof(line), &usable)) {
        if (usable) {
            apply_line(config, line);
        }
    }

    if (ferror(file) != 0) {
        fclose(file);
        return SMP_ERR_IO;
    }
    fclose(file);
    return SMP_OK;
}

char *smp_config_user_path(void)
{
#ifdef _WIN32
    const wchar_t *wide_appdata = _wgetenv(L"APPDATA");
    char *appdata;
    char *smp_dir;
    char *path;

    if (wide_appdata == NULL || wide_appdata[0] == L'\0') {
        return NULL;
    }
    appdata = smp_wide_to_utf8(wide_appdata);
    if (appdata == NULL) {
        return NULL;
    }

    smp_dir = smp_path_join(appdata, "smp");
    free(appdata);
    if (smp_dir == NULL) {
        return NULL;
    }

    path = smp_path_join(smp_dir, "smp.conf");
    free(smp_dir);
    return path;
#else
    const char *base = getenv("XDG_CONFIG_HOME");
    char *config_dir;
    char *smp_dir;
    char *path;

    if (base != NULL && base[0] == '/') {
        config_dir = smp_strdup(base);
    } else {
        const char *home = getenv("HOME");

        if (home == NULL || home[0] == '\0') {
            return NULL;
        }
        config_dir = smp_path_join(home, ".config");
    }
    if (config_dir == NULL) {
        return NULL;
    }

    smp_dir = smp_path_join(config_dir, "smp");
    free(config_dir);
    if (smp_dir == NULL) {
        return NULL;
    }

    path = smp_path_join(smp_dir, "smp.conf");
    free(smp_dir);
    return path;
#endif
}

SmpStatus smp_config_load_user(SmpConfig *config, char *used_path, size_t used_path_size)
{
    char *path;
    SmpStatus status;

    copy_path(used_path, used_path_size, "");
    if (config == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    path = smp_config_user_path();
    if (path == NULL) {
        return SMP_ERR_NOT_FOUND;
    }

    copy_path(used_path, used_path_size, path);
    status = smp_config_load(config, path);
    free(path);
    return status;
}

SmpStatus smp_config_write_template(const char *path)
{
    FILE *file;
    size_t length;
    size_t written;
    bool ok;

    if (path == NULL || path[0] == '\0') {
        return SMP_ERR_INVALID_ARG;
    }
    if (smp_path_exists(path)) {
        return SMP_ERR_IO;
    }

    create_parent_dirs(path);
    file = open_write_file(path);
    if (file == NULL) {
        return SMP_ERR_IO;
    }

    length = sizeof(config_template) - 1u;
    written = fwrite(config_template, 1u, length, file);
    ok = written == length && ferror(file) == 0;
    if (fclose(file) != 0) {
        ok = false;
    }
    return ok ? SMP_OK : SMP_ERR_IO;
}
