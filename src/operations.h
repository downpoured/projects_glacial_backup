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

#ifndef OPERATIONS_H_INCLUDE
#define OPERATIONS_H_INCLUDE

#include "user_config.h"

typedef enum svdp_enum_ops
{
	svdp_op_none = 0,
	svdp_op_backup,
	svdp_op_restore,
	svdp_op_restore_from_past,
	svdp_op_compact,
	svdp_op_verify,
	svdp_op_viewinfo,
	svdp_setting_days_to_keep_prev_versions,
	svdp_setting_approx_archive_size_bytes,
	svdp_setting_compact_threshold_bytes,
	svdp_setting_separate_metadata_enabled,
	svdp_setting_pause_duration,
} svdp_enum_ops;

typedef struct svdp_backup_count {
	uint64_t approx_count_items_in_queue;
	uint64_t count_new_files;
	uint64_t count_new_bytes;
	uint64_t count_new_path;
	uint64_t count_moved_path;
	bstring summary_current_dir;
	uint64_t summary_current_dir_size;
} svdp_backup_count;

typedef struct svdp_backup_state {
	const svdp_application *app;
	const svdp_backupgroup *group;
	svdb_connection db;
	uint64_t collectionid;
	svdp_archiver archiver;
	sv_array dbrows_to_delete;
	bstrlist *messages;
	bstring tmp_resultstring;
	time_t time_since_showing_update;
	uint64_t prev_percent_shown;
	uint64_t pause_duration_seconds;
	svdp_backup_count count;
	void *test_context;
} svdp_backup_state;

typedef struct svdp_restore_state {
	bstring working_dir_archived;
	bstring working_dir_unarchived;
	bstring destdir;
	bstring scope;
	bstring destfullpath;
	bstring tmp_resultstring;
	uint64_t collectionidwanted;
	bstrlist *messages;
	svdp_archiver archiver;
	svdb_connection *db;
	uint64_t countfilesmatch;
	uint64_t countfilescomplete;
	uint64_t separate_metadata_enabled;
	bool restore_owners;
	void *test_context;
} svdp_restore_state;

typedef struct svdp_compact_state {
	bstring working_dir_archived;
	bstring working_dir_unarchived;
	uint64_t expiration_cutoff;
	sv_2d_array archive_statistics;
	sv_array archives_to_remove;
	sv_array archives_to_removefiles;
	bool is_thorough;
	void *test_context;
} svdp_compact_state;

typedef struct svdp_archive_stats {
	uint64_t needed_entries;
	uint64_t needed_size;
	uint64_t old_entries;
	uint64_t old_size;
	uint32_t original_collection;
	uint32_t archive_number;
	sv_array old_individual_files;
} svdp_archive_stats;

check_result svdp_backup(const svdp_application *app, const svdp_backupgroup *group, 
	svdb_connection *db, void *test_context);
check_result svdp_compact(const svdp_application *app, const svdp_backupgroup *group, 
	svdb_connection *db);
check_result svdp_restore(const svdp_application *app, const svdp_backupgroup *group, 
	svdb_connection *db, int optype);

check_result svdp_application_run(svdp_application *app, int optype);
check_result test_hook_get_file_info(void *phook, os_exclusivefilehandle *self, uint64_t *, 
	uint64_t *modtime, bstring permissions);
check_result test_hook_provide_file_list(void *phook, void *context, 
	FnRecurseThroughFilesCallback callback);
check_result test_hook_call_before_processing_queue(void *phook, svdb_connection *db, 
	svdp_archiver *archiver);
check_result test_hook_call_when_restoring_file(void *phook,
	const char *originalpath, bstring destpath);
void svdp_restore_state_close(svdp_restore_state *self);
void show_status_update_queue(svdp_backup_state *op);

check_result svdp_compact_getarchivestats(void *context, bool *, const svdb_contents_row *row);
void svdp_compact_state_close(svdp_compact_state *self);
void svdp_compact_see_what_to_remove(svdp_compact_state *op, uint64_t thresholdsizebytes);
void svdp_compact_archivestats_to_string(const svdp_compact_state *op, bool includefiles, 
	bstring s);
check_result svdp_compact_removeoldfilesfromarchives(svdp_compact_state *op, 
	const svdp_application *app, const svdp_backupgroup *group, svdb_connection *db, 
	const char *readydir, bstrlist *messages);
check_result svdp_compact_removeoldarchives(svdp_compact_state *op, svdb_connection *db,
	const char *appdatadir, const char *groupname, const char *readydir, bstrlist *messages);


#endif
