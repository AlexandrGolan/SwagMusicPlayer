/*
 * SMP - Swag Music Player
 * Terminal handling for POSIX platforms.
 *
 * SPDX-License-Identifier: MIT
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "smp/common.h"
#include "smp/str.h"
#include "smp/term.h"

#define INPUT_BUFFER_SIZE 128

static struct termios saved_termios;
static bool raw_mode_active = false;
static bool tty_available = false;
static int resize_pipe[2] = {-1, -1};
static volatile sig_atomic_t resize_pending = 0;
static char input_buffer[INPUT_BUFFER_SIZE];
static size_t input_length = 0;

static void handle_sigwinch(int signal_number)
{
    unsigned char byte = 1;

    SMP_UNUSED(signal_number);
    resize_pending = 1;
    if (resize_pipe[1] >= 0) {
        ssize_t ignored = write(resize_pipe[1], &byte, 1);
        SMP_UNUSED(ignored);
    }
}

bool smp_term_init(void)
{
    struct termios raw;
    struct sigaction action;
    int pipe_result;

    tty_available = isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
    if (!tty_available) {
        return false;
    }

    if (tcgetattr(STDIN_FILENO, &saved_termios) != 0) {
        tty_available = false;
        return false;
    }

    raw = saved_termios;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(tcflag_t)(OPOST);
    raw.c_cflag |= (tcflag_t)(CS8);
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        tty_available = false;
        return false;
    }
    raw_mode_active = true;

    pipe_result = pipe(resize_pipe);
    if (pipe_result == 0) {
        (void)fcntl(resize_pipe[0], F_SETFL, O_NONBLOCK);
        (void)fcntl(resize_pipe[1], F_SETFL, O_NONBLOCK);
    } else {
        resize_pipe[0] = -1;
        resize_pipe[1] = -1;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigwinch;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    (void)sigaction(SIGWINCH, &action, NULL);

    input_length = 0;
    return true;
}

void smp_term_shutdown(void)
{
    static const char *const cleanups = "\x1b[0m\x1b[?25h\x1b[?1049l\n";

    if (raw_mode_active) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        raw_mode_active = false;
    }

    if (resize_pipe[0] >= 0) {
        close(resize_pipe[0]);
        close(resize_pipe[1]);
        resize_pipe[0] = -1;
        resize_pipe[1] = -1;
    }

    if (tty_available) {
        ssize_t ignored = write(STDOUT_FILENO, cleanups, strlen(cleanups));

        (void)ignored;
    }
}

bool smp_term_is_tty(void)
{
    return tty_available;
}

void smp_term_get_size(int *width, int *height)
{
    struct winsize window;

    if (width != NULL) {
        *width = 80;
    }
    if (height != NULL) {
        *height = 24;
    }

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0 && window.ws_row > 0) {
        if (width != NULL) {
            *width = (int)window.ws_col;
        }
        if (height != NULL) {
            *height = (int)window.ws_row;
        }
        return;
    }

    {
        const char *columns = getenv("COLUMNS");
        const char *lines = getenv("LINES");
        int parsed;

        if (columns != NULL && sscanf(columns, "%d", &parsed) == 1 && parsed > 0 && width != NULL) {
            *width = parsed;
        }
        if (lines != NULL && sscanf(lines, "%d", &parsed) == 1 && parsed > 0 && height != NULL) {
            *height = parsed;
        }
    }
}

void smp_term_write(const char *data, size_t size)
{
    size_t written = 0;

    if (data == NULL || size == 0) {
        return;
    }

    while (written < size) {
        ssize_t result = write(STDOUT_FILENO, data + written, size - written);

        if (result > 0) {
            written += (size_t)result;
            continue;
        }
        if (result < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }
        break;
    }
}

void smp_term_flush(void)
{
    /* Writes go straight to the file descriptor, nothing to flush. */
}

bool smp_term_take_resize(void)
{
    bool pending = resize_pending != 0;

    resize_pending = 0;
    return pending;
}

static bool refill_input(void)
{
    ssize_t result;

    if (input_length >= sizeof(input_buffer)) {
        input_length = 0;
    }

    result = read(STDIN_FILENO, input_buffer + input_length, sizeof(input_buffer) - input_length);
    if (result <= 0) {
        return false;
    }

    input_length += (size_t)result;
    return true;
}

static void consume(size_t count)
{
    if (count >= input_length) {
        input_length = 0;
        return;
    }

    memmove(input_buffer, input_buffer + count, input_length - count);
    input_length -= count;
}

