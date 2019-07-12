/*
operations.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "operations.h"

check_result sv_application_run(sv_app *app, int op)
{
    sv_result currenterr = {};
    sv_group grp = {};
    svdb_db db = {};
    check(load_backup_group(app, &grp, &db, NULL));
    if (db.db)
    {
        sv_log_fmt("run %d with %s", op, cstr(grp.grpname));
        switch (op)
        {
        case sv_run_compact:
            check(sv_compact(app, &grp, &db));
            break;
        case sv_run_backup:
            check(sv_backup(app, &grp, &db, NULL));
            break;
        case sv_run_viewinfo:
            check(sv_app_viewinfo(&grp, &db));
            break;
        case sv_run_verify:
            check(sv_verify_archives(app, &grp, &db, NULL));
            break;
        case sv_run_restore:
            check(sv_restore(app, &grp, &db, true));
            break;
        case sv_run_restore_from_past:
            check(sv_restore(app, &grp, &db, false));
            break;
        default:
            break;
        }
    }

    check(svdb_disconnect(&db));

cleanup:
    sv_grp_close(&grp);
    svdb_close(&db);
    return currenterr;
}

check_result sv_backup(
    const sv_app *app, const sv_group *grp, svdb_db *db, void *test_context)
{
    sv_result currenterr = {};
    sv_backup_state op = {};
    svdb_txn txn = {};
    bstring dbpath = bstring_open();
    uint64_t timestarted = (uint64_t)time(NULL);

    /* 1) initialize */
    op.app = app;
    op.grp = grp;
    op.test_context = test_context;
    op.messages = bstrlist_open();
    op.rows_to_delete = sv_array_open_u64();
    op.prev_percent_shown = UINT64_MAX;
    op.tmp_result = bstring_open();
    os_clr_console();
    sv_app_groupdbpathfromname(app, cstr(grp->grpname), dbpath);
    check(svdb_disconnect(db));
    check(svdb_connect(&op.db, cstr(dbpath)));
    check(svdb_txn_open(&txn, &op.db));
    check(svdb_collectioninsert(&op.db, timestarted, &op.collectionid));
    check(ar_manager_open(&op.archiver, cstr(op.app->path_app_data),
        cstr(op.grp->grpname), cast64u32u(op.collectionid),
        op.grp->approx_archive_size_bytes));
    check(checkbinarypaths(&op.archiver.ar, op.grp->separate_metadata,
        cstr(op.archiver.path_working)));

    /* 2) add files to queue */
    check(sv_backup_addtoqueue(&op));
    check(sv_backup_fromtextfile(
        &op, cstr(op.app->path_app_data), cstr(op.grp->grpname)));
    check(hook_call_before_process_queue(op.test_context, &op.db));
    check(sv_backup_show_user(&op, true));

    /* 3) process files in queue */
    check(ar_manager_begin(&op.archiver));
    check(svdb_files_iter(&op.db,
        sv_makestatus(op.collectionid, sv_filerowstatus_complete), &op,
        sv_backup_processqueue_cb));
    check(sv_backup_show_user(&op, false));
    check(svdb_files_delete(&op.db, &op.rows_to_delete, 0));
    check(sv_backup_recordcollectionstats(&op));
    check(ar_manager_finish(&op.archiver));
    check(sv_backup_record_data_checksums(&op));
    check(svdb_txn_commit(&txn, &op.db));
    check(svdb_disconnect(&op.db));
    check(sv_backup_makecopyofdb(&op, grp, cstr(app->path_app_data)));

    /* 4) show results to user */
    check(sv_backup_show_results(&op));

cleanup:
    svdb_txn_close(&txn, &op.db);
    svdb_close(&op.db);
    sv_backup_state_close(&op);
    bdestroy(dbpath);
    return currenterr;
}

check_result sv_restore(
    const sv_app *app, const sv_group *grp, svdb_db *db, bool latestversion)
{
    sv_result currenterr = {};

    /* 1) initialize */
    sv_restore_state op = {};
    svdb_txn txn = {};
    op.working_dir_archived = bstrcpy(app->path_temp_archived);
    op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
    op.separate_metadata = grp->separate_metadata;
    op.destdir = bstring_open();
    op.scope = bstring_open();
    op.destfullpath = bstring_open();
    op.tmp_result = bstring_open();
    op.messages = bstrlist_open();
    op.db = db;
    check(sv_restore_checkbinarypaths(app, grp, &op));

    /* 2) ask which version to restore */
    check(sv_restore_ask_user_options(app, latestversion, &txn, &op));
    if (!op.user_canceled)
    {
        /* 3) run restore */
        check(svdb_files_iter(op.db, svdb_all_files, &op, &sv_restore_cb));
        sv_restore_show_messages(latestversion, grp, &op);
        check(svdb_txn_rollback(&txn, op.db));
        check(svdb_disconnect(op.db));
        printf("Restore complete for %lld/%lld files.",
            castull(op.countfilescomplete), castull(op.countfilesmatch));
        alert("");
    }

cleanup:
    svdb_txn_close(&txn, op.db);
    sv_restore_state_close(&op);
    return currenterr;
}

check_result sv_compact(const sv_app *app, const sv_group *grp, svdb_db *db)
{
    sv_result currenterr = {};

    /* 1) initialize */
    svdb_txn txn = {};
    sv_compact_state op = {};
    op.working_dir_archived = bstrcpy(app->path_temp_archived);
    op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
    op.archive_stats = sv_2darray_open(sizeof32u(sv_archive_stats));
    bstring msg = bstring_open();

    /* 2) determine what space can be reclaimed. */
    check(sv_compact_ask_user(grp, &op));
    check(svdb_txn_open(&txn, db));
    check(sv_compact_getcutoff(db, grp, &op.expiration_cutoff, time(NULL)));
    if (op.expiration_cutoff && !op.user_canceled)
    {
        /* 3) if "thorough" mode enabled, look in each .tar for old data. */
        check(svdb_contentsiter(db, &op, &sv_compact_getarchivestats));
        sv_compact_see_what_to_remove(&op, grp->compact_threshold_bytes);
        sv_compact_archivestats_to_string(&op, false, msg);
        sv_log_write(cstr(msg));
        check(svdb_txn_commit(&txn, db));

        /* 4) run compaction */
        check(sv_compact_impl(grp, app, db, &op));
    }

cleanup:
    bdestroy(msg);
    svdb_txn_close(&txn, db);
    sv_compact_state_close(&op);
    return currenterr;
}

