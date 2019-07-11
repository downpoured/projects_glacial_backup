/*
util_higher.c

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

static sv_log *p_sv_log = NULL;
void sv_log_register_active_logger(sv_log *logger)
{
    p_sv_log = logger;
}

static check_result sv_log_start_attach_file(
    const char *dir, uint32_t number, sv_file *file, int64_t *start_of_day)
{
    sv_result currenterr = {};
    bstring filename = bstring_open();
    appendnumbertofilename(dir, "log", ".txt", number, filename);
    sv_result result = sv_file_open(file, cstr(filename),
        /* the 'e' means to pass O_CLOEXEC
        so that the fd is closed on calls to exec. */
        islinux ? "abe" : "ab");

    if (result.code)
    {
        check_b(0,
            "We were not able to start logging. Please check "
            "that GlacialBackup is not already running, and try again."
            "\n(Details: could not access %s).",
            cstr(filename));
    }

    /* get the start of the day in seconds */
    time_t curtime = time(NULL);
    struct tm *tmstruct = localtime(&curtime);
    *start_of_day = curtime;
    *start_of_day -= 60 * 60 * tmstruct->tm_hour;
    *start_of_day -= 60 * tmstruct->tm_min;
    *start_of_day -= tmstruct->tm_sec;

    /* write time */
    fprintf(file->file, nativenewline "%04d/%02d/%02d", tmstruct->tm_year + 1900,
        tmstruct->tm_mon + 1, tmstruct->tm_mday);

cleanup:
    sv_result_close(&result);
    bdestroy(filename);
    return currenterr;
}

check_result sv_log_open(sv_log *self, const char *dir)
{
    sv_result currenterr = {};
    set_self_zero();
    self->dir = bfromcstr(dir);
    self->cap_filesize = 8 * 1024 * 1024;

    check(
        readlatestnumberfromfilename(dir, "log", ".txt", &self->logfilenumber));
    if (self->logfilenumber == 0)
        self->logfilenumber = 1;

    check(sv_log_start_attach_file(cstr(self->dir), self->logfilenumber,
        &self->logfile, &self->start_of_day));

cleanup:
    return currenterr;
}

void sv_log_close(sv_log *self)
{
    if (self)
    {
        bdestroy(self->dir);
        sv_file_close(&self->logfile);
        set_self_zero();
    }
}

FILE *sv_log_currentFile(void)
{
    return p_sv_log ? p_sv_log->logfile.file : NULL;
}

void sv_log_addnewlinetime(
    FILE *f, int64_t start_of_day, int64_t totalseconds, long milliseconds)
{
    int64_t seconds_since_midnight = totalseconds - start_of_day;
    int64_t hours = seconds_since_midnight / (60 * 60);
    int64_t secondspasthour = seconds_since_midnight - hours * 60 * 60;
    int64_t minutes = (secondspasthour) / 60;
    int64_t seconds = secondspasthour - minutes * 60;
    fprintf(f, nativenewline "%02d:%02d:%02d:%03d ", (int)hours, (int)minutes,
        (int)seconds, (int)milliseconds);
}

/* we'll check the filesize every 16 log entries */
const uint32_t sv_log_check_size_period = 16;

static void sv_log_check_switch_nextfile(sv_log *self)
{
    /* start a new log file after size reaches 8mb */
    self->counter++;
    if ((self->counter % sv_log_check_size_period) == 0)
    {
        fseek(self->logfile.file, 0, SEEK_END);
        if (ftell(self->logfile.file) > self->cap_filesize)
        {
            sv_file next_file = {};
            int64_t next_start_of_day = 0;
            sv_result try_switch_file = sv_log_start_attach_file(cstr(self->dir),
                self->logfilenumber + 1, &next_file, &next_start_of_day);

            if (try_switch_file.code == 0)
            {
                sv_file_close(&self->logfile);
                self->logfile = next_file;
                self->start_of_day = next_start_of_day;
            }
            else
            {
                sv_file_close(&next_file);
                sv_result_close(&try_switch_file);
            }
        }
    }
}

static void sv_log_addnewline(void)
{
    sv_log_check_switch_nextfile(p_sv_log);
    int64_t seconds;
    int32_t milliseconds;
    os_clock_gettime(&seconds, &milliseconds);
    sv_log_addnewlinetime(
        sv_log_currentFile(), p_sv_log->start_of_day, seconds, milliseconds);
}

