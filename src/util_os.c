
#include "util_os.h"
int max_tries = 20;
uint32_t sleep_between_tries = 250;

#if __linux__
#include <utime.h>
#include <sys/sendfile.h>
#include <sys/wait.h>

bool os_file_exists(const char* filename)
{
	struct stat64 st = {};
	log_errno(int ret, stat64(filename, &st), filename);
	return ret == 0 && (st.st_mode & S_IFDIR) == 0;
}

bool os_dir_exists(const char* filename)
{
	struct stat64 st = {};
	log_errno(int ret, stat64(filename, &st), filename);
	return ret == 0 && (st.st_mode & S_IFDIR) != 0;
}

uint64_t os_getfilesize(const char *filepath)
{
	struct stat64 st = {};
	staticassert(sizeof(st.st_size) == sizeof(uint64_t));
	log_errno(int ret, stat64(filepath, &st), filepath);
	return ret == 0 ? cast64s64u(st.st_size) : 0;
}

uint64_t os_getmodifiedtime(const char* filename)
{
	struct stat64 st = {};
	staticassert(sizeof(st.st_size) == sizeof(uint64_t));
	log_errno(int ret, stat64(filename, &st), filename);
	return ret == 0 ? st.st_ctime : 0;
}

bool os_setmodifiedtime_nearestsecond(const char *filepath, uint64_t t)
{
	bool ret = false;
	struct stat64 st = {};
	staticassert(sizeof(st.st_size) == sizeof(uint64_t));
	log_errno(int retstat, stat64(filepath, &st), filepath);
	if (retstat >= 0)
	{
		struct utimbuf new_times = { 0 };
		new_times.actime = st.st_atime;
		new_times.modtime = (time_t)t;
		log_errno(int rettime, utime(filepath, &new_times));
		ret = (rettime == 0);
	}

	return ret;
}

bool os_create_dir(const char* s)
{
	log_errno(int ret, mkdir(s, 0777 /*permissions*/));
	return ret == 0 || os_dir_exists(s);
}

check_result os_copy_impl(const char *s1, const char *s2, bool overwrite_ok)
{
	sv_result currenterr = {};
	int read_fd = -1, write_fd = -1;
	if (s_equal(s1, s2))
	{
		sv_log_writeFmt("skipping attempted copy of %s onto itself", s1);
		goto cleanup;
	}

	check_b(os_isabspath(s1), "%s", s1);
	check_b(os_isabspath(s2), "%s", s2);
	check_errno(read_fd, open(s1, O_RDONLY | O_BINARY), s1, s2);
	struct stat64 st = {};
	check_errno(_, fstat64(read_fd, &st), s1, s2);
	check_errno(write_fd, open(s2, overwrite_ok ?
		(O_WRONLY | O_CREAT) : (O_WRONLY | O_CREAT | O_EXCL),
		st.st_mode), s1, s2);
	check_errno(_, cast64s32s(sendfile(write_fd, read_fd, NULL, cast64s64u(st.st_size))), s1, s2);

cleanup:
	os_fd_close(&read_fd);
	os_fd_close(&write_fd);
	return currenterr;
}

bool os_copy(const char* s1, const char* s2, bool overwrite_ok)
{
	sv_result res = os_copy_impl(s1, s2, overwrite_ok);
	log_b(res.code == 0, "got %s", cstr(res.msg));
	sv_result_close(&res);
	return res.code == 0;
}

bool os_move(const char *s1, const char *s2, bool overwrite_ok)
{
	if (s_equal(s1, s2))
	{
		sv_log_writeFmt("skipping attempted move of %s onto itself", s1);
		return true;
	}

	if (!os_isabspath(s1) || !os_isabspath(s2))
	{
		sv_log_writeFmt("must have abs paths but given %s, %s", s1, s2);
		return false;
	}

	if (!overwrite_ok)
	{
		struct stat64 st = {};
		errno = 0;
		if (stat64(s2, &st) == 0 || errno != ENOENT)
		{
			sv_log_writeFmt("cannot move %s, %s, dest already exists %d", s1, s2, errno);
			return false;
		}
	}
	
	log_errno(int ret, renameat(AT_FDCWD, s1, AT_FDCWD, s2), s1, s2);
	return ret == 0;
}

check_result os_exclusivefilehandle_open(os_exclusivefilehandle* self, const char* path, bool allowread, bool* filenotfound)
{
	sv_result currenterr = {};
	bstring tmp = bstring_open();
	set_self_zero();
	self->loggingcontext = bfromcstr(path);

	errno = 0;
	self->fd = open(path, O_RDONLY | O_BINARY);
	if (self->fd < 0 && errno == ENOENT && filenotfound)
	{
		set_self_zero();
		*filenotfound = true;
	}
	else
	{
		char errorname[BUFSIZ] = { 0 };
		os_errno_to_buffer(errno, errorname, countof(errorname));
		check_b(self->fd >= 0, "While trying to open %s, we got %s(%d)", path, errorname, errno);
		check_b(self->fd > 0, "expect fd to be > 0");
		int sharing = allowread ? (LOCK_SH | LOCK_NB) : (LOCK_EX | LOCK_NB);
		check_errno(_, flock(self->fd, sharing), path);
	}
	self->os_handle = NULL;
cleanup:
	bdestroy(tmp);
	return currenterr;
}

check_result os_exclusivefilehandle_stat(
	os_exclusivefilehandle* self, uint64_t* size, uint64_t* modtime, uint64_t* permissions)
{
	sv_result currenterr = {};
	struct stat64 st = {};
	check_errno(_, fstat64(self->fd, &st), cstr(self->loggingcontext));
	*size = (uint64_t)st.st_size;
	*modtime = (uint64_t)st.st_mtime;
	*permissions = (uint64_t)st.st_mode;

cleanup:
	return currenterr;
}

bool os_setcwd(const char* s)
{
	errno = 0;
	log_errno(int ret, chdir(s), s);
	return ret == 0;
}

bool os_isabspath(const char* s)
{
	return s && s[0] == '/';
}

bstring os_getthisprocessdir()
{
	errno = 0;
	char buffer[PATH_MAX] = { 0 };
	log_errno(int len, (int)readlink("/proc/self/exe", buffer, sizeof(buffer) - 1));
	if (len != -1)
	{
		buffer[len] = '\0'; /* value might not be null term'd */
		log_b(os_isabspath(buffer) && os_dir_exists(buffer), "path=%s", buffer);
		if (os_isabspath(buffer) && os_dir_exists(buffer))
		{
			bstring dir = bstring_open();
			os_get_parent(buffer, dir);
			return dir;
		}
	}

	return bstring_open();
}

bstring os_get_create_appdatadir()
{
	bstring ret = bstring_open();
	char* confighome = getenv("XDG_CONFIG_HOME");
	if (confighome && os_isabspath(confighome))
	{
		bassignformat(ret, "%s/glacial_backup", confighome);
		if (os_create_dir(cstr(ret)))
			return ret;
	}

	char* home = getenv("HOME");
	if (home && os_isabspath(home))
	{
		bassignformat(ret, "%s/.local/share/glacial_backup", home);
		if (os_create_dir(cstr(ret)))
			return ret;

		bassignformat(ret, "%s/glacial_backup", home);
		if (os_create_dir(cstr(ret)))
			return ret;
	}

	/* indicate failure*/
	bstrClear(ret);
	sv_log_writeLine("Could not create appdatadir");
	return ret;
}

bool os_lock_file_to_detect_other_instances(const char* path, int* out_code)
{
	/* provide O_CLOEXEC so that the duplicate is closed upon exec. */
	log_errno(int pid_file, open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0666), path);
	if (pid_file < 0)
	{
		*out_code = 1;
		return false;
	}

	log_errno(int ret, flock(pid_file, LOCK_EX | LOCK_NB), path);
	if (ret != 0 && errno != EWOULDBLOCK)
	{
		close(pid_file);
		*out_code = errno;
	}

	/* intentionally leak the lock... will be closed on process exit */
	return ret != 0;
}

void os_open_dir_ui(const char* dir)
{
	printf("The directory is %s\n", dir);
	if (os_dir_exists(dir) && os_isabspath(dir))
	{
		bstring s = bformat("xdg-open \"%s\"", dir);
		(void)system(cstr(s));
		bdestroy(s);
	}
}

