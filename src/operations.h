/*
operations.h

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

#include "op_sync_cloud.h"

typedef enum sv_enum_ops
{
    sv_run_none = 0,
    sv_run_backup,
    sv_run_restore,
    sv_run_restore_from_past,
    sv_run_compact,
    sv_run_verify,
    sv_run_viewinfo,
    sv_run_sync_cloud,
    sv_set_days_to_keep_prev_versions,
    sv_set_approx_archive_size_bytes,
    sv_set_compact_threshold_bytes,
    sv_set_separate_metadata_enabled,
    sv_set_pause_duration,
} sv_enum_ops;

typedef struct sv_backup_count
{
    uint64_t approx_items_in_queue;
    uint64_t count_new_files;
    uint64_t count_new_bytes;
    uint64_t count_new_path;
    uint64_t count_moved_path;
    bstring summary_current_dir;
    uint64_t summary_current_dir_size;
} sv_backup_count;

typedef struct sv_backup_state
{
    const sv_app *app;
    const sv_group *grp;
    svdb_db db;
    uint64_t collectionid;
    ar_manager archiver;
    sv_array rows_to_delete;
    bstrlist *messages;
    bstring tmp_result;
    time_t time_since_showing_update;
    uint64_t prev_percent_shown;
    sv_backup_count count;
    void *test_context;
} sv_backup_state;

typedef struct sv_restore_state
{
    bstring working_dir_archived;
    bstring working_dir_unarchived;
    bstring destdir;
    bstring scope;
    bstring destfullpath;
    bstring tmp_result;
    uint64_t collectionidwanted;
    bstrlist *messages;
    ar_manager archiver;
    svdb_db *db;
    uint64_t countfilesmatch;
    uint64_t countfilescomplete;
    uint64_t separate_metadata;
    bool restore_owners;
    bool user_canceled;
    void *test_context;
} sv_restore_state;

typedef struct sv_compact_state
{
    bstring working_dir_archived;
    bstring working_dir_unarchived;
    uint64_t expiration_cutoff;
    sv_2darray archive_stats;
    sv_array archives_to_remove;
    sv_array archives_to_strip;
    uint64_t thresholdsizebytes;
    bool is_thorough;
    bool user_canceled;
    void *test_context;
} sv_compact_state;

typedef struct sv_archive_stats
{
    uint64_t count_new;
    uint64_t size_new;
    uint64_t count_old;
    uint64_t size_old;
    uint32_t original_collection;
    uint32_t archive_number;
    sv_array old_individual_files;
} sv_archive_stats;

check_result sv_backup(
    const sv_app *app, const sv_group *grp, svdb_db *db, void *test_context);
check_result sv_compact(const sv_app *app, const sv_group *grp, svdb_db *db);
check_result sv_restore(
    const sv_app *app, const sv_group *grp, svdb_db *db, bool latest);
void show_status_update_queue(sv_backup_state *op);
check_result sv_application_run(sv_app *app, int optype);
check_result hook_provide_file_list(void *phook, void *context);
check_result hook_call_before_process_queue(void *phook, svdb_db *db);
check_result hook_get_file_info(void *phook, os_lockedfilehandle *self,
    uint64_t *, uint64_t *modtime, bstring permissions);
check_result hook_call_when_restoring_file(
    void *phook, const char *originalpath, bstring destpath);

void sv_restore_state_close(sv_restore_state *self);
check_result sv_restore_file(sv_restore_state *op,
    const sv_file_row *in_files_row, const bstring path,
    const bstring permissions);
check_result sv_restore_cb(void *context, const sv_file_row *in_files_row,
    const bstring path, const bstring permissions);
check_result sv_restore_checkbinarypaths(
    const sv_app *app, const sv_group *grp, sv_restore_state *op);
void sv_restore_show_messages(
    bool latestversion, const sv_group *grp, sv_restore_state *op);
check_result sv_restore_ask_user_options(
    const sv_app *app, bool latestversion, svdb_txn *txn, sv_restore_state *op);

void sv_compact_state_close(sv_compact_state *self);
check_result sv_compact_getcutoff(svdb_db *db, const sv_group *grp,
    uint64_t *collectionid_to_expire, time_t now);
check_result sv_compact_getarchivestats(
    void *context, const sv_content_row *row);
check_result sv_compact_see_what_to_remove_cb(
    void *context, uint32_t x, uint32_t y, byte *val);
void sv_compact_see_what_to_remove(
    sv_compact_state *op, uint64_t thresholdsizebytes);
check_result sv_strip_archive_removing_old_files(sv_compact_state *op,
    unused_ptr(const sv_app), unused_ptr(const sv_group), svdb_db *db,
    bstrlist *messages);
check_result sv_remove_entire_archives(sv_compact_state *op, svdb_db *db,
    const char *appdatadir, const char *grpname, bstrlist *messages);
check_result sv_compact_impl(
    const sv_group *grp, const sv_app *app, svdb_db *db, sv_compact_state *op);
check_result sv_compact_ask_user(const sv_group *grp, sv_compact_state *op);
void sv_compact_archivestats_to_string(
    const sv_compact_state *op, bool includefiles, bstring s);
check_result sv_sync_cloud(
    const sv_app *app, const sv_group *grp, svdb_db *db);

check_result sv_backup_show_results(sv_backup_state *op);
check_result sv_backup_show_user(sv_backup_state *op, bool before);
check_result sv_backup_addtoqueue(sv_backup_state *op);
void sv_backup_state_close(sv_backup_state *self);
void show_status_update_queue(sv_backup_state *op);
void pause_if_requested(sv_backup_state *op);
void show_status_update(sv_backup_state *op, unused_ptr(const char));
void sv_backup_compute_preview(sv_backup_state *op);
check_result sv_backup_record_data_checksums(sv_backup_state *op);
check_result sv_backup_recordcollectionstats(sv_backup_state *op);
check_result sv_backup_addtoqueue_cb(void *context, const bstring filepath,
    uint64_t lmt_from_disk, uint64_t actual_size_from_disk,
    const bstring permissions);
check_result sv_backup_fromtextfile(
    sv_backup_state *op, const char *appdir, const char *grpname);
check_result sv_backup_addfile(sv_backup_state *op, os_lockedfilehandle *handle,
    const bstring path, uint64_t rowid);
check_result sv_backup_processqueue_cb(void *context,
    const sv_file_row *in_files_row, const bstring path, unused(const bstring));
check_result sv_backup_makecopyofdb(
    sv_backup_state *op, const sv_group *grp, const char *appdir);
void sv_backup_compute_preview_on_new_file(
    sv_backup_state *op, const char *path, uint64_t rawcontentslength);
check_result sv_backup_compute_preview_cb(void *context,
    unused_ptr(const sv_file_row), const bstring path, unused(const bstring));

#endif