void sv_log_write(const char *s)
{
    if (sv_log_currentFile())
    {
        sv_log_addnewline();
        fputs(s, sv_log_currentFile());
    }
}

void sv_log_writes(const char *s1, const char *s2)
{
    if (sv_log_currentFile())
    {
        sv_log_addnewline();
        fputs(s1, sv_log_currentFile());
        fputc(' ', sv_log_currentFile());
        fputs(s2, sv_log_currentFile());
    }
}

void sv_log_flush(void)
{
    if (sv_log_currentFile())
    {
        fflush(sv_log_currentFile());
    }
}

#if !CheckBformatStrings
void sv_log_fmt(const char *fmt, ...)
{
    if (sv_log_currentFile())
    {
        sv_log_addnewline();
        va_list args;
        va_start(args, fmt);
        vfprintf(sv_log_currentFile(), fmt, args);
        va_end(args);
    }
}
#endif

/* from 1 to "/path/file001.txt" */
void appendnumbertofilename(const char *dir, const char *prefix,
    const char *suffix, uint32_t number, bstring out)
{
    bsetfmt(out, "%s%s%s%05d%s", dir, pathsep, prefix, number, suffix);
}

/* from "/path/file001.txt" to 1. */
uint32_t readnumberfromfilename(
    const char *prefix, const char *suffix, const char *candidate)
{
    bstring tmp = bstring_open();
    int lenprefix = strlen32s(prefix);
    int lensuffix = strlen32s(suffix);
    int lencandidate = strlen32s(candidate);
    if (s_startwithlen(candidate, lencandidate, prefix, lenprefix) &&
        s_endwithlen(candidate, lencandidate, suffix, lensuffix))
    {
        /* get chars between the template and extension. */
        int len = lencandidate - (lensuffix + lenprefix);
        bassignblk(tmp, candidate + lenprefix, len);
        uint64_t n = 0;
        if (blength(tmp) > 0 && uintfromstr(cstr(tmp), &n))
        {
            bdestroy(tmp);
            return cast64u32u(n);
        }
    }

    bdestroy(tmp);
    return 0;
}

/* from "/path/file001.txt" to 1. */
check_result readlatestnumberfromfilename(const char *dir, const char *prefix,
    const char *suffix, uint32_t *latestnumber)
{
    *latestnumber = 0;
    sv_result currenterr = {};
    bstring fullprefix = bformat("%s%s%s", dir, pathsep, prefix);
    bstrlist *files_seen = bstrlist_open();
    check_b(os_dir_exists(dir), "dir not found %s", dir);
    check(os_listfiles(dir, files_seen, false));
    for (int i = 0; i < files_seen->qty; i++)
    {
        /* don't use strcmp which would sort 02 after 01234 */
        uint32_t number = readnumberfromfilename(
            cstr(fullprefix), suffix, blist_view(files_seen, i));

        *latestnumber = MAX(*latestnumber, number);
    }

cleanup:
    bdestroy(fullprefix);
    bstrlist_close(files_seen);
    return currenterr;
}

int menu_choose(const char *msg, const bstrlist *list, const char *format_each,
    const char *additionalopt1, const char *additionalopt2)
{
    int ret = 0;
    bstring userinput = bstring_open();
    while (true)
    {
        os_clr_console();
        printf("%s\n", msg);
        const char *spec = format_each ? format_each : "%d) %s\n";
        int listindex = 0;
        while (listindex < list->qty)
        {
            printf(spec, listindex + 1, blist_view(list, listindex));
            ++listindex;
        }
        if (additionalopt1)
        {
            printf("%d) %s\n", ++listindex, additionalopt1);
        }
        if (additionalopt2)
        {
            printf("%d) %s\n", ++listindex, additionalopt2);
        }

        ask_user_str("\n\nPlease type a number from the list above "
                     "and press Enter:",
            false, userinput);

        uint64_t n = 0;
        if (uintfromstr(cstr(userinput), &n) && n >= 1 && n <= listindex)
        {
            ret = cast64u32s(n - 1);
            break;
        }
    }

    bdestroy(userinput);
    return ret;
}