check_result sv_backup_addtoqueue_cb(void *context, const bstring path,
    uint64_t actual_lmt, uint64_t actual_size, const bstring perms)
{
    sv_result currenterr = {};
    sv_file_row row = {0};
    sv_backup_state *op = (sv_backup_state *)context;
    uint64_t size = actual_size;
    show_status_update_queue(op);

    if (os_recurse_is_dir(actual_lmt, size) ||
        !sv_grp_isincluded(op->grp, cstr(path), op->messages))
    {
        /* case 1: it's a directory, or it's excluded by user */
        goto cleanup;
    }

    check(svdb_filesbypath(&op->db, path, &row));
    adjustfilesize_if_audio_file(op->grp->separate_metadata,
        get_file_extension_info(cstr(path), blength(path)), size, &size);

    if (row.id != 0 && row.contents_length == size &&
        row.last_write_time == actual_lmt &&
        row.most_recent_collection == op->collectionid - 1 &&
        row.e_status == sv_filerowstatus_complete)
    {
        /* case 2: the file hasn't been changed. */
        sv_log_fmt("queue-same %s %llx", cstr(path), castull(row.id));
        row.most_recent_collection = op->collectionid;
        row.e_status = sv_filerowstatus_complete;
        check(svdb_filesupdate(&op->db, &row, perms));
        check(svdb_contents_setlastreferenced(
            &op->db, row.contents_id, op->collectionid));
    }
    else if (row.id != 0)
    {
        /* case 3: the file has been changed, add to queue. a file is added
        to the queue by setting a flag on a row in the fileslist table. */
        sv_log_fmt("queue-changed %s %llx", cstr(path), castull(row.id));
        op->count.approx_items_in_queue += 1;
        row.most_recent_collection = op->collectionid;
        row.e_status = sv_filerowstatus_queued;
        check(svdb_filesupdate(&op->db, &row, perms));
    }
    else
    {
        /* case 4: it's a new file, add to queue. */
        sv_log_writes("queue-new", cstr(path));
        op->count.approx_items_in_queue += 1;
        check(svdb_filesinsert(&op->db, path, row.most_recent_collection,
            sv_filerowstatus_queued, NULL));
    }

cleanup:
    return currenterr;
}

check_result sv_backup_addtoqueue(sv_backup_state *op)
{
    sv_result currenterr = {};
    bstring tmp = bstring_open();
    if (!op->test_context && op->grp->root_directories->qty == 0)
    {
        alert("\nThere are currently no directories to "
              "be backed up for this group. To add a directory, "
              "go to 'Edit Group' and choose 'Add/remove directories'.");
        goto cleanup;
    }

    check(hook_provide_file_list(op->test_context, op));
    sv_log_fmt("sv_backup_addtoqueue collection=%llu", op->collectionid);
    for (int i = 0; i < op->grp->root_directories->qty; i++)
    {
        const char *dir = blist_view(op->grp->root_directories, i);
        if (os_dir_exists(dir))
        {
            sv_log_writes("sv_backup_addtoqueue", dir);
            printf("Searching %s...\n", dir);
            os_recurse_params params = {
                op, dir, &sv_backup_addtoqueue_cb, PATH_MAX, op->messages};
            check(os_recurse(&params));
        }
        else
        {
            log_b(0, "Note: starting directory \n%s\nwas not found.", dir);
            printf("\nNote: starting directory \n%s\nwas not found. You can "
                   "go to 'Edit Group', 'Add/Remove Directories' to remove this "
                   "directory from the list.\n",
                dir);
        }
    }

cleanup:
    bdestroy(tmp);
    return currenterr;
}

check_result sv_backup_fromtextfile(
    sv_backup_state *op, const char *appdir, const char *grpname)
{
    sv_result currenterr = {};
    os_lockedfilehandle handle = {};
    sv_pseudosplit split = sv_pseudosplit_open("");
    bool filenotfound = false;
    bstring filelistpath = bformat("%s%suserdata%s%s%smanual-file-list.txt",
        appdir, pathsep, pathsep, grpname, pathsep);
    bstring permissions = bstring_open();
    if (!os_file_exists(cstr(filelistpath)))
    {
        goto cleanup;
    }

    check(sv_file_readfile(cstr(filelistpath), split.text));
    bstr_replaceall(split.text, "\r", "");
    sv_pseudosplit_split(&split, '\n');
    printf("manual file list. %d entries...\n", split.splitpoints.length);
    sv_log_fmt("manual file list %s. %d entries...\n", cstr(filelistpath),
        split.splitpoints.length);

    for (uint32_t i = 0; i < split.splitpoints.length; i++)
    {
        if (sv_pseudosplit_viewat(&split, i)[0])
        {
            check(os_lockedfilehandle_tryuntil_open(
                &handle, sv_pseudosplit_viewat(&split, i), true, &filenotfound));

            if (filenotfound)
            {
                printf("not found %s\n", sv_pseudosplit_viewat(&split, i));
                check_b(ask_user("continue?"), "user chose not to continue.");
            }
            else
            {
                uint64_t lmt_from_disk = 0, actual_size_from_disk = 0;
                check(os_lockedfilehandle_stat(&handle, &actual_size_from_disk,
                    &lmt_from_disk, permissions));
                check(sv_backup_addtoqueue_cb(op, split.currentline,
                    lmt_from_disk, actual_size_from_disk, permissions));
            }

            os_lockedfilehandle_close(&handle);
        }
    }

    check_b(op->collectionid, "must have valid collectionid");

cleanup:
    sv_pseudosplit_close(&split);
    os_lockedfilehandle_close(&handle);
    bdestroy(filelistpath);
    bdestroy(permissions);
    return currenterr;
}

void sv_backup_state_close(sv_backup_state *self)
{
    if (self)
    {
        bstrlist_close(self->messages);
        bdestroy(self->tmp_result);
        bdestroy(self->count.summary_current_dir);
        ar_manager_close(&self->archiver);
        sv_array_close(&self->rows_to_delete);
        svdb_close(&self->db);
        set_self_zero();
    }
}

void show_status_update_queue(sv_backup_state *op)
{
    time_t now = time(NULL);
    if (!op->test_context && now - op->time_since_showing_update > 4)
    {
        fputs(".", stdout);
        op->time_since_showing_update = now;
    }
}

void pause_if_requested(sv_backup_state *op)
{
    time_t now = time(NULL);
    if (!op->test_context && now - op->time_since_showing_update > 30)
    {
        for (uint64_t i = 0; i < op->grp->pause_duration_seconds / 2; i++)
        {
            os_sleep(2 * 1000);
            fputs(".", stdout);
        }

        op->time_since_showing_update = time(NULL);
        fputs("\n", stdout);
    }
}

