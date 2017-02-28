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

#ifndef UTIL_H_INCLUDE
#define UTIL_H_INCLUDE

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
#include <fcntl.h>
#include <sys/stat.h>
#include "lib_bstrlib.h"

/* platform differences */
#ifndef _MSC_VER
#include <byteswap.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include <wchar.h>
#include <sys/file.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <inttypes.h>

#define staticassert(e) _Static_assert((e), "")
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#define pathsep "/"
#define nativenewline "\n"
#define ignore_unused_result(x) do {int ignored __attribute__((unused)) = x;} while(0)
#define CONSTRUCT(structname) (structname)
#define DEBUGBREAK() do { if (ask_user_yesno("Break?")) raise(SIGTRAP); } while(0)
#define IO_PREFIX(iofn) iofn
#define __FUNCTION__ __func__

#else /* _MSC_VER */
#include <windows.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <psapi.h>
#include <shlobj.h>

#define staticassert(e) static_assert((e), "")
#define countof _countof
#define pathsep "\\"
#define nativenewline "\r\n"
#define snprintf _snprintf
#define ignore_unused_result(x) (void)x
#define CONSTRUCT(structname) 
#define DEBUGBREAK() do { if (IsDebuggerPresent()) DebugBreak(); } while(0)
#define PATH_MAX MAX_PATH
#pragma warning(disable:4996)
#endif

/* sv_array */
typedef unsigned char byte;
typedef struct sv_array {
	byte *buffer;
	uint32_t elementsize;
	uint32_t capacity; /* in terms of elementsize*/
	uint32_t length; /* in terms of elementsize*/
} sv_array;

typedef uint8_t byte;
sv_array sv_array_open(uint32_t elementsize, uint32_t initialcapacity);
void sv_array_reserve(sv_array *self, uint32_t requestedcapacity);
void sv_array_append(sv_array *self, const void *inbuffer, uint32_t incount);
void sv_array_appendzeros(sv_array *self, uint32_t incount);
void sv_array_truncatelength(sv_array *self, uint32_t newlength);
void sv_array_clear(sv_array *self);
byte *sv_array_at(sv_array *self, uint32_t index);
const byte *sv_array_atconst(const sv_array *self, uint32_t index);
uint64_t sv_array_at64u(const sv_array *self, uint32_t index);
void sv_array_add64u(sv_array *self, uint64_t n);
uint32_t sv_array_64ulowerbound(sv_array *self, uint64_t n);
void sv_array_close(sv_array *self);

/* sv_2d_array */
typedef struct sv_2d_array {
	sv_array arr;
	uint32_t elementsize;
} sv_2d_array;
sv_2d_array sv_2d_array_open(uint32_t elementsize);
void sv_2d_array_ensure_space(sv_2d_array *self, uint32_t d1, uint32_t d2);
const byte *sv_2d_array_atconst(const sv_2d_array *self, uint32_t d1, uint32_t d2);
byte *sv_2d_array_at(sv_2d_array *self, uint32_t d1, uint32_t d2);
void sv_2d_array_close(sv_2d_array *self);

/* sv_pseudosplit */
typedef struct sv_pseudosplit {
	bstring text;
	bstring currentline;
	sv_array splitpoints;
} sv_pseudosplit;

/* like splitting a string, but faster */
sv_pseudosplit sv_pseudosplit_open(const char *s);
void sv_pseudosplit_split(sv_pseudosplit *self, char delim);
const char *sv_pseudosplit_viewat(sv_pseudosplit *self, uint32_t linenumber);
void sv_pseudosplit_at(sv_pseudosplit *self, uint32_t linenumber, bstring s);
uint32_t sv_pseudosplit_pos_to_linenumber(sv_pseudosplit *self, uint64_t offset);
void sv_pseudosplit_close(sv_pseudosplit *self);

/* sv_result */
typedef struct sv_result {
	bstring msg;
	int code;
} sv_result;

extern sv_result OK;
void sv_result_close(sv_result *self);

/* enforce checking return type */
#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CHECKRETURN __attribute__ ((warn_unused_result))
#define unused(type) __attribute__((__unused__)) type TOKENPASTE2(unusedparam, __COUNTER__)
#define unused_ptr(type) __attribute__((__unused__)) type * TOKENPASTE2(unusedparam, __COUNTER__)
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECKRETURN _Check_return_
#define unused(type) type
#define unused_ptr(type) type *
#else
#define CHECKRETURN
#define unused(type) type
#define unused_ptr(type) type *
#endif
#define check_result CHECKRETURN sv_result

