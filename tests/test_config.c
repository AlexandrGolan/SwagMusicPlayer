/*
 * SMP - Swag Music Player
 * Tests for the configuration file parser.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/config.h"
#include "smp/fs.h"
#include "test.h"

void suite_config(void);

static bool color_is(SmpColor color, int red, int green, int blue)
{
    return (int)color.r == red && (int)color.g == green && (int)color.b == blue;
}

static bool write_text_file(const char *path, const char *text)
{
    FILE *file;
    size_t length;
    bool ok;

    file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }

    length = strlen(text);
    ok = fwrite(text, 1u, length, file) == length;
    if (fclose(file) != 0) {
        ok = false;
    }
    return ok;
}

static void check_defaults(const SmpConfig *config)
{
    SMP_CHECK(color_is(config->text, 255, 255, 255));
    SMP_CHECK(color_is(config->accent, 255, 255, 255));
    SMP_CHECK(color_is(config->dim, 255, 255, 255));
    SMP_CHECK(color_is(config->selection_bg, 255, 255, 255));
    SMP_CHECK(color_is(config->playing, 255, 255, 255));
    SMP_CHECK(color_is(config->error, 255, 255, 255));
    SMP_CHECK(color_is(config->progress, 255, 255, 255));
    SMP_CHECK(color_is(config->volume_color, 255, 255, 255));
    SMP_CHECK(color_is(config->border, 255, 255, 255));
    SMP_CHECK(color_is(config->cover, 255, 255, 255));
    SMP_CHECK(color_is(config->spectrum, 255, 255, 255));
    SMP_CHECK_INT(config->volume, 70);
    SMP_CHECK(config->visualizer);
    SMP_CHECK_INT(config->cover_mode, 0);
    SMP_CHECK_INT(config->frame_interval_ms, 80);
}

static void test_defaults(void)
{
    SmpConfig config;

    smp_test_begin("defaults are white with volume 70 and 80 ms");
    smp_config_defaults(&config);
    check_defaults(&config);
    smp_test_end();
}

static void test_color_parse(void)
{
    SmpColor color;

    smp_test_begin("colour spellings parse correctly");
    SMP_CHECK(smp_color_parse("#102030", &color));
    SMP_CHECK(color_is(color, 16, 32, 48));

    SMP_CHECK(smp_color_parse("#abc", &color));
    SMP_CHECK(color_is(color, 0xAA, 0xBB, 0xCC));

    SMP_CHECK(smp_color_parse("#ABC", &color));
    SMP_CHECK(color_is(color, 0xAA, 0xBB, 0xCC));

    SMP_CHECK(smp_color_parse("white", &color));
    SMP_CHECK(color_is(color, 255, 255, 255));

    SMP_CHECK(smp_color_parse("BLACK", &color));
    SMP_CHECK(color_is(color, 0, 0, 0));

    SMP_CHECK(smp_color_parse("default", &color));
    SMP_CHECK(color_is(color, 255, 255, 255));

    SMP_CHECK(smp_color_parse("  #0F0  ", &color));
    SMP_CHECK(color_is(color, 0, 255, 0));

    color.r = 1;
    color.g = 2;
    color.b = 3;
    SMP_CHECK(!smp_color_parse("red", &color));
    SMP_CHECK(!smp_color_parse("#12345", &color));
    SMP_CHECK(!smp_color_parse("#GGGGGG", &color));
    SMP_CHECK(!smp_color_parse("", &color));
    SMP_CHECK(!smp_color_parse(NULL, &color));
    SMP_CHECK(!smp_color_parse("#FFFFFF", NULL));
    SMP_CHECK(color_is(color, 1, 2, 3));
    smp_test_end();
}

static void test_all_color_keys(void)
{
    const char *path = "smp_test_config_colors.conf";
    static const char *text =
        "text = #010101\n"
        "accent = #020202\n"
        "dim = #030303\n"
        "selection_bg = #040404\n"
        "playing = #050505\n"
        "error = #060606\n"
        "progress = #070707\n"
        "volume_bar = #080808\n"
        "border = #090909\n"
        "cover = #0A0A0A\n"
        "spectrum = #0B0B0B\n";
    SmpConfig config;

    smp_test_begin("every colour key maps to its field");
    (void)smp_file_remove(path);
    SMP_CHECK(write_text_file(path, text));
    smp_config_defaults(&config);
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK(color_is(config.text, 1, 1, 1));
    SMP_CHECK(color_is(config.accent, 2, 2, 2));
    SMP_CHECK(color_is(config.dim, 3, 3, 3));
    SMP_CHECK(color_is(config.selection_bg, 4, 4, 4));
    SMP_CHECK(color_is(config.playing, 5, 5, 5));
    SMP_CHECK(color_is(config.error, 6, 6, 6));
    SMP_CHECK(color_is(config.progress, 7, 7, 7));
    SMP_CHECK(color_is(config.volume_color, 8, 8, 8));
    SMP_CHECK(color_is(config.border, 9, 9, 9));
    SMP_CHECK(color_is(config.cover, 0x0A, 0x0A, 0x0A));
    SMP_CHECK(color_is(config.spectrum, 0x0B, 0x0B, 0x0B));
    SMP_CHECK_INT(config.volume, 70);
    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    smp_test_end();
}

static void test_comments_and_blank_lines(void)
{
    const char *path = "smp_test_config_comments.conf";
    static const char *text =
        "# SMP test file\n"
        "; another comment style\n"
        "   # indented comment\n"
        "\n"
        "\t\n"
        "TEXT = #102030   # trailing comment\n"
        "accent = #ABC ; trailing comment\n"
        "dim = WHITE\n"
        "playing = #0F0F0F;comment without space\n"
        "volume = 40      # level, not a colour\n"
        "Visualizer = ON\n"
        "Cover_Mode = ASCII\n"
        "FRAME_INTERVAL_MS = 120#milliseconds\n"
        "border = #0F0F0F\r\n";
    SmpConfig config;

    smp_test_begin("comments, blanks and mixed case keys");
    (void)smp_file_remove(path);
    SMP_CHECK(write_text_file(path, text));
    smp_config_defaults(&config);
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK(color_is(config.text, 16, 32, 48));
    SMP_CHECK(color_is(config.accent, 0xAA, 0xBB, 0xCC));
    SMP_CHECK(color_is(config.playing, 15, 15, 15));
    SMP_CHECK(color_is(config.dim, 255, 255, 255));
    SMP_CHECK_INT(config.volume, 40);
    SMP_CHECK(color_is(config.volume_color, 255, 255, 255));
    SMP_CHECK(config.visualizer);
    SMP_CHECK_INT(config.cover_mode, 1);
    SMP_CHECK_INT(config.frame_interval_ms, 120);
    SMP_CHECK(color_is(config.border, 15, 15, 15));
    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    smp_test_end();
}

static void test_unknown_and_invalid(void)
{
    const char *path = "smp_test_config_invalid.conf";
    static const char *text =
        "text = #123456\n"
        "unknown_key = #FFFFFF\n"
        "frame_interval = 200\n"
        "accent = #GGGGGG\n"
        "playing = notacolour\n"
        "dim = #12\n"
        "error = 42\n"
        "volume_bar = #00FF00\n"
        "volume = notanumber\n"
        "visualizer = perhaps\n"
        "cover_mode = 7\n"
        "frame_interval_ms = soon\n"
        "this line has no equals sign\n"
        "= 42\n"
        "border =\n";
    SmpConfig config;

    smp_test_begin("unknown keys and invalid values are ignored");
    (void)smp_file_remove(path);
    SMP_CHECK(write_text_file(path, text));
    smp_config_defaults(&config);
    config.accent = (SmpColor){11, 22, 33};
    config.playing = (SmpColor){44, 55, 66};
    config.dim = (SmpColor){1, 2, 3};
    config.error = (SmpColor){77, 88, 99};
    config.volume = 42;
    config.visualizer = false;
    config.cover_mode = 1;
    config.frame_interval_ms = 33;

    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK(color_is(config.text, 0x12, 0x34, 0x56));
    SMP_CHECK(color_is(config.accent, 11, 22, 33));
    SMP_CHECK(color_is(config.playing, 44, 55, 66));
    SMP_CHECK(color_is(config.dim, 1, 2, 3));
    SMP_CHECK(color_is(config.error, 77, 88, 99));
    SMP_CHECK(color_is(config.border, 255, 255, 255));
    SMP_CHECK(color_is(config.volume_color, 0, 255, 0));
    SMP_CHECK_INT(config.volume, 42);
    SMP_CHECK(!config.visualizer);
    SMP_CHECK_INT(config.cover_mode, 1);
    SMP_CHECK_INT(config.frame_interval_ms, 33);
    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    smp_test_end();
}

static void test_boolean_spellings(void)
{
    const char *path = "smp_test_config_bool.conf";
    static const char *const values[] = {"true", "yes", "on", "1", "false", "no", "off", "0"};
    static const bool expected[] = {true, true, true, true, false, false, false, false};
    SmpConfig config;
    size_t index;

    smp_test_begin("boolean spellings are accepted");
    (void)smp_file_remove(path);
    for (index = 0u; index < SMP_ARRAY_LEN(values); index++) {
        char text[64];
        int length;
        bool ok;

        length = snprintf(text, sizeof(text), "visualizer = %s\n", values[index]);
        ok = length > 0;
        if (ok) {
            ok = write_text_file(path, text);
        }
        SMP_CHECK(ok);
        if (!ok) {
            break;
        }

        smp_config_defaults(&config);
        config.visualizer = !expected[index];
        SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
        SMP_CHECK(config.visualizer == expected[index]);
    }
    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    smp_test_end();
}

static void test_clamping(void)
{
    const char *path = "smp_test_config_clamp.conf";
    SmpConfig config;

    smp_test_begin("numeric options clamp to their ranges");
    (void)smp_file_remove(path);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "volume = 150\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.volume, 100);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "volume = -25\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.volume, 0);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "volume = 55\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.volume, 55);
    SMP_CHECK(color_is(config.volume_color, 255, 255, 255));

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "frame_interval_ms = 5\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.frame_interval_ms, 16);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "frame_interval_ms = 900\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.frame_interval_ms, 500);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "frame_interval_ms = 16\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.frame_interval_ms, 16);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "cover_mode = 1\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.cover_mode, 1);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "cover_mode = 2\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.cover_mode, 0);

    smp_config_defaults(&config);
    SMP_CHECK(write_text_file(path, "cover_mode = ascii\n"));
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    SMP_CHECK_INT(config.cover_mode, 1);

    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    smp_test_end();
}

static void test_template_roundtrip(void)
{
    const char *path = "smp_test_config_template.conf";
    SmpConfig loaded;

    smp_test_begin("written template loads back as the defaults");
    (void)smp_file_remove(path);
    SMP_CHECK_INT(smp_config_write_template(path), SMP_OK);
    SMP_CHECK(smp_path_exists(path));

    smp_config_defaults(&loaded);
    loaded.text = (SmpColor){1, 2, 3};
    loaded.volume = 3;
    loaded.visualizer = false;
    loaded.cover_mode = 1;
    loaded.frame_interval_ms = 999;
    SMP_CHECK_INT(smp_config_load(&loaded, path), SMP_OK);
    check_defaults(&loaded);

    /* An existing file must never be overwritten. */
    SMP_CHECK_INT(smp_config_write_template(path), SMP_ERR_IO);
    smp_config_defaults(&loaded);
    loaded.volume = 1;
    SMP_CHECK_INT(smp_config_load(&loaded, path), SMP_OK);
    check_defaults(&loaded);

    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    SMP_CHECK(!smp_path_exists(path));
    smp_test_end();
}