void show_status_update(sv_backup_state *op, unused_ptr(const char))
{
    if (op->count.approx_items_in_queue != 0)
    {
        uint64_t percentage_done =
            (100 * (op->count.count_moved_path + op->count.count_new_path)) /
            op->count.approx_items_in_queue;
        percentage_done = MIN(percentage_done, 99);
        if (!op->test_context && percentage_done != op->prev_percent_shown)
        {
            printf("%llu%% complete...\n", castull(percentage_done));
            op->prev_percent_shown = percentage_done;
        }
    }
}

check_result sv_backup_addfile(sv_backup_state *op, os_lockedfilehandle *handle,
    const bstring path, uint64_t rowid)
{
    sv_result currenterr = {};
    sv_file_row newfilesrow = {};
    sv_content_row contentsrow = {};

    bstring permissions = bstring_open();
    uint64_t contentslength = 0, rawcontentslength = 0, modtimeondisk = 0;
    hash256 hash = {};
    uint32_t crc32 = 0;
    efiletype ext = get_file_extension_info(cstr(path), blength(path));
    check(os_lockedfilehandle_stat(
        handle, &rawcontentslength, &modtimeondisk, permissions));
    check(hook_get_file_info(op->test_context, handle, &rawcontentslength,
        &modtimeondisk, permissions));
    adjustfilesize_if_audio_file(
        op->grp->separate_metadata, ext, rawcontentslength, &contentslength);
    check(hash_of_file(handle, op->grp->separate_metadata, ext,
        cstr(op->archiver.ar.audiotag_binary), &hash, &crc32));
    newfilesrow.last_write_time = modtimeondisk;
    newfilesrow.contents_length = contentslength;

    check(svdb_contentsbyhash(
        &op->db, &hash, newfilesrow.contents_length, &contentsrow));

    if (contentsrow.id)
    {
        /* contents are already in an archive */
        sv_log_fmt("addfile seen %s fid=%llx cid=%llx", cstr(path),
            castull(rowid), castull(contentsrow.id));
        check(svdb_contents_setlastreferenced(
            &op->db, contentsrow.id, op->collectionid));
        newfilesrow.contents_id = contentsrow.id;
    }
    else
    {
        /* get the latest contentrowid */
        sv_content_row newcontentsrow = {0};
        check(svdb_contentsinsert(&op->db, &newcontentsrow.id));

        /* add to an archive on disk */
        bool iscompressed = ext != filetype_none;
        sv_log_fmt("addfile new %s fid=%llx cid=%llx", cstr(path),
            castull(rowid), castull(newcontentsrow.id));
        check(ar_manager_add(&op->archiver, cstr(path), iscompressed,
            newcontentsrow.id, &newcontentsrow.archivenumber,
            &newcontentsrow.compressed_contents_length));

        /* add to the database */
        newcontentsrow.hash = hash;
        newcontentsrow.crc32 = crc32;
        newcontentsrow.contents_length = newfilesrow.contents_length;
        newcontentsrow.original_collection = cast64u32u(op->collectionid);
        newcontentsrow.most_recent_collection = op->collectionid;
        check(svdb_contentsupdate(&op->db, &newcontentsrow));
        newfilesrow.contents_id = newcontentsrow.id;
        op->count.count_new_files += 1;
        op->count.count_new_bytes += rawcontentslength;
    }

    /* update the fileslist row */
    newfilesrow.id = rowid;
    newfilesrow.most_recent_collection = op->collectionid;
    newfilesrow.e_status = sv_filerowstatus_complete;
    check(svdb_filesupdate(&op->db, &newfilesrow, permissions));

cleanup:
    bdestroy(permissions);
    return currenterr;
}

check_result sv_backup_processqueue_cb(void *context,
    const sv_file_row *in_files_row, const bstring path, unused(const bstring))
{
    sv_result currenterr = {};
    os_lockedfilehandle handle = {};
    sv_backup_state *op = (sv_backup_state *)context;
    pause_if_requested(op);
    show_status_update(op, cstr(path));
    bool filenotfound = false;

    sv_result result_openhandle = os_lockedfilehandle_tryuntil_open(
        &handle, cstr(path), true, &filenotfound);

    if (result_openhandle.code && in_files_row->contents_id == 0)
    {
        /* case 1: can't access file, it's a file we've never seen in past. */
        sv_log_fmt("queue-noaccessnotseen %s %llx", cstr(path),
            castull(in_files_row->id));
        sv_array_add64u(&op->rows_to_delete, in_files_row->id);
    }
    else if (result_openhandle.code)
    {
        /* case 2: can't access file, use the latest version we've stored. */
        sv_log_fmt(
            "queue-noaccess %s %llx", cstr(path), castull(in_files_row->id));
        bsetfmt(op->tmp_result, "Could not access %s", cstr(path));
        bstrlist_append(op->messages, op->tmp_result);
        check(svdb_contents_setlastreferenced(
            &op->db, in_files_row->contents_id, op->collectionid));
    }
    else if (!result_openhandle.code && filenotfound)
    {
        /* case 3: can't access file because it no longer exists */
        sv_log_fmt(
            "queue-notfound %s %llx", cstr(path), castull(in_files_row->id));
        sv_array_add64u(&op->rows_to_delete, in_files_row->id);
        op->count.count_moved_path++;
    }
    else
    {
        /* case 4: can access file, store its contents */
        check(sv_backup_addfile(op, &handle, path, in_files_row->id));
        op->count.count_new_path++;
    }

cleanup:
    sv_result_close(&result_openhandle);
    os_lockedfilehandle_close(&handle);
    return currenterr;
}

check_result sv_backup_show_user(sv_backup_state *op, bool before)
{
    if (before && !op->test_context)
    {
        printf("\nThere %s %llu added, changed, or renamed file%s.\n\n",
            op->count.approx_items_in_queue == 1 ? "is" : "are",
            castull(op->count.approx_items_in_queue),
            op->count.approx_items_in_queue == 1 ? "" : "s");

        sv_backup_compute_preview(op);
    }
    else if (!op->test_context)
    {
        printf("100%% complete...");
        printf("\n%llu new file%s (%0.3f Mb) archived.\n ",
            castull(op->count.count_new_files),
            op->count.count_new_files == 1 ? "" : "s",
            (double)op->count.count_new_bytes / (1024.0 * 1024));
    }

    return OK;
}

check_result sv_backup_show_results(sv_backup_state *op)
{
    if (op->messages->qty && !op->test_context)
    {
        puts("\nNotes:");
        for (int i = 0; i < op->messages->qty; i++)
        {
            printf("%s\n", blist_view(op->messages, i));
        }
    }

    if (!op->test_context)
    {
        printf("\n\nBackups saved successfully to\n%s\n\nYou can now copy "
               "the contents of this directory to an external drive, or upload "
               "to online storage.",
            cstr(op->archiver.path_readytoupload));
        alert("");
    }

    return OK;
}

