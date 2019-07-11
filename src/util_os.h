/*
util_os.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef UTIL_OS_H_INCLUDE
#define UTIL_OS_H_INCLUDE

#include "util_higher.h"

bool os_file_exists_basic(const char *filepath);
bool os_dir_exists_basic(const char *filepath);
bool os_create_dir_basic(const char *filepath);
bool os_file_or_dir_exists_basic(const char *filepath, bool *is_file);
void os_get_filename(const char *fullpath, bstring filename);
void os_get_parent(const char *fullpath, bstring parent);
void os_split_dir(const char *s, bstring parent, bstring child);
bool os_remove_file_basic(const char *filepath);
void os_ensure_remove_file_basic(const char *filepath);
void os_clr_console(void);
void os_clock_gettime(int64_t *seconds, int32_t *ms);
void os_sleep(uint32_t milliseconds);
void os_set_env(const char *key, const char *val);
void os_win32err_to_buffer(unsigned long err, char *buf, size_t buflen);
void os_errno_to_buffer(int err, char *buf, size_t buflen);
byte *os_aligned_malloc(uint32_t size, uint32_t align);
void os_aligned_free(byte **ptr);

struct stat64;
typedef struct os_perftimer
{
    int64_t timestarted;
} os_perftimer;

os_perftimer os_perftimer_start(void);
double os_perftimer_read(const os_perftimer *timer);
void os_perftimer_close(os_perftimer *timer);
extern const bool islinux;

void sv_file_close(sv_file *self);
check_result sv_file_open_basic(
    sv_file *self, const char *path, const char *mode);

#if __linux__
#define memzero_s(buf, len) memset_s((buf), 0, (len))
#define O_BINARY 0
#else
#define memzero_s(buf, len) SecureZeroMemory((buf), (len))
#endif

#endif
