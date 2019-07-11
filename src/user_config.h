/*
user_config.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef USER_CONFIG_H_INCLUDE
#define USER_CONFIG_H_INCLUDE

#include "dbaccess.h"

typedef struct sv_group
{
    bstring grpname;
    bstrlist *root_directories;
    bstrlist *exclusion_patterns;
    uint32_t days_to_keep_prev_versions;
    uint32_t separate_metadata;
    uint32_t approx_archive_size_bytes;
    uint32_t compact_threshold_bytes;
    uint32_t pause_duration_seconds;
} sv_group;

typedef struct sv_app
{
    bstring path_app_data;
    bstring path_temp_archived;
    bstring path_temp_unarchived;
    sv_log log;
    bstrlist *grp_names;
    bool low_access;
} sv_app;

void sv_grp_close(sv_group *self);
check_result sv_grp_load(svdb_db *db, sv_group *self, const char *grpname);
check_result sv_grp_persist(svdb_db *db, const sv_group *self);
bool sv_grp_isincluded(
    const sv_group *self, const char *path, bstrlist *messages);

void sv_app_close(sv_app *self);
check_result sv_app_load(sv_app *self, const char *dir, bool low_access);
check_result sv_app_run_lowpriv(sv_app *app, int);
check_result sv_app_findgroupnames(sv_app *self);
check_result sv_app_viewinfo(const sv_group *grp, svdb_db *db);
check_result sv_app_creategroup(sv_app *self, int);
check_result sv_app_creategroup_impl(
    sv_app *self, const char *newgroupname, const char *groupdir);
void sv_app_groupdbpathfromname(
    const sv_app *app, const char *grpname, bstring outpath);

void add_default_group_settings(sv_group *grp);
bool check_user_typed_dir(const sv_app *app, bstring newpath);
check_result app_menu_edit_grp_dirs(sv_app *app, int);
check_result app_menu_edit_grp_patterns(sv_app *app, int);
check_result app_edit_setting(sv_app *app, int whichsetting);
check_result load_backup_group(const sv_app *app, sv_group *grp, svdb_db *db,
    const char *optional_groupname);
check_result sv_verify_archives(
    const sv_app *app, const sv_group *grp, svdb_db *db, int *countmismatches);
check_result write_archive_checksum(svdb_db *db, const char *filepath,
    uint64_t compaction_cutoff, bool stillneeded);

#endif
