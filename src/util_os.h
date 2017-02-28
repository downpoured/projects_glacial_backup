#pragma once

#ifdef WIN32

#define stat64 _stat64
inline bool S_ISDIR(int mode)
{
	return (mode & S_IFDIR) != 0;
}

inline int mkdir_mode(const char* path, int)
{
	return _mkdir(path);
}

#else

#define stat64 stat
inline int mkdir_mode(const char* path, int mode)
{
	return mkdir(path, mode);
}

#endif

/* a few file utils */
CHECK_RESULT Result bcfile_writefile(const char* filename, const char* s)
{
	Result currenterr = {};
	bcfile file = bcfile_init();
	check(bcfile_open(&file, filename, "wb"));
	fputs(s, file.file);

cleanup:
	bcfile_close(&file);
	return currenterr;
}

CHECK_RESULT Result bcfile_readfile(const char* filename, bcstr* out)
{
	Result currenterr = {};
	bcstr_clear(out);
	bcfile file = bcfile_init();

	/* get length of file*/
	check(bcfile_open(&file, filename, "rb"));
	check_or_goto_cleanup_msg(fseek(file.file, 0, SEEK_END) == 0, "fseek failed");
	long length_of_file = ftell(file.file);
	check_or_goto_cleanup_msg(length_of_file > 0, "ftell failed");

	/* allocate mem */
	bcarray_addzeros(&out->buffer, length_of_file + 1 /* nul-term */);

	/* go back to beginning of file and copy the data*/
	check_or_goto_cleanup_msg(fseek(file.file, 0, SEEK_SET) == 0, "fseek failed");
	size_t read = fread(&out->buffer.buffer[0], 1, length_of_file, file.file);
	check_or_goto_cleanup_msg(read != 0, "fread failed");

cleanup:
	bcfile_close(&file);
	return currenterr;
}

inline bool os_file_exists(const char* szFilename)
{
	struct stat st = {0};
	return (stat(szFilename, &st) == 0 && !S_ISDIR(st.st_mode));
}

inline bool os_dir_exists(const char* szFilename)
{
	struct stat st = {0};
	return (stat(szFilename, &st) == 0 && S_ISDIR(st.st_mode));
}

inline bool os_create_dir(const char* s)
{
	int mode = 0777;
	int result = mkdir_mode(s, mode);
	if (result != 0 && errno != EEXIST)
		return false;
	else
		return true;
}

inline bool os_remove(const char* s)
{
	// removes file or empty directory. ok if s was previously deleted.
	remove(s);
	struct stat st = {0};
	return stat(s, &st) != 0;
}

inline Uint64 os_getfilesize(const char* s)
{
	struct stat64 st = {0};
	if (stat64(s, &st) == 0)
		return st.st_size;
	else
		return 0;
}

Uint64 util_os_unixtime()
{
	time_t result = time(NULL);
	return (Uint64)result;
}

#ifdef WIN32

void alertdialog(const char* message)
{
	::MessageBoxA(0, message, "", 0);
}

#else /* !WIN32 */

void alertdialog(const char* message)
{
	fprintf(stderr, "%s\nPress any key to continue...\n", message);
	getchar();
}

#endif

/* turn /path/to/file.txt into /path/to/file.0001.txt  */
static void filenamepattern_genimpl(const char* name_template, const char* before_ext, int number, bcstr* out)
{
	bcstr_clear(out);
	bcstr_appendszlen(out, name_template, safecast32u(before_ext-name_template));
	bcstr_addfmt(out, "%04d.%s", number, before_ext);
}

void filenamepattern_gen(const char* name_template, int number, bcstr* out)
{
	// not yet implemented.
	check_fatal_msg(false, "not yet implemented");
}

CHECK_RESULT Result filenamepattern_getlatest(const char* name_template, int *out_number, bcstr* out)
{
	bcstr_clear(out);
	*out_number = 0;
	check_fatal_msg(false, "not yet implemented");

	// not yet implemented.
	bcstr_appendsz(out, name_template);

	///* find the last dot */
	Result currenterr = {};
	return currenterr;
}
