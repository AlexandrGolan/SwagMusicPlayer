#include "config.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

int TERM_WIDTH = 168;
int TERM_HEIGHT = 39;
int LEFT_WIDTH = 84;
int RIGHT_WIDTH = 84;
int IMAGE_WIDTH = 72;
int IMAGE_HEIGHT = 21;
int IMAGE_X = 6;
int IMAGE_Y = 8;
int current_volume = DEFAULT_VOLUME;
int playlist_offset = 0;

void update_terminal_size() {
    struct winsize w;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        if (w.ws_col >= MIN_WIDTH && w.ws_row >= MIN_HEIGHT) {
            TERM_WIDTH = w.ws_col;
            TERM_HEIGHT = w.ws_row;

            LEFT_WIDTH = (int)(TERM_WIDTH * DEFAULT_LEFT_RATIO);
            if (LEFT_WIDTH < 40) {
                LEFT_WIDTH = 40;
            }
            if (LEFT_WIDTH > TERM_WIDTH - 50) {
                LEFT_WIDTH = TERM_WIDTH - 50;
            }

            RIGHT_WIDTH = TERM_WIDTH - LEFT_WIDTH - 1;

            IMAGE_WIDTH = LEFT_WIDTH - 8;
            if (IMAGE_WIDTH > 70) {
                IMAGE_WIDTH = 70;
            }
            if (IMAGE_WIDTH < 30) {
                IMAGE_WIDTH = 30;
            }

            IMAGE_HEIGHT = IMAGE_WIDTH / 2;
            if (IMAGE_HEIGHT > TERM_HEIGHT - 4) {
                IMAGE_HEIGHT = TERM_HEIGHT - 4;
            }
            if (IMAGE_HEIGHT < 12) {
                IMAGE_HEIGHT = 12;
            }
        }
    }
}