bool os_remove(const char* s)
{
	/* removes file or empty directory. ok if s was previously deleted. */
	log_errno(_, remove(s), s);
	struct stat64 st = { 0 };
	errno = 0;
	return stat64(s, &st) != 0 && errno == ENOENT;
}

void os_clr_console()
{
	(void)system("clear");
}

void os_clock_gettime(int64_t* seconds, int32_t* milliseconds)
{
	struct timespec tmspec;
	(void)clock_gettime(CLOCK_REALTIME, &tmspec);
	*seconds = (int64_t)tmspec.tv_sec;
	*milliseconds = cast64s32s(tmspec.tv_nsec / 1000);
}

void os_sleep(uint32_t milliseconds)
{
	usleep(milliseconds * 1000);
}

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
#error platform not yet implemented (different platforms have differing strerror_r behavior)
#endif
}

void os_win32err_to_buffer(unsigned long unused, char* buf, size_t buflen)
{
	(void)unused;
	memset(buf, 0, buflen);
}

void os_open_glacial_backup_website()
{
	int ret = system("xdg-open 'https://github.com/downpoured'");
	sv_log_writeFmt("opening website. retcode=%d", ret);
}

byte* os_aligned_malloc(uint32_t size, uint32_t align)
{
	void *result = 0;
	check_fatal(posix_memalign(&result, align, size) == 0, "posix_memalign failed, errno=%d", errno);
	return (byte*)result;
}

void os_aligned_free(byte** ptr)
{
	sv_freenull(*ptr);
}

check_result os_binarypath(const char* binname, bstring out)
{
	sv_result currenterr = {};
	const char* path = getenv("PATH");
	check_b(path && path[0] != '\0', "Path environment variable not found.");
	sv_pseudosplit spl = sv_pseudosplit_open(path);
	sv_pseudosplit_split(&spl, ':');
	check(os_binarypath_impl(&spl, binname, out));
cleanup:
	sv_pseudosplit_close(&spl);
	return currenterr;
}

bstring os_get_tmpdir(const char* subdirname)
{
	bstring ret = bstring_open();
	char* candidates[] = { "TMPDIR", "TMP", "TEMP", "TEMPDIR" };
	for (int i = 0; i < countof(candidates); i++)
	{
		const char* val = getenv(candidates[i]);
		log_b(!val || os_isabspath(val), "relative temp path? %s", val);
		if (val && os_isabspath(val))
		{
			bassignformat(ret, "%s%s%s", val, pathsep, subdirname);
			if (os_create_dir(cstr(ret)))
			{
				return ret;
			}
		}
	}

	bstrClear(ret);
	return ret;
}

void os_init()
{
	/* not needed in linux */
}

static check_result os_recurse_impl_dir(os_recurse_params* params,
	bstrList* dirs, bool* retriable_err, bstring currentdir, bstring tmpfullpath)
{
	sv_result currenterr = {};
	*retriable_err = false;
	bstrClear(tmpfullpath);
	DIR* dir = opendir(cstr(currentdir));
	if (dir == NULL)
	{
		*retriable_err = true;
		char buf[BUFSIZ] = "";
		os_errno_to_buffer(errno, buf, countof(buf));
		check_b(0, "Could not list \n%s\nbecause of code %s (%d)",
			cstr(currentdir), buf, errno);
	}

	while (true)
	{
		errno = 0;
		struct dirent *entry = readdir(dir);
		if (!entry)
		{
			if (errno != 0)
			{
				*retriable_err = true;
				check_b(0, "Could not continue listing \n%s\nbecause of code %d", cstr(currentdir), errno);
			}
			break;
		}
		else if (s_equal(".", entry->d_name) || s_equal("..", entry->d_name))
		{
			continue;
		}

		bassign(tmpfullpath, currentdir);
		bCatStatic(tmpfullpath, "/");
		bcatcstr(tmpfullpath, entry->d_name);
		if (entry->d_type == DT_LNK)
		{
			sv_log_writeFmt("skipping symlink, %s/%s", cstr(tmpfullpath));
			bstrListAppend(params->symlinks_skipped, tmpfullpath);
		}
		else if (entry->d_type == DT_DIR)
		{
			bstrListAppend(dirs, tmpfullpath);
			check(params->callback(params->context, tmpfullpath, UINT64_MAX, UINT64_MAX, UINT64_MAX));
		}
		else if (entry->d_type == DT_REG)
		{
			/* get file info*/
			struct stat64 st = { 0 };
			errno = 0;
			int statresult = stat64(cstr(tmpfullpath), &st);
			if (statresult < 0 && errno == ENOENT)
			{
				sv_log_writeFmt("interesting, not found during iteration %s", cstr(tmpfullpath));
			}
			else if (statresult < 0)
			{
				bformata(tmpfullpath, " stat failed, errno=%d", errno);
				bstrListAppend(params->dirs_not_accessible, tmpfullpath);
			}
			else
			{
				check(params->callback(params->context, tmpfullpath, st.st_mtime, cast64s64u(st.st_size), st.st_mode));
			}
		}
	}
cleanup:
	closedir(dir);
	return currenterr;
}

check_result os_recurse_impl(os_recurse_params* params, int currentdepth, bstring currentdir, bstring tmpfullpath)
{
	sv_result currenterr = {};
	sv_result attempt_dir = {};
	bstrList* dirs = bstrListCreate();
	for (int attempt = 0; attempt < params->max_tries; attempt++)
	{
		/* traverse just this directory */
		bool retriable_err = false;
		sv_result_close(&attempt_dir);
		attempt_dir = os_recurse_impl_dir(params, dirs, &retriable_err, currentdir, tmpfullpath);
		if (!attempt_dir.code || !retriable_err)
		{
			check(attempt_dir);
			break;
		}
		else
		{
			bstrListClear(dirs);
			os_sleep(cast32s32u(params->sleep_between_tries));
		}
	}

	if (attempt_dir.code)
	{
		/* still seeing a retriable_err after max_tries */
		bstrListAppend(params->dirs_not_accessible, attempt_dir.msg);
	}

	/* recurse into subdirectories */
	if (currentdepth < params->maxRecursionDepth)
	{
		for (uint32_t i = 0; i < dirs->qty; i++)
		{
			bstring dir = dirs->entry[i];
			check(os_recurse_impl(params, currentdepth + 1, dir, tmpfullpath));
		}
	}
	else if (params->maxRecursionDepth != 0)
	{
		check_b(0, "recursion limit reached in dir %s", cstr(currentdir));
	}

cleanup:
	bstrListDestroy(dirs);
	return currenterr;
}

check_result os_recurse(os_recurse_params* params)
{
	sv_result currenterr = {};
	bstring currentdir = bfromcstr(params->root);
	bstring tmpfullpath = bstring_open();
	params->max_tries = MAX(1, params->max_tries);
	check_b(os_isabspath(params->root), "expected full path but got %s", params->root);
	check(os_recurse_impl(params, 0, currentdir, tmpfullpath));

cleanup:
	bdestroy(currentdir);
	bdestroy(tmpfullpath);
	return currenterr;
}

check_result os_run_process(const char* path, const char* const args[], bool unused, bstring unuseds, bstring getoutput, int* retcode)
{
	/* note that by default all file descriptors are passed to child process. use flags like O_CLOEXEC to prevent this. */
	sv_result currenterr = {};
	const int READ = 0, WRITE = 1;
	int parentToChild[2] = {-1, -1};
	int childToParent[2] = {-1, -1};
	pid_t pid = 0;
	check_b(os_isabspath(path), "os_run_process needs full path but given %s.", path);
	check_b(os_file_exists(path), "os_run_process needs existing file but given %s.", path);
	check_errno(_, pipe(parentToChild));
	check_errno(_, pipe(childToParent));
	check_errno(int forkresult, fork());
	if (forkresult == 0)
	{
		/* child */
		check_errno(_, dup2(childToParent[WRITE], STDOUT_FILENO));
		check_errno(_, dup2(childToParent[WRITE], STDERR_FILENO));
		/* closing a descriptor in the child will close it for the child, but not the parent. 
		so close everything we don't need */
		close_set_invalid(childToParent[READ]);
		close_set_invalid(parentToChild[WRITE]);
		close_set_invalid(parentToChild[READ]);
		close(STDIN_FILENO);

		execv(path, (char * const*)args);
		exit(1); /* not reached, execv does not return */
	}
	else
	{
		/* parent */
		char buffer[4096] = "";
		close_set_invalid(parentToChild[READ]);
		close_set_invalid(parentToChild[WRITE]);
		close_set_invalid(childToParent[WRITE]);
		while (true)
		{
			log_errno(int readResult, (int)read(childToParent[READ], buffer, countof(buffer)));
			if (readResult <= 0)
			{
				/* don't exit on read() failure because some errno like EINTR and EAGAIN are fine */
				break;
			}
			else
			{
				bcatblk(getoutput, buffer, readResult);
			}
		}

		int status = -1;
		check_errno(_, waitpid(pid, &status, 0));
		*retcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}

cleanup:
	close_set_invalid(parentToChild[READ]);
	close_set_invalid(parentToChild[WRITE]);
	close_set_invalid(childToParent[READ]);
	close_set_invalid(childToParent[WRITE]);
	(void)unused;
	(void)unuseds;
	return currenterr;
}

