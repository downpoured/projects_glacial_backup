/*
util_os.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "util_os.h"

#if __linux__
#include <sys/wait.h>
#include <utime.h>

/* returns true if path is a file or directory */
bool os_file_or_dir_exists_basic(const char *filepath, bool *is_file)
{
    struct stat64 st = {};
    errno = 0;
    int n = stat64(filepath, &st);
    log_b(n == 0 || errno == ENOENT, "%s %d", filepath, errno);
    if (is_file)
    {
        *is_file = (st.st_mode & S_IFDIR) == 0;
    }

    return n == 0;
}

/* delete file, returns true on success */
bool os_remove_file_basic(const char *filepath)
{
    int st = 0;
    log_errno_to(st, remove(filepath), filepath);
    return st == 0;
}

/* create directory, returns true on success */
bool os_create_dir_basic(const char *filepath)
{
    bool is_file = false;
    bool exists = os_file_or_dir_exists_basic(filepath, &is_file);
    if (exists && is_file)
    {
        log_b(0, "exists && is_file %s filepath", filepath);
        return false;
    }
    else if (exists)
    {
        return true;
    }
    else
    {
        const mode_t mode = 0755;
        int ret = 0;
        log_errno_to(ret, mkdir(filepath, mode), filepath);
        return ret == 0;
    }
}

/* clear console */
void os_clr_console(void)
{
    ignore_unused_result(system("clear"));
}

/* get clock seconds and milliseconds */
void os_clock_gettime(int64_t *s, int32_t *ms)
{
    struct timespec tm;
    (void)clock_gettime(CLOCK_REALTIME, &tm);
    *s = (int64_t)tm.tv_sec;
    *ms = (int32_t)(tm.tv_nsec / (1000 * 1000));
}

/* sleep */
void os_sleep(uint32_t ms)
{
    usleep((__useconds_t)(ms * 1000));
}

/* set environment variable */
void os_set_env(const char *key, const char *val)
{
    setenv(key, val, 1 /* overwrite */);
}

/* from unix error number to a string explanation */
void os_errno_to_buffer(int err, char *s_out, size_t s_out_len)
{
#if defined(__GNU_LIBRARY__) && defined(_GNU_SOURCE)
    char localbuf[BUFSIZ] = "";
    const char *s = strerror_r(err, localbuf, countof(localbuf));
    if (s)
    {
        strncpy(s_out, s, s_out_len - 1);
    }
#else
#error platform not yet implemented
#endif
}

/* from win32 error number to a string explanation */
void os_win32err_to_buffer(unused(unsigned long), char *buf, size_t len)
{
    memset(buf, 0, len);
}

/* allocate aligned memory */
byte *os_aligned_malloc(uint32_t size, uint32_t align)
{
    void *result = 0;
    check_fatal(posix_memalign(&result, align, size) == 0,
        "posix_memalign failed, errno=%d", errno);
    return (byte *)result;
}

/* free aligned memory */
void os_aligned_free(byte **ptr)
{
    sv_freenull(*ptr);
}

/* timer, measures wall-clock times */
os_perftimer os_perftimer_start(void)
{
    os_perftimer ret = {};
    ret.timestarted = time(NULL);
    return ret;
}

/* read from timer */
double os_perftimer_read(const os_perftimer *timer)
{
    time_t unixtime = time(NULL);
    return (double)(unixtime - timer->timestarted);
}

/* close timer */
void os_perftimer_close(unused_ptr(os_perftimer))
{
    /* no allocations, nothing needs to be done. */
}

const bool islinux = true;

#elif _WIN32

