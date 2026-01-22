#ifndef UI_H
#define UI_H

#include "file_utils.h"
#include "player.h"

void draw_interface();
void display_left_panel();
void draw_timer_only();
void draw_status_only();
void draw_track_list_only();
void format_time(int seconds, char *buffer, size_t size);
int read_key();
void handle_input();

#endif
