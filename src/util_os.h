#pragma once

/* a few file utils */
CHECK_RESULT Result bcfile_writefile(const char* filename,
	const char* s, const char* mode)
{
	Result currenterr = {};
	bcfile file = {};
	check(bcfile_open(&file, filename, mode));
	fputs(s, file.file);

cleanup:
	bcfile_close(&file);
	return currenterr;
}

CHECK_RESULT Result bcfile_readfile(const char* filename,
	bcstr* out, const char* mode)
{
	Result currenterr = {};
	bcfile file = {};

	/* get length of file*/
	check(bcfile_open(&file, filename, mode));
	check_b(fseek(file.file, 0, SEEK_END) == 0, "fseek failed");
	long length_of_file = ftell(file.file);
	check_b(length_of_file > 0, "ftell failed");

	/* allocate mem */
	bcstr_data_settozeros(out, length_of_file);

	/* go back to beginning of file and copy the data*/
	check_b(fseek(file.file, 0, SEEK_SET) == 0, "fseek failed");
	size_t read = fread(bcstr_data_get(out), 1, length_of_file, file.file);
	check_b(read != 0, "fread failed");

cleanup:
	bcfile_close(&file);
	return currenterr;
}

Uint64 os_unixtime()
{
	time_t result = time(NULL);
	return (Uint64)result;
}

bool os_file_exists(const char* szFilename)
{
	struct stat st = {};
	return (stat(szFilename, &st) == 0 && (st.st_mode & S_IFDIR) == 0);
}

bool os_dir_exists(const char* szFilename)
{
	struct stat st = {};
	return (stat(szFilename, &st) == 0 && (st.st_mode & S_IFDIR) != 0);
}

/* does not allocate memory, instead modifies fullpath. */
void os_split_dir(char* fullpath, char** dir, char** filename)
{
	char* lastSlash = strrchr(fullpath, pathsep[0]);
	if (lastSlash)
	{
		/* putting a nul-term in the middle effectively splits string in two*/
		*lastSlash = '\0';
		*dir = fullpath;
		*filename = lastSlash + 1;
	}
	else
	{
		/* we were given a leaf name, so set dir to the empty string. */
		*dir = fullpath + strlen(fullpath);
		*filename = fullpath;
	}
}

void os_split_dir_len(char* fullpath, Uint32 fullpathlen, char** dir, Uint32* dirlen, char** filename, Uint32* filenamelen)
{
	os_split_dir(fullpath, dir, filename);
	*filenamelen = safecast32u((fullpath+fullpathlen) - *filename);
	*dirlen = fullpathlen - *filenamelen - 1 /*since we replaced the / with nul*/;
}

void os_split_getfilename(const char* fullpath, Uint32 fullpathlen, const char** name, Uint32* namelen)
{
	const char* lastSlash = strrchr(fullpath, pathsep[0]);
	if (lastSlash)
	{
		*name = lastSlash + 1;
		*namelen = safecast32u((fullpath + fullpathlen) - *name);
	}
	else
	{
		/* we were given a leaf name */
		*name = fullpath;
		*namelen = fullpathlen;
	}
}

void alertdialog(const char* message)
{
	fprintf(stderr, "%s\nPress any key to continue...\n", message);
	if (!g_log.suppress_dialogs)
	{
		getchar();
	}
}

typedef Result(*FnOSRecurseCallback)(void* context,
	int dirnumber, /* tells the recipient if directory has changed */
	const char* filepath, Uint32 filepathlen,
	const byte* nativepath, Uint32 cbnativepathlen,
	Uint64 modtime, Uint64 filesize);

#if __linux__

#define MAKEKEY32 __bswap_32
#define MAKEKEY64 __bswap_64
#define QSORTR_ARGS(paramCmp1, paramCmp2, paramContext) paramCmp1, paramCmp2, paramContext

bool os_create_dir(const char* s)
{
	int result = mkdir(s, 0777 /*permissions*/);
	return (result == 0 || errno == EEXIST);
}

