#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int file_read_line(const char *path, char *buf, size_t sz)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(buf, (int)sz, f))
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    buf[strcspn(buf, "\r\n")] = 0;
    return 0;
}

int file_read_int(const char *path, int *out)
{
    char buf[64];
    if (file_read_line(path, buf, sizeof(buf)) < 0) return -1;

    return sscanf(buf, "%d", out) == 1 ? 0 : -1;
}

int file_read_long(const char *path, long *out)
{
    if (!out) return -1;

    char buf[64];
    if (file_read_line(path, buf, sizeof(buf)) < 0) return -1;

    char *end = NULL;
    long val = strtol(buf, &end, 10);
    if (end == buf) return -1;

    *out = val;
    return 0;
}

int file_read_double(const char *path, double *out)
{
    char buf[64];
    if (file_read_line(path, buf, sizeof(buf)) < 0) return -1;

    return sscanf(buf, "%lf", out) == 1 ? 0 : -1;
}

int file_read_string(const char *path, char *buf, size_t sz)
{
    return file_read_line(path, buf, sz);
}
