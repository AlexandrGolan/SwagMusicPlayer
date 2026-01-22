#include "../include/file_utils.h"
#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Check TIV program is available
 *
 * @return int
 **/
int check_tiv_available() { return system("which tiv > /dev/null 2>&1") == 0; }

/**
 * @brief Find cover image in directory
 *
 * @param directory directory with cover image
 * @return char*
 **/
char *find_cover_image(const char *directory) {
  static char cover_path[512];
  const char *extensions[] = {".png", ".jpg",  ".jpeg", ".gif",
                              ".bmp", ".webp", NULL};

  for (int i = 0; extensions[i] != NULL; i++) {
    snprintf(cover_path, sizeof(cover_path), "%s/cover%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/Cover%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/COVER%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/album%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/Album%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/folder%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/Folder%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;

    snprintf(cover_path, sizeof(cover_path), "%s/front%s", directory,
             extensions[i]);
    if (access(cover_path, F_OK) == 0)
      return cover_path;
  }

  return NULL;
}

/**
 * @brief Render image to ASCII view
 *
 * @param image_path path to cover image
 * @return char*
 **/
char *render_image_to_ansi(const char *image_path) {
  static char buffer[30000];
  char command[512];
  FILE *fp;

  if (image_path == NULL)
    return NULL;

  snprintf(command, sizeof(command), "tiv \"%s\" -w %d -h %d 2>/dev/null",
           image_path, IMAGE_WIDTH, IMAGE_HEIGHT);

  fp = popen(command, "r");
  if (fp == NULL)
    return NULL;

  size_t total_read = 0;
  buffer[0] = '\0';

  char line[256];
  while (fgets(line, sizeof(line), fp) != NULL) {
    size_t line_len = strlen(line);
    if (total_read + line_len < sizeof(buffer) - 1) {
      strcat(buffer, line);
      total_read += line_len;
    } else {
      break;
    }
  }

  pclose(fp);

  if (strlen(buffer) == 0)
    return NULL;

  return buffer;
}