check_result sv_backup_makecopyofdb(
    sv_backup_state *op, const sv_group *grp, const char *appdir)
{
    bstring src = bformat("%s%suserdata%s%s%s%s_index.db", appdir, pathsep,
        pathsep, cstr(grp->grpname), pathsep, cstr(grp->grpname));
    bstring dest = bformat("%s%suserdata%s%s%sreadytoupload%s%05x_index.db",
        appdir, pathsep, pathsep, cstr(grp->grpname), pathsep, pathsep,
        cast64u32u(op->collectionid));

    /* make a copy of it to the upload dir */
    sv_log_fmt("copy %s to %s", cstr(src), cstr(dest));
    os_sleep(op->test_context ? 1 : 250);
    log_b(os_tryuntil_copy(cstr(src), cstr(dest), true), "couldn't copy db.");
    bdestroy(src);
    bdestroy(dest);
    return OK;
}

void sv_backup_compute_preview_on_new_file(
    sv_backup_state *op, const char *path, uint64_t rawcontentslength)
{
    const int noteworthysize = 20 * 1024 * 1024;
    os_get_parent(path, op->tmp_result);
    if (!bstr_equal(op->count.summary_current_dir, op->tmp_result))
    {
        /* recurse-through-directory will make files in the same directory
        adjacent to each other, and so when the directory changes we're done
        with this directory. */
        if (op->count.summary_current_dir_size > noteworthysize)
        {
            printf("\n%s -- %.3fMb of new data\n",
                cstr(op->count.summary_current_dir),
                (double)op->count.summary_current_dir_size / (1024.0 * 1024.0));
        }

        op->count.summary_current_dir_size = 0;
        bassign(op->count.summary_current_dir, op->tmp_result);
    }

    op->count.count_new_bytes += rawcontentslength;
    op->count.summary_current_dir_size += rawcontentslength;
}

check_result sv_backup_compute_preview_cb(void *context,
    unused_ptr(const sv_file_row), const bstring path, unused(const bstring))
{
    sv_result currenterr = {};
    os_lockedfilehandle handle = {};
    sv_backup_state *op = (sv_backup_state *)context;
    bool filenotfound = false;
    sv_result result_handle =
        os_lockedfilehandle_open(&handle, cstr(path), true, &filenotfound);

    if (!filenotfound && !result_handle.code)
    {
        hash256 hash = {};
        uint32_t crc32 = 0;
        uint64_t contentlength = 0, rawlength = 0, modtime = 0;
        efiletype ext = get_file_extension_info(cstr(path), blength(path));
        check(os_lockedfilehandle_stat(&handle, &rawlength, &modtime, NULL));
        adjustfilesize_if_audio_file(
            op->grp->separate_metadata, ext, rawlength, &contentlength);

        /* if this is the first time running backup, skip the db lookup */
        bool file_is_new = op->collectionid == 1;
        if (!file_is_new)
        {
            check(hash_of_file(&handle, op->grp->separate_metadata, ext,
                cstr(op->archiver.ar.audiotag_binary), &hash, &crc32));

            sv_content_row found = {};
            check(svdb_contentsbyhash(&op->db, &hash, contentlength, &found));
            if (!found.id)
            {
                file_is_new = true;
            }
        }

        if (file_is_new)
        {
            sv_backup_compute_preview_on_new_file(op, cstr(path), rawlength);
        }
    }

cleanup:
    sv_result_close(&result_handle);
    os_lockedfilehandle_close(&handle);
    return currenterr;
}

void sv_backup_compute_preview(sv_backup_state *op)
{
    if (ask_user("Show preview? y/n"))
    {
        op->count.summary_current_dir = bstring_open();
        op->count.count_new_bytes = 0;
        check_warn(
            svdb_files_iter(&op->db,
                sv_makestatus(op->collectionid, sv_filerowstatus_complete), op,
                sv_backup_compute_preview_cb),
            "", continue_on_err);
        sv_backup_compute_preview_on_new_file(op, "", 0);
        printf("\nNew files were seen, for a total of %.3fMb new data.\n",
            (double)op->count.count_new_bytes / (1024.0 * 1024.0));
        op->count.count_new_bytes = 0;
        alert("");
    }
}

check_result sv_backup_record_data_checksums(sv_backup_state *op)
{
    sv_result currenterr = {};
    bstring filename = bstring_open();
    bstring filenamestartswith = bformat("%05llx_", castull(op->collectionid));
    const char *dir_readytoupload = cstr(op->archiver.path_readytoupload);
    bstrlist *files = bstrlist_open();
    check(os_listfiles(dir_readytoupload, files, true));
    for (int i = 0; i < files->qty; i++)
    {
        os_get_filename(blist_view(files, i), filename);
        if (s_startwith(cstr(filename), cstr(filenamestartswith)) &&
            !s_contains(cstr(filename), " ") &&
            s_endwith(cstr(filename), ".tar"))
        {
            check(write_archive_checksum(
                &op->db, blist_view(files, i), 0, true /* still needed */));
            fputs(".", stdout);
        }
    }

cleanup:
    bdestroy(filename);
    bdestroy(filenamestartswith);
    return currenterr;
}

check_result sv_backup_recordcollectionstats(sv_backup_state *op)
{
    sv_result currenterr = {};
    time_t timecompleted = time(NULL);
    sv_collection_row updatedrow = {0};
    updatedrow.id = op->collectionid;
    updatedrow.time_finished = cast64s64u(timecompleted);
    check(svdb_filescount(&op->db, &updatedrow.count_total_files));
    updatedrow.count_new_contents = op->count.count_new_files;
    updatedrow.count_new_contents_bytes = op->count.count_new_bytes;
    check(svdb_collectionupdate(&op->db, &updatedrow));
    svdb_collectiontostring(&updatedrow, true, true, op->tmp_result);
    sv_log_writes("Writing collection stats", cstr(op->tmp_result));
    if (!op->test_context)
    {
        printf("   Total number of backed up files: %llu\n\n",
            castull(updatedrow.count_total_files));
    }

cleanup:
    return currenterr;
}

