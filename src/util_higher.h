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

typedef struct sv_log {
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
void sv_log_fmt(const char *fmt, ...);
void sv_log_write(const char *s);
void sv_log_writes(const char *s1, const char *s2);
void sv_log_flush(void);
void sv_log_addnewlinetime(FILE *f,
    int64_t start_of_day,
    int64_t seconds,
    long milliseconds);
void appendnumbertofilename(const char *dir,
    const char *prefix,
    const char *suffix,
    uint32_t number,
    bstring out);
uint32_t readnumberfromfilename(const char *prefix,
    const char *suffix,
    const char *candidate);
check_result readlatestnumberfromfilename(const char *dir,
    const char *prefix,
    const char *suffix,
    uint32_t *latestnumber);

struct sv_app;
typedef sv_result(*FnMenuCallback)(struct sv_app *, int);
typedef struct menu_action_entry {
    const char *message;
    FnMenuCallback callback;
    int arg;
} menu_action_entry;

int menu_choose_long(const bstrlist *list, int groupsize);
typedef const struct menu_action_entry *(*FnMenuGetNextMenu) (
    struct sv_app *);
check_result menu_choose_action(const char *msg,
    const menu_action_entry *entries,
    struct sv_app *app,
    FnMenuGetNextMenu getnextmenu);
int menu_choose(const char *msg,
    const bstrlist *list,
    const char *format_each,
    const char *additionalopt1,
    const char *additionalopt2);


#endif