/* check for integer overflow */
uint32_t cast64u32u(uint64_t n);
int32_t cast64s32s(int64_t n);
int32_t cast64u32s(uint64_t n);
int64_t cast64u64s(uint64_t n);
uint64_t cast64s64u(int64_t n);
int32_t cast32u32s(uint32_t n);
uint32_t cast32s32u(int32_t n);
uint32_t checkedmul32(uint32_t a, uint32_t b);
uint32_t checkedadd32(uint32_t a, uint32_t b);
int32_t checkedadd32s(int32_t a, int32_t b);
uint32_t round_up_to_multiple(uint32_t a, uint32_t mod);
uint32_t nearest_power_of_two32(uint32_t a);
uint64_t make_uint64(uint32_t hi, uint32_t lo);
uint32_t upper32(uint64_t n);
uint32_t lower32(uint64_t n);
uint32_t cast_size_t_32u(size_t n);
int32_t cast_size_t_32s(size_t n);
int32_t castls32s(long n);
uint32_t castlu32s(unsigned long n);
long long castll(int64_t n);
unsigned long long castull(uint64_t n);
#define sizeof32u(x) cast_size_t_32u(sizeof(x))
#define sizeof32s(x) cast_size_t_32s(sizeof(x))
#define countof32u(x) cast_size_t_32u(countof(x))
#define countof32s(x) cast_size_t_32s(countof(x))
#define strlen32u(x) cast_size_t_32u(strlen(x))
#define strlen32s(x) cast_size_t_32s(strlen(x))
#define wcslen32u(x) cast_size_t_32u(wcslen(x))
#define wcslen32s(x) cast_size_t_32s(wcslen(x))

/* allocation */
byte *sv_realloc(byte *ptr, uint32_t a, uint32_t b);
byte *sv_calloc(uint32_t a, uint32_t b);
#define sv_freenull(p) do { free(p); (p) = NULL; } while(0)

/* string utils */
bool s_equal(const char *s1, const char *s2);
bool s_startswithlen(const char *s1, int len1, const char *s2, int len2);
bool s_startswith(const char *s1, const char *s2);
bool s_endswithlen(const char *s1, int len1, const char *s2, int len2);
bool s_endswith(const char *s1, const char *s2);
bool s_contains(const char *s1, const char *s2);
bool s_isalphanum_paren_or_underscore(const char *s);
bool ws_equal(const wchar_t *s1, const wchar_t *s2);
bool uintfromstring(const char *s, uint64_t *result);
bool uintfromstringhex(const char *s, uint64_t *result);
bool fnmatch_simple(const char *pattern, const char *string);
void fnmatch_checkvalid(const char *pattern, bstring response);

inline bstring bstring_open(void)
{
	return bstr_fromstatic("");
}

inline void bstrclear(bstring s)
{
	bassignblk(s, "", 0);
}

#define WARN_NUL_IN_STR 0
#if WARN_NUL_IN_STR || defined(_DEBUG)
#define cstr(s) bstr_warnnull_cstr(s)
#define wcstr(s) wcstr_warnnull_cstr(&(s))
#else
#define cstr(s) ((const char *)(s)->data)
#define wcstr(s) ((const wchar_t *)sv_array_atconst(&(s).arr, 0))
#endif
#define wpcstr(s) ((const wchar_t *)sv_array_atconst(&(s)->arr, 0))
#define checkstr(expression) check_b(BSTR_OK == (expression), "string op failed")
#define bassignformat(param, ...) do { bstrclear(param); bformata(param, __VA_ARGS__); } while(0)
const char *bstr_warnnull_cstr(const bstring s);
const char *bstrlist_view(const bstrlist *list, int index);
bstrlist *bstrlist_copy(const bstrlist *list);
void bstrlist_split(bstrlist *list, const bstring bstr, const bstring bdelim);
void bstrlist_splitcstr(bstrlist *list, const char *s, const char *delim);
void bstrlist_clear(bstrlist *list);
void bstrlist_sort(bstrlist *list);
int bstrlist_append(bstrlist *list, const bstring bs);
int bstrlist_appendcstr(bstrlist *list, const char *s);
int bstrlist_concat(bstrlist *list, const bstrlist *otherlist);
int bstrlist_remove_at(bstrlist *list, int index);
int bstring_alloczeros(bstring s, int len);
void bytes_to_string(const void *b, uint32_t len, bstring s);
int bstr_replaceall(bstring s, const char *find, const char *replacewith);
bool bstr_equal(const bstring s1, const bstring s2);

