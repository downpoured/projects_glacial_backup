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
#define cmdswitch "/"
#define snprintf _snprintf
#define ignore_unused_result(x) (void)x
#define CAST(structname) 
#define DEBUGBREAKIMPL() do { \
	if (IsDebuggerPresent()) DebugBreak(); \
	} while (0)

#else /* !_MSC_VER */
#include <byteswap.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include <wchar.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <inttypes.h>

/* note c99 inline behavior */
#define INLINE static inline
#define STATIC_ASSERT(e) _Static_assert((e), "")
#define NORETURN __attribute__ ((__noreturn__))
#define __FUNCTION__ __func__
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#define pathsep "/"
#define newline "\n"
#define cmdswitch "--"
#define ignore_unused_result(x) do {int ignored __attribute__((unused)) = x;} while(0)
#define CAST(structname) (structname)
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

/* ignore some values (unused), force others to be checked (warn_unused) */
#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CHECK_RESULT __attribute__ ((warn_unused_result))
#define UNUSEDPARAM  TOKENPASTE2(UNUSEDp, __COUNTER__)  __attribute__((unused))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECK_RESULT _Check_return_
#define UNUSEDPARAM  TOKENPASTE2(UNUSEDp, __COUNTER__) 
#else
#define CHECK_RESULT
#define UNUSEDPARAM  TOKENPASTE2(UNUSEDp, __COUNTER__) 
#endif

/* macros that conveniently record line number of failure. */
#define callbc_log_logerror(tag, curerr, scond, ...)  \
	bool brk = bc_log_logerror(&g_log, \
		(curerr), (tag), (scond), \
		__LINE__, __FUNCTION__ , __FILE__,  \
		CAST(bc_log_errcontext_t) { __VA_ARGS__ }); \
	if (brk) { DEBUGBREAK(); }

