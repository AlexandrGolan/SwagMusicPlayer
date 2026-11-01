/*
 * SMP - Swag Music Player
 * Embedded metadata: ID3v2, ID3v1 and RIFF INFO chunks.
 *
 * SPDX-License-Identifier: MIT
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "smp/tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/fs.h"
#include "smp/str.h"

#define TAG_READ_LIMIT (1024u * 1024u)

/* ------------------------------------------------------------------------- */
/* Text helpers                                                              */
/* ------------------------------------------------------------------------- */

/* strnlen is not available everywhere, so bound the length manually. */
static size_t bounded_length(const char *text, size_t max_length)
{
    size_t length = 0;

    while (length < max_length && text[length] != '\0') {
        length++;
    }
    return length;
}

static bool is_valid_utf8(const uint8_t *data, size_t size)
{
    size_t offset = 0;

    while (offset < size) {
        uint8_t lead = data[offset];
        size_t length;
        uint32_t codepoint;
        size_t extra;

        if (lead < 0x80u) {
            offset++;
            continue;
        }
        if ((lead & 0xe0u) == 0xc0u) {
            length = 2;
            codepoint = lead & 0x1fu;
        } else if ((lead & 0xf0u) == 0xe0u) {
            length = 3;
            codepoint = lead & 0x0fu;
        } else if ((lead & 0xf8u) == 0xf0u) {
            length = 4;
            codepoint = lead & 0x07u;
        } else {
            return false;
        }

        if (offset + length > size) {
            return false;
        }
        for (extra = 1; extra < length; extra++) {
            if ((data[offset + extra] & 0xc0u) != 0x80u) {
                return false;
            }
            codepoint = (codepoint << 6) | (data[offset + extra] & 0x3fu);
        }
        if (codepoint < 0x80u || (codepoint >= 0xd800u && codepoint <= 0xdfffu)
            || codepoint > 0x10ffffu)
        {
            return false;
        }
        offset += length;
    }
    return true;
}

/*
 * CP1251 is by far the most common encoding for Cyrillic tags written by
 * Windows-era tools, so it is used as the fallback for legacy text fields.
 */
static const uint16_t cp1251_upper[64] = {
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041a,
    0x041b, 0x041c, 0x041d, 0x041e, 0x041f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425,
    0x0426, 0x0427, 0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430,
    0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043a, 0x043b,
    0x043c, 0x043d, 0x043e, 0x043f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446,
    0x0447, 0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f,
};

static uint32_t cp1251_to_unicode(uint8_t byte)
{
    if (byte < 0x80u) {
        return byte;
    }
    if (byte >= 0xc0u) {
        return cp1251_upper[byte - 0xc0u];
    }
    if (byte == 0xa8u) {
        return 0x0401; /* РЃ */
    }
    if (byte == 0xb8u) {
        return 0x0451; /* lowercase yo */
    }
    return 0xfffdu;
}

static void append_codepoint(StrBuf *out, uint32_t codepoint)
{
    char encoded[4];
    size_t length = smp_utf8_encode(codepoint, encoded);

    if (length > 0) {
        (void)sb_append_n(out, encoded, length);
    }
}

static void append_latin1(StrBuf *out, const uint8_t *data, size_t size)
{
    size_t index;

    for (index = 0; index < size; index++) {
        append_codepoint(out, data[index]);
    }
}

static void append_cp1251(StrBuf *out, const uint8_t *data, size_t size)
{
    size_t index;

    for (index = 0; index < size; index++) {
        append_codepoint(out, cp1251_to_unicode(data[index]));
    }
}

