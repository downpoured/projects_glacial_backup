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
	svdp_setting_days_to_keep_prev_versions,
	svdp_setting_approx_archive_size_bytes,
	svdp_setting_compact_threshold_bytes,
	svdp_setting_separate_metadata_enabled,
} svdp_enum_ops;

typedef struct svdp_backup_state {
	const svdp_application* app;
	const svdp_backupgroup* group;
	svdb_connection db;
	bool preview;
	uint64_t collectionId;
	svdp_archiver archiver;
	sv_array dbrowsToDelete;
	uint64_t approxFilesInQueue;
	uint64_t approxBytesInQueue;
	uint64_t approxBytesCompleted;
	uint64_t processNewFiles;
	uint64_t processNewBytes;
	uint64_t processCountNewpath;
	uint64_t processCountMovedfrompath;
	bstrList* listSkippedSymlink;
	bstrList* listSkippedIterationFailed;
	bstrList* listSkippedFileAccessFailed;
	time_t timestarted;
	uint64_t prevPercentShown;
	void* test_context;
} svdp_backup_state;

typedef struct svdp_restore_state {
	bstring workingdir;
	bstring subworkingdir;
	bstring destdir;
	bstring scope;
	bstring destfullpath;
	bstring errmsg;
	uint64_t collectionidwanted;
	bstrList* messages;
	svdp_archiver archiver;
	svdb_connection* db;
	uint64_t countfiles;
	void* test_context;
} svdp_restore_state;

typedef struct svdp_compact_state {
	uint64_t expiration_cutoff;
	sv_2d_array archivenum_tostats;
	bool is_thorough;
	void* test_context;
} svdp_compact_state;

check_result svdp_backup(const svdp_application* app, const svdp_backupgroup* group, svdb_connection* db, void* test_context);
check_result svdp_compact(const svdp_application* app, const svdp_backupgroup* group, svdb_connection* db);
check_result svdp_restore(const svdp_application* app, const svdp_backupgroup* group, svdb_connection* db, int optype);
check_result svdp_application_run(svdp_application* app, int optype);
check_result test_hook_get_file_info(void* phook, os_exclusivefilehandle* self, uint64_t*, uint64_t* modtime, uint64_t* permissions);
check_result test_hook_provide_file_list(void* phook, void* context, FnRecurseThroughFilesCallback callback);
check_result test_hook_call_before_processing_queue(void* phook, svdb_connection* db);
check_result test_hook_call_when_restoring_file(void* phook, const char* originalpath, bstring destpath);

typedef struct svdp_archive_info {
	uint64_t needed_entries;
	uint64_t needed_size;
	uint64_t old_entries;
	uint64_t old_size;
	uint32_t original_collection;
	uint32_t archive_number;
	sv_array old_individual_files;
} svdp_archive_info;

#endif
