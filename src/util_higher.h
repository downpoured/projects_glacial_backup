/*
util_higher.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef UTIL_HIGHER_H_INCLUDE
#define UTIL_HIGHER_H_INCLUDE

#include "util.h"

typedef struct sv_log
{
    bstring dir;
    sv_file logfile;
    uint32_t logfilenumber;
    uint32_t counter;
    int32_t cap_filesize;
    int64_t start_of_day;
} sv_log;

extern const uint32_t sv_log_check_size_period;
check_result sv_log_open(sv_log *self, const char *dir);
void sv_log_register_active_logger(sv_log *logger);
void sv_log_close(sv_log *self);
void sv_log_write(const char *s);
void sv_log_writes(const char *s1, const char *s2);
void sv_log_flush(void);
void sv_log_addnewlinetime(
    FILE *f, int64_t start_of_day, int64_t seconds, long milliseconds);
void appendnumbertofilename(const char *dir, const char *prefix,
    const char *suffix, uint32_t number, bstring out);
uint32_t readnumberfromfilename(
    const char *prefix, const char *suffix, const char *candidate);
check_result readlatestnumberfromfilename(const char *dir, const char *prefix,
    const char *suffix, uint32_t *latestnumber);

struct sv_app;
typedef sv_result (*FnMenuCallback)(struct sv_app *, int);
typedef struct menu_action_entry
{
    const char *message;
    FnMenuCallback callback;
    int arg;
} menu_action_entry;

int menu_choose_long(const bstrlist *list, int groupsize);
typedef const struct menu_action_entry *(*FnMenuGetNextMenu)(struct sv_app *);
check_result menu_choose_action(const char *msg,
    const menu_action_entry *entries, struct sv_app *app,
    FnMenuGetNextMenu getnextmenu);
int menu_choose(const char *msg, const bstrlist *list, const char *format_each,
    const char *additionalopt1, const char *additionalopt2);

/* -----------------------
------ other utils -------
----------------------- */

typedef sv_result (*sv_2darray_iter_cb)(
    void *context, uint32_t x, uint32_t y, byte *val);

typedef struct sv_2darray
{
    sv_array arr;
    uint32_t elementsize;
} sv_2darray;

sv_2darray sv_2darray_open(uint32_t elementsize);
byte *sv_2darray_get_expand(sv_2darray *self, uint32_t d1, uint32_t d2);
byte *sv_2darray_at(sv_2darray *self, uint32_t d1, uint32_t d2);
void sv_2darray_close(sv_2darray *self);
const byte *sv_2darray_atconst(const sv_2darray *self, uint32_t d1, uint32_t d2);

typedef struct sv_pseudosplit
{
    bstring text;
    bstring currentline;
    sv_array splitpoints;
} sv_pseudosplit;

/* like splitting a string, but faster */
sv_pseudosplit sv_pseudosplit_open(const char *s);
void sv_pseudosplit_split(sv_pseudosplit *self, char delim);
void sv_pseudosplit_close(sv_pseudosplit *self);
const char *sv_pseudosplit_viewat(sv_pseudosplit *self, uint32_t linenumber);

long long castll(int64_t n);
unsigned long long castull(uint64_t n);

bool s_startwithlen(const char *s1, int len1, const char *s2, int len2);
bool s_startwith(const char *s1, const char *s2);
bool s_endwithlen(const char *s1, int len1, const char *s2, int len2);
bool s_endwith(const char *s1, const char *s2);
bool s_contains(const char *s1, const char *s2);
bool s_isalphanum_paren_or_underscore(const char *s);
bool ws_equal(const wchar_t *s1, const wchar_t *s2);

bool fnmatch_simple(const char *pattern, const char *string);
void fnmatch_isvalid(const char *pattern, bstring response);

check_result sv_2darray_foreach(
    sv_2darray *self, sv_2darray_iter_cb cb, void *context);

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
bstring tobase64nospace(const char *s);

typedef struct sv_wstr
{
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

#define chars_to_uint32(c1, c2, c3, c4)                   \
    (((uint32_t)(c1)) << 24) | (((uint32_t)(c2)) << 16) | \
        (((uint32_t)(c3)) << 8) | ((uint32_t)(c4))

#if WARN_NUL_IN_STR || defined(_DEBUG)
#define wcstr(s) wcstr_warnnull_cstr(&(s))
#else
#define wcstr(s) ((const wchar_t *)sv_array_atconst(&(s).arr, 0))
#endif
#define wpcstr(s) ((const wchar_t *)sv_array_atconst(&(s)->arr, 0))

#endif
