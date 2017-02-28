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

#ifndef USER_CONFIG_H_INCLUDE
#define USER_CONFIG_H_INCLUDE

#include "dbaccess.h"

typedef struct svdp_backupgroup {
	bstring groupname;
	bstrlist *root_directories;
	bstrlist *exclusion_patterns;
	uint32_t days_to_keep_prev_versions;
	bstring encryption;
	uint32_t use_encryption;
	uint32_t separate_metadata_enabled;
	uint32_t approx_archive_size_bytes;
	uint32_t compact_threshold_bytes;
	uint32_t pause_duration_seconds;
} svdp_backupgroup;

typedef struct svdp_application {
	bstring path_app_data;
	bstring path_temp_archived;
	bstring path_temp_unarchived;
	sv_log logging;
	bstrlist *backup_group_names;
	bool is_low_access;
} svdp_application;

void svdp_backupgroup_close(svdp_backupgroup *self);
check_result svdp_backupgroup_load(svdb_connection *db,
	svdp_backupgroup *self, const char *groupname);
check_result svdp_backupgroup_persist(svdb_connection *db,
	const svdp_backupgroup *self);
check_result svdp_backupgroup_ask_key(svdp_backupgroup *self,
	const svdp_application *app, bool isbackup);
bool svdp_backupgroup_isincluded(const svdp_backupgroup *self,
	const char *path, bstrlist *messages);

void svdp_application_close(svdp_application *self);
check_result svdp_application_load(svdp_application *self, const char *dir, bool is_low_access);
check_result svdp_application_run_lowpriv(svdp_application *app, int);
check_result svdp_application_findgroupnames(svdp_application *self);
check_result svdp_application_creategroup(svdp_application *self, int);
check_result svdp_application_creategroup_impl(svdp_application *self, 
	const char *newgroupname, const char *groupdir);
check_result svdp_application_viewinfo(const svdp_application *app,
	const svdp_backupgroup *group, svdb_connection *db);
void svdp_application_groupdbpathfromname(const svdp_application *app,
	const char *groupname, bstring outpath);

void add_default_group_settings(svdp_backupgroup *group);
bool isvalidchosendirectory(const svdp_application *app, bstring newpath);
check_result ui_app_menu_edit_group_edit_dirs(svdp_application *app, int);
check_result ui_app_menu_edit_group_edit_excl_patterns(svdp_application *app, int);
check_result ui_app_menu_edit_group_edit_encryption(svdp_application *app, int);
check_result ui_app_edit_setting(svdp_application *app, int whichsetting);
check_result loadbackupgroup(const svdp_application *app, svdp_backupgroup *grp,
	svdb_connection *connection, const char *optional_groupname);
check_result svdp_verify_archive_checksums(const svdp_application *app,
	const svdp_backupgroup *group, svdb_connection *db, int *countmismatches);
check_result write_archive_checksum(svdb_connection *db, const char *filepath, 
	uint64_t compaction_cutoff, bool nolongerneeded);

#endif
