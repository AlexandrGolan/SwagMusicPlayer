#ifndef UI_H
#define UI_H

#include "file_utils.h"
#include "player.h"

void draw_interface();
void display_left_panel();
void draw_timer_only();
void draw_status_only();
void draw_track_list_only();
void draw_progress_bar();
void draw_volume_display();
void format_time(int seconds, char* buffer, size_t size);
int read_key();
void handle_input();
void draw_header();
void draw_footer();
void draw_playlist();
void draw_track_info();
void draw_center_line();
void update_playlist_offset();

#endif