check_result sv_compact_getcutoff(svdb_db *db, const sv_group *grp,
    uint64_t *collectionid_to_expire, time_t now)
{
    /* example: the user has told us that archives expire in '30 days'.
    So look for the last collection that was made >= 30 days from now. */
    sv_result currenterr = {};
    sv_array collectionrows = sv_array_open(sizeof32u(sv_collection_row), 0);
    check(svdb_collectionsget(db, &collectionrows, true));
    *collectionid_to_expire = 0;
    if (collectionrows.length == 0 || collectionrows.length == 1)
    {
        sv_log_write("Nothing to compact, there is no old data.");
        printf("Nothing to compact, there is no old data.\n");
    }
    else
    {
        /* the collections returned are sorted in order from new to old.
        start the loop at 1, to intentionally skip the newest collection. */
        time_t seconds = cast32u32s(grp->days_to_keep_prev_versions * 60 * 60 * 24);
        for (uint32_t i = 1; i < collectionrows.length; i++)
        {
            sv_collection_row *row =
                (sv_collection_row *)sv_array_at(&collectionrows, i);
            sv_log_fmt("found collection %llu that finished at %llu", row->id,
                row->time_finished);
            if (now - cast64u64s(row->time_finished) > seconds)
            {
                sv_log_fmt("choosing collection %llu because %llu > %llu",
                    row->id, now - cast64u64s(row->time_finished), seconds);
                *collectionid_to_expire = row->id;
                break;
            }
        }
    }

cleanup:
    sv_array_close(&collectionrows);
    return currenterr;
}

check_result sv_compact_getarchivestats(void *context, const sv_content_row *row)
{
    /* loop through all content rows to see what content has expired.
    archive_stats is a map from {archiveid} to {info about the archive}. */
    sv_compact_state *op = (sv_compact_state *)context;
    check_fatal(row->original_collection < 1000 * 1000 &&
            row->archivenumber < 1000 * 1000,
        "too large, archivenamept1=%d, archivenamept2=%d",
        row->original_collection, row->archivenumber);

    sv_archive_stats *stats = (sv_archive_stats *)sv_2darray_get_expand(
        &op->archive_stats, row->original_collection, row->archivenumber);
    stats->original_collection = row->original_collection;
    stats->archive_number = row->archivenumber;
    if (row->most_recent_collection > op->expiration_cutoff)
    {
        /* this data is still needed */
        stats->count_new += 1;
        stats->size_new += row->compressed_contents_length;
    }
    else
    {
        /* this data is no longer needed */
        stats->count_old += 1;
        stats->size_old += row->compressed_contents_length;

        if (op->is_thorough)
        {
            if (!stats->old_individual_files.buffer)
            {
                stats->old_individual_files = sv_array_open_u64();
            }

            sv_array_add64u(&stats->old_individual_files, row->id);
        }
    }

    return OK;
}

check_result sv_compact_see_what_to_remove_cb(
    void *context, uint32_t x, uint32_t y, byte *val)
{
    sv_compact_state *op = (sv_compact_state *)context;
    sv_archive_stats *archive = (sv_archive_stats *)val;
    if (archive->count_old > 0 && archive->count_new == 0)
    {
        /* we can delete the entire .tar -- every file is old */
        sv_array_append(&op->archives_to_remove, archive, 1);
        if (!op->test_context)
        {
            printf("%05x_%05x.tar, all %.3fMb no longer needed.\n", x, y,
                (double)archive->size_old / (1024.0 * 1024.0));
        }
    }
    else if (op->is_thorough && archive->size_old > op->thresholdsizebytes)
    {
        /* we can delete some files in the .tar, because it would recover
        at least thresholdsizebytes. */
        sv_array_append(&op->archives_to_strip, archive, 1);
        if (!op->test_context)
        {
            printf("%05x_%05x.tar, %.3fMb of %.3fMb is no longer needed.\n", x,
                y, (double)archive->size_old / (1024.0 * 1024.0),
                (double)(archive->size_old + archive->size_new) /
                    (1024.0 * 1024.0));
        }
    }
    else
    {
        /* free memory */
        sv_array_close(&archive->old_individual_files);
    }

    return OK;
}

void sv_compact_see_what_to_remove(
    sv_compact_state *op, uint64_t thresholdsizebytes)
{
    sv_log_fmt("see_what_to_remove, cutoff=%llu", op->expiration_cutoff);
    op->archives_to_remove = sv_array_open(sizeof32u(sv_archive_stats), 0);
    op->archives_to_strip = sv_array_open(sizeof32u(sv_archive_stats), 0);
    op->thresholdsizebytes = thresholdsizebytes;
    check_warn(sv_2darray_foreach(
                   &op->archive_stats, &sv_compact_see_what_to_remove_cb, op),
        "", continue_on_err);
}

check_result sv_strip_archive_removing_old_files(sv_compact_state *op,
    const sv_app *app, const sv_group *grp, svdb_db *db, bstrlist *msgs)
{
    sv_result currenterr = {};
    bstring tar = bstring_open();
    ar_util ar = ar_util_open();
    svdb_txn txn = {};
    bstring readydir = bformat("%s%suserdata%s%s%sreadytoupload",
        cstr(app->path_app_data), pathsep, pathsep, cstr(grp->grpname), pathsep);

    /* delete rows from db before modifying tars, for transactional integrity.
    if exception occurs, we'll be left with unneeded data in the .tar,
    which is better than being left with db pointing to non-existing data */
    check(checkbinarypaths(&ar, false, NULL));
    check(svdb_txn_open(&txn, db));
    for (uint32_t i = 0; i < op->archives_to_strip.length; i++)
    {
        sv_archive_stats *o =
            (sv_archive_stats *)sv_array_at(&op->archives_to_strip, i);
        bsetfmt(tar, "%s%s%05x_%05x.tar", cstr(readydir), pathsep,
            o->original_collection, o->archive_number);
        sv_log_fmt("striparchives db, cutoff=%llu, tar=%s, #rows=%d",
            op->expiration_cutoff, cstr(tar), o->old_individual_files.length);

        if (os_file_exists(cstr(tar)))
        {
            check(svdb_contents_bulk_delete(db, &o->old_individual_files, 0));
        }
        else
        {
            bsetfmt(ar.tmp_results, "archive not seen %s", cstr(tar));
            bstrlist_append(msgs, ar.tmp_results);
        }
    }

    check(svdb_txn_commit(&txn, db));

    /* strip tar archives, removing the files that have expired. */
    for (uint32_t i = 0; i < op->archives_to_strip.length; i++)
    {
        sv_archive_stats *o =
            (sv_archive_stats *)sv_array_at(&op->archives_to_strip, i);
        bsetfmt(tar, "%s%s%05x_%05x.tar", cstr(readydir), pathsep,
            o->original_collection, o->archive_number);
        sv_log_fmt("striparchives disk, cutoff=%llu, tar=%s, #rows=%d",
            op->expiration_cutoff, cstr(tar), o->old_individual_files.length);

        if (os_file_exists(cstr(tar)))
        {
            sv_result result =
                ar_util_delete(&ar, cstr(tar), cstr(op->working_dir_archived),
                    cstr(op->working_dir_unarchived), &o->old_individual_files);

            if (result.code)
            {
                bstrlist_append(msgs, result.msg);
                sv_result_close(&result);
            }
            else
            {
                /* record the new archive checksum. */
                check(write_archive_checksum(db, cstr(tar),
                    op->expiration_cutoff, true /* still needed */));
            }
        }
    }

cleanup:
    svdb_txn_close(&txn, db);
    ar_util_close(&ar);
    bdestroy(tar);
    bdestroy(readydir);
    return currenterr;
}

