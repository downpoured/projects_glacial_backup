/*
util.h

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
#define IO_PREFIX(iofn) iofn
#define __FUNCTION__ __func__
#define noreturn_start()
#define noreturn_end()
#define ignore_unused_result(x) \
    do {int ignored __attribute__((unused)) = x;} while(0)
#define DEBUGBREAK() \
    do { if (ask_user("Break?")) raise(SIGTRAP); } while(0)

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
#define PATH_MAX MAX_PATH
#pragma warning(disable:4996)
#define noreturn_start() __pragma(warning(disable:4702))
#define noreturn_end() __pragma(warning(default:4702))
#define DEBUGBREAK() \
    do { if (IsDebuggerPresent()) DebugBreak(); } while(0)

#endif

typedef uint8_t byte;
typedef struct sv_array {
    byte *buffer;
    uint32_t elementsize;
    uint32_t capacity;
    uint32_t length;
} sv_array;

sv_array sv_array_open(uint32_t elementsize, uint32_t initiallength);
sv_array sv_array_open_u64();
void sv_array_reserve(sv_array *self, uint32_t requestedcapacity);
void sv_array_append(sv_array *self, const void *inbuffer, uint32_t incount);
void sv_array_appendzeros(sv_array *self, uint32_t incount);
void sv_array_truncatelength(sv_array *self, uint32_t newlength);
byte *sv_array_at(sv_array *self, uint32_t index);
const byte *sv_array_atconst(const sv_array *self, uint32_t index);
uint64_t sv_array_at64u(const sv_array *self, uint32_t index);
void sv_array_add64u(sv_array *self, uint64_t n);
void sv_array_close(sv_array *self);

typedef struct sv_result {
    bstring msg;
    int code;
} sv_result;

extern sv_result OK;
void sv_result_close(sv_result *self);

typedef sv_result(*sv_2darray_iter_cb)(void *context,
    uint32_t x, uint32_t y, byte *val);

typedef struct sv_2darray {
    sv_array arr;
    uint32_t elementsize;
} sv_2darray;

sv_2darray sv_2darray_open(uint32_t elementsize);
byte *sv_2darray_get_expand(sv_2darray *self, uint32_t d1, uint32_t d2);
byte *sv_2darray_at(sv_2darray *self, uint32_t d1, uint32_t d2);
void sv_2darray_close(sv_2darray *self);
const byte *sv_2darray_atconst(const sv_2darray *self,
    uint32_t d1,
    uint32_t d2);

typedef struct sv_pseudosplit {
    bstring text;
    bstring currentline;
    sv_array splitpoints;
} sv_pseudosplit;

/* like splitting a string, but faster */
sv_pseudosplit sv_pseudosplit_open(const char *s);
void sv_pseudosplit_split(sv_pseudosplit *self, char delim);
void sv_pseudosplit_close(sv_pseudosplit *self);
const char *sv_pseudosplit_viewat(sv_pseudosplit *self,
    uint32_t linenumber);

/* enforce checking returned value */
#define check_result CHECKRETURN sv_result
#define CONCAT2(x, y) x ## y
#define CONCAT(x, y) CONCAT2(x, y)
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CHECKRETURN __attribute__ ((warn_unused_result))
#define unused(type) __attribute__((__unused__)) \
    type CONCAT(unusedparam, __COUNTER__)
#define unused_ptr(type) __attribute__((__unused__)) \
    type * CONCAT(unusedparam, __COUNTER__)
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECKRETURN _Check_return_
#define unused(type) type
#define unused_ptr(type) type *
#else
#define CHECKRETURN
#define unused(type) type
#define unused_ptr(type) type *
#endif

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
uint32_t nearest_power_of_two(uint32_t a);
uint64_t make_u64(uint32_t hi, uint32_t lo);
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

byte *sv_realloc(byte *ptr, uint32_t a, uint32_t b);
byte *sv_calloc(uint32_t count, uint32_t size);
#define sv_freenull(p) do { free(p); (p) = NULL; } while(0)

/* string utils */
bool s_equal(const char *s1, const char *s2);
bool s_startwithlen(const char *s1, int len1, const char *s2, int len2);
bool s_startwith(const char *s1, const char *s2);
bool s_endwithlen(const char *s1, int len1, const char *s2, int len2);
bool s_endwith(const char *s1, const char *s2);
bool s_contains(const char *s1, const char *s2);
bool s_isalphanum_paren_or_underscore(const char *s);
bool ws_equal(const wchar_t *s1, const wchar_t *s2);
bool uintfromstr(const char *s, uint64_t *result);
bool uintfromstrhex(const char *s, uint64_t *result);
bool fnmatch_simple(const char *pattern, const char *string);
void fnmatch_isvalid(const char *pattern, bstring response);

