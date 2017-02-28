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
	bstrList *root_directories;
	bstrList *exclusion_patterns;
	uint32_t days_to_keep_prev_versions;
	bstring encryption;
	uint32_t separate_metadata_enabled;
	uint32_t approx_archive_size_bytes;
	uint32_t compact_threshold_bytes;
} svdp_backupgroup;

check_result svdp_backupgroup_load(svdb_connection* db, svdp_backupgroup* self, const char* groupname);
check_result svdp_backupgroup_persist(svdb_connection* db, const svdp_backupgroup* self);
bool svdp_backupgroup_isincluded(const svdp_backupgroup* self, const char* path);
void svdp_backupgroup_close(svdp_backupgroup* self);

typedef struct svdp_application {
	bstring path_app_data;
	sv_log logging;
	bstrList *backup_group_names;
	int32_t current_group_name_index;
	bool is_low_access;
} svdp_application;

check_result svdp_application_load(svdp_application* self, const char* dir, bool is_low_access);
check_result svdp_application_run_lowpriv(svdp_application* app, int);
check_result svdp_application_findgroupnames(svdp_application* self);
check_result svdp_application_creategroup(svdp_application* self, int);
check_result svdp_application_creategroup_impl(svdp_application* self, const char* newgroupname, const char* groupdir);
check_result svdp_application_viewinfo(svdp_application* app, int);
void svdp_application_groupdbpathfromname(const svdp_application* app, const char* groupname, bstring outpath);
void svdp_application_close(svdp_application* self);

void pickgroup(svdp_application* app);
void add_default_exclusion_patterns(svdp_backupgroup* group);
bool isvalidchosendirectory(const svdp_application* app, bstring newpath);
check_result ui_app_menu_edit_group_edit_dirs(svdp_application* app, int);
check_result ui_app_menu_edit_group_edit_excl_patterns(svdp_application* app, int);
check_result ui_app_menu_edit_group_edit_encryption(svdp_application* app, int);
check_result ui_app_edit_setting(svdp_application* app, int whichsetting);
check_result loadbackupgroup(const svdp_application* app, svdp_backupgroup* grp, svdb_connection* connection);

#endif