os_perftimer os_perftimer_start()
{
	os_perftimer ret = {};
	time_t unixtime = time(NULL);
	ret.timestarted = unixtime;
	return ret;
}

double os_perftimer_read(const os_perftimer* timer)
{
	time_t unixtime = time(NULL);
	return (double)(unixtime - timer->timestarted);
}

void os_perftimer_close(os_perftimer* unused)
{
	(void)unused;
	/* no allocations, nothing needs to be done. */
}

void os_print_mem_usage()
{
	puts("Getting memory usage is not yet implemented.");
}

bstring parse_cmd_line_args(int argc, char** argv, bool* is_low)
{
	bstring ret = bstring_open();
	if (argc >= 3 && s_equal("-directory", argv[1]))
	{
		bassigncstr(ret, argv[2]);
		*is_low = (argc >= 4 && s_equal("-low", argv[3]));
	}
	else if (argc > 1)
	{
		printf("warning: unrecognized command-line syntax.");
	}
	return ret;
}

const bool islinux = true;

#elif _WIN32

#include <VersionHelpers.h>
const int maxUtf8BytesPerCodepoint = 4;

/* per-thread buffers for UTF8 to UTF16, so allocs aren't needed */
__declspec(thread) sv_wstr threadlocal1;
__declspec(thread) sv_wstr threadlocal2;
__declspec(thread) sv_wstr tls_contents;

void wide_to_utf8(const WCHAR* wide, bstring output)
{
	sv_result currenterr = {};

	/* reserve max space (4 bytes per codepoint) and resize later */
	int widelen = wcslen32s(wide);
	bstringAllocZeros(output, widelen * maxUtf8BytesPerCodepoint);
	SetLastError(0);
	int written = WideCharToMultiByte(CP_UTF8, 0, wide, widelen,
		(char*)output->data, widelen * maxUtf8BytesPerCodepoint, NULL, NULL);

	check_fatal(written > 0 || wide[0] == L'\0', 
		"toUtf8 failed on string %S lasterr=%lu", wide, GetLastError());
	output->slen = written;
}

void utf8_to_wide(const char* utf8, sv_wstr* output)
{
	sv_result currenterr = {};

	/* reserve size */
	int utf8len = strlen32s(utf8);
	sv_wstrClear(output);
	sv_wstrAllocZeros(output, utf8len);
	SetLastError(0);
	int written = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8len,
		(WCHAR*)output->arr.buffer, cast32u32s(output->arr.length) - 1 /*nul-term*/);

	check_fatal(written >= 0 && (written > 0 || utf8[0] == '\0'), 
		"toWide failed on string %s lasterr=%lu", utf8, GetLastError());
	sv_wstrTruncateLength(output, cast32s32u(written));
}

void os_init()
{
	const int initialbuffersize = 4096;
	threadlocal1 = sv_wstr_open(initialbuffersize);
	threadlocal2 = sv_wstr_open(initialbuffersize);
	tls_contents = sv_wstr_open(initialbuffersize);
	check_fatal(IsWindowsVistaOrGreater(),
		"This program does not support an OS earlier than Windows Vista.");
	SetConsoleOutputCP(CP_UTF8);
}

