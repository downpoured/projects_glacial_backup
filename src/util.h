#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

typedef uint64_t Uint64;
typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef int64_t Int64;
typedef int32_t Int32;
typedef int16_t Int16;
typedef uint8_t byte;

/* platform differences */
#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <psapi.h>
#include <shlobj.h>

#pragma warning(disable:4996)
#define INLINE __forceinline
#define STATIC_ASSERT(e) static_assert((e), "")
#define NORETURN __declspec(noreturn)
#define countof _countof
#define pathsep "\\"
#define newline "\r\n"
#define snprintf _snprintf
#define DEBUGBREAKIMPL() do { \
	if (IsDebuggerPresent()) ::DebugBreak(); \
	} while (0)

#else /* !_MSC_VER */
#include <errno.h>
#include <signal.h>
#include <wchar.h>

#define INLINE inline
#define STATIC_ASSERT(e) _Static_assert((e), "")
#define NORETURN __attribute__ ((__noreturn__))
#define __FUNCTION__ __func__
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#define pathsep "/"
#define newline "\n"
#define DEBUGBREAKIMPL() raise(SIGTRAP)

#endif

/* macros that differentiate ship+debug */
#if defined(_DEBUG)
#define DEBUG_ONLY(code) code
#define DEBUGBREAK() DEBUGBREAKIMPL()
#else
#define DEBUG_ONLY(code)
#define DEBUGBREAK() do {} while(0)
#endif

