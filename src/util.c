/*
util.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "util_files.h"

/* a helper wrapper for heap arrays */
sv_array sv_array_open(uint32_t elementsize, uint32_t initiallength)
{
    sv_array self = {};
    self.elementsize = elementsize;
    if (initiallength)
    {
        self.buffer = sv_calloc(initiallength, elementsize);
    }

    self.capacity = initiallength;
    self.length = initiallength;
    return self;
}

/* ensure enough space, expand buffer if necessary */
void sv_array_reserve(sv_array *self, uint32_t requestedcapacity)
{
    if (requestedcapacity <= self->capacity)
    {
        return;
    }

    /* double container size until large enough.*/
    requestedcapacity = nearest_power_of_two(requestedcapacity);
    self->buffer = sv_realloc(self->buffer, requestedcapacity, self->elementsize);
    self->capacity = requestedcapacity;
}

/* append to array, expand buffer if necessary */
void sv_array_append(sv_array *self, const void *inbuffer, uint32_t incount)
{
    uint32_t index = checkedmul32(self->length, self->elementsize);
    sv_array_reserve(self, checkedadd32(self->length, incount));
    memcpy(&self->buffer[index], inbuffer, checkedmul32(self->elementsize, incount));

    self->length += incount;
}

/* append zeros to array, expand buffer if necessary */
void sv_array_appendzeros(sv_array *self, uint32_t incount)
{
    sv_array_reserve(self, checkedadd32(self->length, incount));
    uint32_t index = checkedmul32(self->length, self->elementsize);
    memset(&self->buffer[index], 0, checkedmul32(self->elementsize, incount));

    self->length += incount;
}

/* adjust array length */
void sv_array_truncatelength(sv_array *self, uint32_t newlength)
{
    check_fatal(newlength <= self->length, "out-of-bounds read");
    self->length = newlength;
}

/* either sets memory contents to 0, or set length to 0 */
void sv_array_set_zeros(sv_array *self)
{
    memset(self->buffer, 0, self->elementsize * self->length);
}

/* get pointer to an element */
const byte *sv_array_atconst(const sv_array *self, uint32_t index)
{
    check_fatal(index < self->length, "out-of-bounds read");
    uint32_t byteindex = checkedmul32(index, self->elementsize);
    return &self->buffer[byteindex];
}

/* helper for making an array of uint64_t. */
sv_array sv_array_open_u64()
{
    sv_array ret = sv_array_open(sizeof32u(uint64_t), 1);
    sv_array_truncatelength(&ret, 0);
    return ret;
}

/* helper for making an array of uint64_t, get value at index. */
uint64_t sv_array_at64u(const sv_array *self, uint32_t index)
{
    check_fatal(self->elementsize == sizeof32u(uint64_t),
        "array was not inited to have element size u64.");
    return *(const uint64_t *)sv_array_atconst(self, index);
}

/* helper for making an array of uint64_t, add a value. */
void sv_array_add64u(sv_array *self, uint64_t n)
{
    check_fatal(self->elementsize == sizeof32u(uint64_t),
        "array was not inited to have element size u64.");
    sv_array_append(self, (byte *)&n, 1);
}

/* free memory */
void sv_array_close(sv_array *self)
{
    if (self)
    {
        sv_freenull(self->buffer);
        set_self_zero();
    }
}

sv_result OK = {};

/* free 'result' message */
void sv_result_close(sv_result *self)
{
    if (self)
    {
        bdestroy(self->msg);
        set_self_zero();
    }
}

/** ----------------------------------------------------------------
 ** casts that check for overflow
 ** ---------------------------------------------------------------- **/

uint32_t cast64u32u(uint64_t n)
{
    check_fatal(n <= (uint64_t)UINT32_MAX, "overflow");
    return (uint32_t)n;
}

int32_t cast64s32s(int64_t n)
{
    check_fatal(n <= (int64_t)INT32_MAX, "overflow");
    return (int32_t)n;
}

int32_t cast64u32s(uint64_t n)
{
    check_fatal(n <= (uint64_t)INT32_MAX, "overflow");
    return (int32_t)n;
}

int64_t cast64u64s(uint64_t n)
{
    check_fatal(n <= (uint64_t)INT64_MAX, "overflow");
    return (int64_t)n;
}

uint64_t cast64s64u(int64_t n)
{
    check_fatal(n >= 0, "overflow");
    return (uint64_t)n;
}

int32_t cast32u32s(uint32_t n)
{
    check_fatal((int64_t)n <= (int64_t)INT32_MAX, "overflow");
    return (int32_t)n;
}

uint32_t cast32s32u(int32_t n)
{
    check_fatal(n >= 0, "overflow");
    return (uint32_t)n;
}

uint32_t checkedmul32(uint32_t a, uint32_t b)
{
    uint32_t ret = a * b;
    check_fatal(a == 0 || ret / a == b, "overflow");
    return ret;
}

