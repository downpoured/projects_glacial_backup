/*
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

#include "util.h"

void os_init();

void sv_file_close(sv_file *self);
check_result sv_file_open(sv_file *self,
	const char *path, const char *mode);
check_result sv_file_writefile(const char *filepath,
	const char *contents, const char *mode);
check_result sv_file_readfile(const char *filepath,
	bstring contents);

/* file handles */
typedef struct os_exclusivefilehandle {
	bstring loggingcontext;
	void *os_handle;
	int fd;
} os_exclusivefilehandle;

void os_exclusivefilehandle_close(os_exclusivefilehandle *self);
void os_fd_close(int *fd);
check_result os_exclusivefilehandle_open(os_exclusivefilehandle *self, 
	const char *path, bool allowread, bool *filenotfound);
check_result os_exclusivefilehandle_tryuntil_open(os_exclusivefilehandle *self,
	const char *path, bool allowread, bool *filenotfound);
check_result os_exclusivefilehandle_stat(os_exclusivefilehandle *self, 
	uint64_t *size, uint64_t *modtime, bstring permissions);

/* filesystem ops */
bool os_file_exists(const char *filepath);
bool os_dir_exists(const char *filepath);
bool os_file_or_dir_exists(const char *filepath, bool *is_file);
void os_get_filename(const char *fullpath, bstring filename);
void os_get_parent(const char *fullpath, bstring parent);
void os_split_dir(const char *fullpath, bstring dir, bstring filepath);
uint64_t os_getfilesize(const char *s);
uint64_t os_getmodifiedtime(const char *s);
bool os_setmodifiedtime_nearestsecond(const char *s, uint64_t t);
bool os_create_dir(const char *s);
bool os_create_dirs(const char *s);
bool os_copy(const char *s1, const char *s2, bool overwrite_ok);
bool os_move(const char *s1, const char *s2, bool overwrite_ok);
bool os_tryuntil_copy(const char *s1, const char *s2, bool overwrite_ok);
bool os_tryuntil_move(const char *s1, const char *s2, bool overwrite_ok);
bool os_setcwd(const char *s);
bool os_isabspath(const char *s);
bool os_issubdirof(const char *s1, const char *s2);
bstring os_getthisprocessdir();
bstring os_get_create_appdatadir();
bool os_lock_file_to_detect_other_instances(const char *path, int *out_code);
void os_open_dir_ui(const char *dir);
bool os_remove(const char *s);
bool os_tryuntil_remove(const char *s);
void os_clr_console();
void os_clock_gettime(int64_t *seconds, int32_t *milliseconds);
void os_sleep(uint32_t milliseconds);
void os_win32err_to_buffer(unsigned long err, char *buf, size_t buflen);
void os_errno_to_buffer(int err, char *buf, size_t buflen);
byte *os_aligned_malloc(uint32_t size, uint32_t align);
void os_aligned_free(byte **ptr);

/* listing directories */
typedef sv_result(*FnRecurseThroughFilesCallback)(void *context,
	const bstring filepath, uint64_t modtime, uint64_t filesize, const bstring permissions);

typedef struct os_recurse_params {
	void *context;
	const char *root;
	FnRecurseThroughFilesCallback callback;
	int max_recursion_depth;
	bstrlist *messages;
} os_recurse_params;

check_result os_recurse(os_recurse_params *params);
check_result os_listfiles(const char *dir, bstrlist *list, bool sorted);
check_result os_listdirs(const char *dir, bstrlist *list, bool sorted);
check_result os_tryuntil_deletefiles(const char *dir, const char *filenamepattern);
check_result os_findlastfilewithextension(const char *dir, const char *extension, bstring path);
check_result os_tryuntil_movebypattern(const char *dir,
	const char *namepattern, const char *destdir, bool canoverwrite, int *moved);
check_result os_binarypath(const char *binname, bstring out);
check_result os_binarypath_impl(sv_pseudosplit *spl, const char *binname, bstring out);
check_result os_set_permissions(const char *filepath, const bstring permissions);
void os_get_permissions(const struct stat64 *st, bstring permissions);
bool os_try_set_readable(const char *filepath);
bool os_recurse_is_dir(uint64_t modtime, uint64_t filesize);
bool os_dir_empty(const char *dir);
bool os_is_dir_writable(const char *dir);

bstring os_get_tmpdir(const char *subdir);
bstring os_make_subdir(const char *parent, const char *leaf);

typedef enum os_proc_op {
	os_proc_none = 0,
	os_proc_stdin,
	os_proc_stdout_stderr
} os_proc_op;

/* process */
check_result os_restart_as_other_user(const char *data_dir);
check_result os_run_process_ex(const char *path,
	const char *const args[], os_proc_op op, os_exclusivefilehandle *providestdin,
	bool fastjoinargs, bstring useargscombined, bstring getoutput, int *retcode);
check_result os_run_process(const char *path, const char *const args[], 
	bool fastjoinargs, bstring useargscombined, bstring getoutput, int *retcode);
int os_tryuntil_run_process(const char *path,
	const char *const args[], bool fastjoinargs, bstring useargscombined, bstring getoutput);
bool argvquote(const char *path,
	const char *const args[] /* NULL-terminated */, bstring result, bool fast);

/* timing */
typedef struct os_perftimer {
	int64_t timestarted;
} os_perftimer;

os_perftimer os_perftimer_start();
double os_perftimer_read(const os_perftimer *timer);
void os_perftimer_close(os_perftimer *timer);
void os_print_mem_usage();
extern const bool islinux;

/* restrict write access */
extern bstring restrict_write_access;
void confirm_writable(const char *s);

#if __linux__
#define mainsig main(int argc, char **argv)
#define memzero_s(buf, len) memset_s((buf), 0, (len))
#define devnull "/dev/null"
bstring parse_cmd_line_args(int argc, char **argv, bool *is_low);
#define O_BINARY 0
#define close_set_invalid(fd) do { if (fd != -1) { close(fd); } fd = -1; } while (0)
#define linuxonly(code) code,
#else
/* it's better to use wmain, because otherwise _wgetenv has to
do extra work, converting all environment variables to utf16. */
#define mainsig wmain(int argc, wchar_t *argv[], wchar_t *[])
#define memzero_s(buf, len) SecureZeroMemory((buf), (len))
#define devnull "NUL"
#define CloseHandleNull(pptr) do { CloseHandle(*(pptr)); *(pptr)=NULL;  } while (0)
#define linuxonly(code)
bstring parse_cmd_line_args(int argc, wchar_t *argv[], bool *is_low);
#endif

#endif