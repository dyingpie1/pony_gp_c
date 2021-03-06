
#ifndef PONY_GP_FILE_UTIL_H
#define PONY_GP_FILE_UTIL_H

#include <stdio.h>
#include <string.h>
#include "../include/memmngr.h"

#define MAX_LINE_LENGTH 4096

extern char comment_sym;
extern const char delimeter[2];
extern const char const_delimeter[2];

char **get_lines(FILE *file);
int get_num_lines(FILE *file);
void reset_file_position(FILE *file);

#endif //PONY_GP_FILE_UTIL_H