check_result sv_remove_entire_archives(sv_compact_state *op, svdb_db *db,
    const char *appdatadir, const char *grpname, bstrlist *messages)
{
    sv_result currenterr = {};
    bstring removedir = bformat("%s%suserdata%s%s%sreadytoremove", appdatadir,
        pathsep, pathsep, grpname, pathsep);
    bstring readydir = bformat("%s%suserdata%s%s%sreadytoupload", appdatadir,
        pathsep, pathsep, grpname, pathsep);
    bstring src = bstring_open();
    bstring dest = bstring_open();
    bstring textfilepath = bstring_open();
    svdb_txn txn = {};
    bool somenotmoved = false;
    check_b(os_create_dir(cstr(removedir)), "couldn't create directory. %s",
        cstr(removedir));

    /* delete rows from db before deleting tars, for transactional integrity.
    if exception occurs, we'll be left with unneeded data in the .tar,
    which is better than being left with db pointing to non-existing data */
    check(svdb_txn_open(&txn, db));
    for (uint32_t i = 0; i < op->archives_to_remove.length; i++)
    {
        sv_archive_stats *o =
            (sv_archive_stats *)sv_array_at(&op->archives_to_remove, i);
        bsetfmt(src, "%s%s%05x_%05x.tar", cstr(readydir), pathsep,
            o->original_collection, o->archive_number);
        sv_log_fmt("removearchives db, cutoff=%llu, tar=%s, #rows=%d",
            op->expiration_cutoff, cstr(src), o->old_individual_files.length);
        check(svdb_contents_bulk_delete(db, &o->old_individual_files, 0));

        /* record that we no longer need this archive */
        check(write_archive_checksum(
            db, cstr(src), op->expiration_cutoff, false /* still needed */));
    }

    check(svdb_txn_commit(&txn, db));

    /* next, move files on disk to a 'to-be-deleted' directory */
    for (uint32_t i = 0; i < op->archives_to_remove.length; i++)
    {
        sv_archive_stats *o =
            (sv_archive_stats *)sv_array_at(&op->archives_to_remove, i);
        bsetfmt(src, "%s%s%05x_%05x.tar", cstr(readydir), pathsep,
            o->original_collection, o->archive_number);
        bsetfmt(dest, "%s%s%05x_%05x.tar", cstr(removedir), pathsep,
            o->original_collection, o->archive_number);
        sv_log_fmt("removearchives disk, cutoff=%llu, tar=%s, #rows=%d",
            op->expiration_cutoff, cstr(src), o->old_individual_files.length);

        if (!os_file_exists(cstr(src)))
        {
            /* archive is no longer present, so write a little text file to
            show the user that this archive is safe to delete. */
            bsetfmt(textfilepath, "%s%s%05x_%05x.tar.txt", cstr(removedir),
                pathsep, o->original_collection, o->archive_number);
            sv_result res = sv_file_writefile(cstr(textfilepath), "", "w");
            sv_log_fmt("wrote text file. %s", res.msg ? cstr(res.msg) : "");
            sv_result_close(&res);
            somenotmoved = true;
        }
        else
        {
            sv_log_fmt("moving %s to %s", cstr(src), cstr(dest));
            bool moved = os_tryuntil_move(cstr(src), cstr(dest), true);
            if (!moved)
            {
                bstring msg =
                    bformat("Could not move %s to %s", cstr(src), cstr(dest));
                bstrlist_append(messages, msg);
                bdestroy(msg);
            }
        }
    }

    if (somenotmoved && !op->test_context)
    {
        printf("Some archives weren't found, so we created text files in %s "
               "as a reminder of which archives can be safely deleted.\n",
            cstr(removedir));
        alert("");
    }
    else if (op->archives_to_remove.length && !op->test_context)
    {
        printf("The archives that are no longer needed were moved to %s.\n",
            cstr(removedir));
        alert("");
    }

cleanup:
    svdb_txn_close(&txn, db);
    bdestroy(removedir);
    bdestroy(src);
    bdestroy(dest);
    bdestroy(textfilepath);
    bdestroy(readydir);
    return currenterr;
}

void sv_compact_state_close(sv_compact_state *self)
{
    if (self)
    {
        for (uint32_t i = 0; i < self->archives_to_strip.length; i++)
        {
            sv_archive_stats *o =
                (sv_archive_stats *)sv_array_at(&self->archives_to_strip, i);
            sv_array_close(&o->old_individual_files);
        }

        for (uint32_t i = 0; i < self->archives_to_remove.length; i++)
        {
            sv_archive_stats *o =
                (sv_archive_stats *)sv_array_at(&self->archives_to_remove, i);
            sv_array_close(&o->old_individual_files);
        }

        sv_2darray_close(&self->archive_stats);
        sv_array_close(&self->archives_to_strip);
        sv_array_close(&self->archives_to_remove);
        bdestroy(self->working_dir_archived);
        bdestroy(self->working_dir_unarchived);
        set_self_zero();
    }
}

check_result sv_compact_ask_user(const sv_group *grp, sv_compact_state *op)
{
    bstrlist *choices = bstrlist_open();
    bstring msg =
        bfromcstr("Compact data.\n\nRunning compaction is a good "
                  "way to reduce the disk space used by GlacialBackup.\n\n");

    if (grp->days_to_keep_prev_versions)
    {
        bformata(msg,
            "Files that you've deleted %d days ago are eligible "
            "to be removed from the backup archives when compaction is run. "
            "(This number of days can be changed in 'Edit Group').",
            grp->days_to_keep_prev_versions);
    }
    else
    {
        bstr_catstatic(msg,
            "Files that you've deleted are eligible to be "
            "removed from the backup archives when compaction is run. "
            "(A number of days to wait before removal can be enabled in "
            "'Edit Group'). ");
    }

    bstr_catstatic(msg, "\n\nPlease choose a level of compaction to run:");
    bstrlist_splitcstr(choices,
        "Quick Compaction. Determine and show a "
        "list of .tar archives that are no longer needed.|"
        "Thorough Compaction. Search within each .tar archive and remove "
        "data that is no longer needed.|Back",
        '|');

    int choice = menu_choose(cstr(msg), choices, NULL, NULL, NULL);
    op->is_thorough = choice == 1;
    op->user_canceled = choice > 1;
    bdestroy(msg);
    return OK;
}

