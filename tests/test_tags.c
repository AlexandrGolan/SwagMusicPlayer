/*
 * SMP - Swag Music Player
 * Tests for embedded metadata parsing.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/fs.h"
#include "smp/tags.h"
#include "test.h"

static void write_syncsafe(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)((value >> 21) & 0x7fu);
    out[1] = (uint8_t)((value >> 14) & 0x7fu);
    out[2] = (uint8_t)((value >> 7) & 0x7fu);
    out[3] = (uint8_t)(value & 0x7fu);
}

static void write_be32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)((value >> 24) & 0xffu);
    out[1] = (uint8_t)((value >> 16) & 0xffu);
    out[2] = (uint8_t)((value >> 8) & 0xffu);
    out[3] = (uint8_t)(value & 0xffu);
}

static void write_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xffu);
    out[1] = (uint8_t)((value >> 8) & 0xffu);
    out[2] = (uint8_t)((value >> 16) & 0xffu);
    out[3] = (uint8_t)((value >> 24) & 0xffu);
}

/* Appends an ID3v2.3 text frame and returns the number of bytes written. */
static size_t append_text_frame(uint8_t *out, const char *id, const char *text, uint8_t encoding)
{
    size_t text_length = strlen(text);
    size_t payload = 1u + text_length;

    memcpy(out, id, 4);
    write_be32(out + 4, (uint32_t)payload);
    out[8] = 0;
    out[9] = 0;
    out[10] = encoding;
    memcpy(out + 11, text, text_length);
    return 10u + payload;
}

static size_t append_utf16_frame(uint8_t *out, const char *id, const char *ascii_text)
{
    size_t length = strlen(ascii_text);
    size_t index;
    size_t payload = 1u + 2u + length * 2u;

    memcpy(out, id, 4);
    write_be32(out + 4, (uint32_t)payload);
    out[8] = 0;
    out[9] = 0;
    out[10] = 1; /* UTF-16 with BOM */
    out[11] = 0xff;
    out[12] = 0xfe;
    for (index = 0; index < length; index++) {
        out[13 + index * 2] = (uint8_t)ascii_text[index];
        out[14 + index * 2] = 0;
    }
    return 10u + payload;
}

static void test_id3v2_utf8(void)
{
    uint8_t buffer[512];
    size_t offset = 10;
    uint32_t body_size;
    SmpTags tags;

    smp_test_begin("id3v2.3 utf8 frames");
    offset += append_text_frame(buffer + offset, "TIT2", "Duvet", 3);
    offset += append_text_frame(buffer + offset, "TPE1", "Boa", 3);
    offset += append_text_frame(buffer + offset, "TALB", "Twilight", 3);

    body_size = (uint32_t)(offset - 10u);
    memcpy(buffer, "ID3", 3);
    buffer[3] = 3;
    buffer[4] = 0;
    buffer[5] = 0;
    write_syncsafe(buffer + 6, body_size);

    smp_tags_init(&tags);
    SMP_CHECK_INT(smp_id3v2_size(buffer, offset), offset);
    SMP_CHECK_INT(smp_tags_parse_id3v2(buffer, offset, &tags), SMP_TAG_FORMAT_ID3V2);
    SMP_CHECK_STR(tags.title, "Duvet");
    SMP_CHECK_STR(tags.artist, "Boa");
    SMP_CHECK_STR(tags.album, "Twilight");
    smp_test_end();
}

static void test_id3v2_encodings(void)
{
    uint8_t buffer[512];
    size_t offset = 10;
    uint32_t body_size;
    SmpTags tags;

    smp_test_begin("id3v2 latin1 and utf16 frames");
    /* 0xE9 is 'Г©' in ISO-8859-1. */
    offset += append_text_frame(buffer + offset, "TIT2", "Caf\xe9", 0);
    offset += append_utf16_frame(buffer + offset, "TPE1", "Aya");

    body_size = (uint32_t)(offset - 10u);
    memcpy(buffer, "ID3", 3);
    buffer[3] = 3;
    buffer[4] = 0;
    buffer[5] = 0;
    write_syncsafe(buffer + 6, body_size);

    smp_tags_init(&tags);
    SMP_CHECK_INT(smp_tags_parse_id3v2(buffer, offset, &tags), SMP_TAG_FORMAT_ID3V2);
    SMP_CHECK_STR(tags.title, "Caf\xc3\xa9");
    SMP_CHECK_STR(tags.artist, "Aya");
    smp_test_end();
}