uint32_t checkedadd32(uint32_t a, uint32_t b)
{
    uint32_t ret = a + b;
    check_fatal(ret >= a && ret >= b, "overflow");
    return ret;
}

int32_t checkedadd32s(int32_t a, int32_t b)
{
    check_fatal(!(((b > 0) && (a > INT_MAX - b)) || ((b < 0) && (a < INT_MIN - b))),
        "overflow");

    return a + b;
}

/* get nearest power of two that is >= input */
uint32_t nearest_power_of_two(uint32_t a)
{
    check_fatal(a < UINT32_MAX / 4, "overflow");
    if (a == 0)
    {
        return 1;
    }

    uint32_t result = 1;
    while (result < a)
    {
        result <<= 1;
    }

    return result;
}

/* join to 32bit ints into a 64bit int */
uint64_t make_u64(uint32_t hi, uint32_t lo)
{
    return lo | (((uint64_t)hi) << 32);
}

/* get upper 32 bits */
uint32_t upper32(uint64_t n)
{
    return (uint32_t)(n >> 32);
}

/* get lower 32 bits */
uint32_t lower32(uint64_t n)
{
    return (uint32_t)n;
}

/* wrap calloc and exit if allocation fails */
byte *sv_calloc(uint32_t count, uint32_t size)
{
    void *ret = calloc(count, size);
    check_fatal(ret != NULL, "calloc returned NULL");
    return (byte *)ret;
}

/* wrap realloc and exit if allocation fails */
byte *sv_realloc(byte *ptr, uint32_t a, uint32_t b)
{
    void *ret = realloc(ptr, checkedmul32(a, b));
    check_fatal(ret != NULL, "realloc returned NULL");
    return (byte *)ret;
}