bool os_copy(const char* s1, const char* s2, bool overwrite_ok)
{
	bool ret = false;
	struct stat st;
	off_t offset = 0;
	int write_fd = 0;

	/* Open the input file and get its permissions. */
	int read_fd = open(s1, O_RDONLY);
	if (read_fd && fstat(read_fd, &st) == 0)
	{
		int wflags = overwrite_ok ? (O_WRONLY | O_CREAT) : (O_WRONLY | O_CREAT | O_EXCL);
		int write_fd = open(s2, wflags, st.st_mode);
		if (write_fd)
		{
			ssize_t bytes = sendfile(write_fd, read_fd, &offset, st.st_size);
			ret = bytes >= 0;
		}
	}

	if (read_fd) close(read_fd);
	if (write_fd) close(write_fd);
	return ret;
}

CHECK_RESULT Result bcfile_opennative(bcfile* self, const byte* nativepath, const char* mode)
{
	return bcfile_open(self, (const char*)nativepath, mode);
}

bool os_isabspath(const char* s)
{
	return s && s[0] == '/';
}

bcstr os_getthisprocessdir()
{
	char buffer[PATH_MAX] = "";
	int len = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
	if (len != -1)
	{
		buffer[len] = '\0';
		char *dir=NULL, *filename=NULL;
		os_split_dir(buffer, &dir, &filename);
		return bcstr_openwith(dir);
	}
	else
	{
		return bcstr_openwith("");
	}
}

bcstr os_get_create_appdatadir()
{
	bcstr ret = bcstr_open();
	char* confighome = getenv("XDG_CONFIG_HOME");
	if (confighome)
	{
		bcstr_append(&ret, confighome, "/glacial_backup");
		if (os_create_dir(ret.s))
			return ret;
	}
	
	char* home = getenv("HOME");
	if (home)
	{
		bcstr_clear(&ret);
		bcstr_append(&ret, home, "/.local/share/glacial_backup");
		if (os_create_dir(ret.s))
			return ret;
		
		bcstr_clear(&ret);
		bcstr_append(&ret, home, "/glacial_backup");
		if (os_create_dir(ret.s))
			return ret;
	}
	
	/* indicate failure*/
	bcstr_clear(&ret);
	return ret;
}

bool os_are_other_instances_running(const char* path, int* out_code)
{
	int pid_file = open(path, O_CREAT | O_RDWR, 0666);
	if (!pid_file)
	{
		*out_code = 1;
		return false;
	}

	int rc = flock(pid_file, LOCK_EX | LOCK_NB);
	if (rc != 0 && errno != EWOULDBLOCK)
	{
		*out_code = (int)errno;
	}

	/* intentionally leak the lock... will be closed on process exit */
	return rc != 0;
}

bool os_open_text_editor(const char* filename)
{
	puts("Opening the configuration file in nano...");
	puts("Exit when your changes have been made.");
	if (s_contains(filename, "-") || s_contains(filename, " ")|| s_contains(filename, "\\"))
	{
		return false;
	}
	else
	{
		bcstr strcmd = bcstr_openwith("nano \"", filename, "\"");
		ignore_unused_result(system(strcmd.s));
		bcstr_close(&strcmd);
		return true;
	}
}

void os_open_dir_ui(const char* dir)
{
	printf("The directory is %s\n", dir);
	bcstr s = bcstr_openwith("xdg-open \"", dir, "\"");
	ignore_unused_result(system(s.s));
	bcstr_close(&s);
}

bool os_remove(const char* s)
{
	// removes file or empty directory. ok if s was previously deleted.
	remove(s);
	struct stat st = {0};
	return stat(s, &st) != 0;
}
void clearscreen()
{
	ignore_unused_result(system("clear"));
}