static void test_id3v2_24(void)
{
    uint8_t buffer[256];
    size_t offset = 10;
    uint32_t body_size;

    smp_test_begin("id3v2.4 syncsafe frame sizes");
    offset += append_text_frame(buffer + offset, "TIT2", "Modern", 3);

    body_size = (uint32_t)(offset - 10u);
    memcpy(buffer, "ID3", 3);
    buffer[3] = 4;
    buffer[4] = 0;
    buffer[5] = 0;
    write_syncsafe(buffer + 6, body_size);

    {
        SmpTags tags;

        smp_tags_init(&tags);
        SMP_CHECK_INT(smp_tags_parse_id3v2(buffer, offset, &tags), SMP_TAG_FORMAT_ID3V2);
        SMP_CHECK_STR(tags.title, "Modern");
    }
    smp_test_end();
}

static void test_id3v2_22(void)
{
    uint8_t buffer[256];
    size_t offset = 10;
    uint32_t body_size;
    const char *text = "Oldie";

    smp_test_begin("id3v2.2 three character frames");
    memcpy(buffer + offset, "TT2", 3);
    buffer[offset + 3] = 0;
    buffer[offset + 4] = 0;
    buffer[offset + 5] = (uint8_t)(1u + strlen(text));
    buffer[offset + 6] = 3;
    memcpy(buffer + offset + 7, text, strlen(text));
    offset += 6u + 1u + strlen(text);

    body_size = (uint32_t)(offset - 10u);
    memcpy(buffer, "ID3", 3);
    buffer[3] = 2;
    buffer[4] = 0;
    buffer[5] = 0;
    write_syncsafe(buffer + 6, body_size);

    {
        SmpTags tags;

        smp_tags_init(&tags);
        SMP_CHECK_INT(smp_tags_parse_id3v2(buffer, offset, &tags), SMP_TAG_FORMAT_ID3V2);
        SMP_CHECK_STR(tags.title, "Oldie");
    }
    smp_test_end();
}

static void test_id3v1(void)
{
    uint8_t record[128];
    SmpTags tags;

    smp_test_begin("id3v1 with cp1251 text");
    memset(record, 0, sizeof(record));
    memcpy(record, "TAG", 3);
    /* "РџСЂРёРІРµС‚" in CP1251. */
    memcpy(record + 3, "\xcf\xf0\xe8\xe2\xe5\xf2", 6);
    memcpy(record + 33, "Artist", 6);
    memcpy(record + 63, "Album", 5);

    smp_tags_init(&tags);
    SMP_CHECK(smp_tags_parse_id3v1(record, sizeof(record), &tags));
    SMP_CHECK_STR(tags.title, "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82");
    SMP_CHECK_STR(tags.artist, "Artist");
    SMP_CHECK_STR(tags.album, "Album");
    smp_test_end();
}

static void test_id3v1_absent(void)
{
    uint8_t record[128];
    SmpTags tags;

    smp_test_begin("id3v1 absent is detected");
    memset(record, 0, sizeof(record));
    smp_tags_init(&tags);
    SMP_CHECK(!smp_tags_parse_id3v1(record, sizeof(record), &tags));
    smp_test_end();
}

static void test_riff_info(void)
{
    uint8_t buffer[256];
    size_t offset = 12;
    const char *title = "My Song";
    size_t title_size = strlen(title) + 1u;

    smp_test_begin("riff INFO chunks");

    memcpy(buffer, "RIFF", 4);
    memcpy(buffer + 8, "WAVE", 4);

    memcpy(buffer + offset, "LIST", 4);
    write_le32(buffer + offset + 4, (uint32_t)(4u + 8u + title_size));
    memcpy(buffer + offset + 8, "INFO", 4);
    offset += 12;

    memcpy(buffer + offset, "INAM", 4);
    write_le32(buffer + offset + 4, (uint32_t)title_size);
    memcpy(buffer + offset + 8, title, title_size);
    offset += 8u + title_size;

    write_le32(buffer + 4, (uint32_t)(offset - 8u));

    {
        SmpTags tags;

        smp_tags_init(&tags);
        SMP_CHECK_INT(smp_tags_parse_riff(buffer, offset, &tags), SMP_TAG_FORMAT_RIFF_INFO);
        SMP_CHECK_STR(tags.title, "My Song");
    }
    smp_test_end();
}