int menu_choose_long(const bstrlist *list, int groupsize)
{
    int ret = -1;
    int ngroups = (list->qty / groupsize) + 1;
    bstrlist *listgroups = bstrlist_open();
    bstrlist *listsubgroup = bstrlist_open();
    for (int i = 0; i < ngroups; i++)
    {
        char buf[BUFSIZ] = {0};
        snprintf(buf, countof(buf) - 1, "%d to %d", (i * groupsize) + 1,
            ((i + 1) * groupsize));
        bstrlist_appendcstr(listgroups, buf);
    }

    while (true)
    {
        int chosengroup = menu_choose("", listgroups, NULL, "Back", NULL);
        if (chosengroup >= listgroups->qty)
        {
            break;
        }

        bstrlist_clear(listsubgroup);
        for (int i = chosengroup * groupsize;
             i < chosengroup * groupsize + groupsize && i < list->qty; i++)
        {
            bstrlist_appendcstr(listsubgroup, blist_view(list, i));
        }

        int chosen = menu_choose("", listsubgroup, NULL, "Back", NULL);
        if (chosen < listsubgroup->qty)
        {
            ret = chosengroup * groupsize + chosen;
            break;
        }
    }

    bstrlist_close(listsubgroup);
    bstrlist_close(listgroups);
    return ret;
}

check_result menu_choose_action(const char *msg,
    const menu_action_entry *entries, struct sv_app *app,
    FnMenuGetNextMenu fngetnextmenu)
{
    bstring userinput = bstring_open();

    while (true)
    {
        if (fngetnextmenu)
        {
            entries = fngetnextmenu(app);
        }

        os_clr_console();
        printf("%s\n", msg);
        uint64_t number = 1;
        const menu_action_entry *iter = entries;
        while (iter->callback || iter->message)
        {
            printf("%llu) %s\n", castull(number), iter->message);
            number++;
            iter++;
        }

        uint64_t n = 0;
        ask_user_str("\n\nPlease type a number from the list above "
                     "and press Enter:",
            false, userinput);

        if (uintfromstr(cstr(userinput), &n) && n >= 1 && n < number)
        {
            const menu_action_entry *action = entries + n - 1;
            if (action->callback)
            {
                os_clr_console();
                sv_result result = action->callback(app, action->arg);
                if (result.code)
                {
                    printf("\n\nMessage: %s", cstr(result.msg));
                    alert("");
                    sv_result_close(&result);
                }

                continue;
            }
            else
            {
                break;
            }
        }
    }

    bdestroy(userinput);
    return OK;
}

/*
----------------more utils-----------------------
*/

sv_2darray sv_2darray_open(uint32_t elementsize)
{
    sv_2darray ret = {};
    ret.arr = sv_array_open(sizeof32u(sv_array), 0);
    ret.elementsize = elementsize;
    return ret;
}

byte *sv_2darray_get_expand(sv_2darray *self, uint32_t d1, uint32_t d2)
{
    if (d1 >= self->arr.length)
    {
        uint32_t formerlength = self->arr.length;
        sv_array_appendzeros(&self->arr, (d1 + 1) - formerlength);
        for (uint32_t i = formerlength; i < self->arr.length; i++)
        {
            sv_array *child = (sv_array *)sv_array_at(&self->arr, i);
            *child = sv_array_open(self->elementsize, 0);
        }
    }

    sv_array *child = (sv_array *)sv_array_at(&self->arr, d1);
    if (d2 >= child->length)
    {
        sv_array_appendzeros(child, (d2 + 1) - child->length);
    }

    return sv_2darray_at(self, d1, d2);
}

byte *sv_2darray_at(sv_2darray *self, uint32_t d1, uint32_t d2)
{
    return (byte *)sv_2darray_atconst(self, d1, d2);
}

const byte *sv_2darray_atconst(const sv_2darray *self, uint32_t d1, uint32_t d2)
{
    check_fatal(d1 < self->arr.length, "out-of-bounds read");
    const sv_array *child = (const sv_array *)sv_array_atconst(&self->arr, d1);
    check_fatal(d2 < child->length, "out-of-bounds read");
    return sv_array_atconst(child, d2);
}