CHECK_RESULT Result os_recurse_impl(void* context, int depthcountdown, int dirnumber, 
	int* symlinksskipped, bcstr* currentdir, bcstr* tmpfullpath, bcstr* tmpnativepath, FnOSRecurseCallback callback)
{
	Result currenterr = {};
	bcstrlist dirs = bcstrlist_open();
	DIR *dir = opendir(currentdir->s);
	check_b(dir!=NULL, "could not list directory ", currentdir->s);

	struct dirent *entry = NULL;
	while ((entry = readdir(dir)) != NULL)
	{
		if (s_equal(".", entry->d_name)||s_equal("..", entry->d_name))
		{
			continue;
		}
		else if (entry->d_type == DT_LNK)
		{
			bc_log_writef(&g_log, "skipping symlink, %s/%s", currentdir->s, entry->d_name);
			(*symlinksskipped)++;
		}
		else if (entry->d_type == DT_DIR)
		{
			/* add to list, and concatenate to get full path*/
			bcstrlist_pushbackszlen(&dirs, currentdir->s, bcstr_length(currentdir));
			bcstr* pstr = bcstrlist_viewstr(&dirs, dirs.length-1);
			bcstr_addchar(pstr, '/');
			bcstr_appendszlen(pstr, entry->d_name, strlen(entry->d_name));
		}
		else if (entry->d_type == DT_REG)
		{
			/* concatenate to get full path*/
			bcstr_clear(tmpfullpath);
			bcstr_appendszlen(tmpfullpath, currentdir->s, bcstr_length(currentdir));
			bcstr_addchar(tmpfullpath, '/');
			bcstr_appendszlen(tmpfullpath, entry->d_name, strlen(entry->d_name));

			/* in posix, nativepath is just a copy of the path */
			bcstr_clear(tmpnativepath);
			bcstr_appendszlen(tmpnativepath, tmpfullpath->s, bcstr_length(tmpfullpath));

			/* get file info*/
			struct stat64 st = {0};
			int statresult = stat64(tmpfullpath->s, &st);
			check_b(statresult == 0 && dir!=NULL, "stat failed on file ", tmpfullpath->s);

			check(callback(context, dirnumber,
				bcstr_data_get(tmpfullpath),
				bcstr_length(tmpfullpath),
				(byte*) bcstr_data_get(tmpnativepath),
				bcstr_length(tmpnativepath),
				st.st_mtime, st.st_size));
		}
	}

	/* recurse into directories */
	if (depthcountdown > 0)
	{
		for (Uint32 i = 0; i < dirs.length; i++)
		{
			bcstr* str = bcstrlist_viewstr(&dirs, i);
			dirnumber += 1;
			check(os_recurse_impl(context, depthcountdown-1, dirnumber,
				symlinksskipped, str, tmpfullpath, tmpnativepath, callback));
		}
	}
	else if (dirnumber > 0)
	{
		/* don't throw this error if we were told the max depth is 0.*/
		check_b(0, "recursion limit reached in dir ", currentdir->s);
	}

cleanup:
	bcstrlist_close(&dirs);
	closedir(dir);
	return currenterr;
}

CHECK_RESULT Result os_recurse(void* context, const char* root,
	int* symlinksskipped, int maxRecursionDepth, FnOSRecurseCallback callback)
{
	Result currenterr = {};
	bcstr currentdir = bcstr_openwith(root);
	bcstr tmpfullpath = bcstr_open();
	bcstr tmpnativepath = bcstr_open();
	*symlinksskipped = 0;
	check(os_recurse_impl(context, maxRecursionDepth, 0,
		symlinksskipped, &currentdir, &tmpfullpath, &tmpnativepath, callback));

cleanup:
	bcstr_close(&currentdir);
	bcstr_close(&tmpfullpath);
	bcstr_close(&tmpnativepath);
	return currenterr;
}

#elif _WIN32

#define strcasecmp _stricmp
#define qsort_r qsort_s
#define stat64 _stat64
#define MAKEKEY32 _byteswap_ulong
#define MAKEKEY64 _byteswap_uint64
#define QSORTR_ARGS(paramCmp1, paramCmp2, paramContext) paramContext, paramCmp1, paramCmp2
#define PATH_MAX MAX_PATH