bool os_file_exists(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	log_win32(DWORD fileAttr, GetFileAttributesW(wcstr(threadlocal1)), INVALID_FILE_ATTRIBUTES, s);
	return fileAttr != INVALID_FILE_ATTRIBUTES && ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool os_dir_exists(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	log_win32(DWORD fileAttr, GetFileAttributesW(wcstr(threadlocal1)), INVALID_FILE_ATTRIBUTES, s);
	return fileAttr != INVALID_FILE_ATTRIBUTES && ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

uint64_t os_getfilesize(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	WIN32_FILE_ATTRIBUTE_DATA data = { 0 };
	log_win32(BOOL ret, GetFileAttributesEx(wcstr(threadlocal1), GetFileExInfoStandard, &data), FALSE, s);
	if (!ret)
	{
		return 0;
	}

	ULARGE_INTEGER size;
	size.HighPart = data.nFileSizeHigh;
	size.LowPart = data.nFileSizeLow;
	return size.QuadPart;
}

uint64_t os_getmodifiedtime(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	WIN32_FILE_ATTRIBUTE_DATA data = { 0 };
	log_win32(BOOL ret, GetFileAttributesEx(wcstr(threadlocal1), GetFileExInfoStandard, &data), FALSE, s);
	if (!ret)
	{
		return 0;
	}

	ULARGE_INTEGER time;
	time.HighPart = data.ftLastWriteTime.dwHighDateTime;
	time.LowPart = data.ftLastWriteTime.dwLowDateTime;
	return time.QuadPart;
}

bool os_setmodifiedtime_nearestsecond(const char* s, uint64_t t)
{
	bool ret = false;
	os_exclusivefilehandle h = {};
	bool filenotfound = false;
	sv_result res = os_exclusivefilehandle_open(&h, s, true, &filenotfound);
	if (res.code == 0 && !filenotfound && h.os_handle != INVALID_HANDLE_VALUE)
	{
		FILETIME ft = { lower32(t), upper32(t) };
		log_win32(ret, !!SetFileTime(h.os_handle, NULL, NULL, &ft), false, s);
	}

	sv_result_close(&res);
	return ret;
}

bool os_create_dir(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	log_win32(BOOL ret, CreateDirectoryW(wcstr(threadlocal1), NULL), FALSE, s);
	return ret || os_dir_exists(s);
}

bool os_copy(const char* s1, const char* s2, bool overwrite_ok)
{
	utf8_to_wide(s1, &threadlocal1);
	utf8_to_wide(s2, &threadlocal2);
	if (wcscmp(wcstr(threadlocal1), wcstr(threadlocal2)) == 0)
	{
		sv_log_writeFmt("skipping attempted copy of %s onto itself", s1);
		return true;
	}
	else
	{
		log_win32(BOOL ret, CopyFileW(wcstr(threadlocal1), wcstr(threadlocal2), !overwrite_ok), FALSE, s1, s2);
		return ret != FALSE;
	}
}

bool os_move(const char* s1, const char* s2, bool overwrite_ok)
{
	utf8_to_wide(s1, &threadlocal1);
	utf8_to_wide(s2, &threadlocal2);
	if (wcscmp(wcstr(threadlocal1), wcstr(threadlocal2)) == 0)
	{
		sv_log_writeFmt("skipping attempted move of %s onto itself", s1);
		return true;
	}
	else
	{
		DWORD flags = overwrite_ok ? MOVEFILE_REPLACE_EXISTING : 0UL;
		log_win32(BOOL ret, MoveFileExW(wcstr(threadlocal1), wcstr(threadlocal2), flags), FALSE, s1, s2);
		return ret != FALSE;
	}
}

check_result sv_file_open(sv_file* self, const char* path, const char* mode)
{
	sv_result currenterr = {};
	utf8_to_wide(path, &threadlocal1);
	utf8_to_wide(mode, &threadlocal2);
	errno_t err = _wfopen_s(&self->file, wcstr(threadlocal1), wcstr(threadlocal2));
	check_b(self->file, "failed to open path %s (%S), got %d", path, wcstr(threadlocal1), err);

cleanup:
	return currenterr;
}

check_result os_exclusivefilehandle_open(os_exclusivefilehandle* self, 
	const char* path, bool allowread, bool* filenotfound)
{
	sv_result currenterr = {};
	bstring tmp = bstring_open();
	set_self_zero();
	self->loggingcontext = bfromcstr(path);
	utf8_to_wide(path, &threadlocal1);

	SetLastError(0);
	DWORD sharing = allowread ? FILE_SHARE_READ : 0; /* allow others to read, but not write */
	self->os_handle = CreateFileW(wcstr(threadlocal1), GENERIC_READ, sharing,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (self->os_handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND && filenotfound)
	{
		*filenotfound = true;
	}
	else
	{
		char buf[BUFSIZ] = "";
		os_win32err_to_buffer(GetLastError(), buf, countof(buf));
		check_b(self->os_handle, "We could not exclusively open the file %s because of %s (%lu)",
			path, buf, GetLastError());
		check_errno(self->fd, _open_osfhandle((intptr_t)self->os_handle, O_RDONLY | O_BINARY), path);
	}

cleanup:
	bdestroy(tmp);
	return currenterr;
}

check_result os_exclusivefilehandle_stat(os_exclusivefilehandle* self, 
	uint64_t* size, uint64_t* modtime, uint64_t* permissions)
{
	sv_result currenterr = {};
	LARGE_INTEGER filesize = {};

	check_win32(BOOL ret, GetFileSizeEx(self->os_handle, &filesize), FALSE, cstr(self->loggingcontext));
	*size = cast64s64u(filesize.QuadPart);

	FILETIME ftCreate, ftAccess, ftWrite;
	check_win32(ret, GetFileTime(self->os_handle, &ftCreate, &ftAccess, &ftWrite), FALSE, cstr(self->loggingcontext));
	*modtime = make_uint64(ftWrite.dwHighDateTime, ftWrite.dwLowDateTime);
	*permissions = 0; /* on windows, we don't store file permissions */
cleanup:
	return currenterr;
}

bool os_setcwd(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	log_win32(BOOL ret, SetCurrentDirectoryW(wcstr(threadlocal1)), FALSE, s);
	return ret != FALSE;
}

bool os_isabspath(const char* s)
{
	return s && s[0] != '\0' && s[1] == ':' && s[2] == '\\';
}

bstring os_getthisprocessdir()
{
	bstring fullpath = bstring_open(), dir = bstring_open(), retvalue = bstring_open();
	WCHAR buffer[PATH_MAX] = L"";
	log_win32(DWORD chars, GetModuleFileNameW(nullptr, buffer, countof(buffer)), 0);
	if (chars > 0 && buffer[0])
	{
		wide_to_utf8(buffer, fullpath);
		os_get_parent(cstr(fullpath), dir);
		log_b(os_isabspath(cstr(dir)) && os_dir_exists(cstr(dir)), "path=%s", cstr(dir));
		if (os_isabspath(cstr(dir)) && os_dir_exists(cstr(dir)))
		{
			bassign(retvalue, dir);
		}
	}

	bdestroy(fullpath);
	bdestroy(dir);
	return retvalue;
}

bstring os_get_create_appdatadir()
{
	bstring ret = bstring_open();
	WCHAR buff[PATH_MAX] = L"";
	HRESULT hr = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, buff);
	log_b(hr == S_OK, "SHGetFolderPath returned %lu", hr);
	if (SUCCEEDED(hr))
	{
		wide_to_utf8(buff, ret);
		if (blength(ret) && os_isabspath(cstr(ret)))
		{
			bcatcstr(ret, "\\downpoured_glacial_backup");
			if (!os_create_dir(cstr(ret)))
			{
				/* indicate failure */
				bstrClear(ret);
			}
		}
	}

	return ret;
}

bool os_lock_file_to_detect_other_instances(const char* path, int* out_code)
{
	/* fopen_s, unlike fopen, defaults to taking an exclusive lock. */
	FILE* file = NULL;
	utf8_to_wide(path, &threadlocal1);
	errno_t ret = _wfopen_s(&file, wcstr(threadlocal1), L"wb");
	log_b(ret == 0, "path %s _wfopen_s returned %d", path, ret);

	/* tell the caller if we received an interesting errno. */
	if (ret != 0 && ret != EACCES)
	{
		*out_code = ret;
	}

	/* intentionally leak the file-handle... will be closed on process exit */
	return file == NULL;
}

void os_open_dir_ui(const char* dir)
{
	bool exists = os_dir_exists(dir);
	utf8_to_wide(dir, &threadlocal1);

	wprintf(L"The directory is %s %s\n", wcstr(threadlocal1), exists ? L"" : L"(not found)");
	if (exists)
	{
		WCHAR buff[PATH_MAX] = L"";
		if (GetWindowsDirectoryW(buff, _countof(buff)))
		{
			sv_wstrClear(&threadlocal2);
			sv_wstrAppend(&threadlocal2, buff);
			sv_wstrAppend(&threadlocal2, L"\\explorer.exe \"");
			sv_wstrAppend(&threadlocal2, wcstr(threadlocal1));
			sv_wstrAppend(&threadlocal2, L"\"");
			(void)_wsystem(wcstr(threadlocal2));
		}
	}
}

bool os_remove(const char* s)
{
	utf8_to_wide(s, &threadlocal1);
	log_win32(DWORD fileAttr, GetFileAttributesW(wcstr(threadlocal1)), INVALID_FILE_ATTRIBUTES, s);
	if (fileAttr != INVALID_FILE_ATTRIBUTES && ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0))
	{
		log_win32(_, RemoveDirectoryW(wcstr(threadlocal1)), FALSE, s);
	}
	else if (fileAttr != INVALID_FILE_ATTRIBUTES)
	{
		log_win32(_, DeleteFileW(wcstr(threadlocal1)), FALSE, s);
	}

	/* ok if s was previously deleted. */
	return !os_file_or_dir_exists(s);
}

void os_clr_console()
{
	(void)system("cls");
}

void os_clock_gettime(int64_t* seconds, int32_t* milliseconds)
{
	/* based on code by Asain Kujovic under the MIT license */
	FILETIME wintime;
	GetSystemTimeAsFileTime(&wintime);
	ULARGE_INTEGER time64;
	time64.HighPart = wintime.dwHighDateTime;
	time64.LowPart = wintime.dwLowDateTime;
	time64.QuadPart -= 116444736000000000i64;  /* 1jan1601 to 1jan1970 */
	*seconds = (int64_t)time64.QuadPart / 10000000i64;
	*milliseconds = (int32_t)((time64.QuadPart - (*seconds * 10000000i64)) / 10000);
}

void os_sleep(uint32_t milliseconds)
{
	Sleep(milliseconds);
}

void os_errno_to_buffer(int err, char *str, size_t str_len)
{
	(void)strerror_s(str, str_len - 1, err);
}

void os_win32err_to_buffer(unsigned long err, char* buf, size_t buflen)
{
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
		NULL, (DWORD)err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, cast64u32u(buflen), NULL);
}

void os_open_glacial_backup_website()
{
	int ret = system("start https://github.com/downpoured");
	sv_log_writeFmt("opening website. retcode=%d", ret);
}

byte* os_aligned_malloc(uint32_t size, uint32_t align)
{
	void *result = _aligned_malloc(size, align);
	check_fatal(result, "_aligned_malloc failed");
	return (byte*)result;
}

void os_aligned_free(byte** ptr)
{
	_aligned_free(*ptr);
	*ptr = NULL;
}

check_result os_binarypath(const char* binname, bstring out)
{
	sv_result currenterr = {};
	const wchar_t* path = _wgetenv(L"PATH");
	check_b(path && path[0] != L'\0', "Path environment variable not found.");
	sv_pseudosplit spl = sv_pseudosplit_open("");
	wide_to_utf8(path, spl.text);
	sv_pseudosplit_split(&spl, ';');
	check(os_binarypath_impl(&spl, binname, out));

cleanup:
	sv_pseudosplit_close(&spl);
	return currenterr;
}