static void test_missing_file(void)
{
    const char *path = "smp_test_config_missing.conf";
    SmpConfig config;

    smp_test_begin("missing file returns NOT_FOUND, values stay");
    (void)smp_file_remove(path);

    smp_config_defaults(&config);
    config.text = (SmpColor){7, 8, 9};
    config.volume = 12;
    config.visualizer = false;
    config.cover_mode = 1;
    config.frame_interval_ms = 33;

    SMP_CHECK_INT(smp_config_load(&config, path), SMP_ERR_NOT_FOUND);
    SMP_CHECK(color_is(config.text, 7, 8, 9));
    SMP_CHECK_INT(config.volume, 12);
    SMP_CHECK(!config.visualizer);
    SMP_CHECK_INT(config.cover_mode, 1);
    SMP_CHECK_INT(config.frame_interval_ms, 33);
    SMP_CHECK(!smp_path_exists(path));

    SMP_CHECK_INT(smp_config_load(NULL, path), SMP_ERR_INVALID_ARG);
    SMP_CHECK_INT(smp_config_load(&config, NULL), SMP_ERR_INVALID_ARG);
    smp_test_end();
}

static void test_utf8_path(void)
{
    const char *path = "smp_test_config_СЋРЅРёРєРѕРґ.conf";
    SmpConfig config;

    smp_test_begin("template round trip with a UTF-8 path");
    (void)smp_file_remove(path);
    SMP_CHECK_INT(smp_config_write_template(path), SMP_OK);
    SMP_CHECK(smp_path_exists(path));

    smp_config_defaults(&config);
    config.text = (SmpColor){1, 2, 3};
    SMP_CHECK_INT(smp_config_load(&config, path), SMP_OK);
    check_defaults(&config);

    SMP_CHECK_INT(smp_file_remove(path), SMP_OK);
    SMP_CHECK(!smp_path_exists(path));
    smp_test_end();
}

static void test_user_path(void)
{
    char *path = smp_config_user_path();

    smp_test_begin("user configuration path names smp.conf");
    SMP_CHECK(path == NULL || strcmp(smp_path_basename(path), "smp.conf") == 0);
    free(path);
    smp_test_end();
}

void suite_config(void)
{
    smp_test_suite("config");
    test_defaults();
    test_color_parse();
    test_all_color_keys();
    test_comments_and_blank_lines();
    test_unknown_and_invalid();
    test_boolean_spellings();
    test_clamping();
    test_template_roundtrip();
    test_missing_file();
    test_utf8_path();
    test_user_path();
}
