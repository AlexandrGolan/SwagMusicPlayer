#include "../include/file_utils.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "../include/config.h"

int check_tiv_available() {
    return system("which tiv > /dev/null 2>&1") == 0;
}

char* find_cover_image(const char* directory) {
    static char cover_path[512];
    const char* filenames[] = {"cover",
                               "Cover",
                               "COVER",
                               "album",
                               "Album",
                               "ALBUM",
                               "folder",
                               "Folder",
                               "FOLDER",
                               "front",
                               "Front",
                               "FRONT",
                               "art",
                               "Art",
                               "ART",
                               NULL};

    const char* extensions[] = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", NULL};

    for (int f = 0; filenames[f] != NULL; f++) {
        for (int i = 0; extensions[i] != NULL; i++) {
            snprintf(cover_path, sizeof(cover_path), "%s/%s%s", directory, filenames[f], extensions[i]);
            if (access(cover_path, F_OK) == 0) {
                return cover_path;
            }
        }
    }

    DIR* dir = opendir(directory);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            const char* name = entry->d_name;
            const char* ext = strrchr(name, '.');

            if (ext
                && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0
                    || strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".gif") == 0
                    || strcasecmp(ext, ".bmp") == 0 || strcasecmp(ext, ".webp") == 0))
            {
                if (strstr(name, "cover") || strstr(name, "Cover") || strstr(name, "album")
                    || strstr(name, "Album") || strstr(name, "folder") || strstr(name, "Folder"))
                {
                    snprintf(cover_path, sizeof(cover_path), "%s/%s", directory, name);
                    closedir(dir);
                    return cover_path;
                }
            }
        }
        closedir(dir);
    }

    return NULL;
}

char* render_image_to_ansi(const char* image_path) {
    static char buffer[MAX_ANSI_BUFFER];
    char command[512];
    FILE* fp;

    if (image_path == NULL) {
        return NULL;
    }

    snprintf(command,
             sizeof(command),
             "tiv \"%s\" -w %d -h %d 2>/dev/null",
             image_path,
             IMAGE_WIDTH,
             IMAGE_HEIGHT);

    fp = popen(command, "r");
    if (fp == NULL) {
        return NULL;
    }

    size_t total_read = 0;
    buffer[0] = '\0';

    char line[MAX_LINE_LENGTH];
    int line_count = 0;

    while (fgets(line, sizeof(line), fp) != NULL && total_read < sizeof(buffer) - 1
           && line_count < IMAGE_HEIGHT)
    {
        size_t line_len = strlen(line);

        if (line_len > 0) {
            if (total_read + line_len < sizeof(buffer) - 1) {
                strcat(buffer, line);
                total_read += line_len;
                line_count++;
            }
        }
    }

    pclose(fp);

    if (total_read == 0) {
        return NULL;
    }

    return buffer;
}