/* returns true if path is a file or directory */
bool os_file_or_dir_exists_basic(const char *filepath, bool *is_file)
{
    SetLastError(0);
    SetLastError(0);
    DWORD file_attr = GetFileAttributesA(filepath);
    log_b(file_attr != INVALID_FILE_ATTRIBUTES ||
            GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND,
        "%s %lu", filepath, GetLastError());
    if (is_file)
    {
        *is_file = ((file_attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
    }

    return file_attr != INVALID_FILE_ATTRIBUTES;
}

/* delete file, returns true on success */
bool os_remove_file_basic(const char *filepath)
{
    /* returns false on failure */
    bool ret = false;
    log_win32_to(ret, DeleteFileA(filepath) != FALSE, false, filepath);
    return ret;
}

/* create directory, returns true on success */
bool os_create_dir_basic(const char *path)
{
    bool is_file = false;
    bool exists = os_file_or_dir_exists_basic(path, &is_file);
    if (exists && is_file)
    {
        log_b(0, "exists && is_file %s", path);
        return false;
    }
    else if (exists)
    {
        return true;
    }
    else
    {
        bool ret = false;
        log_win32_to(ret, CreateDirectoryA(path, NULL) != FALSE, false, path);
        return ret;
    }
}

/* clear console */
void os_clr_console(void)
{
    (void)system("cls");
}

/* get clock seconds and milliseconds */
void os_clock_gettime(int64_t *s, int32_t *ms)
{
    /* based on code by Asain Kujovic released under the MIT license */
    FILETIME wintime;
    GetSystemTimeAsFileTime(&wintime);
    ULARGE_INTEGER time64;
    time64.HighPart = wintime.dwHighDateTime;
    time64.LowPart = wintime.dwLowDateTime;
    time64.QuadPart -= 116444736000000000i64; /* 1jan1601 to 1jan1970 */
    const long long n = 10000000i64;
    *s = (int64_t)time64.QuadPart / n;
    *ms = (int32_t)((time64.QuadPart - (*s * n)) / 10000);
}

/* sleep */
void os_sleep(uint32_t ms)
{
    Sleep(ms);
}

/* set environment variable */
void os_set_env(const char *key, const char *val)
{
    _putenv_s(key, val);
}

/* from unix error number to a string explanation */
void os_errno_to_buffer(int err, char *str, size_t str_len)
{
    (void)strerror_s(str, str_len - 1, err);
}

/* from win32 error number to a string explanation */
void os_win32err_to_buffer(unsigned long err, char *buf, size_t buflen)
{
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, (DWORD)err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
        cast64u32u(buflen), NULL);
}

/* allocate aligned memory */
byte *os_aligned_malloc(uint32_t size, uint32_t align)
{
    void *result = _aligned_malloc(size, align);
    check_fatal(result, "_aligned_malloc failed");
    return (byte *)result;
}

/* free aligned memory */
void os_aligned_free(byte **ptr)
{
    _aligned_free(*ptr);
    *ptr = NULL;
}

/* timer using QueryPerformanceCounter. very precise over short spans. */
os_perftimer os_perftimer_start(void)
{
    os_perftimer ret = {};
    LARGE_INTEGER time = {};
    QueryPerformanceCounter(&time);
    ret.timestarted = time.QuadPart;
    return ret;
}

/* read from timer */
double os_perftimer_read(const os_perftimer *timer)
{
    LARGE_INTEGER time = {};
    QueryPerformanceCounter(&time);
    int64_t diff = time.QuadPart - timer->timestarted;
    LARGE_INTEGER freq = {};
    QueryPerformanceFrequency(&freq);
    return (diff) / ((double)freq.QuadPart);
}

/* close timer */
void os_perftimer_close(os_perftimer *timer)
{
    /* no allocations, nothing needs to be done. */
    unused(timer);
}

const bool islinux = false;

#else
#error "platform not yet supported"
#endif

/* return true if path is a file */
bool os_file_exists_basic(const char *filepath)
{
    bool is_file = false;
    return os_file_or_dir_exists_basic(filepath, &is_file) && is_file;
}

/* return true if path is a directory */
bool os_dir_exists_basic(const char *filepath)
{
    bool is_file = false;
    return os_file_or_dir_exists_basic(filepath, &is_file) && !is_file;
}

/* split path and return the filename */
void os_get_filename(const char *fullpath, bstring filename)
{
    const char *last_slash = strrchr(fullpath, pathsep[0]);
    if (!islinux)
    {
        const char *last_fslash = strrchr(fullpath, '/');
        if (last_fslash && last_fslash > last_slash)
        {
            last_slash = last_fslash;
        }
    }

    if (last_slash)
    {
        bassigncstr(filename, last_slash + 1);
    }
    else
    {
        bassigncstr(filename, fullpath);
    }
}

/* split path and return the parent directory */
void os_get_parent(const char *fullpath, bstring dir)
{
    const char *last_slash = strrchr(fullpath, pathsep[0]);
    if (!islinux)
    {
        const char *last_fslash = strrchr(fullpath, '/');
        if (last_fslash && last_fslash > last_slash)
        {
            last_slash = last_fslash;
        }
    }

    if (last_slash)
    {
        bassignblk(dir, fullpath, cast64s32s(last_slash - fullpath));
    }
    else
    {
        /* no parent was provided. */
        bstrclear(dir);
    }
}

/* split path into parent and filename */
void os_split_dir(const char *s, bstring parent, bstring child)
{
    os_get_parent(s, parent);
    os_get_filename(s, child);
}

/* delete file and check that it was deleted */
void os_ensure_remove_file_basic(const char *filepath)
{
    os_remove_file_basic(filepath);
    check_b_warn(!os_file_exists_basic(filepath));
}

/* wrapper around fopen */
check_result sv_file_open_basic(
    sv_file *self, const char *path, const char *mode)
{
    sv_result currenterr = {};
    errno = 0;
    self->file = fopen(path, mode);
    check_b(self->file, "failed to open %s %s, got %d", path, mode, errno);

cleanup:
    return currenterr;
}

/* close wrapper around fopen */
void sv_file_close(sv_file *self)
{
    if (self && self->file)
    {
        fclose(self->file);
        set_self_zero();
    }
}