static void append_utf16(StrBuf *out, const uint8_t *data, size_t size, bool big_endian)
{
    size_t offset = 0;

    while (offset + 1u < size) {
        uint32_t unit = big_endian ? ((uint32_t)data[offset] << 8) | data[offset + 1u]
                                   : ((uint32_t)data[offset + 1u] << 8) | data[offset];

        offset += 2u;

        if (unit >= 0xd800u && unit <= 0xdbffu && offset + 1u < size) {
            uint32_t low = big_endian ? ((uint32_t)data[offset] << 8) | data[offset + 1u]
                                      : ((uint32_t)data[offset + 1u] << 8) | data[offset];

            if (low >= 0xdc00u && low <= 0xdfffu) {
                offset += 2u;
                unit = 0x10000u + ((unit - 0xd800u) << 10) + (low - 0xdc00u);
            }
        }

        append_codepoint(out, unit);
    }
}

/* Converts raw bytes to UTF-8 using the given ID3 encoding byte. */
static void decode_id3_text(uint8_t encoding, const uint8_t *data, size_t size, StrBuf *out)
{
    switch (encoding) {
    case 1: /* UTF-16 with BOM */
        if (size >= 2u && data[0] == 0xffu && data[1] == 0xfeu) {
            append_utf16(out, data + 2, size - 2u, false);
        } else if (size >= 2u && data[0] == 0xfeu && data[1] == 0xffu) {
            append_utf16(out, data + 2, size - 2u, true);
        } else {
            append_utf16(out, data, size, false);
        }
        break;
    case 2: /* UTF-16 big endian, no BOM */
        append_utf16(out, data, size, true);
        break;
    case 3: /* UTF-8 */
        if (is_valid_utf8(data, size)) {
            (void)sb_append_n(out, (const char *)data, size);
        } else {
            append_cp1251(out, data, size);
        }
        break;
    case 0: /* ISO-8859-1 */
    default:
        append_latin1(out, data, size);
        break;
    }
}