const int maxUtf8BytesPerCodepoint = 4;

/* returns strlen(narrow) */
void wide_to_utf8(const WCHAR* wide, Uint32 widelen, bcstr* narrow)
{
	/* reserve the max space (4bytes per codepoint) and shrink later */
	bcstr_data_settozeros(narrow, widelen*maxUtf8BytesPerCodepoint);
	int nBytesWritten = WideCharToMultiByte(CP_UTF8, 0, wide, widelen,
		bcstr_data_get(narrow), bcstr_length(narrow), NULL, NULL);

	bcstr_truncate(narrow, nBytesWritten);
}

/* returns wcslen(out) */
void ascii_to_bcwstr(const char* sz, Uint32 szlen, bcwstr* out)
{
	/* only convert ANSI because we specify CP_ACP; strlen(sz)==wcslen(out) */
	bcwstr_clear(out);
	bcarray_addzeros(&out->buffer, szlen + 1 /*nul-term*/);
	out->s = (WCHAR*)&out->buffer.buffer[0]; /* update pointer */
	int nBytesWritten = MultiByteToWideChar(CP_ACP, 0, sz, szlen,
		(WCHAR*)&out->buffer.buffer[0], out->buffer.length - 1 /*nul-term*/);

	bcarray_truncatelength(&out->buffer, nBytesWritten + 1);
}

bool os_create_dir(const char* s)
{
	int result = _mkdir(s);
	return ((result == 0 || errno == EEXIST) && os_dir_exists(s));
}

bool os_copy(const char* s1, const char* s2, bool overwrite_ok)
{
	return CopyFileA(s1, s2, !overwrite_ok) != FALSE;
}

CHECK_RESULT Result bcfile_opennative(bcfile* self, const byte* nativepath, const char* mode)
{
	Result currenterr = {};
	const WCHAR* wzmode = NULL;
	if (s_equal(mode, "rb"))
	{
		wzmode = L"rb";
	}
	else if (s_equal(mode, "r"))
	{
		wzmode = L"r";
	}
	else
	{
		check_b(0);
	}

	self->file = _wfopen((const WCHAR*)nativepath, wzmode);
	check_b(self->file, "failed to open path ", "", 0, 0,
		(const WCHAR*)nativepath);

cleanup:
	return currenterr;
}

bool os_isabspath(const char* s)
{
	return s && s[1] == ':' && s[2] == '\\';
}

bcstr os_getthisprocessdir()
{
	char buffer[MAX_PATH + 1] = "";
	DWORD chars = GetModuleFileNameA(nullptr, buffer, countof(buffer));
	if (chars > 0 && buffer[0])
	{
		char *dir=NULL, *filename=NULL;
		os_split_dir(buffer, &dir, &filename);
		return bcstr_openwith(dir);
	}
	else
	{
		return bcstr_openwith("");
	}
}

bcstr os_get_create_appdatadir()
{
	char buffer[MAX_PATH + 1] = "";
	HRESULT hr = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, buffer);
	if (!SUCCEEDED(hr))
	{
		return bcstr_open();
	}

	bcstr ret = bcstr_openwith(buffer, pathsep, "glacial_backup");
	if (!os_create_dir(ret.s))
	{
		/* indicate failure */
		bcstr_assign(&ret, "");
	}
	return ret;
}

bool os_are_other_instances_running(const char* path, int* out_code)
{
	/* fopen_s, unlike fopen, defaults to taking an exclusive lock. */
	FILE* file = NULL;
	errno_t ret = fopen_s(&file, path, "wb");

	/* tell the caller if we received an interesting errno. */
	if (ret != 0 && ret != EACCES)
	{
		*out_code = (int)ret;
	}

	/* intentionally leak the file-handle... will be closed on process exit */
	return file == NULL;
}