check_result sv_2darray_foreach(sv_2darray *self,
    sv_2darray_iter_cb cb,
    void *context);

#define WARN_NUL_IN_STR 0
#if WARN_NUL_IN_STR || defined(_DEBUG)
#define cstr(s) bstr_warnnull_cstr(s)
#define wcstr(s) wcstr_warnnull_cstr(&(s))
#else
#define cstr(s) ((const char *)(s)->data)
#define wcstr(s) ((const wchar_t *)sv_array_atconst(&(s).arr, 0))
#endif
#define wpcstr(s) ((const wchar_t *)sv_array_atconst(&(s)->arr, 0))
#define bstring_open() bstr_fromstatic("")
#define bstrclear(s) bassignblk(s, "", 0)
const char *bstr_warnnull_cstr(const bstring s);
const char *blist_view(const bstrlist *list, int index);
void bstrlist_split(bstrlist *list, const bstring bstr, char cdelim);
void bstrlist_splitcstr(bstrlist *list, const char *s, char cdelim);
void bstrlist_clear(bstrlist *list);
void bstrlist_sort(bstrlist *list);
int bstrlist_append(bstrlist *list, const bstring bs);
int bstrlist_appendcstr(bstrlist *list, const char *s);
int bstrlist_remove_at(bstrlist *list, int index);
int bstring_calloc(bstring s, int len);
void bytes_to_string(const void *b, uint32_t len, bstring s);
int bstr_replaceall(bstring s, const char *find, const char *replacewith);
bool bstr_equal(const bstring s1, const bstring s2);

#define chars_to_uint32(c1, c2, c3, c4) \
    (((uint32_t)(c1)) << 24) | (((uint32_t)(c2)) << 16) | /* allow cast */ \
    (((uint32_t)(c3)) << 8) | ((uint32_t)(c4)) /* allow cast */

typedef struct sv_wstr {
    sv_array arr;
} sv_wstr;

sv_wstr sv_wstr_open(uint32_t initial_length);
const wchar_t *wcstr_warnnull_cstr(const sv_wstr *s);
void sv_wstr_close(sv_wstr *self);
void sv_wstr_calloc(sv_wstr *self, uint32_t len);
void sv_wstr_truncate(sv_wstr *self, uint32_t expected_len);
void sv_wstr_append(sv_wstr *self, const wchar_t *s);
void sv_wstr_clear(sv_wstr *self);
void sv_wstrlist_clear(sv_array *list);

void ask_user_str(const char *prompt, bool q_to_cancel, bstring out);
bool ask_user(const char *prompt);
uint32_t ask_user_int(const char *prompt, int valmin, int valmax);
void alert(const char *message);


typedef enum
{
    continue_on_err = 0,
    exit_on_err,
} erespondtoerr;

typedef struct sv_file {
    FILE *file;
} sv_file;

/* error handling */
void check_b_hit(void);
void debugbreak(void);
void die(void);
void quiet_warnings(bool b);
void check_warn_impl(sv_result res,
    const char *msg,
    const char *fnname,
    erespondtoerr respondtoerr);

#ifdef _DEBUG
#define debugonly(x) x
#else
#define debugonly(x)
#endif