/* Trims trailing NULs and spaces, then copies into a fixed tag field. */
static void assign_field(char *field, size_t field_size, const StrBuf *text)
{
    const char *data = sb_cstr(text);
    size_t length = strlen(data);
    size_t offset = 0;
    size_t copy_size;

    while (length > 0 && (data[length - 1] == '\0' || data[length - 1] == ' '
                          || data[length - 1] == '\r' || data[length - 1] == '\n'))
    {
        length--;
    }
    while (offset < length && (data[offset] == ' ' || data[offset] == '\0')) {
        offset++;
    }

    copy_size = length - offset;
    if (copy_size >= field_size) {
        copy_size = field_size - 1u;
        /* Never cut a UTF-8 sequence in half. */
        while (copy_size > 0 && (((unsigned char)data[offset + copy_size]) & 0xc0u) == 0x80u) {
            copy_size--;
        }
    }

    if (copy_size > 0) {
        memcpy(field, data + offset, copy_size);
    }
    field[copy_size] = '\0';

    for (size_t index = 0; index < copy_size; index++) {
        if ((unsigned char)field[index] < 0x20u) {
            field[index] = ' ';
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void smp_tags_init(SmpTags *tags)
{
    if (tags == NULL) {
        return;
    }
    memset(tags, 0, sizeof(*tags));
    snprintf(tags->title, sizeof(tags->title), "%s", "");
    snprintf(tags->artist, sizeof(tags->artist), "%s", "");
    snprintf(tags->album, sizeof(tags->album), "%s", "");
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8)
           | (uint32_t)data[3];
}

static uint32_t read_be24(const uint8_t *data)
{
    return ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];
}

static uint32_t read_syncsafe32(const uint8_t *data)
{
    return ((uint32_t)(data[0] & 0x7fu) << 21) | ((uint32_t)(data[1] & 0x7fu) << 14)
           | ((uint32_t)(data[2] & 0x7fu) << 7) | (uint32_t)(data[3] & 0x7fu);
}

size_t smp_id3v2_size(const uint8_t *data, size_t size)
{
    uint32_t body_size;

    if (data == NULL || size < 10u) {
        return 0;
    }
    if (data[0] != 'I' || data[1] != 'D' || data[2] != '3') {
        return 0;
    }
    if (data[3] == 0xffu || data[4] == 0xffu) {
        return 0;
    }

    body_size = read_syncsafe32(data + 6);
    if (body_size > 64u * 1024u * 1024u) {
        return 0;
    }
    return 10u + (size_t)body_size;
}

/* Removes the 0x00 bytes that ID3v2 unsynchronisation inserted after 0xFF. */
static void deunsync(const uint8_t *source, size_t size, StrBuf *out, uint32_t *out_size)
{
    size_t index = 0;
    uint32_t written = 0;

    while (index < size) {
        uint8_t byte = source[index];

        (void)sb_append_n(out, (const char *)&byte, 1);
        written++;
        index++;

        if (byte == 0xffu && index < size && source[index] == 0x00u) {
            index++;
        }
    }
    *out_size = written;
}

SmpTagFormat smp_tags_parse_id3v2(const uint8_t *data, size_t size, SmpTags *tags)
{
    StrBuf cache;
    const uint8_t *body;
    size_t body_size;
    uint8_t major;
    uint8_t flags;
    size_t offset = 0;
    uint32_t deunsynced_size = 0;
    SmpTagFormat format = SMP_TAG_FORMAT_UNKNOWN;

    if (data == NULL || tags == NULL || size < 10u) {
        return SMP_TAG_FORMAT_UNKNOWN;
    }
    if (data[0] != 'I' || data[1] != 'D' || data[2] != '3') {
        return SMP_TAG_FORMAT_UNKNOWN;
    }

    major = data[3];
    flags = data[5];
    body_size = smp_id3v2_size(data, size);
    if (body_size < 10u) {
        return SMP_TAG_FORMAT_UNKNOWN;
    }
    if (body_size > size) {
        body_size = size;
    }
    body = data + 10;
    body_size -= 10u;

    sb_init(&cache);

    if ((flags & 0x80u) != 0) {
        deunsync(body, body_size, &cache, &deunsynced_size);
        body = (const uint8_t *)sb_cstr(&cache);
        body_size = deunsynced_size;
    }

    /* Skip the extended header when present. */
    if ((flags & 0x40u) != 0 && major >= 3) {
        if (body_size >= 4u) {
            uint32_t extended = major == 4 ? read_syncsafe32(body) : read_be32(body);

            if (extended >= 4u && (size_t)extended <= body_size) {
                body += extended;
                body_size -= extended;
            } else {
                body += 4u;
                body_size -= 4u;
            }
        }
    }

    while (offset + (major == 2 ? 6u : 10u) <= body_size) {
        const uint8_t *frame = body + offset;
        char id[5];
        uint32_t frame_size;
        size_t header_size;
        uint8_t encoding;
        const uint8_t *content;
        size_t content_size;
        StrBuf text;

        if (major == 2) {
            id[0] = (char)frame[0];
            id[1] = (char)frame[1];
            id[2] = (char)frame[2];
            id[3] = '\0';
            frame_size = read_be24(frame + 3);
            header_size = 6u;
        } else {
            id[0] = (char)frame[0];
            id[1] = (char)frame[1];
            id[2] = (char)frame[2];
            id[3] = (char)frame[3];
            id[4] = '\0';
            frame_size = major == 4 ? read_syncsafe32(frame + 4) : read_be32(frame + 4);
            header_size = 10u;
        }

        if (id[0] == '\0') {
            break;
        }
        if (frame_size == 0 || offset + header_size + frame_size > body_size) {
            break;
        }

        content = frame + header_size;
        content_size = frame_size;

        if (content_size < 1u) {
            offset += header_size + frame_size;
            continue;
        }

        encoding = content[0];
        content++;
        content_size--;

        sb_init(&text);
        decode_id3_text(encoding, content, content_size, &text);

        if (strcmp(id, "TIT2") == 0 || strcmp(id, "TT2") == 0) {
            assign_field(tags->title, sizeof(tags->title), &text);
            format = SMP_TAG_FORMAT_ID3V2;
        } else if (strcmp(id, "TPE1") == 0 || strcmp(id, "TP1") == 0) {
            assign_field(tags->artist, sizeof(tags->artist), &text);
            format = SMP_TAG_FORMAT_ID3V2;
        } else if (strcmp(id, "TALB") == 0 || strcmp(id, "TAL") == 0) {
            assign_field(tags->album, sizeof(tags->album), &text);
            format = SMP_TAG_FORMAT_ID3V2;
        }

        sb_free(&text);
        offset += header_size + frame_size;
    }

    sb_free(&cache);
    return format;
}

bool smp_tags_parse_id3v1(const uint8_t *data, size_t size, SmpTags *tags)
{
    const uint8_t *record;
    StrBuf text;
    bool found = false;

    if (data == NULL || tags == NULL || size < 128u) {
        return false;
    }

    record = data + (size - 128u);
    if (record[0] != 'T' || record[1] != 'A' || record[2] != 'G') {
        return false;
    }

    sb_init(&text);

    if (record[3] != '\0') {
        const uint8_t *field = record + 3;
        size_t length = bounded_length((const char *)field, 30);

        if (is_valid_utf8(field, length)) {
            (void)sb_append_n(&text, (const char *)field, length);
        } else {
            append_cp1251(&text, field, length);
        }
        assign_field(tags->title, sizeof(tags->title), &text);
        found = true;
    }

    sb_clear(&text);
    if (record[33] != '\0') {
        const uint8_t *field = record + 33;
        size_t length = bounded_length((const char *)field, 30);

        if (is_valid_utf8(field, length)) {
            (void)sb_append_n(&text, (const char *)field, length);
        } else {
            append_cp1251(&text, field, length);
        }
        assign_field(tags->artist, sizeof(tags->artist), &text);
        found = true;
    }

    sb_clear(&text);
    if (record[63] != '\0') {
        const uint8_t *field = record + 63;
        size_t length = bounded_length((const char *)field, 30);

        if (is_valid_utf8(field, length)) {
            (void)sb_append_n(&text, (const char *)field, length);
        } else {
            append_cp1251(&text, field, length);
        }
        assign_field(tags->album, sizeof(tags->album), &text);
        found = true;
    }

    sb_free(&text);
    return found;
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16)
           | ((uint32_t)data[3] << 24);
}