check_result sv_2darray_foreach(
    sv_2darray *self, sv_2darray_iter_cb cb, void *context)
{
    sv_result currenterr = {};
    for (uint32_t i = 0; i < self->arr.length; i++)
    {
        sv_array *child = (sv_array *)sv_array_at(&self->arr, i);
        for (uint32_t j = 0; j < child->length; j++)
        {
            check(cb(context, i, j, sv_2darray_at(self, i, j)));
        }
    }

cleanup:
    return currenterr;
}

void sv_2darray_close(sv_2darray *self)
{
    if (self)
    {
        for (uint32_t i = 0; i < self->arr.length; i++)
        {
            sv_array *child = (sv_array *)sv_array_at(&self->arr, i);
            sv_array_close(child);
        }

        sv_array_close(&self->arr);
        set_self_zero();
    }
}

sv_pseudosplit sv_pseudosplit_open(const char *s)
{
    sv_pseudosplit ret = {};
    ret.currentline = bstring_open();
    ret.text = bfromcstr(s);
    ret.splitpoints = sv_array_open_u64();
    return ret;
}

void sv_pseudosplit_split(sv_pseudosplit *self, char delim)
{
    /* like splitting a string, but faster */
    sv_array_truncatelength(&self->splitpoints, 0);

    /* by convention the first line begins at 0. */
    sv_array_add64u(&self->splitpoints, 0);
    for (int i = 0; i < blength(self->text); i++)
    {
        if (cstr(self->text)[i] == delim)
        {
            sv_array_add64u(&self->splitpoints, cast32s32u(i + 1));
        }
    }
}

const char *sv_pseudosplit_viewat(sv_pseudosplit *self, uint32_t linenum)
{
    check_fatal(
        linenum < self->splitpoints.length, "attempted read out-of-bounds");

    /* if it's the last line, go to the end of the string */
    uint64_t offset1 = sv_array_at64u(&self->splitpoints, linenum);
    uint64_t offset2 = (linenum == self->splitpoints.length - 1)
        ? (cast32s32u(blength(self->text)))
        : (sv_array_at64u(&self->splitpoints, linenum + 1) - 1);

    bassignblk(self->currentline, cstr(self->text) + offset1,
        cast64u32s(offset2 - offset1));

    return cstr(self->currentline);
}

void sv_pseudosplit_close(sv_pseudosplit *self)
{
    if (self)
    {
        bdestroy(self->currentline);
        bdestroy(self->text);
        sv_array_close(&self->splitpoints);
        set_self_zero();
    }
}

long long castll(int64_t n)
{
    staticassert(sizeof(long long) >= sizeof(int64_t));
    return (long long)n;
}

unsigned long long castull(uint64_t n)
{
    staticassert(sizeof(unsigned long long) >= sizeof(uint64_t));
    return (unsigned long long)n;
}

bool s_startwithlen(const char *s1, int len1, const char *s2, int len2)
{
    return (len1 < len2) ? false : memcmp(s1, s2, cast32s32u(len2)) == 0;
}

bool s_startwith(const char *s1, const char *s2)
{
    return s_startwithlen(s1, strlen32s(s1), s2, strlen32s(s2));
}

bool s_endwithlen(const char *s1, int len1, const char *s2, int len2)
{
    return (len1 < len2) ? false
                         : memcmp(s1 + (len1 - len2), s2, cast32s32u(len2)) == 0;
}

bool s_endwith(const char *s1, const char *s2)
{
    return s_endwithlen(s1, strlen32s(s1), s2, strlen32s(s2));
}

bool s_contains(const char *s1, const char *s2)
{
    return strstr(s1, s2) != 0;
}

bool s_isalphanum_paren_or_underscore(const char *s)
{
    while (*s)
    {
        if (*s != '_' && *s != '-' && *s != '(' && *s != ')' &&
            !isalnum((unsigned char)*s))
        {
            return false;
        }

        s++;
    }

    return true;
}

bool ws_equal(const wchar_t *s1, const wchar_t *s2)
{
    return wcscmp(s1, s2) == 0;
}

/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.
www.rpi.edu/dept/acm/packages/Bin-Man/1.1/distrib/libglob/fnmatch.c

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* No wildcard can ever match `/'.  */
const int FNM_PATHNAME = (1 << 0);

/* Backslashes don't quote special chars. */
const int FNM_NOESCAPE = (1 << 1);

/* Leading `.' is matched only explicitly. */
const int FNM_PERIOD = (1 << 2);