#define check(expression) do { \
    currenterr = (expression); \
    if (currenterr.code) { \
        sv_log_fmt("leaving scope, %s", (#expression)); \
        sv_log_flush(); \
        goto cleanup; \
    } } while(0)

#define check_b(cond, ...) do { \
    if (!(cond)) { \
        sv_log_fmt(__VA_ARGS__); \
        sv_log_fmt("leaving scope, %s fn %s line %d", \
            (#cond), __FUNCTION__, __LINE__); \
        check_b_hit(); \
        currenterr.msg = bformat(__VA_ARGS__); \
        currenterr.code = -1; \
        debugonly(if (0) { printf(__VA_ARGS__); }) \
        goto cleanup; \
    } } while(0)

#define check_fatal(cond, ...) do { \
    if (!(cond)) { \
        sv_log_fmt(__VA_ARGS__); \
        sv_log_fmt("exiting, %s fn %s line %d", \
            (#cond), __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        die(); \
    } } while(0)

#define check_sql(db, code) do { \
    int coderesult = (code); \
    if (coderesult != SQLITE_OK && \
        coderesult != SQLITE_ROW && \
        coderesult != SQLITE_DONE) { \
        check_b(0, "sqlite returned %s %d " #code, \
            ((db) ? sqlite3_errmsg(db) : \
            sqlite3_errstr(coderesult)), coderesult); \
    } } while (0)

#define check_warn(p1, p2, p3) \
    check_warn_impl((p1), (p2), __FUNCTION__, (p3))

#define log_b(cond, ...) do { \
    if (!(cond)) { \
        sv_log_fmt("'%s', '%s'", (#cond), __FUNCTION__); \
        sv_log_fmt(__VA_ARGS__); \
        debugonly(if (0) { printf(__VA_ARGS__); }) \
    } } while(0)

#define log_errno_to(varname, expression, ...)                             \
    errno = 0;                                                                     \
    varname = (expression);                             \
    int CONCAT(lasterrno, __LINE__) = GetLastError(); \
    const char *CONCAT(log_errnocontext, __LINE__)[4] = {__VA_ARGS__};             \
    do                                                                             \
    {                                                                              \
        if ((varname) < 0)                                  \
        {                                                                          \
            log_errno_impl(#expression, CONCAT(lasterrno, __LINE__), CONCAT(log_errnocontext, __LINE__), \
                __FUNCTION__, __LINE__);                                           \
        }                                                                          \
    } while (0)

#define log_errno(expression, ...) \
    int CONCAT(errno_unnamed, __LINE__); \
    log_errno_to(CONCAT(errno_unnamed, __LINE__), expression, __VA_ARGS__); \

#define check_errno(expression, ...)           \
    log_errno(expression, __VA_ARGS__);        \
    do                                                             \
    {                                                              \
        if (CONCAT(errno_unnamed, __LINE__)) < 0)                  \
        {                                                          \
            check_errno_impl(&currenterr, #expression, CONCAT(lasterrno, __LINE__),      \
                CONCAT(log_errnocontext, __LINE__), __FUNCTION__); \
            goto cleanup;                                          \
        }                                                          \
    } while (0)

#define log_win32_to(vardatatype, varname, expression, failureval, ...) \
    SetLastError(0);                                                         \
    varname = (expression);                  \
    vardatatype CONCAT(lasterrval, __LINE__) = GetLastError();                     \
    const char *CONCAT(errcontext, __LINE__)[4] = {__VA_ARGS__};             \
    do                                                                       \
    {                                                                        \
        if ((varname) == (failureval))                      \
        {                                                                    \
            log_errwin32_impl(#expression, CONCAT(lasterrval, __LINE__),     \
                CONCAT(errcontext, __LINE__), __FUNCTION__, __LINE__);       \
        }                                                                    \
    } while (0) \

#define log_win32(vardatatype, expression, failureval, ...) \
    vardatatype CONCAT(errval_unused, __LINE__); \
    log_win32_to(CONCAT(errval_unused, __LINE__), expression, failureval, __VA_ARGS__); \

#define check_win32(vardatatype, expression, failureval, ...)    \
    log_win32(vardatatype, expression, failureval, __VA_ARGS__); \
    do                                                                            \
    {                                                                             \
        if ((CONCAT(lasterrval, __LINE__)) == (failureval))                           \
        {                                                                         \
            check_errwin32_impl(&currenterr, #expression,                         \
                CONCAT(lasterrval, __LINE__), CONCAT(errcontext, __LINE__),       \
                __FUNCTION__);                                                    \
            goto cleanup;                                                         \
        }                                                                         \
    } while (0)

#define CheckBformatStrings 0
#if CheckBformatStrings && _DEBUG
#undef bsetfmt
#define bsetfmt(s, ...) s; printf(__VA_ARGS__)
#define bformata(s, ...) s; printf(__VA_ARGS__)
#define bformat(...) (printf(__VA_ARGS__), hhh)
#define sv_log_fmt printf
#endif

extern int _;
void log_errno_impl(const char *exp,
    int nerrno,
    const char *context[4],
    const char *fn,
    int lineno);
void check_errno_impl(sv_result *currenterr,
    const char *exp,
    int nerrno,
    const char *context[4],
    const char *fn);
void log_errwin32_impl(const char *exp,
    unsigned long nerrno,
    const char *context[4],
    const char *fn,
    int lineno);
void check_errwin32_impl(sv_result *currenterr,
    const char *exp,
    unsigned long nerrno,
    const char *context[4],
    const char *fn);

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define set_self_zero() \
    memset((self), 0, sizeof(*(self)))

#define s_and_len(s) \
    ("" s ""), (countof32s(s) - 1)

#endif