bstring os_get_tmpdir(const char* subdirname)
{
	bstring ret = bstring_open();
	wchar_t* candidates[] = { L"TMPDIR", L"TMP", L"TEMP", L"TEMPDIR" };
	for (int i = 0; i < countof(candidates); i++)
	{
		const wchar_t* val = _wgetenv(candidates[i]);
		if (val)
		{
			wide_to_utf8(val, ret);
			log_b(os_isabspath(cstr(ret)) && os_dir_exists(cstr(ret)), "path=%s", cstr(ret));
			if (blength(ret) && os_isabspath(cstr(ret)) && os_dir_exists(cstr(ret)))
			{
				bformata(ret, "%s%s", pathsep, subdirname);
				if (os_create_dir(cstr(ret)))
				{
					return ret;
				}
			}
		}
	}

	bstrClear(ret);
	return ret;
}

void createsearchspec(sv_wstr* dir, sv_wstr* buffer)
{
	WCHAR lastCharOfPath = *(wpcstr(dir) + dir->arr.length - 2);
	sv_wstrClear(buffer);
	sv_wstrAppend(buffer, wpcstr(dir));
	if (lastCharOfPath == L'\\')
	{
		sv_wstrAppend(buffer, L"*"); /* "C:\" to "C:\*" */
	}
	else
	{
		sv_wstrAppend(buffer, L"\\*"); /* "C:\path" to "C:\path\*" */
	}
}

static check_result os_recurse_impl_dir(os_recurse_params* params, 
	sv_array* w_dirs, bool* retriable_err, sv_wstr* currentdir, sv_wstr* tmpfullpath, bstring tmpfullpath_utf8)
{
	sv_result currenterr = {};
	WIN32_FIND_DATAW findFileData = { 0 };
	*retriable_err = false;

	bstrClear(tmpfullpath_utf8);
	createsearchspec(currentdir, tmpfullpath);
	SetLastError(0);
	HANDLE hFind = FindFirstFileW(wpcstr(tmpfullpath), &findFileData);
	if (hFind == INVALID_HANDLE_VALUE && 
		GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_NO_MORE_FILES)
	{
		*retriable_err = true;
		char buf[BUFSIZ] = "";
		os_win32err_to_buffer(GetLastError(), buf, countof(buf));
		check_b(0, "Could not list \n%S\nbecause of code %s (%lu)",
			wpcstr(tmpfullpath), buf, GetLastError());
	}

	while (hFind != INVALID_HANDLE_VALUE)
	{
		if (wcscmp(L".", findFileData.cFileName) != 0 &&
			wcscmp(L"..", findFileData.cFileName) != 0)
		{
			/* get the full wide path */
			sv_wstrClear(tmpfullpath);
			sv_wstrAppend(tmpfullpath, wpcstr(currentdir));
			sv_wstrAppend(tmpfullpath, L"\\");
			sv_wstrAppend(tmpfullpath, findFileData.cFileName);
			wide_to_utf8(wpcstr(tmpfullpath), tmpfullpath_utf8);
		}

		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			sv_log_writeFmt("SKIPPED: symlink %s", cstr(tmpfullpath_utf8));
			bstrListAppend(params->symlinks_skipped, tmpfullpath_utf8);
		}
		else if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			wcscmp(L".", findFileData.cFileName) != 0 &&
			wcscmp(L"..", findFileData.cFileName) != 0)
		{
			/* add to a list of directories */
			sv_wstr dirpath = sv_wstr_open(PATH_MAX);
			sv_wstrAppend(&dirpath, wpcstr(tmpfullpath));
			sv_array_append(w_dirs, (const byte*)&dirpath, 1);
			check(params->callback(params->context, tmpfullpath_utf8, UINT64_MAX, UINT64_MAX, UINT64_MAX));
		}
		else if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			uint64_t modtime = make_uint64(findFileData.ftLastWriteTime.dwHighDateTime, 
				findFileData.ftLastWriteTime.dwLowDateTime);
			uint64_t filesize = make_uint64(findFileData.nFileSizeHigh, 
				findFileData.nFileSizeLow);

			check(params->callback(params->context, tmpfullpath_utf8, modtime, filesize, 0));
		}

		SetLastError(0);
		BOOL foundNextFile = FindNextFileW(hFind, &findFileData);
		if (!foundNextFile && GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_NO_MORE_FILES)
		{
			/* hFind is invalid after any failed FindNextFile; don't retry even for ERROR_ACCESS_DENIED */
			*retriable_err = true;
			char buf[BUFSIZ] = "";
			os_win32err_to_buffer(GetLastError(), buf, countof(buf));
			check_b(0, "Could not continue listing after \n%S\nbecause of code %s (%lu)",
				wpcstr(tmpfullpath), buf, GetLastError());
		}
		else if (!foundNextFile)
		{
			break;
		}
	}
cleanup:
	FindClose(hFind);
	return currenterr;
}

check_result os_recurse_impl(os_recurse_params* params, int currentdepth, 
	sv_wstr* currentdir, sv_wstr* tmpfullpath, bstring tmpfullpath_utf8)
{
	sv_result currenterr = {};
	sv_result attempt_dir = {};
	sv_array w_dirs = sv_array_open(sizeof32u(sv_wstr), 0);
	for (int attempt = 0; attempt < params->max_tries; attempt++)
	{
		/* traverse just this directory */
		bool retriable_err = false;
		sv_result_close(&attempt_dir);
		attempt_dir = os_recurse_impl_dir(params, &w_dirs, &retriable_err, currentdir, tmpfullpath, tmpfullpath_utf8);
		if (!attempt_dir.code || !retriable_err)
		{
			check(attempt_dir);
			break;
		}
		else
		{
			sv_wstrListClear(&w_dirs);
			os_sleep(params->sleep_between_tries);
		}
	}

	if (attempt_dir.code)
	{
		/* still seeing a retriable_err after max_tries */
		bstrListAppend(params->dirs_not_accessible, attempt_dir.msg);
	}

	/* recurse into subdirectories */
	if (currentdepth < params->maxRecursionDepth)
	{
		for (uint32_t i = 0; i < w_dirs.length; i++)
		{
			sv_wstr* dir = (sv_wstr*)sv_array_at(&w_dirs, i);
			check(os_recurse_impl(params, currentdepth + 1, dir, tmpfullpath, tmpfullpath_utf8));
		}
	}
	else if (params->maxRecursionDepth != 0)
	{
		check_b(0, "recursion limit reached in dir %S", wpcstr(currentdir));
	}

cleanup:
	sv_wstrListClear(&w_dirs);
	sv_array_close(&w_dirs);
	return currenterr;
}

check_result os_recurse(os_recurse_params* params)
{
	sv_result currenterr = {};
	sv_wstr currentdir = sv_wstr_open(PATH_MAX);
	sv_wstr tmpfullpath = sv_wstr_open(PATH_MAX);
	bstring tmpfullpath_utf8 = bstring_open();
	utf8_to_wide(params->root, &currentdir);
	params->max_tries = max(1, params->max_tries);
	check_b(os_isabspath(params->root), "expected full path but got %s", params->root);
	check(os_recurse_impl(params, 0, &currentdir, &tmpfullpath, tmpfullpath_utf8));

cleanup:
	sv_wstr_close(&currentdir);
	sv_wstr_close(&tmpfullpath);
	bdestroy(tmpfullpath_utf8);
	return currenterr;
}