/* we want to have all files in a directory in order,
uninterupted by other dirs.
*/
CHECK_RESULT Result os_recurse_impl(void* context, int depthcountdown, int dirnumber, int* symlinksskipped, bcwstr* currentdir, bcwstr* tmpwz, bcstr* tmpsz, FnOSRecurseCallback callback)
{
	Result currenterr = {};
	bcarray w_dirs = bcarray_open(sizeof32(bcwstr), 0);

	/* create a search spec, "C:\path" to "C:\path\*" */
	WCHAR* wzcurrentdir = (WCHAR*)&currentdir->buffer.buffer[0];
	Uint32 currentdirlen = bcwstr_length(currentdir);
	bcwstr_appendwzlen(currentdir, L"\\*", countof32(L"\\*")-1);

	/* FindFirstFile */
	WIN32_FIND_DATAW findFileData = {0};
	HANDLE hFind(FindFirstFileW(wzcurrentdir, &findFileData));
	check_b(hFind != INVALID_HANDLE_VALUE,
		"FindFirstFile failed in dir ", "", 0,0, currentdir->s);

	/* from "C:\path\*" back to "C:\path" */
	wzcurrentdir[currentdirlen] = L'\0';

	do
	{
		if (wcscmp(L".", findFileData.cFileName)==0 || wcscmp(L"..", findFileData.cFileName)==0)
		{
			continue;
		}
		else if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			logwritef("skipping symlink in directory %S", currentdir->s);
			(*symlinksskipped)++;
		}
		else if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			/* add it to a list, we'll process this later*/
			bcwstr str = bcwstr_open();
			bcwstr_appendwzlen(&str, currentdir->s, currentdirlen);
			bcwstr_appendwzlen(&str, L"\\", countof32(L"\\")-1);
			bcwstr_appendwz(&str, findFileData.cFileName);
			bcarray_add(&w_dirs, (const byte*)&str, 1);
		}
		else
		{
			Uint64 modtime = make_uint64(findFileData.ftLastWriteTime.dwHighDateTime,
				findFileData.ftLastWriteTime.dwLowDateTime);

			Uint64 filesize = make_uint64(findFileData.nFileSizeHigh,
				findFileData.nFileSizeLow);

			/* get the full wide path */
			bcwstr_clear(tmpwz);
			bcwstr_appendwzlen(tmpwz, currentdir->s, currentdirlen);
			bcwstr_appendwzlen(tmpwz, L"\\", countof32(L"\\")-1);
			bcwstr_appendwz(tmpwz, findFileData.cFileName);

			/* convert to utf-8 */
			wide_to_utf8(tmpwz->s, bcwstr_length(tmpwz), tmpsz);
			check_b(bcstr_length(tmpsz), "could not convert utf16 ","", 0,0, tmpwz->s);

			/* call callback */
			check(callback(context, dirnumber,
				bcstr_data_get(tmpsz),
				bcstr_length(tmpsz),
				&tmpwz->buffer.buffer[0],
				bcwstr_length(tmpwz)*sizeof(WCHAR),
				modtime, filesize));
		}
	} while (FindNextFileW(hFind, &findFileData) != 0);

	/* recurse into directories */
	if (depthcountdown > 0)
	{
		for (Uint32 i=0; i<w_dirs.length; i++)
		{
			bcwstr* str = (bcwstr*)bcarray_at(&w_dirs, i);
			dirnumber += 1;
			check(os_recurse_impl(context, depthcountdown-1, dirnumber,
				symlinksskipped, str, tmpwz, tmpsz, callback));
		}
	}
	else if (dirnumber > 0)
	{
		/* don't throw this error if we were told the max depth is 0.*/
		check_b(0, "recursion limit reached, ","",0,0, currentdir->s);
	}

cleanup:
	for (Uint32 i=0; i<w_dirs.length; i++)
	{
		bcwstr_close((bcwstr*)bcarray_at(&w_dirs, i));
	}
	bcarray_close(&w_dirs);
	FindClose(hFind);
	return currenterr;
}

