/*
 * SMP - Swag Music Player
 * Terminal handling for Windows (VT sequences + console input).
 *
 * SPDX-License-Identifier: MIT
 */
#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "smp/common.h"
#include "smp/term.h"

static HANDLE input_handle = INVALID_HANDLE_VALUE;
static HANDLE output_handle = INVALID_HANDLE_VALUE;
static DWORD saved_input_mode = 0;
static DWORD saved_output_mode = 0;
static bool console_active = false;
static bool resize_pending = false;
static bool output_is_console = false;

/* The handle may be needed before smp_term_init(), for example in debug modes. */
static void ensure_output_handle(void)
{
    DWORD mode = 0;

    if (output_handle == INVALID_HANDLE_VALUE || output_handle == NULL) {
        output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (output_handle == INVALID_HANDLE_VALUE || output_handle == NULL) {
        return;
    }
    output_is_console = GetConsoleMode(output_handle, &mode) != 0;
}

bool smp_term_init(void)
{
    DWORD input_mode = 0;
    DWORD output_mode = 0;

    input_handle = GetStdHandle(STD_INPUT_HANDLE);
    output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (input_handle == INVALID_HANDLE_VALUE || output_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!GetConsoleMode(input_handle, &input_mode) || !GetConsoleMode(output_handle, &output_mode)) {
        return false;
    }

    saved_input_mode = input_mode;
    saved_output_mode = output_mode;
    output_is_console = true;

    input_mode &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT
                           | ENABLE_QUICK_EDIT_MODE);
    input_mode |= ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;

    if (!SetConsoleMode(input_handle, input_mode)) {
        return false;
    }

    output_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    (void)SetConsoleMode(output_handle, output_mode);
    (void)SetConsoleOutputCP(CP_UTF8);

    console_active = true;
    return true;
}

void smp_term_shutdown(void)
{
    static const char *const cleanup = "\x1b[0m\x1b[?25h\n";

    smp_term_write(cleanup, strlen(cleanup));

    if (console_active) {
        (void)SetConsoleMode(input_handle, saved_input_mode);
        (void)SetConsoleMode(output_handle, saved_output_mode);
        console_active = false;
    }
}

bool smp_term_is_tty(void)
{
    return console_active;
}

void smp_term_get_size(int *width, int *height)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (width != NULL) {
        *width = 80;
    }
    if (height != NULL) {
        *height = 24;
    }

    ensure_output_handle();

    if (output_handle == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(output_handle, &info)) {
        return;
    }

    if (width != NULL) {
        *width = (int)(info.srWindow.Right - info.srWindow.Left + 1);
    }
    if (height != NULL) {
        *height = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
    }
}

void smp_term_write(const char *data, size_t size)
{
    int wide_length;
    wchar_t *wide;

    if (data == NULL || size == 0) {
        return;
    }

    ensure_output_handle();

    if (!output_is_console) {
        DWORD written = 0;

        (void)WriteFile(output_handle, data, (DWORD)size, &written, NULL);
        return;
    }

    wide_length = MultiByteToWideChar(CP_UTF8, 0, data, (int)size, NULL, 0);
    if (wide_length <= 0) {
        return;
    }

    wide = (wchar_t *)malloc((size_t)wide_length * sizeof(wchar_t));
    if (wide == NULL) {
        return;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, data, (int)size, wide, wide_length) == wide_length) {
        DWORD written = 0;

        (void)WriteConsoleW(output_handle, wide, (DWORD)wide_length, &written, NULL);
    }

    free(wide);
}

void smp_term_flush(void)
{
    /* WriteConsoleW writes immediately. */
}

bool smp_term_take_resize(void)
{
    bool pending = resize_pending;

    resize_pending = false;
    return pending;
}

static bool key_from_record(const INPUT_RECORD *record, SmpKey *key)
{
    const KEY_EVENT_RECORD *event;

    if (record->EventType == WINDOW_BUFFER_SIZE_EVENT) {
        resize_pending = true;
        return false;
    }
    if (record->EventType != KEY_EVENT) {
        return false;
    }

    event = &record->Event.KeyEvent;
    if (!event->bKeyDown) {
        return false;
    }

    switch (event->wVirtualKeyCode) {
    case VK_UP:
        key->code = SMP_KEY_UP;
        return true;
    case VK_DOWN:
        key->code = SMP_KEY_DOWN;
        return true;
    case VK_LEFT:
        key->code = SMP_KEY_LEFT;
        return true;
    case VK_RIGHT:
        key->code = SMP_KEY_RIGHT;
        return true;
    case VK_RETURN:
        key->code = SMP_KEY_ENTER;
        return true;
    case VK_ESCAPE:
        key->code = SMP_KEY_ESCAPE;
        return true;
    case VK_TAB:
        key->code = SMP_KEY_TAB;
        return true;
    case VK_BACK:
        key->code = SMP_KEY_BACKSPACE;
        return true;
    case VK_PRIOR:
        key->code = SMP_KEY_PAGE_UP;
        return true;
    case VK_NEXT:
        key->code = SMP_KEY_PAGE_DOWN;
        return true;
    case VK_HOME:
        key->code = SMP_KEY_HOME;
        return true;
    case VK_END:
        key->code = SMP_KEY_END;
        return true;
    case VK_SPACE:
        key->code = SMP_KEY_SPACE;
        key->character = ' ';
        return true;
    default:
        break;
    }

    if (event->uChar.UnicodeChar == 0x03) {
        key->code = SMP_KEY_CTRL_C;
        return true;
    }

    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && event->uChar.UnicodeChar != 0) {
        if (event->uChar.UnicodeChar == 0x03) {
            key->code = SMP_KEY_CTRL_C;
            return true;
        }
    }

    if (event->uChar.UnicodeChar != 0) {
        key->code = SMP_KEY_CHAR;
        key->character = (uint32_t)event->uChar.UnicodeChar;
        return true;
    }

    return false;
}

bool smp_term_poll(SmpKey *key, int timeout_ms)
{
    DWORD wait_result;
    DWORD available = 0;

    if (key == NULL || !console_active) {
        return false;
    }

    key->code = SMP_KEY_NONE;
    key->character = 0;

    wait_result = WaitForSingleObject(input_handle, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);
    if (wait_result != WAIT_OBJECT_0) {
        return false;
    }

    while (GetNumberOfConsoleInputEvents(input_handle, &available) && available > 0) {
        INPUT_RECORD record;
        DWORD read = 0;

        if (!ReadConsoleInputW(input_handle, &record, 1, &read) || read == 0) {
            break;
        }

        if (key_from_record(&record, key)) {
            return true;
        }
    }

    return false;
}

#endif /* _WIN32 */