check_result os_run_process_ex(const char* path, const char* const args[], 
	os_proc_op op, os_exclusivefilehandle* providestdin, bool fastjoinargs, 
	bstring useargscombined, bstring getoutput, int* retcode)
{
	sv_result currenterr = {};
	bstrClear(getoutput);
	HANDLE childstd_in_rd = NULL;
	HANDLE childstd_in_wr = NULL;
	HANDLE childstd_out_rd = NULL;
	HANDLE childstd_out_wr = NULL;
	PROCESS_INFORMATION procinfo = { 0 };
	STARTUPINFOW startinfo = { 0 };
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	check_b(os_isabspath(path), "os_run_process needs full path but given %s.", path);
	check_b(os_file_exists(path), "os_run_process needs existing file but given %s.", path);
	check_b((op == os_proc_stdin) == (providestdin != 0), "bad parameters.");

	argvquote(path, args, useargscombined, fastjoinargs);
	utf8_to_wide(cstr(useargscombined), &threadlocal1);
	startinfo.cb = sizeof(STARTUPINFO);

	/* 
	Don't use popen() or system(). Simpler, and does work, but slower (opens a shell process), 
	harder to run securely, will fail in gui contexts, sometimes needs all file buffers flushed, 
	and there are reports that repeated system() calls will alert a/v software
	
	Make sure we avoid deadlock.
	Deadlock example: we 1) redirect stdout and stdin 
	2) write to the stdin pipe and close it 
	3) read from the stdout pipe and close it
	This might appear to work, because of buffering, but consider a child that writes a lot of 
	data to stdout before it even listens to stdin.
	The child will fill up the buffer and then hang waiting for someone to listen to its stdout.
	*/
	check_win32(_, CreatePipe(&childstd_out_rd, &childstd_out_wr, &sa, 0), FALSE, cstr(useargscombined));
	check_win32(_, SetHandleInformation(childstd_out_rd, HANDLE_FLAG_INHERIT, 0), FALSE, cstr(useargscombined));
	check_win32(_, CreatePipe(&childstd_in_rd, &childstd_in_wr, &sa, 0), FALSE, cstr(useargscombined));
	check_win32(_, SetHandleInformation(childstd_in_wr, HANDLE_FLAG_INHERIT, 0), FALSE, cstr(useargscombined));

	/* If we had a child process that called CloseHandle on its stderr or stdout, we would use duplicatehandle. */
	startinfo.hStdError = op == os_proc_stdin ? INVALID_HANDLE_VALUE : childstd_out_wr;
	startinfo.hStdOutput = op == os_proc_stdin ? INVALID_HANDLE_VALUE : childstd_out_wr;
	startinfo.hStdInput = op == os_proc_stdin ? childstd_in_rd : INVALID_HANDLE_VALUE;
	startinfo.dwFlags |= STARTF_USESTDHANDLES;
	
	check_win32(_, CreateProcessW(NULL,
		(WCHAR*)wcstr(threadlocal1), /* nb: CreateProcess needs a writable buffer */
		NULL,          /* process security attributes */
		NULL,          /* primary thread security attributes */
		TRUE,          /* handles are inherited */
		0,             /* creation flags */
		NULL,          /* use parent's environment */
		NULL,          /* use parent's current directory */
		&startinfo,
		&procinfo),
		FALSE, cstr(useargscombined));

	char buf[4096] = "";
	CloseHandleNull(&childstd_out_wr);
	CloseHandleNull(&childstd_in_rd);
	if (op == os_proc_stdin)
	{
		LARGE_INTEGER location = { 0 };
		check_b(providestdin->os_handle > 0, "invalid file handle %s", cstr(providestdin->loggingcontext));
		check_win32(_, SetFilePointerEx(providestdin->os_handle, location, NULL, FILE_BEGIN), 
			FALSE, cstr(providestdin->loggingcontext));

		while (true)
		{
			DWORD dwRead = 0;
			BOOL bSuccess = ReadFile(providestdin->os_handle, buf, countof(buf), &dwRead, NULL);
			if (!bSuccess || dwRead == 0)
			{
				break;
			}

			DWORD dwWritten = 0;
			log_win32(bSuccess, WriteFile(childstd_in_wr, buf, dwRead, &dwWritten, NULL), 
				FALSE, cstr(providestdin->loggingcontext));

			check_b(dwRead == dwWritten, "%s wanted to write %d but only wrote %d",
				cstr(providestdin->loggingcontext), dwRead, dwWritten);

			if (!bSuccess)
			{
				break;
			}
		}
	}
	else if (op == os_proc_stdout_stderr)
	{
		while (true)
		{
			SetLastError(0);
			DWORD dwRead = 0;
			BOOL ret = ReadFile(childstd_out_rd, buf, countof(buf), &dwRead, NULL);
			log_b(ret || GetLastError() == 0 || GetLastError() == ERROR_BROKEN_PIPE, "lasterr=%lu", GetLastError());
			if (!ret || dwRead == 0)
			{
				break;
			}
			
			bcatblk(getoutput, buf, dwRead);
		}
	}
	else
	{
		check_b(0, "not reached");
	}

	CloseHandleNull(&childstd_in_wr);
	log_win32(DWORD result, WaitForSingleObject(procinfo.hProcess, INFINITE), WAIT_FAILED, cstr(useargscombined));
	log_b(result == WAIT_OBJECT_0, "got wait result %lu", result);

	DWORD dwret = 1;
	log_win32(_, GetExitCodeProcess(procinfo.hProcess, &dwret), FALSE, cstr(useargscombined));
	log_b(*retcode == 0, "cmd=%s ret=%d", cstr(useargscombined), *retcode);
	*retcode = (int)dwret;

cleanup:
	CloseHandleNull(&procinfo.hProcess);
	CloseHandleNull(&procinfo.hThread);
	CloseHandleNull(&childstd_in_rd);
	CloseHandleNull(&childstd_in_wr);
	CloseHandleNull(&childstd_out_rd);
	CloseHandleNull(&childstd_out_wr);
	return currenterr;
}

check_result os_run_process(const char* path, const char* const args[], 
	bool fastjoinargs, bstring useargscombined, bstring getoutput, int* retcode)
{
	return os_run_process_ex(path, args, os_proc_stdout_stderr, NULL, 
		fastjoinargs, useargscombined, getoutput, retcode);
}

os_perftimer os_perftimer_start()
{
	os_perftimer ret = {};
	LARGE_INTEGER time = { 0 };
	QueryPerformanceCounter(&time);
	ret.timestarted = time.QuadPart;
	return ret;
}

double os_perftimer_read(const os_perftimer* timer)
{
	LARGE_INTEGER time = { 0 };
	QueryPerformanceCounter(&time);
	int64_t diff = time.QuadPart - timer->timestarted;
	LARGE_INTEGER freq = { 0 };
	QueryPerformanceFrequency(&freq);
	return (diff) / ((double)freq.QuadPart);
}

void os_perftimer_close(os_perftimer*)
{
	/* no allocations, nothing needs to be done. */
}

void os_print_mem_usage()
{
	PROCESS_MEMORY_COUNTERS_EX obj = {};
	obj.cb = sizeof(obj);
	log_win32(BOOL ret, GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&obj, obj.cb), FALSE);
	if (ret)
	{
		printf("\tPageFaultCount: %lu\n", obj.PageFaultCount);
		printf("\tPeakWorkingSetSize:(%fMb)\n", obj.PeakWorkingSetSize / (1024.0*1024.0));
		printf("\tWorkingSetSize: (%fMb)\n", obj.WorkingSetSize / (1024.0*1024.0));
		printf("\tPagefileUsage: (%fMb)\n", obj.PagefileUsage / (1024.0*1024.0));
		printf("\tPeakPagefileUsage: (%fMb)\n", obj.PeakPagefileUsage / (1024.0*1024.0));
		printf("\tPrivateUsage: (%fMb)\n", obj.PrivateUsage / (1024.0*1024.0));
	}
}

check_result os_run_process_as(PROCESS_INFORMATION* outinfo, WCHAR* cmd, const WCHAR* user, const WCHAR* pass)
{
	sv_result currenterr = {};
	memset(outinfo, 0, sizeof(*outinfo));

	/* We'll use CreateProcessWithLogonW instead of LogonUser, according to MSDN
	it needs fewer privileges and so is likely to work on more configs. */
	STARTUPINFOW siStartInfo = { 0 };
	siStartInfo.cb = sizeof(STARTUPINFOW);
	check_win32(_, CreateProcessWithLogonW(user, NULL /* domain */, pass, 
		0, NULL, cmd, 0, 0, 0, &siStartInfo, outinfo), FALSE);

cleanup:
	return currenterr;
}