/* Parses the sub-chunks of a LIST payload that starts with "INFO". */
static bool parse_info_block(const uint8_t *data, size_t size, SmpTags *tags)
{
    size_t offset = 4u;
    bool found = false;

    if (data == NULL || tags == NULL || size < 8u || memcmp(data, "INFO", 4) != 0) {
        return false;
    }

    while (offset + 8u <= size) {
        const uint8_t *item = data + offset;
        uint32_t item_size = read_le32(item + 4);
        const uint8_t *value = item + 8u;
        size_t value_size;
        StrBuf text;

        if ((size_t)item_size > size - offset - 8u) {
            item_size = (uint32_t)(size - offset - 8u);
        }
        value_size = bounded_length((const char *)value, item_size);

        sb_init(&text);
        if (is_valid_utf8(value, value_size)) {
            (void)sb_append_n(&text, (const char *)value, value_size);
        } else {
            append_cp1251(&text, value, value_size);
        }

        if (memcmp(item, "INAM", 4) == 0) {
            assign_field(tags->title, sizeof(tags->title), &text);
            found = true;
        } else if (memcmp(item, "IART", 4) == 0) {
            assign_field(tags->artist, sizeof(tags->artist), &text);
            found = true;
        } else if (memcmp(item, "IPRD", 4) == 0 || memcmp(item, "IALB", 4) == 0) {
            assign_field(tags->album, sizeof(tags->album), &text);
            found = true;
        }

        sb_free(&text);
        offset += 8u + ((item_size + 1u) & ~1u);
    }

    return found;
}

