#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

int file_read_int(const char *path, int *out);

int file_read_long(const char *path, long *out);

int file_read_double(const char *path, double *out);

int file_read_string(const char *path, char *buf, size_t sz);

#endif // UTIL_H