CHECK_RESULT Result os_recurse(void* context, const char* root,
	int* symlinksskipped, int maxRecursionDepth, FnOSRecurseCallback callback)
{
	Result currenterr = {};
	bcwstr currentdir = bcwstr_open();
	bcwstr tmpwz = bcwstr_open();
	bcstr tmpsz = bcstr_open();
	*symlinksskipped = 0;
	ascii_to_bcwstr(root, strlen32(root), &currentdir);
	check_b(currentdir.buffer.length > 1,
		"must be a ascii-only string of length > 0, but got ", root);
	check(os_recurse_impl(context, maxRecursionDepth, 0,
		symlinksskipped, &currentdir, &tmpwz, &tmpsz, callback));

cleanup:
	bcwstr_close(&currentdir);
	bcwstr_close(&tmpwz);
	bcstr_close(&tmpsz);
	return currenterr;
}

static void os_run_process_readfrompipe(HANDLE pipe, bcstr* out)
{
	DWORD bytes_read = 0;
	CHAR buffer[4096] = "";

	while (ReadFile(pipe, buffer, countof(buffer), &bytes_read, NULL) && bytes_read > 0)
	{
		bcstr_appendszlen(out, buffer, bytes_read);
	}
}

CHECK_RESULT Result os_run_process(const char* binary, char* const argv[],
	bcstr* getstdout, bcstr* getstderr, int* retcode)
{
	Result currenterr = {};
	bcstr quotedbinary = bcstr_openwith("\"", binary, "\"");
	bcstr quotedargs = bcstr_open();
	PROCESS_INFORMATION piProcInfo = {};
	SECURITY_ATTRIBUTES saAttr = {};
	STARTUPINFOA siStartInfo = {};

	/* quote the arguments */
	while (argv && *argv) {
		bcstr_append(&quotedargs, "\"");
		bcstr_append(&quotedargs, *argv);
		bcstr_append(&quotedargs, "\" ");
		argv++;
	}

	/* pipes need to be inherited*/
	saAttr.nLength = sizeof(saAttr);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	/* create pipes for stdout, stderr. only Wr should be inherited. */
	HANDLE g_hChildStd_OUT_Rd = NULL;
	HANDLE g_hChildStd_OUT_Wr = NULL;
	HANDLE g_hChildStd_ERR_Rd = NULL;
	HANDLE g_hChildStd_ERR_Wr = NULL;
	check_b(CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0));
	check_b(SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0));
	check_b(CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &saAttr, 0));
	check_b(SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0));

	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_ERR_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	/*
	warning:
	possibility of deadlock if reading from both stdout, stderr into different pipes
	see http://blogs.msdn.com/b/oldnewthing/archive/2011/07/07/10183884.aspx
	and http://www.redmountainsw.com/wordpress/1587/createprocess-and-redirecting-stdin-stdout-stderr-doesnt-have-to-be-difficult-but-it-is/
	*/
	check_fatal(0, "not yet implemented");

	check_b(CreateProcessA(quotedbinary.s,
		bcstr_data_get(&quotedargs),     /* args as non-const char* */
		NULL,          /* process security attributes  */
		NULL,          /* primary thread security attributes  */
		TRUE,          /* handles are inherited  */
		0,             /* creation flags  */
		NULL,          /* use parent's environment  */
		NULL,          /* use parent's current directory  */
		&siStartInfo,
		&piProcInfo));

	WaitForSingleObjectEx(piProcInfo.hProcess, INFINITE, FALSE);

cleanup:
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);
	CloseHandle(g_hChildStd_OUT_Rd);
	CloseHandle(g_hChildStd_OUT_Wr);
	CloseHandle(g_hChildStd_ERR_Rd);
	CloseHandle(g_hChildStd_ERR_Wr);

	bcstr_close(&quotedbinary);
	bcstr_close(&quotedargs);
	return currenterr;
}