static bool decode_key(SmpKey *key)
{
    unsigned char first;

    if (input_length == 0) {
        return false;
    }

    first = (unsigned char)input_buffer[0];

    if (first == 0x1b) {
        if (input_length == 1) {
            /* A lone escape key press. */
            key->code = SMP_KEY_ESCAPE;
            consume(1);
            return true;
        }

        if (input_buffer[1] == '[') {
            if (input_length < 3) {
                return false;
            }

            switch (input_buffer[2]) {
            case 'A':
                key->code = SMP_KEY_UP;
                consume(3);
                return true;
            case 'B':
                key->code = SMP_KEY_DOWN;
                consume(3);
                return true;
            case 'C':
                key->code = SMP_KEY_RIGHT;
                consume(3);
                return true;
            case 'D':
                key->code = SMP_KEY_LEFT;
                consume(3);
                return true;
            case 'H':
                key->code = SMP_KEY_HOME;
                consume(3);
                return true;
            case 'F':
                key->code = SMP_KEY_END;
                consume(3);
                return true;
            default:
                break;
            }

            /* Page up/down and home/end arrive as "ESC [ n ~". */
            if (input_length >= 4 && input_buffer[3] == '~') {
                switch (input_buffer[2]) {
                case '5':
                    key->code = SMP_KEY_PAGE_UP;
                    consume(4);
                    return true;
                case '6':
                    key->code = SMP_KEY_PAGE_DOWN;
                    consume(4);
                    return true;
                case '1':
                case '7':
                    key->code = SMP_KEY_HOME;
                    consume(4);
                    return true;
                case '4':
                case '8':
                    key->code = SMP_KEY_END;
                    consume(4);
                    return true;
                default:
                    break;
                }
            }

            consume(1);
            return false;
        }

        if (input_buffer[1] == 'O') {
            if (input_length < 3) {
                return false;
            }

            switch (input_buffer[2]) {
            case 'A':
                key->code = SMP_KEY_UP;
                break;
            case 'B':
                key->code = SMP_KEY_DOWN;
                break;
            case 'C':
                key->code = SMP_KEY_RIGHT;
                break;
            case 'D':
                key->code = SMP_KEY_LEFT;
                break;
            case 'H':
                key->code = SMP_KEY_HOME;
                break;
            case 'F':
                key->code = SMP_KEY_END;
                break;
            default:
                consume(3);
                return false;
            }
            consume(3);
            return true;
        }

        key->code = SMP_KEY_ESCAPE;
        consume(1);
        return true;
    }

    if (first == '\r' || first == '\n') {
        key->code = SMP_KEY_ENTER;
        consume(1);
        return true;
    }
    if (first == '\t') {
        key->code = SMP_KEY_TAB;
        consume(1);
        return true;
    }
    if (first == 0x7f || first == 0x08) {
        key->code = SMP_KEY_BACKSPACE;
        consume(1);
        return true;
    }
    if (first == ' ') {
        key->code = SMP_KEY_SPACE;
        key->character = ' ';
        consume(1);
        return true;
    }
    if (first == 0x03) {
        key->code = SMP_KEY_CTRL_C;
        consume(1);
        return true;
    }
    if (first < 0x20) {
        consume(1);
        return false;
    }

    {
        uint32_t codepoint = 0;
        size_t consumed = smp_utf8_decode(input_buffer, &codepoint);

        if (consumed == 0) {
            consume(1);
            return false;
        }
        if (consumed > input_length) {
            return false; /* wait for the rest of the sequence */
        }

        key->code = SMP_KEY_CHAR;
        key->character = codepoint;
        consume(consumed);
        return true;
    }
}

bool smp_term_poll(SmpKey *key, int timeout_ms)
{
    struct pollfd descriptors[2];
    int count = 1;
    int result;

    if (key == NULL) {
        return false;
    }
    key->code = SMP_KEY_NONE;
    key->character = 0;

    if (input_length > 0 && decode_key(key)) {
        return key->code != SMP_KEY_NONE;
    }

    descriptors[0].fd = STDIN_FILENO;
    descriptors[0].events = POLLIN;
    descriptors[0].revents = 0;

    if (resize_pipe[0] >= 0) {
        descriptors[1].fd = resize_pipe[0];
        descriptors[1].events = POLLIN;
        descriptors[1].revents = 0;
        count = 2;
    }

    result = poll(descriptors, (nfds_t)count, timeout_ms);
    if (result <= 0) {
        if (result == 0 && input_length == 1 && input_buffer[0] == 0x1b) {
            key->code = SMP_KEY_ESCAPE;
            consume(1);
            return true;
        }
        return false;
    }

    if (count == 2 && (descriptors[1].revents & POLLIN) != 0) {
        char buffer[32];

        while (read(resize_pipe[0], buffer, sizeof(buffer)) > 0) {
            /* drain */
        }
        resize_pending = 1;
    }

    if ((descriptors[0].revents & POLLIN) != 0) {
        if (refill_input() && decode_key(key)) {
            return key->code != SMP_KEY_NONE;
        }
    }

    return false;
}

#endif /* !_WIN32 */

/*
 * POSIX has no built-in folder chooser; the caller falls back to the usage
 * message and the user passes the directory as an argument.
 */
bool smp_term_pick_directory(char *out, size_t size)
{
    if (out != NULL && size > 0u) {
        out[0] = '\0';
    }
    return false;
}