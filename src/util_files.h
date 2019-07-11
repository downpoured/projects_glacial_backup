/*
util_files.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef UTIL_FILES_H_INCLUDE
#define UTIL_FILES_H_INCLUDE

#include "util_os.h"

void os_init();

check_result sv_file_open(sv_file *self,
    const char *path, const char *mode);
check_result sv_file_writefile(const char *filepath,
    const char *contents, const char *mode);
check_result sv_file_readfile(const char *filepath,
    bstring contents);

typedef struct os_lockedfilehandle {
    bstring loggingcontext;
    void *os_handle;
    int fd;
} os_lockedfilehandle;

void os_lockedfilehandle_close(os_lockedfilehandle *self);
void os_fd_close(int *fd);
check_result os_lockedfilehandle_open(os_lockedfilehandle *self,
    const char *path, bool allowread, bool *filenotfound);
check_result os_lockedfilehandle_tryuntil_open(os_lockedfilehandle *self,
    const char *path, bool allowread, bool *filenotfound);
check_result os_lockedfilehandle_stat(os_lockedfilehandle *self,
    uint64_t *size, uint64_t *modtime, bstring permissions);

bool os_file_exists(const char *filepath);
bool os_dir_exists(const char *filepath);
bool os_file_or_dir_exists(const char *filepath, bool *is_file);
uint64_t os_getfilesize(const char *s);
uint64_t os_getmodifiedtime(const char *s);
bool os_setmodifiedtime_nearestsecond(const char *s, uint64_t t);
bool os_create_dir(const char *s);
bool os_create_dirs(const char *s);
bool os_copy(const char *s1, const char *s2, bool overwrite);
bool os_move(const char *s1, const char *s2, bool overwrite);
bool os_tryuntil_copy(const char *s1, const char *s2, bool overwrite);
bool os_tryuntil_move(const char *s1, const char *s2, bool overwrite);
bool os_setcwd(const char *s);
bool os_isabspath(const char *s);
bool os_issubdirof(const char *s1, const char *s2);
bstring os_getthisprocessdir();
bstring os_get_create_appdatadir();
bool os_detect_other_instances(const char *path, int *out_code);
void os_open_dir_ui(const char *dir);
bool os_remove(const char *s);
bool os_tryuntil_remove(const char *s);

typedef sv_result(*FnRecurseThroughFilesCallback)(void *context,
    const bstring filepath,
    uint64_t modtime,
    uint64_t filesize,
    const bstring permissions);

typedef struct os_recurse_params {
    void *context;
    const char *root;
    FnRecurseThroughFilesCallback callback;
    int max_recursion_depth;
    bstrlist *messages;
} os_recurse_params;

struct stat64;
check_result os_listfiles(const char *dir,
    bstrlist *list, bool sorted);
check_result os_listdirs(const char *dir,
    bstrlist *list, bool sorted);
check_result os_tryuntil_deletefiles(const char *dir,
    const char *filenamepattern);
check_result os_findlastfilewithextension(const char *dir,
    const char *extension, bstring path);
check_result os_tryuntil_movebypattern(const char *dir,
    const char *pattern, const char *destdir, bool overwrite, int *moved);
check_result os_binarypath_impl(sv_pseudosplit *spl,
    const char *binname, bstring out);
check_result os_set_permissions(const char *filepath,
    const bstring permissions);
check_result os_recurse(os_recurse_params *params);
check_result os_binarypath(const char *binname, bstring out);
void os_get_permissions(const struct stat64 *st, bstring permissions);
bool os_try_set_readable(const char *filepath, bool readable);
bool os_recurse_is_dir(uint64_t modtime, uint64_t filesize);
bool os_get_short_path(const char *path, bstring shortpath);
bool os_dir_empty(const char *dir);
bool os_is_dir_writable(const char *dir);
bstring os_get_tmpdir(const char *subdir);

check_result os_restart_as_other_user(const char *data_dir);
check_result os_run_process(const char *path,
    const char *const args[],
    bstring output,
    bstring useargscombined,
    bool fastjoinargs,
    const char *stdout_to_file,
    os_lockedfilehandle *providestdin,
    int *outretcode);
check_result os_tryuntil_run(const char *path,
    const char *const args[],
    bstring output,
    bstring useargscombined,
    bool fastjoinargs,
    int acceptretcode,
    const char *stdout_to_file);
bool argvquote(const char *path,
    const char *const args[],
    bstring result,
    bool fast);
#define close_set_invalid(fd) \
    do { if (fd != -1) { \
    close(fd); } fd = -1; } while (0)
#define CloseHandleNull(pptr) \
    do { CloseHandle(*(pptr)); \
    *(pptr)=NULL;  } while (0)


extern const bool islinux;

/* restrict write access */
extern bstring restrict_write_access;
void confirm_writable(const char *s);

#if __linux__
#define mainsig main(int argc, char **argv)
#define linuxonly(code) code,
bstring parse_cmd_line_args(int argc, char **argv, bool *is_low);
#else
/* use wmain() instead of main() to indicate a UTF16 environment,
and get a small perf increase when referencing _wgetenv */
#define mainsig wmain(int argc, wchar_t *argv[], wchar_t *[])
#define linuxonly(code)
bstring parse_cmd_line_args(int argc, wchar_t *argv[], bool *is_low);
const unsigned int S_IRUSR = 0000400, S_IRGRP = 0000040;
#endif



#endif