bool os_open_text_editor(const char* filename)
{
	puts("Opening the configuration file in notepad...");
	puts("Exit notepad when your changes have been made.");
	SHELLEXECUTEINFOA shExecInfo = {};
	shExecInfo.cbSize = sizeof(shExecInfo);
	shExecInfo.lpVerb = "open";
	shExecInfo.lpFile = filename;
	shExecInfo.lpDirectory = NULL;
	shExecInfo.nShow = SW_SHOW;
	BOOL b = ShellExecuteExA(&shExecInfo);
	if (b && shExecInfo.hProcess)
	{
		WaitForSingleObject(shExecInfo.hProcess, INFINITE);
		CloseHandle(shExecInfo.hProcess);
		return true;
	}
	return false;
}

void os_open_dir_ui(const char* dir)
{
	printf("The directory is %s\n", dir);
	bcstr s = bcstr_openwith("explorer.exe \"", dir, "\"");
	ignore_unused_result(system(s.s));
	bcstr_close(&s);
}

bool os_remove(const char* s)
{
	// removes file or empty directory. ok if s was previously deleted.
	if (os_dir_exists(s))
		rmdir(s);
	else
		unlink(s);

	struct stat st = {0};
	return stat(s, &st) != 0;
}
void clearscreen()
{
	ignore_unused_result(system("cls"));
}
#else
#error "platform not yet supported"
#endif

Uint64 os_getfilesize(const char* s)
{
	struct stat64 st = {0};
	if (stat64(s, &st) == 0)
		return st.st_size;
	else
		return 0;
}

/* sorting utils*/
typedef int(*FuncCompareWithContext)(QSORTR_ARGS(const void*, const void*, void*));

int bcarray_sort_memcmp(QSORTR_ARGS(const void*pt1, const void*pt2, void* context))
{
	Uint32* comparesize = (Uint32*)context;
	check_fatal(comparesize);
	return memcmp(pt1, pt2, *comparesize);
}

void bcarray_sort(bcarray* self)
{
	Uint32 comparesize = self->elementsize;
	qsort_r(&self->buffer[0],
		self->length, self->elementsize, &bcarray_sort_memcmp, &comparesize);
}

/* GNU Lesser General Public License, version 2.1 (LGPL-2.1), section 3:
You may opt to apply the terms of the ordinary GNU General Public License
instead of this License. */
void* bsearch_r(const void *key, const void *base,
	size_t nmemb, size_t size, void* context,
	FuncCompareWithContext compar)
{
	size_t l = 0;
	size_t u = nmemb;
	while (l < u)
	{
		size_t mid = (l + u) / 2;
		const void *p = (void *)(((const char *)base) + (mid * size));
		int comparison = compar(QSORTR_ARGS(key, p, context));
		if (comparison < 0)
			u = mid;
		else if (comparison > 0)
			l = mid + 1;
		else
			return (void *)p;
	}

	return NULL;
}

/* bsearch finds any match, but we want the -first- match*/
Int32 bcarray_walk_first_match(bcarray* self, Int32 index, const void *key, Uint32 keysize)
{
	while (index - 1 >= 0 &&
		memcmp(key, bcarray_at(self, index - 1), keysize) == 0)
	{
		index--;
	}

	return index;
}

Int32 bcarray_bsearch(bcarray* self, const void *key, Uint32 keysize)
{
	byte* p = (byte*)bsearch_r(key, &self->buffer[0], self->length, self->elementsize, &keysize, bcarray_sort_memcmp);
	if (!p)
		return -1;

	check_fatal((p-&self->buffer[0]) % self->elementsize == 0);
	Int32 index = safecast32s((p-&self->buffer[0]) / self->elementsize);
	check_fatal(index >= 0);

	return bcarray_walk_first_match(self, index, key, keysize);
}