check_result sv_compact_impl(
    const sv_group *grp, const sv_app *app, svdb_db *db, sv_compact_state *op)
{
    sv_result currenterr = {};
    bstrlist *msgs = bstrlist_open();
    if (op->archives_to_remove.length + op->archives_to_strip.length == 0)
    {
        alert("We did not find data that can be compacted.");
    }
    else
    {
        alert("As listed above, we found some data that can be compacted.");
        check(sv_remove_entire_archives(
            op, db, cstr(app->path_app_data), cstr(grp->grpname), msgs));
        check(sv_strip_archive_removing_old_files(op, app, grp, db, msgs));
        if (msgs->qty)
        {
            puts("Notes:");
            for (int i = 0; i < msgs->qty; i++)
            {
                puts(blist_view(msgs, i));
            }
        }

        alert("\nCompact complete.");
    }

cleanup:
    bstrlist_close(msgs);
    return currenterr;
}

check_result sv_choosecollection(svdb_db *db, const char *readydir,
    bstring dbfilechosen, uint64_t *collectionidchosen)
{
    sv_result currenterr = {};
    bstrclear(dbfilechosen);
    *collectionidchosen = 0;
    bstring s = bstring_open();
    bstring dbpath = bstring_open();
    bstrlist *choices = bstrlist_open();
    sv_array arr = sv_array_open(sizeof32u(sv_collection_row), 0);
    check(svdb_collectionsget(db, &arr, true));
    for (uint32_t i = 0; i < arr.length; i++)
    {
        sv_collection_row *row = (sv_collection_row *)sv_array_at(&arr, i);
        svdb_collectiontostring(row, true, false, s);
        bstrlist_append(choices, s);
    }

    if (choices->qty == 0)
    {
        alert("There are no previous collections.\n");
        goto cleanup;
    }

    while (true)
    {
        int index = menu_choose_long(choices, 10 /* show in groups of 10. */);
        if (index >= 0 && index < cast32u32s(arr.length))
        {
            sv_collection_row *row =
                (sv_collection_row *)sv_array_at(&arr, cast32s32u(index));
            bsetfmt(dbpath, "%s%s%05x_index.db", readydir, pathsep,
                cast64u32u(row->id));
            if (os_file_exists(cstr(dbpath)))
            {
                os_clr_console();
                printf("%s\n\n", blist_view(choices, index));
                bassign(dbfilechosen, dbpath);
                *collectionidchosen = row->id;
                break;
            }
            else
            {
                printf("The file \n%s\n was not found. Please download it "
                       "and try again, or pick another collection to restore "
                       "from.\n",
                    cstr(dbpath));
                alert("");
            }
        }
        else
        {
            break;
        }
    }

cleanup:
    bdestroy(s);
    bdestroy(dbpath);
    bstrlist_close(choices);
    sv_array_close(&arr);
    return currenterr;
}

check_result sv_restore_file(sv_restore_state *op,
    const sv_file_row *in_files_row, const bstring path, const bstring perms)
{
    sv_result currenterr = {};
    sv_content_row contentsrow = {};
    os_lockedfilehandle handle = {};
    bstring archivepath = bstring_open();
    bstring hashexpected = bstring_open();
    bstring hashgot = bstring_open();
    hash256 hash = {};
    uint32_t crc = 0;
    check(svdb_contentsbyid(op->db, in_files_row->contents_id, &contentsrow));
    check_b(contentsrow.id != 0, "did not get correct contents row.");

    /* get src path */
    bsetfmt(archivepath, "%s%s%05x_%05x.tar",
        cstr(op->archiver.path_readytoupload), pathsep,
        contentsrow.original_collection, contentsrow.archivenumber);

    /* get dest path */
    check_b(blength(path) >= 4, "path length is too short %s", cstr(path));
    const char *pathwithoutroot = cstr(path) + (islinux ? 1 : 3);
    bsetfmt(
        op->destfullpath, "%s%s%s", cstr(op->destdir), pathsep, pathwithoutroot);
    check_b(islinux || blength(op->destfullpath) < PATH_MAX,
        "The length of the resulting path would have been too long, please "
        "choose a shorter destination directory.");
    check(hook_call_when_restoring_file(
        op->test_context, cstr(path), op->destfullpath));
    check(ar_manager_restore(&op->archiver, cstr(archivepath), contentsrow.id,
        cstr(op->working_dir_archived), cstr(op->destfullpath)));

    /* apply lmt */
    log_b(os_setmodifiedtime_nearestsecond(
              cstr(op->destfullpath), in_files_row->last_write_time),
        "%s %llu", cstr(op->destfullpath),
        castull(in_files_row->last_write_time));

    /* confirm filesize */
    efiletype ext = get_file_extension_info(
        cstr(op->destfullpath), blength(op->destfullpath));
    bool is_separate_audio =
        op->separate_metadata && ext != filetype_binary && ext != filetype_none;

    if (!is_separate_audio)
    {
        check_b(contentsrow.contents_length ==
                os_getfilesize(cstr(op->destfullpath)),
            "restoring %08llx to %s expected size %llu but got %llu",
            castull(contentsrow.id), cstr(op->destfullpath),
            castull(contentsrow.contents_length),
            castull(os_getfilesize(cstr(op->destfullpath))));
    }

    /* confirm hash */
    if (!is_separate_audio || blength(op->archiver.ar.audiotag_binary))
    {
        /* set file readable, e.g. if we're restoring from other user */
        log_b(os_try_set_readable(cstr(op->destfullpath), true), "%s",
            cstr(op->destfullpath));
        check(os_lockedfilehandle_open(
            &handle, cstr(op->destfullpath), true, NULL));
        check(hash_of_file(&handle, cast64u32u(op->separate_metadata), ext,
            cstr(op->archiver.ar.audiotag_binary), &hash, &crc));

        /* compare 256bit hashes and not the crc */
        hash256tostr(&contentsrow.hash, hashexpected);
        hash256tostr(&hash, hashgot);
        check_b(s_equal(cstr(hashexpected), cstr(hashgot)),
            "restoring %08llx to %s expected hash %s but got %s",
            castull(contentsrow.id), cstr(op->destfullpath), cstr(hashexpected),
            cstr(hashgot));
    }

    /* apply posix permissions */
    if (islinux && op->restore_owners && !op->test_context)
    {
        sv_result res = os_set_permissions(cstr(op->destfullpath), perms);
        if (res.code)
        {
            bcatcstr(res.msg, cstr(op->destfullpath));
            check(res);
        }
    }

cleanup:
    os_lockedfilehandle_close(&handle);
    bdestroy(archivepath);
    bdestroy(hashexpected);
    bdestroy(hashgot);
    return currenterr;
}