static void test_read_file_fallback(void)
{
    const char *path = "smp_test_tags_track01.wav";
    FILE *file;
    SmpTags tags;

    smp_test_begin("tags fall back to the file name");
    file = fopen(path, "wb");
    SMP_CHECK(file != NULL);
    if (file != NULL) {
        fputs("not really a wav file", file);
        fclose(file);
    }

    smp_tags_init(&tags);
    SMP_CHECK_INT(smp_tags_read_file(path, &tags), SMP_OK);
    SMP_CHECK_STR(tags.title, "smp_test_tags_track01");
    SMP_CHECK_STR(tags.artist, "Unknown artist");
    SMP_CHECK_STR(tags.album, "Unknown album");

    (void)smp_file_remove(path);
    smp_test_end();
}

static void test_read_file_id3v2(void)
{
    const char *path = "smp_test_tags_id3.mp3";
    uint8_t buffer[256];
    size_t offset = 10;
    uint32_t body_size;
    FILE *file;
    SmpTags tags;

    smp_test_begin("tags are read from a real file");

    offset += append_text_frame(buffer + offset, "TIT2", "File Title", 3);
    offset += append_text_frame(buffer + offset, "TPE1", "File Artist", 3);
    body_size = (uint32_t)(offset - 10u);
    memcpy(buffer, "ID3", 3);
    buffer[3] = 3;
    buffer[4] = 0;
    buffer[5] = 0;
    write_syncsafe(buffer + 6, body_size);

    file = fopen(path, "wb");
    SMP_CHECK(file != NULL);
    if (file != NULL) {
        SMP_CHECK_INT(fwrite(buffer, 1, offset, file), offset);
        fclose(file);
    }

    smp_tags_init(&tags);
    SMP_CHECK_INT(smp_tags_read_file(path, &tags), SMP_OK);
    SMP_CHECK_STR(tags.title, "File Title");
    SMP_CHECK_STR(tags.artist, "File Artist");

    (void)smp_file_remove(path);
    smp_test_end();
}

static void test_riff_info_after_data(void)
{
    const char *path = "smp_test_tags_tail_info.wav";
    uint8_t header[44];
    const char *title = "Tail Title";
    size_t title_size = strlen(title) + 1u;
    FILE *file;
    SmpTags tags;

    smp_test_begin("riff INFO placed after the data chunk");

    memset(header, 0, sizeof(header));
    memcpy(header, "RIFF", 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    write_le32(header + 16, 16);
    header[20] = 1;
    header[22] = 2;
    write_le32(header + 24, 48000);
    write_le32(header + 28, 192000);
    header[32] = 4;
    header[34] = 16;
    memcpy(header + 36, "data", 4);
    write_le32(header + 40, 2048u);

    file = fopen(path, "wb");
    SMP_CHECK(file != NULL);
    if (file != NULL) {
        uint8_t *silence = (uint8_t *)calloc(1, 2048);

        fwrite(header, 1, sizeof(header), file);
        if (silence != NULL) {
            fwrite(silence, 1, 2048, file);
            free(silence);
        }

        /* LIST/INFO comes after the samples, exactly like some encoders do. */
        {
            uint8_t list[64];
            size_t offset = 0;

            memcpy(list + offset, "LIST", 4);
            write_le32(list + offset + 4, (uint32_t)(4u + 8u + title_size));
            offset += 8;
            memcpy(list + offset, "INFO", 4);
            offset += 4;
            memcpy(list + offset, "INAM", 4);
            write_le32(list + offset + 4, (uint32_t)title_size);
            offset += 8;
            memcpy(list + offset, title, title_size);
            offset += title_size;
            fwrite(list, 1, offset, file);
        }
        fclose(file);
    }

    smp_tags_init(&tags);
    SMP_CHECK_INT(smp_tags_read_file(path, &tags), SMP_OK);
    SMP_CHECK_STR(tags.title, "Tail Title");

    (void)smp_file_remove(path);
    smp_test_end();
}

void suite_tags(void)
{
    smp_test_suite("tags");
    test_riff_info_after_data();
    test_id3v2_utf8();
    test_id3v2_encodings();
    test_id3v2_24();
    test_id3v2_22();
    test_id3v1();
    test_id3v1_absent();
    test_riff_info();
    test_read_file_fallback();
    test_read_file_id3v2();
}