check_result os_restart_as_other_user(const char* data_dir)
{
	sv_result currenterr = {};
	bstring othername = bstring_open();
	bstring otherpass = bstring_open();
	sv_wstr wothername = sv_wstr_open(0);
	sv_wstr wotherpass = sv_wstr_open(0);
	PROCESS_INFORMATION piProcInfo = { 0 };
	sv_result res = {};
	WCHAR modulepath[PATH_MAX] = L"";
	check_win32(_, GetModuleFileNameW(nullptr, modulepath, countof(modulepath)), 0, 
		"Could not get path to glacial_backup.exe");
	check_win32(_, GetFileAttributesW(modulepath), INVALID_FILE_ATTRIBUTES, 
		"Could not get path to glacial_backup.exe");

	check_b(os_dir_exists(data_dir), "not found %s", data_dir);

	/* get utf16 version of data_dir */
	utf8_to_wide(data_dir, &threadlocal1);

	/* build arguments in tls_path2 */
	sv_wstrClear(&threadlocal2);
	sv_wstrAppend(&threadlocal2, modulepath);
	sv_wstrAppend(&threadlocal2, L" -directory \"");
	sv_wstrAppend(&threadlocal2, wcstr(threadlocal1));
	sv_wstrAppend(&threadlocal2, L"\" -low");

	/* CreateProcess takes a writable buffer, luckily we already have one. */
	WCHAR* command = (WCHAR*)wcstr(threadlocal2);
	while (true)
	{
		ask_user_prompt("Please enter the username, or 'q' to cancel.", false, othername);
		if (s_equal(cstr(othername), "q"))
		{
			break;
		}
		ask_user_prompt("Please enter the password, or 'q' to cancel.", false, otherpass);
		if (s_equal(cstr(otherpass), "q"))
		{
			break;
		}

		utf8_to_wide(cstr(othername), &wothername);
		utf8_to_wide(cstr(otherpass), &wotherpass);
		sv_result_close(&res);
		res = os_run_process_as(&piProcInfo, command, wcstr(wothername), wcstr(wotherpass));
		if (res.code)
		{
			printf("Could not start process, %s.\n", cstr(res.msg));
			if (!ask_user_yesno("Try again?"))
			{
				break;
			}
		}
		else
		{
			alertdialog("Started other process. This process will now exit.");
			exit(0);
		}
	}

cleanup:
	CloseHandleNull(&piProcInfo.hProcess);
	CloseHandleNull(&piProcInfo.hThread);
	sv_result_close(&res);
	memzero_s(otherpass->data, cast32s32u(max(0, otherpass->slen)));
	memzero_s(wotherpass.arr.buffer, wotherpass.arr.elementsize * wotherpass.arr.length);
	bdestroy(othername);
	bdestroy(otherpass);
	sv_wstr_close(&wothername);
	sv_wstr_close(&wotherpass);
	return currenterr;
}

bstring parse_cmd_line_args(int argc, wchar_t* argv[], bool* is_low)
{
	bstring ret = bstring_open();
	if (argc >= 3 && wcscmp(L"-directory", argv[1]) == 0)
	{
		wide_to_utf8(argv[2], ret);
		*is_low = (argc >= 4 && wcscmp(L"-low", argv[3]) == 0);
	}
	else if (argc > 1)
	{
		printf("warning: unrecognized command-line syntax.");
	}
	return ret;
}

const bool islinux = false;

#else
#error "platform not yet supported"
#endif

void sv_file_close(sv_file* self)
{
	if (self && self->file)
	{
		fclose(self->file);
		set_self_zero();
	}
}

check_result sv_file_writefile(const char* filename,
	const char* contents, const char* mode)
{
	sv_result currenterr = {};
	sv_file file = {};
	check(sv_file_open(&file, filename, mode));
	fputs(contents, file.file);

cleanup:
	sv_file_close(&file);
	return currenterr;
}

check_result sv_file_readfile(const char* filename, bstring contents)
{
	sv_result currenterr = {};
	sv_file file = {};

	/* get length of file*/
	uint64_t filesize = os_getfilesize(filename);
	check_b(filesize < 10 * 1024 * 1024, "file %s is too large to be read into a string", filename);

	/* allocate mem */
	bstrClear(contents);
	checkstr(bstringAllocZeros(contents, cast64u32s(filesize)));

	/* copy the data. don't support 'text' mode because the length might differ */
	check(sv_file_open(&file, filename, "rb"));
	size_t read = fread(contents->data, 1, (size_t)filesize, file.file);
	contents->slen = cast64u32s(filesize);
	check_b(filesize == 0 || read != 0, "fread failed");

cleanup:
	sv_file_close(&file);
	return currenterr;
}

void os_fd_close(int* fd)
{
	if (*fd && *fd != -1)
	{
		log_errno(_, close(*fd), NULL);
		*fd = 0;
	}
}

check_result os_exclusivefilehandle_tryuntil_open(os_exclusivefilehandle* self, 
	const char* path, bool allowread, bool* filenotfound)
{
	sv_result res = { 0, 1 };
	for (int attempt = 0; res.code && attempt < max_tries; attempt++)
	{
		/* we're trying to open an exclusive handle, so it might take a few attempts */
		sv_result_close(&res);
		res = os_exclusivefilehandle_open(self, path, allowread, filenotfound);
		if (res.code)
		{
			os_sleep(sleep_between_tries);
		}
	}

	return res;
}

void os_exclusivefilehandle_close(os_exclusivefilehandle* self)
{
	if (self->fd && self->fd != -1)
	{
		/* According to msdn, don't call CloseHandle on os_handle, closing fd is sufficient. */
		os_fd_close(&self->fd);
		bdestroy(self->loggingcontext);
		set_self_zero();
	}
}

bool os_tryuntil_remove(const char* s)
{
	for (int attempt = 0; attempt < max_tries; attempt++)
	{
		if (os_remove(s))
		{
			return true;
		}

		os_sleep(sleep_between_tries);
	}
	return false;
}

bool os_tryuntil_move(const char* s1, const char* s2, bool overwrite_ok)
{
	for (int attempt = 0; attempt < max_tries; attempt++)
	{
		if (os_move(s1, s2, overwrite_ok))
		{
			return true;
		}

		os_sleep(sleep_between_tries);
	}
	return false;
}

bool os_tryuntil_copy(const char* s1, const char* s2, bool overwrite_ok)
{
	for (int attempt = 0; attempt < max_tries; attempt++)
	{
		if (os_copy(s1, s2, overwrite_ok))
		{
			return true;
		}

		os_sleep(sleep_between_tries);
	}
	return false;
}

bool os_create_dirs(const char* s)
{
	if (!os_isabspath(s))
	{
		return false;
	}

	if (os_dir_exists(s) || os_create_dir(s))
	{
		return true;
	}

	bstrList* list = bstrListCreate();
	bstring parent = bfromcstr(s);
	while (s_contains(cstr(parent), pathsep))
	{
		bstrListAppend(list, parent);
		os_get_parent(bstrListViewAt(list, list->qty - 1), parent);
	}

	for (int i = list->qty - 1; i >= 0; i--)
	{
		if (!os_create_dir(bstrListViewAt(list, i)))
		{
			return false;
		}
	}

	bstrListDestroy(list);
	bdestroy(parent);
	return os_dir_exists(s);
}

/* on failure, places stderr into getoutput */
int os_tryuntil_run_process(const char* path, const char* const args[], bool fastjoinargs, 
	bstring useargscombined, bstring getoutput)
{
	int retcode = INT_MAX;
	for (int attempt = 0; attempt < max_tries; attempt++)
	{
		sv_result res = os_run_process(path, args, fastjoinargs, useargscombined, getoutput, &retcode);
		
		if (res.code || retcode)
		{
			bconcat(getoutput, res.msg);
			sv_result_close(&res);
			os_sleep(sleep_between_tries);
			retcode = retcode ? retcode : -1;
		}
		else
		{
			break;
		}
	}

	return retcode;
}

bool os_file_or_dir_exists(const char* filename)
{
	return os_file_exists(filename) || os_dir_exists(filename);
}

void os_get_filename(const char* fullpath, bstring filename)
{
	const char* lastSlash = strrchr(fullpath, pathsep[0]);
	if (lastSlash)
	{
		bassigncstr(filename, lastSlash + 1);
	}
	else
	{
		bassigncstr(filename, fullpath);
	}
}

void os_get_parent(const char* fullpath, bstring dir)
{
	const char* lastSlash = strrchr(fullpath, pathsep[0]);
	if (lastSlash)
	{
		bassignblk(dir, fullpath, cast64s32s(lastSlash - fullpath));
	}
	else
	{
		/* we were given a leaf name, so set dir to the empty string. */
		bstrClear(dir);
	}
}

void os_split_dir(const char* fullpath, bstring dir, bstring filename)
{
	os_get_parent(fullpath, dir);
	os_get_filename(fullpath, filename);
}

bool os_recurse_is_dir(uint64_t modtime, uint64_t filesize)
{
	return modtime == UINT64_MAX && filesize == UINT64_MAX;
}