Result os_listdirfiles_callback(void* context, int UNUSEDPARAM,
	const char* filepath, Uint32 filepathlen, const byte* UNUSEDPARAM,
	Uint32 UNUSEDPARAM, Uint64 UNUSEDPARAM, Uint64 UNUSEDPARAM)
{
	bcstrlist* list = (bcstrlist*)context;
	check_fatal(list);
	bcstrlist_pushback(list, filepath);
	return ResultOk;
}

CHECK_RESULT Result os_listdirfiles(const char* dir, bcstrlist* list, bool sorted)
{
	Result currenterr = {};
	bcstrlist_clear(list, false);
	int symlinksskipped = 0;
	check(os_recurse((void*)list, dir, &symlinksskipped, 0 /*depth*/, &os_listdirfiles_callback));

	if (sorted)
		bcstrlist_sort(list);

cleanup:
	return currenterr;
}

CHECK_RESULT Result os_removechildren(const char* dir)
{
	Result currenterr = {};
	bcstrlist files_seen = bcstrlist_open();
	if (os_dir_exists(dir))
	{
		check(os_listdirfiles(dir, &files_seen, false));
		for (Uint32 i=0; i<files_seen.length; i++)
		{
			check_b(os_remove(
				bcstrlist_viewat(&files_seen, i)));
		}
	}

cleanup:
	bcstrlist_close(&files_seen);
	return currenterr;
}

#define szfilenamepattern_digits "5"

/* go from "/path/file", ".txt", "/path/file00007.txt" to 7. */
int filenamepattern_matches_template(const char* prefix, const char* ext, const char* candidate, bcstr* tmp)
{
	if (!s_startswith(candidate, prefix))
		return -1;

	if (!s_endswith(candidate, ext))
		return -1;

	/* for chars between the template and extension, ensure a digit and append to tmp.*/
	bcstr_clear(tmp);
	for (size_t i = strlen(prefix); i<strlen(candidate) - strlen(ext); i++)
	{
		bcstr_addchar(tmp, candidate[i]);
	}

	int n = 0;
	int npattern_digits = szfilenamepattern_digits[0]-'0';
	if (bcstr_length(tmp) == npattern_digits && s_intfromstring(tmp->s, &n))
		return n;
	else
		return -1;
}

/* go from "/path/file", ".txt", 7 to "/path/file00007.txt" */
void filenamepattern_gen(const char* prefix, const char* ext, int number, bcstr* out)
{
	bcstr_clear(out);
	bcstr_addfmt(out, "%s%0" szfilenamepattern_digits "d%s", prefix, number, ext);
}

/* find the largest number in the form "/path/file00007.txt" in this dir. */
CHECK_RESULT Result filenamepattern_getlatest(const char* prefix, const char* ext, int *out_number, bcstr* out)
{
	Result currenterr = {};
	bcstr tmp = bcstr_open();
	bcstr_clear(out);
	*out_number = 0;

	/* find directory of prefix*/
	char* namesplit = strdup(prefix), *dir, *file;
	os_split_dir(namesplit, &dir, &file);

	/* list files in directory */
	bcstrlist seen = bcstrlist_open();
	check(os_listdirfiles(dir, &seen, true /*sort*/));

	Uint32 i = seen.length;
	while (i--)
	{
		int val = filenamepattern_matches_template(prefix, ext, bcstrlist_viewat(&seen, i), &tmp);
		if (val != -1)
		{
			/* verify that it matches the pattern */
			filenamepattern_gen(prefix, ext, val, &tmp);
			check_b(s_equal(tmp.s, bcstrlist_viewat(&seen, i)));

			*out_number = val;
			bcstr_assign(out, bcstrlist_viewat(&seen, i));
			break;
		}
	}

	/* nothing found, so start with 1. */
	if (!*out_number)
	{
		*out_number = 1;
		filenamepattern_gen(prefix, ext, 1, out);
	}

cleanup:
	bcstr_close(&tmp);
	bcstrlist_close(&seen);
	free(namesplit);
	return currenterr;
}