/* some return codes shouldn't be ignored */
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CHECK_RESULT __attribute__ ((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECK_RESULT _Check_return_
#else
#define CHECK_RESULT
#endif

/* a few forward declares */
struct Result;
struct bc_log;
struct bcstr;
extern struct bc_log g_log;
void alertdialog(const char* message);
bool bc_log_logerror(struct bc_log* self, struct Result* result,
	int errortag, const char* stringifiedcode, int lineno,
	const char* fnname, const char* file, const char* format, ...);

/* why macros? can record line numbers and debug-break at the right place. */
#define check_or_goto_cleanup_msg_tag(tag, cond, ...) \
	do { \
		if (!(cond)) { \
			bool should_break = bc_log_logerror(&g_log, \
				(&currenterr), (tag), (#cond), \
			  __LINE__, __FUNCTION__ , __FILE__, __VA_ARGS__); \
			if (should_break) { DEBUGBREAK(); } \
			goto cleanup; \
		} \
	} while(0);

#define check_error_msg_tag(tag, is_fatal, cond, ...) \
	do { \
		if (!(cond)) { \
			bool should_break = bc_log_logerror(&g_log, \
				NULL, (tag), (#cond), \
				__LINE__, __FUNCTION__ , __FILE__, __VA_ARGS__); \
			if (should_break) { DEBUGBREAK(); } \
			if (is_fatal) { die(__VA_ARGS__); } \
		} \
	} while(0)

#define check(result) \
	do { \
		currenterr = (result); \
		if (currenterr.errortag != 0) goto cleanup; \
	} while (0)

#define check_or_goto_cleanup_msg(cond, ...) \
	check_or_goto_cleanup_msg_tag(-1, (cond), __VA_ARGS__)
#define check_or_goto_cleanup(cond) \
	check_or_goto_cleanup_msg_tag(-1, (cond), "")
#define check_or_goto_cleanup_tag(tag, cond) \
	check_or_goto_cleanup_msg_tag((tag), (cond), "")
#define goto_cleanup_msg(...) \
	check_or_goto_cleanup_msg_tag(-1, false,  __VA_ARGS__)
#define check_fatal_msg(cond, ...) \
	check_error_msg_tag(-1, true, (cond), __VA_ARGS__)
#define check_fatal_tag(tag, cond) \
	check_error_msg_tag((tag), true, (cond), "")
#define check_fatal(cond) \
	check_error_msg_tag(-1, true, (cond), "")
#define check_warn_msg(cond, ...) \
	check_error_msg_tag(-1, false, (cond), __VA_ARGS__)
#define check_warn_tag(tag, cond) \
	check_error_msg_tag((tag), false, (cond), "")
#define check_warn(cond) \
	check_error_msg_tag(-1, false, (cond), "")
#define set_self_zero() \
	memset(self, 0, sizeof(*self))

/* structures for error-handling */
typedef struct Result
{
	int errortag; /* 0 is no error, -1 is unspecified error */
	char* errormsg; /* error handler should call Result_close. */
} Result;

void Result_close(Result* self)
{
	free(self->errormsg);
	set_self_zero();
}

NORETURN void die(char* format, ...)
{
	va_list args;
	va_start(args, format);
	fputs("Error: ", stderr);
	vfprintf(stderr, format, args);
	fputs("\n", stderr);
	va_end(args);
	DEBUG_ONLY(alertdialog("error occurred."));
	exit(1);
}

/* file wrapper, for consistency */
typedef struct bcfile {
	FILE* file;
} bcfile;

bcfile bcfile_init(void)
{
	bcfile ret = {};
	return ret;
}

CHECK_RESULT Result bcfile_open(bcfile* self, const char* path, const char* mode)
{
	Result currenterr = {};
	self->file = fopen(path, mode);
	check_or_goto_cleanup_msg(self->file, "failed to open %s", path);

cleanup:
	return currenterr;
}

void bcfile_close(bcfile* self)
{
	if (self && self->file)
	{
		fclose(self->file);
		set_self_zero();
	}
}

/* integer overflow */
Uint32 safemul32(Uint32 a, Uint32 b)
{
	Uint32 ret = a*b;
	check_fatal_msg(a==0 || ret/a==b, "potential overflow");
	return ret;
}

Uint32 safeadd32(Uint32 a, Uint32 b)
{
	Uint32 ret = a+b;
	check_fatal_msg(ret>=a && ret>=b, "potential overflow");
	return ret;
}

Int32 safeadd32s(Int32 a, Int32 b)
{
	check_fatal_msg(!
		(((b > 0) && (a > INT_MAX - b)) ||
		((b < 0) && (a < INT_MIN - b))),
		"potential overflow");
	
	return a+b;
}

Uint32 align_to_multiple(Uint32 a, Uint32 mod)
{
	Uint32 sum = safeadd32(a, mod);
	if (sum == 0)
		return mod;
	else
		return safemul32((sum - 1)/mod, mod);
}

Uint32 nearest_power_of_two32(size_t a)
{
	check_fatal(a < UINT32_MAX/4);
	if (a==0)
		return 1;

	Uint32 result = 1;
	while (result < a)
		result <<= 1;
	return result;
}

/* allocation */
byte* bc_malloc(Uint32 a, Uint32 b)
{
	void* ret = malloc(safemul32(a, b));
	check_fatal(ret != NULL);
	return (byte*)ret;
}

byte* bc_realloc(byte* ptr, Uint32 a, Uint32 b)
{
	void* ret = realloc(ptr, safemul32(a, b));
	check_fatal(ret != NULL);
	return (byte*)ret;
}

byte* bc_calloc(Uint32 a, Uint32 b)
{
	void* ret = calloc(a, b);
	check_fatal(ret != NULL);
	return (byte*)ret;
}

Uint32 safecast32u(size_t n)
{
	check_fatal(n < UINT32_MAX);
	return (Uint32)n;
}

#define sizeof32(x) safecast32u(sizeof(x))
#define strlen32(x) safecast32u(strlen(x))
#define countof32(x) safecast32u(countof(x))
#define bc_freenull(ptr) \
	do { \
		free(ptr); \
		(ptr)=NULL; \
	} while (0)


/* dynamically-sized array */
typedef struct bcarray {
	byte* buffer;
	Uint32 elementsize;
	Uint32 capacity; /* in terms of elementsize*/
	Uint32 length; /* in terms of elementsize*/
} bcarray;

bcarray bcarray_init(Uint32 elementsize, Uint32 initialcapacity)
{
	/* don't allocate memory yet if initialcapacity==0 */
	bcarray self = {};
	self.elementsize = elementsize;
	if (initialcapacity)
	{
		self.buffer = bc_malloc(initialcapacity, elementsize);
		self.capacity = initialcapacity;
	}
	return self;
}

void bcarray_reserve(bcarray* self, Uint32 requestedcapacity)
{
	if (requestedcapacity <= self->capacity)
		return;

	/* double container size until large enough.*/
	requestedcapacity = nearest_power_of_two32(requestedcapacity);
	self->buffer = bc_realloc(self->buffer, requestedcapacity, self->elementsize);
	self->capacity = requestedcapacity;
}

void bcarray_add(bcarray* self, const byte* inbuffer, Uint32 incount)
{
	bcarray_reserve(self, safeadd32(self->length, incount));
	Uint32 index = safemul32(self->length, self->elementsize);
	memcpy(&self->buffer[index], inbuffer, safemul32(self->elementsize, incount));
	self->length += incount;
}

void bcarray_addzeros(bcarray* self, Uint32 incount)
{
	bcarray_reserve(self, safeadd32(self->length, incount));
	Uint32 index = safemul32(self->length, self->elementsize);
	memset(&self->buffer[index], 0, safemul32(self->elementsize, incount));
	self->length += incount;
}

void bcarray_truncatelength(bcarray* self, Uint32 newlength)
{
	check_fatal(newlength <= self->length);
	self->length = newlength;
}

void bcarray_clear(bcarray* self)
{
	bcarray_truncatelength(self, 0);
}

/* performs bounds check. */
byte* bcarray_at(bcarray* self, Uint32 index)
{
	check_fatal(index < self->length);
	Uint32 index_bytes = safemul32(index, self->elementsize);
	return &self->buffer[index_bytes];
}

/* performs bounds check. */
const byte* bcarray_atconst(const bcarray* self, Uint32 index)
{
	check_fatal(index < self->length);
	Uint32 index_bytes = safemul32(index, self->elementsize);
	return &self->buffer[index_bytes];
}

/* helper for making an array of Uint64. */
Uint64* bcarray_at64u(bcarray* self, Uint32 index)
{
	check_fatal(self->elementsize == sizeof32(Uint64));
	return (Uint64*) bcarray_at(self, index);
}

/* helper for making an array of Uint64. */
void bcarray_add64u(bcarray* self, Uint64 n)
{
	check_fatal(self->elementsize == sizeof32(Uint64));
	bcarray_add(self, (byte*)&n, 1);
}

void bcarray_close(bcarray* self)
{
	if (self)
	{
		free(self->buffer);
		set_self_zero();
	}
}

/* strings. nul characters within the string are supported.
consumers shouldn't directly change the underlying buffer. */
typedef struct bcstr {
	bcarray buffer;
	const char* s; /* pointer into buffer, for convenience */
} bcstr;

void bcstr_appendszlen(bcstr* self, const char* sz, Uint32 length)
{
	/* start on top of the previous nul term */
	if (self->buffer.length > 0)
		bcarray_truncatelength(&self->buffer, self->buffer.length - 1);

	/* copy over string*/
	bcarray_add(&self->buffer, (const byte*)sz, length);

	/* add nul term */
	char bufferNulterm[] = {0};
	bcarray_add(&self->buffer, (const byte*)&bufferNulterm[0],
		1 /*count of elements*/);

	/* update convenience pointer (could have changed if we realloc'd) */
	self->s = (const char*)&self->buffer.buffer[0];
}

void bcstr_appendsz(bcstr* self, const char* sz)
{
	bcstr_appendszlen(self, sz, strlen32(sz));
}

/* providing sugar syntax, bcstr_appendmany(&str, "a", "b"); */
typedef struct StringParams {
	const char* strings[10];
} StringParams;

#ifdef _MSC_VER
#define CASTSTRPARAMS
#else
#define CASTSTRPARAMS (StringParams)
#endif

#define bcstr_append(self, ...) bcstr_append_impl((self), CASTSTRPARAMS {{ __VA_ARGS__ }}); 
void bcstr_append_impl(bcstr* self, StringParams strings)
{
	for (int i=0; i<countof(strings.strings); i++)
		if (strings.strings[i])
			bcstr_appendsz(self, strings.strings[i]);
}

#define bcstr_initwith(...) bcstr_initwith_impl(CASTSTRPARAMS {{ __VA_ARGS__ }}); 
bcstr bcstr_initwith_impl(StringParams strings)
{
	bcstr ret = {};
	ret.buffer = bcarray_init(sizeof32(char), 0);
	bcstr_append_impl(&ret, strings);
	return ret;
}

bcstr bcstr_init(void)
{
	/* a creative future optimization might be to 
	skip allocation and set .s to the static string "". */
	return bcstr_initwith("");
}

void bcstr_clear(bcstr* self)
{
	if (self->buffer.length > 0)
	{
		bcarray_clear(&self->buffer);
		bcstr_appendsz(self, ""); /*adds null char*/
	}
}

void bcstr_assign(bcstr* self, const char* sz)
{
	bcarray_clear(&self->buffer);
	bcstr_appendsz(self, sz);
}

void bcstr_addchar(bcstr* self, char c)
{
	char buffer[2] = {c, '\0'};
	bcstr_appendsz(self, buffer);
}

/* on format error, returns -1 and leaves string unchanged */
int bcstr_addvfmt(bcstr* self, const char *fmt, va_list args)
{
	/* determine the length */
	va_list copy;
	va_copy(copy, args);
	int size = vsnprintf(NULL /*destination*/, 0 /*max size*/, fmt, copy);
	va_end(copy);

	/* if format error, return early*/
	if (size < 0)
	{
		return size;
	}

	Uint32 startpoint = 0;
	if (self->buffer.length > 0)
	{
		/* start on top of the previous nul term */
		startpoint = self->buffer.length - 1;
		bcarray_truncatelength(&self->buffer, self->buffer.length - 1);
	}
	else
	{
		/* there's no previous nul term, so start at beginning of array */
		startpoint = 0;
	}

	/* add zeros to reserve enough space, and provide nul-term*/
	bcarray_addzeros(&self->buffer, size + 1 /*nul-term*/);

	/* print directly into buffer*/
	size = vsnprintf((char*)bcarray_at(&self->buffer, startpoint), size+1, fmt, args);

	/* update convenience pointer (could have changed if we realloc'd) */
	self->s = (const char*)&self->buffer.buffer[0];
	return size;
}

/* on format error, returns -1 and leaves string unchanged */
int bcstr_addfmt(bcstr* self, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int size = bcstr_addvfmt(self, fmt, args);
	va_end(args);
	return size;
}

Uint32 bcstr_length(const bcstr* self)
{
	return self->buffer.length ? self->buffer.length-1 : 0;
}

CHECK_RESULT Result bcstr_truncate(bcstr* self, Uint32 newlength)
{
	Result currenterr = {};
	check_or_goto_cleanup(newlength <= self->buffer.length);
	bcarray_truncatelength(&self->buffer, newlength + 1);
	*(bcarray_at(&self->buffer, newlength)) = 0; /* null term*/

cleanup:
	return currenterr;
}

void bcstr_close(bcstr* self)
{
	if (self)
	{
		bcarray_close(&self->buffer);
		set_self_zero();
	}
}

bool s_equal(const char* s1, const char* s2)
{
	return strcmp(s1, s2) == 0;
}

bool s_startswith(const char* s1, const char* s2)
{
	return strncmp((s1), (s2), strlen(s2)) == 0;
}

bool s_endswith(const char* s1, const char*s2)
{
	size_t len1 = strlen(s1), len2 = strlen(s2);
	if (len2>len1) return false;
	return strcmp(s1 + len1 - len2, s2) == 0;
}

bool s_contains(const char* s1, const char* s2)
{
	return strstr(s1, s2) != 0;
}

/* input must consist of only numerals.*/
bool s_intfromstring(const char* s, int* result)
{
	*result = 0;
	if (!s || !s[0])
	{
		return false;
	}

	const char* ptrIter = s;
	while (*ptrIter != '\0')
	{
		if (!isdigit(*(ptrIter++)))
		{
			return false;
		}
	}

	char* ptrresult = 0;
	int res = strtol(s, &ptrresult, 10);
	if (!*ptrresult)
	{
		*result = res;
	}

	return true;
}

/* string array. */
typedef struct bc_strarray {
	bcarray arr;
} bc_strarray;

bc_strarray bc_strarray_init()
{
	bc_strarray self = {};
	self.arr = bcarray_init(sizeof32(bcstr), 0);
	return self;
}

void bc_strarray_pushback(bc_strarray* self, const char* s)
{
	bcstr str = bcstr_initwith(s);
	bcarray_add(&self->arr, (byte*)&str, 1);
}

#define bc_strarray_initwith(...) bc_strarray_initwith_impl(CASTSTRPARAMS {{ __VA_ARGS__ }})
bc_strarray bc_strarray_initwith_impl(StringParams strings)
{
	bc_strarray ret = bc_strarray_init();
	for (int i = 0; i < countof(strings.strings); i++)
		if (strings.strings[i])
			bc_strarray_pushback(&ret, strings.strings[i]);
	return ret;
}

bcstr bc_strarray_pop(bc_strarray* self)
{
	bcstr ret = *(bcstr*)bcarray_at(&self->arr, self->arr.length-1);
	bcarray_truncatelength(&self->arr, self->arr.length-1);
	return ret;
}

void bc_strarray_clear(bc_strarray* self)
{
	while (self->arr.length)
	{
		bcstr s = bc_strarray_pop(self);
		bcstr_close(&s);
	}
}

/* considered offering view into buffer, but that'd be easy to misuse. */
void bc_strarray_at(const bc_strarray* self, Uint32 index, bcstr* out)
{
	const bcstr* s = (const bcstr*)bcarray_atconst(&self->arr, index);
	bcstr_assign(out, s->s);
}

Uint32 bc_strarray_length(const bc_strarray* self)
{
	return self->arr.length;
}

void bc_strarray_add_str_split(bc_strarray* self, const char* sz, char cDelim)
{
	bcstr strTemp = bcstr_init();
	const char* szStart = sz;
	const char* szNext = 0;
	while ((szNext = strchr(szStart, cDelim)) != 0)
	{
		bcstr_clear(&strTemp);
		bcstr_appendszlen(&strTemp, szStart, safecast32u(szNext-szStart));
		bc_strarray_pushback(self, strTemp.s);
		szStart = szNext+1;
	}

	bc_strarray_pushback(self, szStart);
	bcstr_close(&strTemp);
}

static int ComparisonFn_bcstrarray(const void* pt1, const void* pt2)
{
	const bcstr* str1 = (const bcstr*)pt1;
	const bcstr* str2 = (const bcstr*)pt2;
	return strcmp(str1->s, str2->s);
}

void bc_strarray_sort(bc_strarray* self)
{
	qsort(&self->arr.buffer[0], self->arr.length, 
		self->arr.elementsize, ComparisonFn_bcstrarray);
}

bcstr bc_strarray_tostring(bc_strarray* self)
{
	bcstr ret = bcstr_init();
	bcstr_addfmt(&ret, "length=%d\n", bc_strarray_length(self));

	bcstr tmp = bcstr_init();
	for (Uint32 i=0; i<bc_strarray_length(self); i++)
	{
		bc_strarray_at(self, i, &tmp);
		bcstr_addfmt(&ret, "index %d:'%s'\n", i, tmp.s);
	}

	bcstr_close(&tmp);
	return ret;
}

void bc_strarray_close(bc_strarray* self)
{
	if (self)
	{
		bc_strarray_clear(self);
		bcarray_close(&self->arr);
		set_self_zero();
	}
}

/* logging */
typedef struct bc_log
{
	bcfile file;
	bcstr filename_template;
	bcstr filename_current;
	int filenumber_current;
	int approx_max_filesize;
	Uint32 counter;
	bool suppress_dialogs;
} bc_log;

/* forward declares for logging */
Uint64 util_os_unixtime();
CHECK_RESULT struct Result filenamepattern_getlatest(const char* filename_template,
	int *out_number, struct bcstr* out);
void filenamepattern_gen(const char* filename_template,
	int number, struct bcstr* out);

bc_log bc_log_init(void)
{
	bc_log ret = {};
	return ret;
}

void bc_log_close(bc_log* self)
{
	if (self)
	{
		bcfile_close(&self->file);
		bcstr_close(&self->filename_template);
		bcstr_close(&self->filename_current);
		set_self_zero();
	}
}

CHECK_RESULT Result bc_log_open(bc_log* self, const char* filename_template)
{
	Result currenterr = {};
	set_self_zero();
	bcstr_assign(&self->filename_template, filename_template);
	self->approx_max_filesize = 8*1024*1024;
	self->suppress_dialogs = false;
	
	/* get filename*/
	self->filename_current = bcstr_init();
	check(filenamepattern_getlatest(filename_template,
		&self->filenumber_current, &self->filename_current));

	/* open file */
	self->file = bcfile_init();
	check(bcfile_open(&self->file, self->filename_current.s, "ab"));

cleanup:
	return currenterr;
}

void bc_log_checkfilesize(bc_log* self)
{
	const int how_often_checkfilesize = 32;
	if (self->counter++ % how_often_checkfilesize == 0)
	{
		fseek(self->file.file, 0, SEEK_END);
		if (ftell(self->file.file) > self->approx_max_filesize)
		{
			/* get next filename */
			bcstr next_filename = bcstr_init();
			self->filenumber_current++;
			filenamepattern_gen(self->filename_template.s, self->filenumber_current, &next_filename);

			/* open file, if the open fails we'll just keep using the old one. */
			bcfile next_file = bcfile_init();
			Result result = bcfile_open(&next_file, next_filename.s, "ab");
			if (result.errortag)
			{
				check_warn_msg(0, "failed to open next log file %s", next_filename.s);
				bcfile_close(&next_file);
				bcstr_close(&next_filename);
			}
			else
			{
				/* move-semantics, replace current file+filename with this file+filename*/
				bcfile_close(&self->file);
				bcstr_close(&self->filename_current);
				self->file = next_file;
				self->filename_current = next_filename;
			}

			Result_close(&result);
		}
	}
}

void bc_log_writeraw(bc_log* self, const char* text)
{
	if (self->file.file)
	{
		fputs(text, self->file.file);
	}
}

void bc_log_write(bc_log* self, const char* text)
{
	if (self->file.file)
	{
		bc_log_checkfilesize(self);

		Uint64 approx_seconds = util_os_unixtime() - 16436*24*60*60 /* days since 2015 */;
		Uint32 secs_rounded = approx_seconds % 60;
		Uint32 min_rounded = (approx_seconds / 60) % 60;
		Uint64 hr_rounded = (approx_seconds / (60 * 60));
		Uint32 hr_number = hr_rounded % 24;
		Uint32 rough_days = (Uint32)(hr_rounded / 24);
		
		fprintf(self->file.file, newline "%d %d:%d:%d %s ",
			rough_days, hr_number, min_rounded, secs_rounded, text);
	}
}

void bc_log_writef(bc_log* self, const char* fmt, ...)
{
	if (self->file.file)
	{
		bc_log_write(self, "");
		va_list args;
		va_start(args, fmt);
		int read = vfprintf(self->file.file, fmt, args);
		va_end(args);

		if (read < 0 && !s_contains(fmt, "warn"))
			check_warn_msg(0, "format failure in bc_log_writef, format string is '%s'", fmt);
	}
}

/* providing sugar syntax, writemany("a", "b", "c"); */
#define bc_log_writemany(self, ...) bc_log_writemany_impl(CASTSTRPARAMS {{ __VA_ARGS__ }}); 
void bc_log_writemany_impl(bc_log* self, StringParams strings)
{
	bc_log_write(self, "");
	for (int i=0; i<countof(strings.strings); i++)
		if (strings.strings[i])
			bc_log_writeraw(self, strings.strings[i]);
}

bool bc_log_logerror(bc_log* self, Result* result,
	int errortag, const char* stringifiedcode, int lineno,
	const char* fnname, const char* file, const char* fmt, ...)
{
	/* first, write to the file before any memory allocs. */
	if (self->file.file)
	{
		bc_log_writeraw(&g_log, newline "error or warning occurred." newline);
		bc_log_writeraw(&g_log, stringifiedcode);
		bc_log_writeraw(&g_log, " ");
		bc_log_writeraw(&g_log, fmt ? fmt : "");
		fflush(self->file.file);
	}

	/* now build a long string with all the info we have */
	bcstr str_err = bcstr_initwith("error or warning ");
	if (fmt && fmt[0])
	{
		va_list args;
		va_start(args, fmt);
		bcstr_addvfmt(&str_err, fmt, args);
		va_end(args);
	}

	bcstr_addfmt(&str_err, newline " on line %d, fn %s, file %s ", lineno, fnname, file);
	if (errortag != 0 && errortag != -1)
		bcstr_addfmt(&str_err, newline " tag %d", errortag);

	/* write long string to log*/
	if (self->file.file)
	{
		bc_log_write(&g_log, str_err.s);
		fflush(self->file.file);
	}

	/* show dialog */
	if (!self->suppress_dialogs)
	{
		DEBUG_ONLY(alertdialog(str_err.s));
	}
	
	/* set string, recipient has responsibility to free. */
	if (result)
	{
		result->errortag = errortag;
		result->errormsg = strdup(str_err.s); 
	}

	bcstr_close(&str_err);
	return !self->suppress_dialogs;
}

/* wchar_t string */
typedef struct bcwstr {
	bcarray buffer;
	const wchar_t* s; /* pointer into buffer, for convenience */
} bcwstr;

void bcwstr_appendwz(bcwstr* self, const wchar_t* wz)
{
	Uint32 length = safecast32u(wcslen(wz));

	/* start on top of the previous nul term */
	if (self->buffer.length > 0)
		bcarray_truncatelength(&self->buffer, self->buffer.length - 1);

	/*copy over string*/
	bcarray_add(&self->buffer, (const byte*)wz, length);

	/* add nul term */
	wchar_t bufferNulterm[] = {0};
	bcarray_add(&self->buffer, (const byte*)&bufferNulterm[0], 1 /*count of elements*/);

	/* update convenience pointer (could have changed if we realloc'd) */
	self->s = (const wchar_t*)&self->buffer.buffer[0];
}

void bcwstr_clear(bcwstr* self)
{
	if (self->buffer.length > 0) /* this check can save an allocation */
	{
		bcarray_clear(&self->buffer);
		bcwstr_appendwz(self, L""); /*adds null char*/
	}
}

bcwstr bcwstr_initwz(const wchar_t* wz)
{
	bcwstr ret = {};
	ret.buffer = bcarray_init(sizeof32(wchar_t), 0);
	bcwstr_appendwz(&ret, wz);
	return ret;
}

void bcwstr_close(bcwstr* self)
{
	if (self)
	{
		bcarray_close(&self->buffer);
		set_self_zero();
	}
}

Uint64 make_uint64(Uint32 hi, Uint32 lo)
{
	return lo | (((Uint64)hi) << 32);
}

Uint32 chars_into_uint32(unsigned char c1, unsigned char c2,
	unsigned char c3, unsigned char c4)
{
	return (((Uint32)c1) << 24) | (((Uint32)c2) << 16) |
		(((Uint32)c3) << 8) | ((Uint32)c4);
}