check_result sv_restore_cb(void *context, const sv_file_row *in_files_row,
    const bstring path, const bstring permissions)
{
    sv_restore_state *op = (sv_restore_state *)context;
    if (fnmatch_simple(cstr(op->scope), cstr(path)))
    {
        op->countfilesmatch++;
        op->countfilescomplete++;
        log_b(in_files_row->e_status == sv_filerowstatus_complete &&
                in_files_row->most_recent_collection == op->collectionidwanted,
            "%s, at the original time the backup was taken this file was "
            "not available, so we will recover a valid but previous version. "
            "%d %llu %llu",
            cstr(path), in_files_row->e_status,
            castull(in_files_row->most_recent_collection),
            castull(op->collectionidwanted));

        sv_result res = sv_restore_file(op, in_files_row, path, permissions);
        if (res.code)
        {
            bsetfmt(op->tmp_result, "%s: %s", cstr(path), cstr(res.msg));
            bstrlist_append(op->messages, op->tmp_result);
            sv_result_close(&res);
            op->countfilescomplete--;
        }
    }

    return OK;
}

check_result sv_restore_checkbinarypaths(
    const sv_app *app, const sv_group *grp, sv_restore_state *op)
{
    sv_result currenterr = {};
    check(ar_manager_open(&op->archiver, cstr(app->path_app_data),
        cstr(grp->grpname), 0, grp->approx_archive_size_bytes));
    sv_result result = checkbinarypaths(&op->archiver.ar, grp->separate_metadata,
        cstr(op->archiver.path_working));
    if (result.code)
    {
        if (grp->separate_metadata && s_contains(cstr(result.msg), "ffmpeg") &&
            ask_user(
                "We were not able to find ffmpeg. Please try to install "
                "ffmpeg first.\nShould we continue without installing ffmpeg "
                "(files can be restored successfully, but file content "
                "verification will be skipped)? y/n"))
        {
            sv_result_close(&result);
            check(checkbinarypaths(&op->archiver.ar, false, NULL));
            bstr_assignstatic(op->archiver.ar.audiotag_binary, "");
        }

        check(result);
    }

cleanup:
    return currenterr;
}

void sv_restore_show_messages(
    bool latestversion, const sv_group *grp, sv_restore_state *op)
{
    if (op->messages->qty)
    {
        printf("Remember that if the .tar files have been have been uploaded "
               "but are no longer present locally, the tar files must be moved "
               "back to \n%s\n\n",
            cstr(op->archiver.path_readytoupload));
        if (!latestversion && grp->days_to_keep_prev_versions)
        {
            printf("Also, remember that when Compaction is run, some data "
                   "that is older than %d days is made unavailable.\n\n",
                grp->days_to_keep_prev_versions);
        }
        else if (!latestversion)
        {
            printf("Also, remember that when Compaction is run, some data "
                   "from previous versions is made unavailable.\n\n");
        }

        alert("");
        puts("Notes:");
        for (int i = 0; i < op->messages->qty; i++)
        {
            puts(blist_view(op->messages, i));
        }
    }
}

check_result sv_restore_ask_user_options(
    const sv_app *app, bool latestversion, svdb_txn *txn, sv_restore_state *op)
{
    /* ask the user
    1) which version to restore from
    2) which files to restore
    3) where to save the files to */
    sv_result currenterr = {};
    bstring prev_dbfile = bstring_open();
    op->user_canceled = true;
    if (latestversion)
    {
        os_clr_console();
        check(svdb_txn_open(txn, op->db));
        check(svdb_collectiongetlast(op->db, &op->collectionidwanted));
        if (!op->collectionidwanted)
        {
            alert("There are no collections to restore.\n");
            goto cleanup;
        }
    }
    else
    {
        check(sv_choosecollection(op->db, cstr(op->archiver.path_readytoupload),
            prev_dbfile, &op->collectionidwanted));
        if (op->collectionidwanted && blength(prev_dbfile))
        {
            check(svdb_disconnect(op->db));
            check(svdb_connect(op->db, cstr(prev_dbfile)));
            check(svdb_txn_open(txn, op->db));
        }
        else
        {
            goto cleanup;
        }
    }

    printf("Which files should we restore? Enter '*' to restore all files, "
           "or a pattern like '*.mp3' or '%s', or 'q' to cancel.\n",
        islinux ? "/directory/path/*" : "C:\\myfiles\\path\\*");
    ask_user_str("", true, op->scope);
    if (!blength(op->scope))
    {
        goto cleanup;
    }

    fnmatch_isvalid(cstr(op->scope), op->tmp_result);
    check_b(blength(op->tmp_result) == 0, "This is not a valid pattern. %s",
        cstr(op->tmp_result));
    os_clr_console();
    ask_user_str(
        "Please enter the full path to an output directory. It "
        "should be currently empty and have enough free hard drive space. "
        "Or enter 'q' to cancel.",
        true, op->destdir);

    if (!blength(op->destdir) || !check_user_typed_dir(app, op->destdir))
    {
        goto cleanup;
    }
    else if (!os_dir_empty(cstr(op->destdir)))
    {
        alert("This directory does not appear to be empty. Please first "
              "ensure that the directory is empty.");
        goto cleanup;
    }
    else if (!os_is_dir_writable(cstr(op->destdir)))
    {
        alert("It doesn't look like we have write access to this directory.");
        goto cleanup;
    }

    if (islinux &&
        ask_user("\nRestore users, groups, and permissions "
                 "(requires root access)? y/n"))
    {
        op->restore_owners = true;
    }

    op->user_canceled = false;
    check_b(os_create_dirs(cstr(op->working_dir_archived)), "couldn't create %s",
        cstr(op->working_dir_archived));
    check_b(os_create_dirs(cstr(op->working_dir_unarchived)),
        "couldn't create %s", cstr(op->working_dir_unarchived));
    check(os_tryuntil_deletefiles(cstr(op->working_dir_archived), "*"));
    check(os_tryuntil_deletefiles(cstr(op->working_dir_unarchived), "*"));
    sv_log_fmt("restoring collection id %llu scope=%s output=%s",
        op->collectionidwanted, cstr(op->scope), cstr(op->destdir));

cleanup:
    bdestroy(prev_dbfile);
    return currenterr;
}

void sv_restore_state_close(sv_restore_state *self)
{
    if (self)
    {
        bdestroy(self->working_dir_archived);
        bdestroy(self->working_dir_unarchived);
        bdestroy(self->destdir);
        bdestroy(self->scope);
        bdestroy(self->destfullpath);
        bdestroy(self->tmp_result);
        bstrlist_close(self->messages);
        ar_manager_close(&self->archiver);
        set_self_zero();
    }
}