/* are strings equal */
bool s_equal(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

/* string with only numerals to an integer */
bool uintfromstr(const char *s, uint64_t *result)
{
    /* input must consist of only numerals.*/
    staticassert(sizeof(unsigned long long) == sizeof(uint64_t));
    *result = 0;
    if (!s || !s[0] || strspn(s, "1234567890") != strlen(s))
    {
        return false;
    }

    /* must check errno in case input is greater than UINT64_MAX */
    errno = 0;
    char *ptrresult = NULL;
    uint64_t res = (uint64_t)strtoull(s, &ptrresult, 10);
    if (!*ptrresult && errno == 0)
    {
        *result = res;
        return true;
    }

    return false;
}

bool uintfromstrhex(const char *s, uint64_t *result)
{
    /* input must consist of only numerals.*/
    staticassert(sizeof(unsigned long long) == sizeof(uint64_t));
    *result = 0;
    if (!s || !s[0] || strspn(s, "1234567890abcdefABCDEF") != strlen(s))
    {
        return false;
    }

    /* must check errno in case input is greater than UINT64_MAX */
    errno = 0;
    char *ptrresult = NULL;
    uint64_t res = (uint64_t)strtoull(s, &ptrresult, 16);
    if (!*ptrresult && errno == 0)
    {
        *result = res;
        return true;
    }

    return false;
}

static bool g_quiet_warnings = false;

/* check for embedded NUL terminators */
const char *bstr_warnnull_cstr(const bstring s)
{
    check_fatal(s && s->data, "ptr is null");
    if (strlen32s((const char *)s->data) != s->slen)
    {
        if (!g_quiet_warnings)
        {
            alert("cstr() not allowed if "
                  "string contains binary data.");
        }

        return NULL;
    }

    return (const char *)s->data;
}

/* console: ask user for string */
void ask_user_str(const char *prompt, bool q_to_cancel, bstring out)
{
    bstrclear(out);
    bool allow_empty = false;
    puts(prompt);
    fflush(stdout);
    limited_while_true()
    {
        char buffer[BUFSIZ] = {0};
        if (fgets(buffer, countof(buffer) - 1, stdin) == NULL || buffer[0] == '\n' ||
            buffer[0] == '\0')
        {
            if (allow_empty)
                break;
            else
                continue;
        }

        if (buffer[strlen(buffer) - 1] == '\n')
        {
            bassignblk(out, buffer, strlen32s(buffer) - 1);
            break;
        }
        else
        {
            /* eat extra characters */
            while (getchar() != '\n')
                ;
            printf(
                "This is too long, limit of %d characters\n", countof32s(buffer) - 1);
            continue;
        }
    }

    if (q_to_cancel && s_equal(cstr(out), "q"))
    {
        bstrclear(out);
    }
}

/* console: ask user for boolean */
bool ask_user(const char *prompt)
{
    bstring result = bstring_open();
    bool ret = false;
    limited_while_true()
    {
        ask_user_str(prompt, false, result);
        if (s_equal("y", cstr(result)))
        {
            ret = true;
            break;
        }
        else if (s_equal("n", cstr(result)))
        {
            ret = false;
            break;
        }
    }

    bdestroy(result);
    return ret;
}

/* console: ask user for integer */
uint32_t ask_user_int(const char *prompt, int valmin, int valmax)
{
    bstring userinput = bstring_open();
    uint32_t ret = 0;
    limited_while_true()
    {
        uint64_t n = 0;
        ask_user_str(prompt, false, userinput);
        if (uintfromstr(cstr(userinput), &n) && n >= valmin && n <= valmax)
        {
            ret = cast64u32u(n);
            break;
        }
        else
        {
            printf("Must be between %d and %d.\n", valmin, valmax);
        }
    }

    bdestroy(userinput);
    return ret;
}

/* console: show message and wait for y/n confirmation */
void alert_prompt(const char *message)
{
    printf("%s\n\n", message);
    if (!ask_user("Continue? y/n"))
    {
        exit(0);
    }
}

/* console: show message and wait for confirmation */
void alert(const char *message)
{
    fprintf(stderr, "%s\nPress Enter to continue...\n", message);
    (void)getchar();
}


/* silence warnings (for tests) */
void quiet_warnings(bool b)
{
    g_quiet_warnings = b;
}

/* are we currently quieted */
bool is_quiet()
{
    return g_quiet_warnings;
}

/* we hit something, break into debugger */
void check_b_hit(void)
{
    fflush(stdout);
    fflush(stderr);
    sv_log_flush();
    if (!g_quiet_warnings)
    {
        debugbreak();
    }
}

/* break into debugger */
void debugbreak(void)
{
#if _DEBUG
    alert("a recoverable error occurred, attach debugger to investigate.");
    DEBUGBREAK();
#endif
}

/* exit program */
void die(void)
{
    fflush(stdout);
    fflush(stderr);
    sv_log_flush();
    alert("\n\nWe will now exit.");
    debugbreak();
    exit(1);
}

/* implement 'warn' which is sometimes recoverable */
void check_warn_impl(
    sv_result res, const char *msg, const char *function, erespondtoerr respondtoerr)
{
    if (res.code)
    {
        log_b(0, "%s %s", function, cstr(res.msg));
        sv_log_flush();

        if (!g_quiet_warnings || respondtoerr != continue_on_err)
        {
            printf("%s\n%s\n", msg ? msg : "", cstr(res.msg));
            alert("");
        }

        sv_result_close(&res);
        if (respondtoerr == exit_on_err)
        {
            die();
        }
    }
}

/* helper to view entry in a string list */
const char *blist_view(const bstrlist *list, int index)
{
    check_fatal(list && index >= 0 && index < list->qty, "index is out-of-bounds");

    return cstr(list->entry[index]);
}

/* log error number */
void log_errno_impl(
    const char *exp, int nerrno, const char *context[4], const char *fn, int lineno)
{
    char errorname[BUFSIZ] = {0};
    os_errno_to_buffer(nerrno, errorname, countof(errorname));
    sv_log_fmt("'%s' returned errno=%s(%d) in %s line %d %s %s %s %s", exp, errorname,
        nerrno, fn, lineno, context[0] ? context[0] : "", context[1] ? context[1] : "",
        context[2] ? context[2] : "", context[3] ? context[3] : "");
}

/* create a nice error-message if errno indicates error */
void check_errno_impl(sv_result *currenterr, const char *exp, int nerrno,
    const char *context[4], const char *fn)
{
    char errorname[BUFSIZ] = {0};
    os_errno_to_buffer(nerrno, errorname, countof(errorname));
    currenterr->code = 1;
    currenterr->msg = bformat("Got %s(%d) in %s %s %s %s %s (%s)", errorname, nerrno,
        fn, context[0] ? context[0] : "", context[1] ? context[1] : "",
        context[2] ? context[2] : "", context[3] ? context[3] : "", exp);
    check_b_hit();
}

/* log a win32 error */
void log_errwin32_impl(const char *exp, unsigned long nerrno, const char *context[4],
    const char *fn, int lineno)
{
    char errorname[BUFSIZ] = {0};
    os_win32err_to_buffer(nerrno, errorname, countof(errorname));
    sv_log_fmt("'%s' returned errno=%s(%d) in %s line %d, %s %s %s %s", exp, errorname,
        nerrno, fn, lineno, context[0] ? context[0] : "", context[1] ? context[1] : "",
        context[2] ? context[2] : "", context[3] ? context[3] : "");
}

/* create a nice error-message if win32 indicates error */
void check_errwin32_impl(sv_result *currenterr, const char *exp, unsigned long nerrno,
    const char *context[4], const char *fn)
{
    char errorname[BUFSIZ] = {0};
    os_win32err_to_buffer(nerrno, errorname, countof(errorname));
    currenterr->code = 1;
    currenterr->msg = bformat("Got %s(%d) in %s, %s %s %s %s (%s)", errorname, nerrno,
        fn, context[0] ? context[0] : "", context[1] ? context[1] : "",
        context[2] ? context[2] : "", context[3] ? context[3] : "", exp);
    check_b_hit();
}

