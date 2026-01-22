#ifndef FILE_UTILS_H
#define FILE_UTILS_H

int check_tiv_available();
char* find_cover_image(const char* directory);
char* render_image_to_ansi(const char* image_path);
int count_visible_chars(const char* str);

#endif
