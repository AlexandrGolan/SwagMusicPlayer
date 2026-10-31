/*
 * SMP - Swag Music Player
 * Terminal abstraction: raw mode, size, input and output.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_TERM_H
#define SMP_TERM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SMP_KEY_NONE = 0,
    SMP_KEY_CHAR,
    SMP_KEY_UP,
    SMP_KEY_DOWN,
    SMP_KEY_LEFT,
    SMP_KEY_RIGHT,
    SMP_KEY_ENTER,
    SMP_KEY_ESCAPE,
    SMP_KEY_SPACE,
    SMP_KEY_TAB,
    SMP_KEY_BACKSPACE,
    SMP_KEY_PAGE_UP,
    SMP_KEY_PAGE_DOWN,
    SMP_KEY_HOME,
    SMP_KEY_END,
    SMP_KEY_CTRL_C,
} SmpKeyCode;

typedef struct {
    SmpKeyCode code;
    uint32_t character; /* valid for SMP_KEY_CHAR */
} SmpKey;

/* Puts the terminal into raw mode. Returns false when stdin is not a TTY. */
bool smp_term_init(void);

/* Restores the original terminal state. */
void smp_term_shutdown(void);

void smp_term_get_size(int *width, int *height);

bool smp_term_is_tty(void);

/*
 * Waits up to `timeout_ms` for input. Returns false on timeout.
 * Use SMP_WAIT_FOREVER (-1) to block.
 */
bool smp_term_poll(SmpKey *key, int timeout_ms);

void smp_term_write(const char *data, size_t size);
void smp_term_flush(void);

/* Returns true once after the terminal was resized. */
bool smp_term_take_resize(void);

#endif /* SMP_TERM_H */