SmpTagFormat smp_tags_parse_riff(const uint8_t *data, size_t size, SmpTags *tags)
{
    size_t offset;
    SmpTagFormat format = SMP_TAG_FORMAT_UNKNOWN;

    if (data == NULL || tags == NULL || size < 12u) {
        return SMP_TAG_FORMAT_UNKNOWN;
    }
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return SMP_TAG_FORMAT_UNKNOWN;
    }

    offset = 12u;
    while (offset + 8u <= size) {
        const uint8_t *chunk = data + offset;
        uint32_t chunk_size = read_le32(chunk + 4);
        size_t payload = offset + 8u;

        if ((size_t)chunk_size > size - payload) {
            chunk_size = (uint32_t)(size - payload);
        }

        if (memcmp(chunk, "LIST", 4) == 0 && chunk_size >= 4u
            && memcmp(data + payload, "INFO", 4) == 0)
        {
            if (parse_info_block(data + payload, chunk_size, tags)) {
                format = SMP_TAG_FORMAT_RIFF_INFO;
            }
        }

        offset = payload + ((chunk_size + 1u) & ~1u);
    }

    return format;
}

/* Moves the cursor forward by `count` bytes; 64-bit safe on both platforms. */
static bool skip_bytes(FILE *file, uint64_t count)
{
#ifdef _WIN32
    return _fseeki64(file, (__int64)count, SEEK_CUR) == 0;
#else
    return fseeko(file, (off_t)count, SEEK_CUR) == 0;
#endif
}

/*
 * Walks the RIFF chunks of an open file. The LIST/INFO block is often placed
 * after the (potentially huge) data chunk, so the file is streamed instead of
 * being read into memory.
 */
static bool stream_riff_tags(FILE *file, SmpTags *tags)
{
    uint8_t header[12];
    bool found = false;

    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        return false;
    }
    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    for (;;) {
        uint8_t chunk[8];
        uint64_t chunk_size;
        uint64_t padded;

        if (fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk)) {
            break;
        }

        chunk_size = read_le32(chunk + 4);
        padded = chunk_size + (chunk_size & 1u);

        if (memcmp(chunk, "LIST", 4) == 0 && chunk_size >= 4u && chunk_size <= 4u * 1024u * 1024u) {
            uint8_t *payload = (uint8_t *)malloc((size_t)chunk_size);

            if (payload == NULL) {
                return found;
            }
            if (fread(payload, 1, (size_t)chunk_size, file) != (size_t)chunk_size) {
                free(payload);
                break;
            }
            if (memcmp(payload, "INFO", 4) == 0) {
                found = parse_info_block(payload, (size_t)chunk_size, tags) || found;
            }
            free(payload);
            if ((chunk_size & 1u) != 0u && !skip_bytes(file, 1u)) {
                break;
            }
        } else if (!skip_bytes(file, padded)) {
            break;
        }
    }

    return found;
}

/* ------------------------------------------------------------------------- */
/* Vorbis comments (FLAC and Ogg Vorbis)                                     */
/* ------------------------------------------------------------------------- */