bool os_dir_empty(const char* dir)
{
	/* returns false if directory could not be accessed. */
	bstrList* list = bstrListCreate();
	sv_result resultListFiles = os_listfiles(dir, list, false);
	int countFiles = list->qty;
	sv_result resultListDirs = os_listdirs(dir, list, false);
	int countDirs = list->qty;
	bool ret = resultListFiles.code == 0 && resultListDirs.code == 0 && countFiles + countDirs == 0;
	sv_result_close(&resultListFiles);
	sv_result_close(&resultListDirs);
	bstrListDestroy(list);
	return ret;
}

bool os_is_dir_writable(const char* dir)
{
	bool ret = true;
	bstring canary = bformat("%s%stestwrite.tmp", dir, pathsep);
	sv_result result = sv_file_writefile(cstr(canary), "test", "wb");
	if (result.code)
	{
		sv_result_close(&result);
		ret = false;
	}

	os_tryuntil_remove(cstr(canary));
	bdestroy(canary);
	return ret;
}

bool os_issubdirof(const char* s1, const char* s2)
{
	bstring s1slash = bformat("%s%s", s1, pathsep);
	bstring s2slash = bformat("%s%s", s2, pathsep);
	bool ret = s_startswithlen(cstr(s2slash), blength(s2slash), cstr(s1slash), blength(s1slash));
	bdestroy(s1slash);
	bdestroy(s2slash);
	return ret;
}

static check_result os_listdirs_callback(void* context,
	const bstring filepath, uint64_t modtime, uint64_t filesize, uint64_t unused)
{
	bstrList* list = (bstrList*)context;
	if (os_recurse_is_dir(modtime, filesize))
	{
		bstrListAppend(list, filepath);
	}
	
	(void)unused;
	return OK;
}

static check_result os_listfiles_callback(void* context,
	const bstring filepath, uint64_t modtime, uint64_t filesize, uint64_t unused)
{
	bstrList* list = (bstrList*)context;
	if (!os_recurse_is_dir(modtime, filesize))
	{
		bstrListAppend(list, filepath);
	}
	
	(void)unused;
	return OK;
}

check_result os_listfilesordirs(const char* dir, bstrList* list, bool sorted, bool filesordirs)
{
	sv_result currenterr = {};
	bstrListClear(list);
	os_recurse_params params = { (void*)list, dir,
		filesordirs ? &os_listfiles_callback : &os_listdirs_callback, 0 /* max depth */,
		NULL, NULL, max_tries, cast32u32s(sleep_between_tries) };
	check(os_recurse(&params));

	if (sorted)
		bstrListSort(list);

cleanup:
	return currenterr;
}

check_result os_listfiles(const char* dir, bstrList* list, bool sorted)
{
	sv_result currenterr = {};
	check_b(os_dir_exists(dir), "path=%s", dir);
	check(os_listfilesordirs(dir, list, sorted, true));
cleanup:
	return currenterr;
}

check_result os_listdirs(const char* dir, bstrList* list, bool sorted)
{
	sv_result currenterr = {};
	check_b(os_dir_exists(dir), "path=%s", dir);
	check(os_listfilesordirs(dir, list, sorted, false));
cleanup:
	return currenterr;
}

check_result os_tryuntil_deletefiles(const char* dir, const char* filenamepattern)
{
	sv_result currenterr = {};
	bstring filenameonly = bstring_open();
	bstrList* files_seen = bstrListCreate();
	check_b(os_isabspath(dir), "path=%s", dir);
	if (os_dir_exists(dir))
	{
		check(os_listfiles(dir, files_seen, false));
		for (int i = 0; i < files_seen->qty; i++)
		{
			os_get_filename(bstrListViewAt(files_seen, i), filenameonly);
			if (fnmatch_simple(filenamepattern, cstr(filenameonly)))
			{
				check_b(os_tryuntil_remove(bstrListViewAt(files_seen, i)),
					"failed to delete file %s", bstrListViewAt(files_seen, i));
			}
		}
	}

cleanup:
	bdestroy(filenameonly);
	bstrListDestroy(files_seen);
	return currenterr;
}

check_result os_tryuntil_movebypattern(const char* dir, const char* namepattern, 
	const char* destdir, bool canoverwrite, int* moved)
{
	sv_result currenterr = {};
	bstring nameonly = bstring_open();
	bstring destpath = bstring_open();
	bstring movefailed = bstring_open();
	bstrList* files_seen = bstrListCreate();
	check(os_listfiles(dir, files_seen, false));
	for (int i = 0; i < files_seen->qty; i++)
	{
		os_get_filename(bstrListViewAt(files_seen, i), nameonly);
		if (fnmatch_simple(namepattern, cstr(nameonly)))
		{
			bassignformat(destpath, "%s%s%s", destdir, pathsep, cstr(nameonly));
			if (os_tryuntil_move(bstrListViewAt(files_seen, i), cstr(destpath), canoverwrite))
			{
				(*moved)++;
			}
			else
			{
				bformata(movefailed, "Move from %s to %s failed ", 
					bstrListViewAt(files_seen, i), cstr(destpath));
			}
		}
	}

	check_b(blength(movefailed) == 0, "%s", cstr(movefailed));

cleanup:
	bdestroy(nameonly);
	bdestroy(destpath);
	bdestroy(movefailed);
	bstrListDestroy(files_seen);
	return currenterr;
}

check_result os_findlastfilewithextension(const char* dir, const char* extension, bstring path)
{
	sv_result currenterr = {};
	bstrClear(path);
	bstrList* list = bstrListCreate();
	check(os_listfiles(dir, list, true));
	for (int i = list->qty - 1; i >= 0; i--)
	{
		if (s_endswith(bstrListViewAt(list, i), extension))
		{
			bassigncstr(path, bstrListViewAt(list, i));
			break;
		}
	}
cleanup:
	bstrListDestroy(list);
	return currenterr;
}

bstring os_make_subdir(const char* parent, const char* leaf)
{
	bstring ret = bstring_open();
	bassignformat(ret, "%s%s%s", parent, pathsep, leaf);
	if (os_create_dir(cstr(ret)))
	{
		return ret;
	}
	else
	{
		bstrClear(ret);
		return ret;
	}
}

check_result os_binarypath_impl(sv_pseudosplit* spl, const char* binname, bstring out)
{
	for (uint32_t i = 0; i < spl->splitpoints.length; i++)
	{
		const char* dir = sv_pseudosplit_viewat(spl, i);
		bassignformat(out, "%s%s%s", dir, pathsep, binname);
		if (os_file_exists(cstr(out)))
		{
			return OK;
		}
	}

	bstrClear(out);
	return OK;
}


// Daniel Colascione, blogs.msdn.microsoft.com
void argvquoteone(const char* arg, bstring result)
{
	bconchar(result, '"');
	const char* argend = arg + strlen(arg);
	for (const char* it = arg; ; it++)
	{
		int countBackslashes = 0;
		while (it != argend && *it == L'\\') {
			it++;
			countBackslashes++;
		}

		if (it == argend) {
			/* Escape all backslashes, but let the terminating
			double quotation mark we add below be interpreted
			as a metacharacter. */
			for (int j = 0; j < countBackslashes * 2; j++)
			{
				bconchar(result, '\\');
			}

			break;
		}
		else if (*it == L'"') {
			/* Escape all backslashes and the following
			double quotation mark. */
			for (int j = 0; j < countBackslashes * 2 + 1; j++)
			{
				bconchar(result, '\\');
			}

			bconchar(result, *it);
		}
		else {
			/* Backslashes aren't special here. */
			for (int j = 0; j < countBackslashes; j++)
			{
				bconchar(result, '\\');
			}

			bconchar(result, *it);
		}
	}
	bconchar(result, '"');
}

void argvquote(const char* path, const char* const args[] /* NULL-terminated */, bstring result, bool fast)
{
	bassigncstr(result, "\"");
	bcatcstr(result, path);
	bconchar(result, '"');
	while (args && *args)
	{
		bconchar(result, ' ');
		if (fast)
		{
			bconchar(result, '"');
			bcatcstr(result, *args);
			bconchar(result, '"');
		}
		else
		{
			argvquoteone(*args, result);
		}
		args++;
	}
}