#define chars_to_uint32(c1, c2, c3, c4) \
		(((uint32_t)(c1)) << 24) | (((uint32_t)(c2)) << 16) | /* allow cast */ \
		(((uint32_t)(c3)) << 8) | ((uint32_t)(c4)) /* allow cast */

/* sv_wstr */
typedef struct sv_wstr {
	sv_array arr;
} sv_wstr;

sv_wstr sv_wstr_open(uint32_t initial_length);
const wchar_t *wcstr_warnnull_cstr(const sv_wstr *s);
void sv_wstr_close(sv_wstr *self);
void sv_wstr_alloczeros(sv_wstr *self, uint32_t len);
void sv_wstr_truncate(sv_wstr *self, uint32_t expected_len);
void sv_wstr_append(sv_wstr *self, const wchar_t *s);
void sv_wstr_clear(sv_wstr *self);
void sv_wstrlist_clear(sv_array *list);

/* ui */
void ask_user_prompt(const char *prompt, bool q_to_cancel, bstring out);
bool ask_user_yesno(const char *prompt);
uint32_t ask_user_int(const char *prompt, int valmin, int valmax);
void alertdialog(const char *message);

/* ui menus */
typedef enum
{
	continue_on_err = 0,
	exit_on_err,
} erespondtoerr;

struct svdp_application;
typedef sv_result(*FnMenuCallback)(struct svdp_application *, int);
typedef struct ui_numbered_menu_spec_entry {
	const char *message;
	FnMenuCallback callback;
	int arg;
} ui_numbered_menu_spec_entry;

typedef const struct ui_numbered_menu_spec_entry *(*FnMenuGetNextMenu) (struct svdp_application *);
int ui_numbered_menu_pick_from_long_list(const bstrlist *list, int groupsize);
check_result ui_numbered_menu_show(const char *msg, const ui_numbered_menu_spec_entry *entries,
	struct svdp_application *app, FnMenuGetNextMenu getnextmenu);
int ui_numbered_menu_pick_from_list(const char *msg, const bstrlist *list,
	const char *format_each, const char *additionalopt1, const char *additionalopt2);

/* sv_file */
typedef struct sv_file {
	FILE *file;
} sv_file;

/* sv_log */
typedef struct sv_log {
	bstring dir;
	sv_file logfile;
	uint32_t logfilenumber;
	uint32_t counter;
	int32_t cap_filesize;
	int64_t start_of_day;
} sv_log;

check_result sv_log_open(sv_log *self, const char *dir);
void sv_log_register_active_logger(sv_log *logger);
void sv_log_close(sv_log *self);
void sv_log_writefmt(const char *fmt, ...);
void sv_log_writeline(const char *s);
void sv_log_writelines(const char *s1, const char *s2);
void sv_log_flush(void);
void sv_log_addnewlinetime(FILE *f, int64_t start_of_day, int64_t seconds, long milliseconds);
void appendnumbertofilename(const char *dir,
	const char *prefix, const char *suffix, uint32_t number, bstring out);
uint32_t readnumberfromfilename(bstring tmp,
	const char *prefix, const char *suffix, const char *candidate);
check_result readlatestnumberfromfilename(const char *dir,
	const char *prefix, const char *suffix, uint32_t *latestnumber);

/* error handling */
void check_b_hit(void);
void debugbreak(void);
void die(void);
void set_debugbreaks_enabled(bool b);
void check_warn_impl(sv_result res,
	const char *msg, const char *fnname, erespondtoerr respondtoerr);

#ifdef _DEBUG
#define debugonly(x) x
#else
#define debugonly(x)
#endif

/* use debugonly(if (0) printf...) to let the compiler validate format strings. */