static bool key_equals(const uint8_t *key, size_t length, const char *name)
{
    size_t name_length = strlen(name);
    size_t index;

    if (length != name_length) {
        return false;
    }

    for (index = 0; index < length; index++) {
        char left = (char)key[index];
        char right = name[index];

        if (left >= 'a' && left <= 'z') {
            left = (char)(left - 'a' + 'A');
        }
        if (right >= 'a' && right <= 'z') {
            right = (char)(right - 'a' + 'A');
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}

static bool parse_vorbis_comments(const uint8_t *data, size_t size, SmpTags *tags)
{
    uint32_t vendor_length;
    uint32_t count;
    uint32_t index;
    size_t offset;
    bool found = false;

    if (data == NULL || tags == NULL || size < 8u) {
        return false;
    }

    vendor_length = read_le32(data);
    offset = 4u + (size_t)vendor_length;
    if (offset + 4u > size) {
        return false;
    }

    count = read_le32(data + offset);
    offset += 4u;

    for (index = 0; index < count; index++) {
        uint32_t length;
        const uint8_t *entry;
        size_t key_length = 0;
        StrBuf text;

        if (offset + 4u > size) {
            break;
        }
        length = read_le32(data + offset);
        offset += 4u;
        if ((size_t)length > size - offset) {
            break;
        }

        entry = data + offset;
        while (key_length < (size_t)length && entry[key_length] != '=') {
            key_length++;
        }

        if (key_length < (size_t)length) {
            const char *value = (const char *)entry + key_length + 1u;
            size_t value_length = (size_t)length - key_length - 1u;

            sb_init(&text);
            if (is_valid_utf8((const uint8_t *)value, value_length)) {
                (void)sb_append_n(&text, value, value_length);
            } else {
                append_cp1251(&text, (const uint8_t *)value, value_length);
            }

            if (key_equals(entry, key_length, "TITLE")) {
                assign_field(tags->title, sizeof(tags->title), &text);
                found = true;
            } else if (key_equals(entry, key_length, "ARTIST")) {
                assign_field(tags->artist, sizeof(tags->artist), &text);
                found = true;
            } else if (key_equals(entry, key_length, "ALBUM")) {
                assign_field(tags->album, sizeof(tags->album), &text);
                found = true;
            }
            sb_free(&text);
        }

        offset += (size_t)length;
    }

    return found;
}

/* Reads the FLAC metadata blocks and parses the VORBIS_COMMENT block. */
static bool parse_flac_metadata(FILE *file, SmpTags *tags)
{
    uint8_t marker[4];

    if (fread(marker, 1, sizeof(marker), file) != sizeof(marker)) {
        return false;
    }
    if (memcmp(marker, "fLaC", 4) != 0) {
        return false;
    }

    for (;;) {
        uint8_t header[4];
        uint8_t type;
        bool last;
        uint32_t length;

        if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
            break;
        }

        last = (header[0] & 0x80u) != 0;
        type = (uint8_t)(header[0] & 0x7fu);
        length = ((uint32_t)header[1] << 16) | ((uint32_t)header[2] << 8) | (uint32_t)header[3];

        if (type == 4u) {
            uint8_t *payload;
            bool found;

            if (length > 8u * 1024u * 1024u) {
                return false;
            }
            payload = (uint8_t *)malloc(length);
            if (payload == NULL) {
                return false;
            }
            if (fread(payload, 1, length, file) != length) {
                free(payload);
                return false;
            }
            found = parse_vorbis_comments(payload, length, tags);
            free(payload);
            return found;
        }

        if (!skip_bytes(file, length)) {
            break;
        }
        if (last) {
            break;
        }
    }

    return false;
}

/* Assembles the Ogg packets and parses the "\x03vorbis" comment packet. */
static bool parse_ogg_comments(FILE *file, SmpTags *tags)
{
    const size_t limit = 256u * 1024u;
    uint8_t *data;
    uint8_t *packet;
    size_t size;
    size_t offset = 0;
    size_t packet_length = 0;
    bool found = false;

    data = (uint8_t *)malloc(limit);
    packet = (uint8_t *)malloc(limit);
    if (data == NULL || packet == NULL) {
        free(data);
        free(packet);
        return false;
    }

    size = fread(data, 1, limit, file);
    if (size < 27u) {
        free(packet);
        free(data);
        return false;
    }

    while (offset + 27u <= size) {
        uint8_t header_type;
        uint8_t segments;
        size_t table;
        size_t payload;
        size_t payload_length = 0;
        size_t index;

        if (memcmp(data + offset, "OggS", 4) != 0) {
            break;
        }

        header_type = data[offset + 5];
        segments = data[offset + 26];
        table = offset + 27u;
        if (table + (size_t)segments > size) {
            break;
        }

        for (index = 0; index < (size_t)segments; index++) {
            payload_length += data[table + index];
        }

        payload = table + (size_t)segments;
        if (payload + payload_length > size) {
            break;
        }

        if ((header_type & 0x01u) == 0) {
            packet_length = 0;
        }
        if (packet_length + payload_length <= limit) {
            memcpy(packet + packet_length, data + payload, payload_length);
            packet_length += payload_length;
        }

        /* A lacing value below 255 terminates the packet. */
        if (segments > 0 && data[table + (size_t)segments - 1u] < 255u) {
            if (packet_length >= 7u && memcmp(packet, "\x03vorbis", 7) == 0) {
                found = parse_vorbis_comments(packet + 7, packet_length - 7u, tags);
                break;
            }
            packet_length = 0;
        }

        offset = payload + payload_length;
    }

    free(packet);
    free(data);
    return found;
}

static bool has_text(const char *text){
    return text != NULL && text[0] != '\0';
}

SmpStatus smp_tags_read_file(const char *path, SmpTags *tags)
{
    void *head = NULL;
    size_t head_size = 0;
    size_t id3_size;
    SmpTagFormat format = SMP_TAG_FORMAT_UNKNOWN;

    if (path == NULL || tags == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    smp_tags_init(tags);

    if (smp_path_has_extension(path, ".wav")) {
        FILE *file = smp_file_open(path);

        if (file == NULL) {
            return SMP_ERR_IO;
        }
        if (stream_riff_tags(file, tags)) {
            format = SMP_TAG_FORMAT_RIFF_INFO;
        }
        fclose(file);
    } else if (smp_path_has_extension(path, ".flac")) {
        FILE *file = smp_file_open(path);

        if (file == NULL) {
            return SMP_ERR_IO;
        }
        if (parse_flac_metadata(file, tags)) {
            format = SMP_TAG_FORMAT_VORBIS_COMMENT;
        }
        fclose(file);
    } else if (smp_path_has_extension(path, ".ogg") || smp_path_has_extension(path, ".oga")) {
        FILE *file = smp_file_open(path);

        if (file == NULL) {
            return SMP_ERR_IO;
        }
        if (parse_ogg_comments(file, tags)) {
            format = SMP_TAG_FORMAT_VORBIS_COMMENT;
        }
        fclose(file);
    } else {
        if (smp_file_read(path, TAG_READ_LIMIT, &head, &head_size) != SMP_OK) {
            return SMP_ERR_IO;
        }

        id3_size = smp_id3v2_size((const uint8_t *)head, head_size);
        if (id3_size > 0) {
            format = smp_tags_parse_id3v2((const uint8_t *)head, id3_size, tags);
        }
        free(head);
    }

    if (format == SMP_TAG_FORMAT_UNKNOWN) {
        void *tail = NULL;
        size_t tail_size = 0;

        if (smp_file_read_tail(path, 128, &tail, &tail_size) == SMP_OK) {
            if (smp_tags_parse_id3v1((const uint8_t *)tail, tail_size, tags)) {
                format = SMP_TAG_FORMAT_ID3V1;
            }
            free(tail);
        }
    }

    if (!has_text(tags->title)) {
        const char *base = smp_path_basename(path);
        const char *dot = smp_path_extension(path);
        size_t length = (dot != NULL) ? (size_t)(dot - base) : strlen(base);
        size_t capacity = sizeof(tags->title) - 1u;

        if (length > capacity) {
            length = capacity;
            /* Never cut a UTF-8 sequence in half. */
            while (length > 0 && (((unsigned char)base[length]) & 0xc0u) == 0x80u) {
                length--;
            }
        }
        memcpy(tags->title, base, length);
        tags->title[length] = '\0';
    }

    if (!has_text(tags->artist)) {
        snprintf(tags->artist, sizeof(tags->artist), "%s", "Unknown artist");
    }
    if (!has_text(tags->album)) {
        snprintf(tags->album, sizeof(tags->album), "%s", "Unknown album");
    }

    return SMP_OK;
}