#define check_b_tag(tag, cond, ...) do { \
	if (!(cond)) { \
		callbc_log_logerror(tag, &currenterr, (#cond), __VA_ARGS__); \
		goto cleanup; \
	} } while(0)

#define check_error_tag(tag, is_fatal, cond, ...) do { \
	if (!(cond)) { \
		callbc_log_logerror((tag), NULL, (#cond), __VA_ARGS__); \
		if (is_fatal) die( __LINE__, __FUNCTION__, (#cond)); \
	} } while(0)

#define check_b(cond, ...) \
	check_b_tag(-1, (cond), __VA_ARGS__)

#define check_fatal(cond, ...) \
	check_error_tag(-1, true, (cond), __VA_ARGS__)

#define check_warn(cond, ...) \
	check_error_tag(-1, false, (cond), __VA_ARGS__)

#define check(result) do { \
	currenterr = (result); \
	if (currenterr.errortag != 0) goto cleanup; \
	} while (0)

#define set_self_zero() \
	memset(self, 0, sizeof(*self))

struct Result;
struct bc_log;
struct bcstr;
extern struct bc_log g_log;
void alertdialog(const char* message);

/* structures for error-handling */
typedef struct bc_log_errcontext_t {
	const char* sz1;
	const char* sz2;
	long long int n1;
	long long int n2;
	const wchar_t* wz;
} bc_log_errcontext_t;

typedef struct Result
{
	char* errormsg; /* error handler should call Result_close. */
	int errortag; /* 0 is no error, -1 is unspecified error */
} Result;

void Result_close(Result* self)
{
	free(self->errormsg);
	set_self_zero();
}

bool bc_log_logerror(struct bc_log* self, struct Result* result,
	int errortag, const char* stringifiedcode, int lineno,
	const char* fnname, const char* file,
	bc_log_errcontext_t params);

NORETURN void die(int line, const char* fnname, char* s)
{
	fprintf(stderr, "Error: in fn %s line%d\n%s\n",
		fnname, line, s);
	DEBUG_ONLY(alertdialog("error occurred."));
	exit(1);
}

const Result ResultOk = {};

/* file wrapper, just for consistency */
typedef struct bcfile {
	FILE* file;
} bcfile;

CHECK_RESULT Result bcfile_open(bcfile* self, const char* path, const char* mode)
{
	Result currenterr = {};
	set_self_zero();
	self->file = fopen(path, mode);
	check_b(self->file, "failed to open ", path);

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

/* check for integer overflow */
Uint32 safemul32(Uint32 a, Uint32 b)
{
	Uint32 ret = a*b;
	check_fatal(a==0 || ret/a==b, "potential overflow");
	return ret;
}

Uint32 safeadd32(Uint32 a, Uint32 b)
{
	Uint32 ret = a+b;
	check_fatal(ret>=a && ret>=b, "potential overflow");
	return ret;
}

Int32 safeadd32s(Int32 a, Int32 b)
{
	check_fatal(!
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
	if (a == 0)
	{
		return 1;
	}

	Uint32 result = 1;
	while (result < a)
	{
		result <<= 1;
	}

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

Uint32 safecast32u(unsigned long long n)
{
	check_fatal(n <= UINT32_MAX);
	return (Uint32)n;
}

Int32 safecast32s(long long n)
{
	check_fatal(n <= INT_MAX);
	return (Int32)n;
}

#define sizeof32(x) safecast32u(sizeof(x))
#define countof32(x) safecast32u(countof(x))
#define strlen32(x) safecast32u(strlen(x))
#define wcslen32(x) safecast32u(wcslen(x))
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

bcarray bcarray_open(Uint32 elementsize, Uint32 initialcapacity)
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

byte* bcarray_at(bcarray* self, Uint32 index)
{
	check_fatal(index < self->length);
	Uint32 index_bytes = safemul32(index, self->elementsize);
	return &self->buffer[index_bytes];
}

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
	return (Uint64*)bcarray_at(self, index);
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
consumers should not directly change the underlying buffer. */
typedef struct bcstr {
	bcarray __buffer; /* if access needed, use data_get() */
	const char* s; /* pointer into buffer, for convenience */
} bcstr;

void bcstr_appendszlen(bcstr* self, const char* sz, Uint32 length)
{
	/* start on top of the previous nul term */
	if (self->__buffer.length > 0)
		bcarray_truncatelength(&self->__buffer, self->__buffer.length - 1);

	/* copy over string*/
	bcarray_add(&self->__buffer, (const byte*)sz, length);

	/* add nul term */
	char bufferNulterm[] = {0};
	bcarray_add(&self->__buffer, (const byte*)&bufferNulterm[0],
		1 /*count of elements*/);

	/* update convenience pointer (could have changed if we realloc'd) */
	self->s = (const char*)&self->__buffer.buffer[0];
}

/* providing sugar syntax, bcstr_appendmany(&str, "a", "b"); */
typedef struct StringParams {
	const char* strings[10];
} StringParams;

#define bcstr_append(self, ...) \
	bcstr_append_impl((self), CAST(StringParams) {{ __VA_ARGS__ }})
void bcstr_append_impl(bcstr* self, StringParams strings)
{
	for (int i=0; i<countof(strings.strings); i++)
		if (strings.strings[i])
			bcstr_appendszlen(self, strings.strings[i], strlen32(strings.strings[i]));
}

#define bcstr_openwith(...) \
	bcstr_openwith_impl(CAST(StringParams) {{ __VA_ARGS__ }})
bcstr bcstr_openwith_impl(StringParams strings)
{
	bcstr ret = {};
	ret.__buffer = bcarray_open(sizeof32(char), 0);
	bcstr_append_impl(&ret, strings);
	return ret;
}

bcstr bcstr_open(void)
{
	/* a future optimization might be to
	skip allocation and set .s to the static string "". */
	bcstr ret = {};
	ret.__buffer = bcarray_open(sizeof32(char), 0);
	bcstr_appendszlen(&ret, "", 0);
	return ret;
}

void bcstr_clear(bcstr* self)
{
	if (self->__buffer.length > 0)
	{
		bcarray_clear(&self->__buffer);
		bcstr_appendszlen(self, "", 0); /*adds null char*/
	}
}

void bcstr_assign(bcstr* self, const char* sz)
{
	bcarray_clear(&self->__buffer);
	bcstr_appendszlen(self, sz, strlen32(sz));
}

void bcstr_assignszlen(bcstr* self, const char* sz, Uint32 len)
{
	bcarray_clear(&self->__buffer);
	bcstr_appendszlen(self, sz, len);
}

void bcstr_addchar(bcstr* self, char c)
{
	char buffer[2] = {c, '\0'};
	bcstr_appendszlen(self, buffer, 1);
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
	if (self->__buffer.length > 0)
	{
		/* start on top of the previous nul term */
		startpoint = self->__buffer.length - 1;
		bcarray_truncatelength(&self->__buffer, self->__buffer.length - 1);
	}
	else
	{
		/* there's no previous nul term, so start at beginning of array */
		startpoint = 0;
	}

	/* add zeros to reserve enough space, and provide nul-term*/
	bcarray_addzeros(&self->__buffer, size + 1 /*nul-term*/);

	/* print directly into buffer*/
	size = vsnprintf((char*)bcarray_at(&self->__buffer, startpoint), size+1, fmt, args);

	/* update convenience pointer (could have changed if we realloc'd) */
	self->s = (const char*)&self->__buffer.buffer[0];
	return size;
}

/* on format error, returns -1 and leaves string unchanged */
void bcstr_addfmt_withoutcheck(bcstr* self, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	(void)bcstr_addvfmt(self, fmt, args);
	va_end(args);
}

/* help prevent using user-controlled buffer as format spec. */
#define bcstr_addfmt(self, msg, ...) do { \
	STATIC_ASSERT(sizeof(msg)!=sizeof(char*)); \
	bcstr_addfmt_withoutcheck((self), (msg), ##__VA_ARGS__); \
	} while (0)

Uint32 bcstr_length(const bcstr* self)
{
	return self->__buffer.length ? self->__buffer.length-1 : 0;
}

void bcstr_truncate(bcstr* self, Uint32 newlength)
{
	check_fatal(newlength <= self->__buffer.length);
	bcarray_truncatelength(&self->__buffer, newlength + 1);
	*(bcarray_at(&self->__buffer, newlength)) = 0; /* null term*/
}

void bcstr_trimwhitespace(bcstr* self)
{
	if (bcstr_length(self) == 0)
		return;

	/* look backwards until finding a non-space character*/
	Uint32 index = bcstr_length(self);
	while (index--)
	{
		if (!isspace(self->s[index]))
			break;
	}

	bcstr_truncate(self, index+1);
}

/* creates a string of (stringlength+1) zeros. */
void bcstr_data_settozeros(bcstr* self, Uint32 stringlength)
{
	bcstr_clear(self);
	bcarray_addzeros(&self->__buffer, stringlength +
		1/*nulterm*/ - self->__buffer.length /*already allocated*/);

	/* update convenience pointer (could have changed if we realloc'd) */
	self->s = (const char*)&self->__buffer.buffer[0];
}

char* bcstr_data_get(bcstr* self)
{
	return (char*)&self->__buffer.buffer[0];
}

void bcstr_close(bcstr* self)
{
	if (self)
	{
		bcarray_close(&self->__buffer);
		set_self_zero();
	}
}

bool s_equal(const char* s1, const char* s2)
{
	return strcmp(s1, s2) == 0;
}

bool s_startswithlen(const char* s1, Uint32 len1, const char* s2, Uint32 len2)
{
	if (len1 < len2)
	{
		return false;
	}

	return memcmp(s1, s2, len2) == 0;
}

bool s_startswith(const char* s1, const char* s2)
{
	return s_startswithlen(s1, strlen32(s1), s2, strlen32(s2));
}

bool s_endswithlen(const char* s1, Uint32 len1, const char* s2, Uint32 len2)
{
	if (len1 < len2)
	{
		return false;
	}

	return memcmp(s1+(len1-len2), s2, len2) == 0;
}

bool s_endswith(const char* s1, const char*s2)
{
	return s_endswithlen(s1, strlen32(s1), s2, strlen32(s2));
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
		/* note that isdigit should take unsigned char */
		if (!isdigit((unsigned char)*(ptrIter++)))
		{
			return false;
		}
	}

	/* warning: strtoul silently fails when passed a number that is too large,
	one must check errno. */
	errno = 0;
	char* ptrresult = NULL;
	int res = strtoul(s, &ptrresult, 10);
	if (!*ptrresult && errno == 0)
	{
		*result = res;
		return true;
	}

	return false;
}

/* string array. */
typedef struct bcstrlist {
	bcarray arr;
	Uint32 length; /* distinct from arr.length, because we reuse memory */
} bcstrlist;

bcstrlist bcstrlist_open()
{
	bcstrlist self = {};
	self.arr = bcarray_open(sizeof32(bcstr), 0);
	return self;
}

/* returned pointer will have a short lifetime -- use with care. */
const char* bcstrlist_viewat(const bcstrlist* self, Uint32 index)
{
	check_fatal(index < self->length);
	return ((const bcstr*)bcarray_atconst(&self->arr, index))->s;
}

bcstr* bcstrlist_viewstr(bcstrlist* self, Uint32 index)
{
	check_fatal(index < self->length);
	return (bcstr*)bcarray_at(&self->arr, index);
}

/* to reduce allocations, we reuse strings */
void bcstrlist_pushbackszlen(bcstrlist* self, const char* s, Uint32 len)
{
	check_fatal(self->length <= self->arr.length);
	if (self->length == self->arr.length)
	{
		/* we'll have to make a new string */
		bcstr str = bcstr_open();
		bcstr_appendszlen(&str, s, len);
		bcarray_add(&self->arr, (byte*)&str, 1);
	}
	else
	{
		/* re-use existing string */
		bcstr* prevstr = (bcstr*)bcarray_at(&self->arr, self->length);
		bcstr_assignszlen(prevstr, s, len);
	}
	self->length++;
}

void bcstrlist_pushback(bcstrlist* self, const char* s)
{
	bcstrlist_pushbackszlen(self, s, strlen32(s));
}

void bcstrlist_pop(bcstrlist* self)
{
	check_fatal(self->length > 0);
	self->length--;
}

void bcstrlist_clear(bcstrlist* self, bool release_unused_memory)
{
	if (release_unused_memory)
	{
		/* note self->arr.length, not self->length here*/
		for (Uint32 i=0; i<self->arr.length; i++)
		{
			bcstr_close((bcstr*)bcarray_at(&self->arr, i));
		}
	}

	self->length = 0;
}

#define bcstrlist_openwith(...) \
	bcstrlist_openwith_impl(CAST(StringParams) {{ __VA_ARGS__ }})
bcstrlist bcstrlist_openwith_impl(StringParams strings)
{
	bcstrlist ret = bcstrlist_open();
	for (int i = 0; i < countof(strings.strings); i++)
	{
		if (strings.strings[i])
		{
			bcstrlist_pushback(&ret, strings.strings[i]);
		}
	}

	return ret;
}

/* returns ownership, unlike bcstrlist_viewat. */
void bcstrlist_at(const bcstrlist* self, Uint32 index, bcstr* out)
{
	check_fatal(index < self->length);
	const char* s = bcstrlist_viewat(self, index);
	Uint32 len = bcstr_length((const bcstr*)bcarray_atconst(&self->arr, index));
	bcstr_assignszlen(out, s, len);
}

void bcstrlist_add_str_split(bcstrlist* self, const char* sz, char cDelim)
{
	bcstr strTemp = bcstr_open();
	const char* szStart = sz;
	const char* szNext = 0;
	while ((szNext = strchr(szStart, cDelim)) != 0)
	{
		bcstr_clear(&strTemp);
		bcstr_appendszlen(&strTemp, szStart, safecast32u(szNext-szStart));
		bcstrlist_pushback(self, strTemp.s);
		szStart = szNext+1;
	}

	bcstrlist_pushback(self, szStart);
	bcstr_close(&strTemp);
}

static int ComparisonFn_bcstrlist(const void* pt1, const void* pt2)
{
	return strcmp(
		((const bcstr*)pt1)->s,
		((const bcstr*)pt2)->s);
}

void bcstrlist_sort(bcstrlist* self)
{
	qsort(&self->arr.buffer[0], self->length /*not self->arr.length*/,
		self->arr.elementsize, ComparisonFn_bcstrlist);
}

bcstr bcstrlist_tostring(bcstrlist* self)
{
	bcstr ret = bcstr_open();
	bcstr_addfmt(&ret, "length=%d\n", self->length);

	for (Uint32 i=0; i<self->length; i++)
	{
		bcstr_addfmt(&ret, "index %d:'%s'\n",
			i, bcstrlist_viewat(self, i));
	}

	return ret;
}

void bcstrlist_close(bcstrlist* self)
{
	if (self)
	{
		bcstrlist_clear(self, true /*release memory*/);
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
	Uint32 counter;
	bool suppress_dialogs;

	/* adjustable for testing */
	int approx_max_filesize;
	Uint32 how_often_checkfilesize;
} bc_log;

/* forward declares for logging */
Uint64 os_unixtime();
CHECK_RESULT struct Result filenamepattern_getlatest(const char* filename_template,
	const char* ext, int *out_number, struct bcstr* out);
void filenamepattern_gen(const char* filename_template, const char* ext,
	int number, struct bcstr* out);

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

	self->file;
	self->filename_template = bcstr_openwith(filename_template);
	self->filename_current = bcstr_open();
	self->suppress_dialogs = false;
	self->approx_max_filesize = 8*1024*1024;
	self->how_often_checkfilesize = 32;

	/* get filename*/
	check(filenamepattern_getlatest(filename_template, ".txt",
		&self->filenumber_current, &self->filename_current));

	/* open file */
	check(bcfile_open(&self->file, self->filename_current.s, "ab"));

cleanup:
	return currenterr;
}

void bc_log_checkfilesize(bc_log* self)
{
	self->counter++;
	if (self->counter % self->how_often_checkfilesize == 0)
	{
		fseek(self->file.file, 0, SEEK_END);
		if (ftell(self->file.file) > self->approx_max_filesize)
		{
			/* get next filename */
			bcstr next_filename = bcstr_open();
			self->filenumber_current++;
			filenamepattern_gen(self->filename_template.s, ".txt", self->filenumber_current, &next_filename);

			/* open file, if the open fails we'll just keep using the old one. */
			bcfile next_file = {};
			Result result = bcfile_open(&next_file, next_filename.s, "ab");
			if (result.errortag)
			{
				check_warn(0, "failed to open next log file ", next_filename.s);
				bcfile_close(&next_file);
				bcstr_close(&next_filename);
				Result_close(&result);
			}
			else
			{
				/* replace current file+filename with this file+filename*/
				bcfile_close(&self->file);
				bcstr_close(&self->filename_current);
				self->file = next_file;
				self->filename_current = next_filename;
			}
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

		Uint64 approx_seconds = os_unixtime() - 16436*24*60*60 /*days since 2015*/;
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

		if (read < 0 && !s_contains(fmt, "warn") /*avoid inf recursion*/)
			check_warn(0, "format failure in bc_log_writef"
			"format string is ", fmt);
	}
}

void bc_log_flush(bc_log* self)
{
	if (self->file.file)
		fflush(self->file.file);

}

bool bc_log_logerror(struct bc_log* self, struct Result* result,
	int errortag, const char* stringifiedcode, int lineno,
	const char* fnname, const char* file, bc_log_errcontext_t params)
{
	/* first, write to the file before any memory allocs. */
	bc_log_writeraw(self, newline "error or warning occurred. ");
	bc_log_writeraw(self, stringifiedcode);
	bc_log_flush(self);

	/* now build a long string with all the info we have */
	bcstr str_err = bcstr_openwith("error or warning ");
	bcstr_append(&str_err, params.sz1, params.sz2);
	if (params.n1)
		bcstr_addfmt(&str_err, " %d ", params.n1);
	if (params.n2)
		bcstr_addfmt(&str_err, " %d ", params.n2);
	if (params.wz)
		bcstr_addfmt(&str_err, " %S ", params.wz);

	bcstr_addfmt(&str_err, newline " on line %d, fn %s, file %s ", lineno, fnname, file);
	if (errortag != 0 && errortag != -1)
		bcstr_addfmt(&str_err, newline " tag %d", errortag);

	/* write long string to log*/
	if (self->file.file)
	{
		bc_log_write(self, str_err.s);
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

/* help prevent using user-controlled buffer as format spec. */
#define logwritef(msg, ...) do { \
	STATIC_ASSERT(sizeof(msg)!=sizeof(char*));\
	 bc_log_writef(&g_log, (msg), ##__VA_ARGS__); \
	} while (0)

/* wchar_t string */
typedef struct bcwstr {
	bcarray buffer;
	const wchar_t* s; /* pointer into buffer, for convenience */
} bcwstr;

void bcwstr_appendwzlen(bcwstr* self, const wchar_t* wz, Uint32 length)
{
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

void bcwstr_appendwz(bcwstr* self, const wchar_t* wz)
{
	bcwstr_appendwzlen(self, wz, wcslen32(wz));
}

void bcwstr_clear(bcwstr* self)
{
	if (self->buffer.length > 0) /* this check can save an allocation */
	{
		bcarray_clear(&self->buffer);
		bcwstr_appendwzlen(self, L"", 0); /*adds null char*/
	}
}

Uint32 bcwstr_length(bcwstr* self)
{
	return self->buffer.length ? self->buffer.length - 1 : 0;
}

bcwstr bcwstr_open(void)
{
	bcwstr ret = {};
	ret.buffer = bcarray_open(sizeof32(wchar_t), 0);
	bcwstr_appendwzlen(&ret, L"", 0);
	return ret;
}

bcwstr bcwstr_openwz(const wchar_t* wz)
{
	bcwstr ret = {};
	ret.buffer = bcarray_open(sizeof32(wchar_t), 0);
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

uint64_t make_uint64(uint32_t hi, uint32_t lo)
{
	return lo | (((uint64_t)hi) << 32);
}

#define chars_to_uint32(c1, c2, c3, c4) \
		(((Uint32)(c1)) << 24) | (((Uint32)(c2)) << 16) | \
		(((Uint32)(c3)) << 8) | ((Uint32)(c4))

void bytes_to_string(const byte* b, Uint32 len, bcstr* s)
{
	bcstr_clear(s);
	for (Uint32 i = 0; i < len; i++)
		bcstr_addfmt(s, "%02x", b[i]);
}

void print_error_if_ocurred(const char* scontext, Result* err)
{
	if (err->errortag)
	{
		printf("error occurred, %s\ndetails: %s (%d)\n",
			scontext,
			err->errormsg ? err->errormsg : "",
			err->errortag);

		alertdialog("");
	}

	Result_close(err);
}

void ask_user_prompt(const char* prompt, bool allow_empty, bcstr* out)
{
	bcstr_clear(out);
	while (1)
	{
		char buffer[1024] = "";
		puts(prompt);
		fflush(stdout);
		if (fgets(buffer, countof(buffer)-1, stdin) == NULL || buffer[0] == '\n')
		{
			if (allow_empty)
			{
				break;
			}
			else
			{
				continue;
			}
		}

		if (buffer[strlen(buffer) - 1] == '\n')
		{
			buffer[strlen(buffer) - 1] = '\0';
			bcstr_assign(out, buffer);
			break;
		}
		else
		{
			/* eat excess characters */
			while (getchar()!='\n');
			printf("String entered was too long, max of %d characters\n", countof32(buffer) - 1);
			continue;
		}
	}
}

bool ask_user_yesno(const char* prompt)
{
	bcstr result = bcstr_open();
	bool ret = false;
	while (1)
	{
		ask_user_prompt(prompt, false, &result);
		if (s_equal(result.s, "y"))
		{
			ret = true;
		}
		else if (s_equal(result.s, "n"))
		{
			ret = false;
		}
		else
		{
			continue;
		}

		break;
	}
	bcstr_close(&result);
	return ret;
}

int ask_user_int(const char* prompt, int valmin, int valmax)
{
	bcstr result = bcstr_open();
	int ret = 0;
	while (1)
	{
		ask_user_prompt(prompt, false, &result);

		int n = INT_MIN;
		if (s_intfromstring(result.s, &n) && n >= valmin && n <= valmax)
		{
			ret = n;
			break;
		}
	}
	bcstr_close(&result);
	return ret;
}

Uint32 ask_user_intchoice(const char* prompt, const bcstrlist* list)
{
	puts(prompt);
	puts("0) cancel");
	for (Uint32 i = 0; i < list->length; i++)
	{
		printf("%d) %s", i + 1, bcstrlist_viewat(list, i));
	}
	return ask_user_int("", 0, list->length);
}