/* Match STRING against the filename pattern PATTERN, returning zero if
it matches, FNM_NOMATCH if not. (no support for char groups) */
const int FNM_NOMATCH = 1;
static int fnmatch(const char *pattern, const char *string, int flags)
{
    register const char *p = pattern, *n = string;
    register char c;
    const int __FNM_FLAGS = (FNM_PATHNAME | FNM_NOESCAPE | FNM_PERIOD);
    if ((flags & ~__FNM_FLAGS) != 0)
    {
        errno = EINVAL;
        return (-1);
    }

    while ((c = *p++) != '\0')
    {
        switch (c)
        {
        case '?':
            if (*n == '\0')
                return (FNM_NOMATCH);
            else if ((flags & FNM_PATHNAME) && *n == '/')
                return (FNM_NOMATCH);
            else if ((flags & FNM_PERIOD) && *n == '.' &&
                (n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
                return (FNM_NOMATCH);
            break;

        case '\\':
            if (!(flags & FNM_NOESCAPE))
                c = *p++;
            if (*n != c)
                return (FNM_NOMATCH);
            break;

        case '*':
            if ((flags & FNM_PERIOD) && *n == '.' &&
                (n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
                return (FNM_NOMATCH);

            for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
                if (((flags & FNM_PATHNAME) && *n == '/') ||
                    (c == '?' && *n == '\0'))
                    return (FNM_NOMATCH);

            if (c == '\0')
                return (0);

            {
                char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;
                for (--p; *n != '\0'; ++n)
                    if ((c == '[' || *n == c1) &&
                        fnmatch(p, n, flags & ~FNM_PERIOD) == 0)
                        return (0);
                return (FNM_NOMATCH);
            }

        default:
            if (c != *n)
                return (FNM_NOMATCH);
        }

        ++n;
    }

    if (*n == '\0')
        return (0);

    return (FNM_NOMATCH);
}

bool fnmatch_simple(const char *pattern, const char *string)
{
    return s_equal(pattern, "*")
        ? true
        : fnmatch(pattern, string, FNM_NOESCAPE) != FNM_NOMATCH;
}

void fnmatch_isvalid(const char *pattern, bstring response)
{
    bstrclear(response);
    if (pattern[0] == '\0')
    {
        bassigncstr(response, "pattern cannot be empty");
    }

    for (int i = 0; i < strlen32s(pattern); i++)
    {
        if (pattern[i] == '[' || pattern[i] == ']')
        {
            bassigncstr(response, "character groups [] not supported");
        }
        else if ((unsigned char)pattern[i] >= 128)
        {
            bassigncstr(response, "non-ansi chars not supported");
        }
    }
}

void bstrlist_split(bstrlist *list, const bstring s, char delim)
{
    struct genBstrList blist = {};
    bstrlist_clear(list);
    blist.b = (bstring)s;
    blist.bl = list;
    if (bsplitcb(s, (byte)delim, 0, bscb, &blist) < 0)
    {
        bstrlist_clear(list);
    }
}

void bstrlist_splitcstr(bstrlist *list, const char *s, char delim)
{
    bstring bs = bfromcstr(s);
    bstrlist_split(list, bs, delim);
    bdestroy(bs);
}

int bstrlist_append(bstrlist *list, const bstring bs)
{
    if (!list)
    {
        return BSTR_ERR;
    }

    bstrlist_alloc(list, list->qty + 1);
    list->entry[list->qty] = bstrcpy(bs);
    list->qty++;
    return BSTR_OK;
}

int bstrlist_appendcstr(bstrlist *list, const char *s)
{
    if (!list)
    {
        return BSTR_ERR;
    }

    bstrlist_alloc(list, list->qty + 1);
    list->entry[list->qty] = bfromcstr(s);
    list->qty++;
    return BSTR_OK;
}

int bstrlist_remove_at(bstrlist *list, int index)
{
    if (!list || index < 0 || index >= list->qty)
    {
        return BSTR_ERR;
    }

    bdestroy(list->entry[index]);
    for (int i = index + 1; i < list->qty; i++)
    {
        list->entry[i - 1] = list->entry[i];
    }

    list->qty--;
    return BSTR_OK;
}

void bstrlist_clear(bstrlist *list)
{
    for (int i = 0; i < list->qty; i++)
    {
        bdestroy(list->entry[i]);
    }

    list->qty = 0;
}

static int qsort_bstrlist(const void *p1, const void *p2)
{
    bstring *left = (bstring *)p1;
    bstring *right = (bstring *)p2;
    return bstrcmp(*left, *right);
}

void bstrlist_sort(bstrlist *list)
{
    qsort(&list->entry[0], (size_t)list->qty, sizeof(list->entry[0]),
        &qsort_bstrlist);
}

int bstring_calloc(bstring s, int len)
{
    len += 1; /* room for nul char */
    if (BSTR_OK != ballocmin(s, len))
    {
        return BSTR_ERR;
    }

    check_fatal(s->mlen >= len, "failed to allocate %d %d", s->mlen, len);
    memset(&s->data[0], 0, (size_t)s->mlen);
    return BSTR_OK;
}

void bytes_to_string(const void *b, uint32_t len, bstring s)
{
    char buf[32] = "";
    const byte *bytes = (const byte *)b;
    bstrclear(s);
    for (uint32_t i = 0; i < len; i++)
    {
        int n = snprintf(buf, countof(buf) - 1, "%02x", bytes[i]);
        bcatblk(s, buf, n);
    }
}

int bstr_replaceall(bstring s, const char *find, const char *replacewith)
{
    bstring bfind = bfromcstr(find);
    bstring breplacewith = bfromcstr(replacewith);
    int ret = bReplaceAll(s, bfind, breplacewith, 0);
    bdestroy(bfind);
    bdestroy(breplacewith);
    return ret;
}

bool bstr_equal(const bstring s1, const bstring s2)
{
    return s1 && s2 && s1->slen == s2->slen &&
        memcmp(cstr(s1), cstr(s2), cast32s32u(s1->slen)) == 0;
}

/* wide strings */

const wchar_t *wcstr_warnnull_cstr(const sv_wstr *s)
{
    check_fatal(s && s->arr.length, "ptr is null");
    for (uint32_t i = 0; i < s->arr.length - 1; i++)
    {
        if (*(const wchar_t *)sv_array_atconst(&s->arr, i) == L'\0')
        {
            if (!is_quiet())
            {
                alert("wcstr() not allowed if "
                      "string contains binary data.");
            }

            return NULL;
        }
    }

    return (const wchar_t *)sv_array_atconst(&s->arr, 0);
}

sv_wstr sv_wstr_open(uint32_t initial_length)
{
    initial_length += 1;
    check_fatal(
        initial_length >= 1 && initial_length < INT_MAX, "invalid length");

    sv_wstr self = {};
    self.arr = sv_array_open(sizeof(wchar_t), 0);
    sv_array_reserve(&self.arr, initial_length);
    sv_array_appendzeros(&self.arr, 1); /* add nul char*/
    return self;
}

void sv_wstr_close(sv_wstr *self)
{
    if (self)
    {
        sv_array_close(&self->arr);
        set_self_zero();
    }
}

void sv_wstr_calloc(sv_wstr *self, uint32_t len)
{
    /* first sv_wstr_calloc(),
    then copy text to buffer,
    then sv_wstr_truncate() */
    len += 1;
    sv_wstr_clear(self);
    sv_array_appendzeros(&self->arr, len);
}

void sv_wstr_truncate(sv_wstr *self, uint32_t len)
{
    sv_array_truncatelength(&self->arr, len + 1);
    *(wchar_t *)sv_array_at(&self->arr, len) = L'\0';
}

void sv_wstr_clear(sv_wstr *self)
{
    sv_array_truncatelength(&self->arr, 0);
    sv_wstr_append(self, L""); /* ensure null term */
}

void sv_wstr_append(sv_wstr *self, const wchar_t *s)
{
    /* erase old null term */
    if (self->arr.length > 0)
    {
        sv_array_truncatelength(&self->arr, self->arr.length - 1);
    }

    sv_array_append(&self->arr, (const byte *)s, wcslen32u(s));

    /* add new null term */
    sv_array_appendzeros(&self->arr, 1);
}

void sv_wstrlist_clear(sv_array *arr)
{
    for (uint32_t i = 0; i < arr->length; i++)
    {
        sv_wstr_close((sv_wstr *)sv_array_at(arr, i));
    }

    sv_array_truncatelength(arr, 0);
}