#define check(expression) do { \
	currenterr = (expression); \
	if (currenterr.code) { \
		sv_log_writefmt("leaving scope, %s", (#expression)); \
		sv_log_flush(); \
		goto cleanup; \
	} } while(0)

#define check_b(cond, ...) do { \
	if (!(cond)) { \
		sv_log_writefmt(__VA_ARGS__); \
		sv_log_writefmt("leaving scope, %s fn %s line %d", \
			(#cond), __FUNCTION__, __LINE__); \
		check_b_hit(); \
		currenterr.msg = bformat(__VA_ARGS__); \
		currenterr.code = -1; \
		debugonly(if (0) { printf(__VA_ARGS__); }) \
		goto cleanup; \
	} } while(0)

#define check_fatal(cond, ...) do { \
	if (!(cond)) { \
		sv_log_writefmt(__VA_ARGS__); \
		sv_log_writefmt("exiting, %s fn %s line %d", \
			(#cond), __FUNCTION__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		die(); \
	} } while(0)

#define check_sql(db, code) do { \
	int coderesult = (code); \
	if (coderesult != SQLITE_OK && coderesult != SQLITE_ROW && coderesult != SQLITE_DONE) { \
		check_b(0, "sqlite returned %s %d " #code, \
			((db) ? sqlite3_errmsg(db) : \
			sqlite3_errstr(coderesult)), coderesult); \
	} } while (0)

#define check_warn(p1, p2, p3) \
	check_warn_impl((p1), (p2), __FUNCTION__, (p3))

#define log_b(cond, ...) do { \
	if (!(cond)) { \
		sv_log_writefmt("'%s', '%s'", (#cond), __FUNCTION__); \
		sv_log_writefmt(__VA_ARGS__); \
		debugonly(if (0) { printf(__VA_ARGS__); }) \
	} } while(0)

#define log_errno(vardatatypeandname, expression, ...) \
	errno = 0; \
	int TOKENPASTE2(log_errnoval, __LINE__) = (expression); \
	vardatatypeandname = TOKENPASTE2(log_errnoval, __LINE__); \
	const char *TOKENPASTE2(log_errnocontext , __LINE__)[4] = {__VA_ARGS__}; \
	do { if ((TOKENPASTE2(log_errnoval, __LINE__)) < 0) { \
		log_errno_impl( \
			#expression, errno, TOKENPASTE2(log_errnocontext, __LINE__), __FUNCTION__, __LINE__); \
	} } while (0)

#define check_errno(vardatatypeandname, expression, ...) \
	log_errno(vardatatypeandname, expression, __VA_ARGS__); \
	do { if ((TOKENPASTE2(log_errnoval, __LINE__)) < 0) { \
		check_errno_impl( \
			&currenterr, #expression, errno, TOKENPASTE2(log_errnocontext, __LINE__), __FUNCTION__); \
		goto cleanup; \
	} } while (0)

#define log_win32(vardatatypeandname, expression, failureval, ...) \
	SetLastError(0); \
	auto TOKENPASTE2(log_errwin32val, __LINE__) = (expression); \
	vardatatypeandname = TOKENPASTE2(log_errwin32val, __LINE__); \
	DWORD TOKENPASTE2(log_errwin32lasterr, __LINE__) = GetLastError(); \
	const char *TOKENPASTE2(log_errwin32context, __LINE__)[4] = {__VA_ARGS__}; \
	do { if ((TOKENPASTE2(log_errwin32val, __LINE__)) == (failureval)) { \
		log_errwin32_impl(#expression, TOKENPASTE2(log_errwin32lasterr, __LINE__), \
			TOKENPASTE2(log_errwin32context, __LINE__), __FUNCTION__, __LINE__); \
	} } while (0)

#define check_win32(vardatatypeandname, expression, failureval, ...) \
	log_win32(vardatatypeandname, expression, failureval, __VA_ARGS__); \
	do { if ((TOKENPASTE2(log_errwin32val, __LINE__)) == (failureval)) { \
		check_errwin32_impl(&currenterr, #expression, TOKENPASTE2(log_errwin32lasterr, __LINE__), \
			TOKENPASTE2(log_errwin32context, __LINE__), __FUNCTION__); \
		goto cleanup; \
	} } while (0)


#define CheckBformatStrings 0
#if CheckBformatStrings && _DEBUG
#undef bassignformat
#define bassignformat(s, ...) s; printf(__VA_ARGS__)
#define bformata(s, ...) s; printf(__VA_ARGS__)
#define bformat(...) (printf(__VA_ARGS__), hhh)
#define sv_log_writefmt printf
extern bstring PlaceholderCheckFormatStrings;
#endif

void log_errno_impl(const char *exp,
	int nerrno, const char *context[4], const char *fn, int lineno);
void check_errno_impl(sv_result *currenterr,
	const char *exp, int nerrno, const char *context[4], const char *fn);
void log_errwin32_impl(const char *exp,
	unsigned long nerrno, const char *context[4], const char *fn, int lineno);
void check_errwin32_impl(sv_result *currenterr,
	const char *exp, unsigned long nerrno, const char *context[4], const char *fn);
extern int _;

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define set_self_zero() \
	memset((self), 0, sizeof(*(self)))

#define str_and_len(s) \
	("" s ""), (countof32u(s) - 1)

#define str_and_len32s(s) \
	("" s ""), (countof32s(s) - 1)

#endif
