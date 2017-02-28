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

#include "operations.h"

/* 
Backup
----------------------------------------------------------------
Recurse through every file, if it is not excluded. search database by path.
	a) If database has this path with the same length+last write time,
			Set the status to statusFileComplete
	b) If database has this path, but the file was changed,
			Set the status to statusFileNeedsWork
	c) If the database does not have this path
			Add a row with this path, status is statusFileNeedsWork 

Loop through all database rows with status < statusFileComplete
	(this will include both files added in previous stage and files that have been moved/removed)
	a) If path no longer exists,
			Add row to removal-list
	b) If path cannot be accessed, and it was just added in previous stage
			Add row to removal-list
	c) If path cannot be accessed, and it has actual backed-up content
			Pretend like the file exists and has not changed. Increment the contents row.
	d) If path can be accessed
			Take lock on file so it isn't changed by another process while we add it
			Add it to archive and get the archive id
			Update row, setting filesize, archive id, and status to statusFileComplete

Adding to an archive
	Rather than storing the entire path, names are in the form 00000.file, named after the contentsid in hex.
	We'd rather have a more user-readable name, but having the file named in this way adds simplicity.
		(Otherwise, we'd have to call 7z l to see what is in an archive, and that is unreliable due to char encodings)
		As a workaround we'll at least add a text file called filenames.txt to the archive
		(the filenames wouldn't be accurate after a while anyways, since we re-use contents)
	We use a heuristic based on file extension; binary files like mp3 are stored with no compression, which is much faster.

Make a copy of the database into the ready-to-upload directory

Restore
----------------------------------------------------------------
First, let the user decide which old database to connect to. (The newest database only has filenames for the latest collection.)
Let the user choose a scope (a fnmatch pattern)
Let the user choose an output directory (must be empty)
Simply iterate through all fileslist rows, and if the path matches the scope, extract files.
Add any failures to a list of strings to be shown afterwards; do not halt progress on every error.

Compact
----------------------------------------------------------------
Summarizing archive freshness
	First, determine which collectionid is the 'cutoff point'. For example, user chooses that files expire after 30 days, 
	so in this case we would find the first collectionid before 30 days
	Create a datastructure that maps archiveid to a struct counting how many expired and how many non-expired files are present.
		(archiveid is the collectionid + number for that archive; the filename in the form 0000_0000.7z reflects this)
	a hashmap/trie would be suitable for this mapping, 
	but we'll use a 2D array where the 1st dimension is collectionid and 2nd dimension is number.
	Go through every row in tblcontents.
		If the content is needed, increment the total in the struct.
		If the content is not needed, increment the total of not-needed files.
			Also, add the contentid to a list of contentids in this archive that are ok to be removed.

Removing entire archives
	If the struct shows that no needed-files are present in an archive, we'll just remove the entire archive
	If we're doing this offline and don't have access to the archive itself, 
	create a text file in a 'ready-to-delete' directory as an indicator to the user
	1) Start transaction
	2) delete from contentids where archiveid=x
	3) Commit transaction
	4) Move the archive on disk into a 'readytodelete' directory.
	Moving the files must be done *after* database operation is complete and committed. Otherwise we will have inconsistent state --
	remember that there's nothing stopping the contents here from being used again in case the same file re-appears. 

Removing individual files from archives
	This can be done to save disk space, but note that it is slow, 
	causing a rewrite of the rest of the archive (would be even slower if we didn't disable 'solid' mode).
	And so we don't remove individual files from archives unless a threshold is met. 
	The compact_threshold_bytes setting.
	For example, if compact_threshold_bytes is 10*1024*1024, 
	we won't remove files from an archive unless we know that 10mb(uncompressed at least) can be saved
	And we can remove the files in bulk by sending many command-line parameters to 7z.

*/

check_result svdp_backup_addtoqueue_cb(void* context,
	const bstring filepath, uint64_t lmt_from_disk, uint64_t actual_size_from_disk, uint64_t flags_from_disk)
{
	sv_result currenterr = {};
	svdp_backup_state* op = (svdp_backup_state*)context;
	uint64_t size_from_disk = actual_size_from_disk;
	if (!os_recurse_is_dir(lmt_from_disk, size_from_disk) &&
		svdp_backupgroup_isincluded(op->group, cstr(filepath)))
	{
		svdb_files_row row = { 0 };
		check(svdb_filesbypath(&op->db, filepath, &row));
		adjustfilesize_if_audio_file(op->group->separate_metadata_enabled, 
			get_file_extension_info(cstr(filepath), cast32s32u(blength(filepath))), size_from_disk, &size_from_disk);

		if (row.id != 0 && row.contents_length == size_from_disk && row.last_write_time == lmt_from_disk &&
			row.flags == flags_from_disk && row.most_recent_collection == op->collectionId - 1 && 
			row.e_status == sv_filerowstatus_complete)
		{
			/* the file hasn't been changed. */
			sv_log_writeLines("queue-same", cstr(filepath));
			row.most_recent_collection = op->collectionId;
			row.e_status = sv_filerowstatus_complete;
			check(svdb_filesupdate(&op->db, &row));
			check(svdb_contents_setlastreferenced(&op->db, row.contents_id, op->collectionId));
		}
		else if (row.id != 0)
		{
			/* the file has been changed. */
			sv_log_writeLines("queue-changed", cstr(filepath));
			row.most_recent_collection = op->collectionId;
			row.e_status = sv_filerowstatus_queued;
			check(svdb_filesupdate(&op->db, &row));
			op->approxBytesInQueue += actual_size_from_disk;
			op->approxFilesInQueue += 1;
		}
		else
		{
			/* it's a new file. */
			sv_log_writeLines("queue-new", cstr(filepath));
			check(svdb_filesinsert(&op->db, filepath, row.most_recent_collection, sv_filerowstatus_queued, NULL));
			op->approxBytesInQueue += actual_size_from_disk;
			op->approxFilesInQueue += 1;
		}
	}

cleanup:
	return currenterr;
}

check_result svdp_backup_addtoqueue(svdp_backup_state* op)
{
	sv_result currenterr = {};
	bstring tmp = bstring_open();
	check(test_hook_provide_file_list(op->test_context, op, &svdp_backup_addtoqueue_cb));
	for (int i = 0; i < op->group->root_directories->qty; i++)
	{
		const char* dir = bstrListViewAt(op->group->root_directories, i);
		if (!os_dir_exists(dir))
		{
			bassignformat(tmp, "Note: we could not find the starting directory \n%s\n\n"
				"We can continue safely, but thought we would ask if this was intended. Continue?", dir);
			check_b(ask_user_yesno(cstr(tmp)), "The user chose to cancel the backup because "
				"directory %s not found.", dir);
		}

		printf("Searching %s...\n", dir);
		os_recurse_params params = { op, dir, &svdp_backup_addtoqueue_cb, PATH_MAX /*max depth*/,
			op->listSkippedSymlink, op->listSkippedIterationFailed,
			20 /*tries*/, 250 /*sleep between tries*/ };

		check(os_recurse(&params));
	}

cleanup:
	bdestroy(tmp);
	return currenterr;
}

void svdp_backup_state_close(svdp_backup_state* self)
{
	bstrListDestroy(self->listSkippedSymlink);
	bstrListDestroy(self->listSkippedIterationFailed);
	bstrListDestroy(self->listSkippedFileAccessFailed);
	svdp_archiver_close(&self->archiver);
	sv_array_close(&self->dbrowsToDelete);
	svdb_connection_close(&self->db);
}

void show_status_update(svdp_backup_state* op)
{
	uint64_t percentageDone = (100*op->approxBytesCompleted) / op->approxBytesInQueue;
	percentageDone = MIN(percentageDone, 99); /* don't show something like "104%" */
	if (percentageDone != op->prevPercentShown)
	{
		printf("%02llu%% complete...\n", castull(percentageDone));
		op->prevPercentShown = percentageDone;
	}
}

check_result svdp_backup_addfile(svdp_backup_state* op, os_exclusivefilehandle* handle, 
	const bstring path, uint64_t rowid)
{
	sv_result currenterr = {};
	svdb_files_row newfilesrow = {};
	svdb_contents_row contentsrow = {};
	uint64_t filesize = 0, modtimeondisk = 0, permissionsondisk = 0, filesizeondisk_do_not_persist = 0;
	hash256 hash = {};
	uint32_t crc32 = 0;
	knownfileextension ext = get_file_extension_info(cstr(path), cast32s32u(blength(path)));
	check(os_exclusivefilehandle_stat(
		handle, &filesizeondisk_do_not_persist, &modtimeondisk, &permissionsondisk));

	check(test_hook_get_file_info(op->test_context, handle, &filesizeondisk_do_not_persist,
		&modtimeondisk, &permissionsondisk));
	
	/* in certain cases we ignore filesize; filesizeondisk_do_not_persist shouldn't be saved to db. */
	adjustfilesize_if_audio_file(op->group->separate_metadata_enabled, ext, 
		filesizeondisk_do_not_persist, &filesize);

	newfilesrow.flags = permissionsondisk;
	newfilesrow.last_write_time = modtimeondisk;
	newfilesrow.contents_length = filesize;
	check(hash_of_file(handle, op->group->separate_metadata_enabled, 
		ext, cstr(op->archiver.path_audiometadata_binary), &hash, &crc32));
	
	/* is this file already in an archive? */
	check(svdb_contentsbyhash(&op->db, &hash, newfilesrow.contents_length, &contentsrow));
	if (contentsrow.id)
	{
		sv_log_writeLines("process-addupdate", cstr(path));
		check(svdb_contents_setlastreferenced(&op->db, contentsrow.id, op->collectionId));
		newfilesrow.contents_id = contentsrow.id;
	}
	else
	{
		/* get the latest contentrowid */
		svdb_contents_row newcontentsrow = { 0 };
		check(svdb_contentsinsert(&op->db, &newcontentsrow.id));

		/* add to an archive on disk */
		bool iscompressed = ext != knownfileextensionNone;
		sv_log_writeLines("process-addnew", cstr(path));
		check(svdp_archiver_addfile(&op->archiver, cstr(path), iscompressed, 
			newcontentsrow.id, &newcontentsrow.archivenumber));

		/* add to the database */
		newcontentsrow.hash = hash;
		newcontentsrow.crc32 = crc32;
		newcontentsrow.contents_length = newfilesrow.contents_length;
		newcontentsrow.original_collection = cast64u32u(op->collectionId);
		newcontentsrow.most_recent_collection = op->collectionId;
		check(svdb_contentsupdate(&op->db, &newcontentsrow));
		newfilesrow.contents_id = newcontentsrow.id;
		op->processNewFiles += 1;
		op->processNewBytes += filesizeondisk_do_not_persist;
	}

	/* update the filesrow */
	newfilesrow.id = rowid;
	newfilesrow.most_recent_collection = op->collectionId;
	newfilesrow.e_status = sv_filerowstatus_complete;
	check(svdb_filesupdate(&op->db, &newfilesrow));

cleanup:
	return currenterr;
}

check_result svdp_backup_processqueue_cb(void* context, unusedboolptr(),
	const svdb_files_row* in_files_row, const bstring path)
{
	sv_result currenterr = {};
	os_exclusivefilehandle handle = {};
	svdp_backup_state* op = (svdp_backup_state*)context;
	show_status_update(op);
	bool filenotfound = false;

	sv_result result_openhandle = os_exclusivefilehandle_tryuntil_open(
		&handle, cstr(path), true, &filenotfound);

	if (result_openhandle.code && in_files_row->contents_id == 0)
	{
		/* can't access the file, but this is a file we haven't backed up yet */
		sv_log_writeLines("process-newfilenotaccess", cstr(path));
		sv_array_add64u(&op->dbrowsToDelete, in_files_row->id);
	}
	else if (result_openhandle.code)
	{
		/* can't access the file: pretend like the file exists and has not changed.  */
		sv_log_writeLines("process-notaccess", cstr(path));
		bstrListAppend(op->listSkippedFileAccessFailed, result_openhandle.msg);
		check(svdb_contents_setlastreferenced(&op->db, in_files_row->contents_id, op->collectionId));
	}
	else if (!result_openhandle.code && filenotfound)
	{
		/* path no longer exists */
		sv_log_writeLines("process-notfound", cstr(path));
		sv_array_add64u(&op->dbrowsToDelete, in_files_row->id);
		op->processCountMovedfrompath++;
	}
	else
	{
		/* can access the file */
		check(svdp_backup_addfile(op, &handle, path, in_files_row->id));
		op->processCountNewpath++;
	}

cleanup:
	sv_result_close(&result_openhandle);
	os_exclusivefilehandle_close(&handle);
	return currenterr;
}

check_result svdp_backup_makecopyofdb(svdp_backup_state* op, const svdp_backupgroup* group, const char* readydir)
{
	bstring src = bformat("%s%s..%s%s_index.db", readydir, pathsep, pathsep, cstr(group->groupname));
	bstring dest = bformat("%s%s%05x_index.db", readydir, pathsep, cast64u32u(op->collectionId));
	
	/* make a copy of it to the upload dir, ok if this extra copy fails. */
	/* future feature might be to call drop index and vacuum on the copy, to save disk space */
	os_sleep(250);
	log_b(os_tryuntil_copy(cstr(src), cstr(dest), true), "could not copy db.");
	bdestroy(src);
	bdestroy(dest);
	return OK;
}

check_result svdp_backup_createcollection(svdp_backup_state* op)
{
	sv_result currenterr = {};
	time_t curtime = time(NULL);
	check(svdb_collectioninsert(&op->db, (uint64_t)curtime, &op->collectionId));
	check_b(op->collectionId, "must have valid collectionId");
cleanup:
	return currenterr;
}

check_result svdp_backup_recordcollectionstats(svdp_backup_state* op)
{
	sv_result currenterr = {};
	time_t timecompleted = time(NULL);
	svdb_collections_row updatedrow = { 0 };
	updatedrow.id = op->collectionId;
	updatedrow.time_finished = (uint64_t)timecompleted;
	check(svdb_filesgetcount(&op->db, &updatedrow.count_total_files));
	updatedrow.count_new_contents = op->processNewFiles;
	updatedrow.count_new_contents_bytes = op->processNewBytes;
	check(svdb_collectionupdate(&op->db, &updatedrow));
	printf("Currently stored files: %llu\n\n", castull(updatedrow.count_total_files));
cleanup:
	return currenterr;
}

check_result svdp_backup(const svdp_application* app, const svdp_backupgroup* group, 
	svdb_connection* db, void* test_context)
{
	sv_result currenterr = {};
	svdp_backup_state op = {};
	svdb_txn txn = {};
	bstring dbpath = bstring_open();
	op.app = app;
	op.group = group;
	op.preview = !test_context;
	op.test_context = test_context;
	op.listSkippedSymlink = bstrListCreate();
	op.listSkippedIterationFailed = bstrListCreate();
	op.listSkippedFileAccessFailed = bstrListCreate();
	op.dbrowsToDelete = sv_array_open(sizeof32u(uint64_t), 0);
	op.timestarted = time(NULL);
	if (!test_context && group->root_directories->qty == 0)
	{
		printf("You haven't yet added directories to be backed up. Please go to 'Edit Group' "
			"and choose 'Add/remove directories...'");

		goto cleanup;
	}
	
	svdp_application_groupdbpathfromname(app, cstr(group->groupname), dbpath);
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(&op.db, cstr(dbpath)));
	check(svdb_txn_open(&txn, &op.db));
	check(svdp_backup_createcollection(&op));
	check(svdp_archiver_open(&op.archiver, cstr(op.app->path_app_data), cstr(op.group->groupname),
		cast64u32u(op.collectionId), op.group->approx_archive_size_bytes, cstr(op.group->encryption)));
	check(checkexternaltoolspresent(&op.archiver, op.group->separate_metadata_enabled));
	check(checkffmpegworksasexpected(
		&op.archiver, op.group->separate_metadata_enabled, cstr(op.archiver.workingdir)));

	check(svdp_backup_addtoqueue(&op));
	check(test_hook_call_before_processing_queue(op.test_context, &op.db));
	if (op.preview)
	{
		printf("There are %llu new/changed files.\n "
			"Please see the logs in the directory\n%s\n for more details.",
			castull(op.approxFilesInQueue), cstr(app->logging.dir));
		check_b(ask_user_yesno("Continue?"), "User chose to cancel backup operation.");
	}
	
	check(svdp_archiver_beginarchiving(&op.archiver));
	check(svdb_files_in_queue(&op.db, sv_makestatus(op.collectionId, sv_filerowstatus_complete), 
		&op, svdp_backup_processqueue_cb));
	if (op.preview)
	{
		printf("%llu files (%0.4f Mb) are about to be added.\n "
			"%llu new paths, %llu moved from paths.\n"
			"Please see the logs in the directory\n%s\n for more details.",
			castull(op.processNewFiles), (double)op.processNewBytes/(1024.0*1024), 
			castull(op.processCountNewpath), castull(op.processCountMovedfrompath),
			cstr(app->logging.dir));
		check_b(ask_user_yesno("Continue?"), "User chose to cancel backup operation.");
	}

	check(svdb_files_bulk_delete(&op.db, &op.dbrowsToDelete, 0));
	check(svdp_backup_recordcollectionstats(&op));
	check(svdp_archiver_finisharchiving(&op.archiver));
	check(svdb_txn_commit(&txn, &op.db));
	check(svdb_connection_disconnect(&op.db));
	check(svdp_backup_makecopyofdb(&op, group, cstr(op.archiver.readydir)));
	if ((op.listSkippedFileAccessFailed->qty || op.listSkippedIterationFailed->qty || 
		op.listSkippedSymlink->qty) && !op.test_context && 
		ask_user_yesno("Backup complete. A few files were skipped. Show details?"))
	{
		printf("The following %d symlinks were skipped:\n", op.listSkippedSymlink->qty);
		for (int i = 0; i < op.listSkippedSymlink->qty; i++)
			printf("\t%s\n", bstrListViewAt(op.listSkippedSymlink, i));

		alertdialog("");
		printf("We kept the previous version, but could not archive the latest version of %d files:\n", 
			op.listSkippedFileAccessFailed->qty);
		for (int i = 0; i < op.listSkippedFileAccessFailed->qty; i++)
			printf("\t%s\n", bstrListViewAt(op.listSkippedFileAccessFailed, i));

		alertdialog("");
		printf("The following %d directories could not be listed:\n", op.listSkippedIterationFailed->qty);
		for (int i = 0; i < op.listSkippedIterationFailed->qty; i++)
			printf("\t%s\n", bstrListViewAt(op.listSkippedIterationFailed, i));
	}

	if (!op.test_context)
	{
		alertdialog("Backup complete.");
	}

cleanup:
	svdb_txn_close(&txn, &op.db);
	svdb_connection_close(&op.db);
	svdp_backup_state_close(&op);
	bdestroy(dbpath);
	return currenterr;
}

check_result svdp_application_run(svdp_application* app, int optype)
{
	sv_result currenterr = {};
	svdp_backupgroup backupgroup = {};
	svdb_connection connection = {};
	bstring newpath = bstring_open();
	pickgroup(app);
	if (app->current_group_name_index >= 0)
	{
		check(loadbackupgroup(app, &backupgroup, &connection));
		if (optype == svdp_op_backup)
		{
			check(svdp_backup(app, &backupgroup, &connection, NULL));
		}
		else if (optype == svdp_op_compact)
		{
			check(svdp_compact(app, &backupgroup, &connection));
		}
		else if (optype == svdp_op_restore ||
			optype == svdp_op_restore_from_past)
		{
			check(svdp_restore(app, &backupgroup, &connection, optype));
		}

		check(svdb_connection_disconnect(&connection));
	}

cleanup:
	svdp_backupgroup_close(&backupgroup);
	svdb_connection_close(&connection);
	bdestroy(newpath);
	return currenterr;
}

/* Look for files that are too old -- the ones with latest_seen_collectionid <= collectionid_to_expire. */
check_result svdp_compact_getexpirationcutoff(svdb_connection* db, const svdp_backupgroup* group, 
	uint64_t* collectionid_to_expire, time_t now)
{
	sv_result currenterr = {};
	sv_array collectionrows = sv_array_open(sizeof32u(svdb_collections_row), 0);
	check(svdb_collectionsget(db, &collectionrows, true));
	*collectionid_to_expire = 0;
	if (collectionrows.length == 0 || collectionrows.length == 1)
	{
		printf("Nothing to compact, there is no old data.\n");
	}
	else
	{
		/* the collections returned are sorted in order from newest to oldest. 
		by starting the loop at 1 we intentionally skip over the newest collection. */
		time_t days_duration_seconds = group->days_to_keep_prev_versions * 60 * 60 * 24;
		for (uint32_t i = 1; i < collectionrows.length; i++)
		{
			svdb_collections_row* row = (svdb_collections_row*)sv_array_at(&collectionrows, i);
			if (now - cast64u64s(row->time_finished) > days_duration_seconds)
			{
				*collectionid_to_expire = row->id;
				break;
			}
		}
	}

cleanup:
	sv_array_close(&collectionrows);
	return currenterr;
}

/* loop through all rows to build a map from {archiveid} to {stats about the archive}. */
check_result svdp_compact_maparchiveidtostats(void* context, unusedboolptr(), const svdb_contents_row* row)
{
	svdp_compact_state* op = (svdp_compact_state*)context;
	check_fatal(row->original_collection < 1000 * 1000 && row->archivenumber < 1000 * 1000,
		"too large, archivenamept1=%d, archivenamept2=%d", row->original_collection, row->archivenumber);

	sv_2d_array_ensure_space(&op->archivenum_tostats, row->original_collection, row->archivenumber);
	svdp_archive_info* summary = (svdp_archive_info*)sv_2d_array_at(&op->archivenum_tostats, 
		row->original_collection, row->archivenumber);
	summary->original_collection = row->original_collection;
	summary->archive_number = row->archivenumber;
	if (row->most_recent_collection > op->expiration_cutoff)
	{
		summary->needed_entries += 1;
		summary->needed_size += row->contents_length;
	}
	else
	{
		summary->old_entries += 1;
		summary->old_size += row->contents_length;

		if (op->is_thorough)
		{
			if (!summary->old_individual_files.buffer)
			{
				summary->old_individual_files = sv_array_open(sizeof32u(uint64_t), 1);
			}

			sv_array_add64u(&summary->old_individual_files, row->id);
		}
	}

	return OK;
}


void svdp_compact_see_what_to_remove(sv_2d_array* summarizearchives, uint64_t thresholdsizebytes, 
	sv_array* archives_to_remove, sv_array* archives_to_compact, bool isthorough)
{
	for (uint32_t i = 0; i < summarizearchives->arr.length; i++)
	{
		sv_array* subarray = (sv_array*)sv_array_at(&summarizearchives->arr, i);
		for (uint32_t j = 0; j < subarray->length; j++)
		{
			svdp_archive_info* o = (svdp_archive_info*)sv_array_at(subarray, j);
			if (o->old_entries > 0 && o->needed_entries == 0)
			{
				/* clear candidate for removal -- everything there is old */
				sv_array_append(archives_to_remove, o, 1);
				printf("%05x_%05x.tar, all %.4fMb no longer needed.\n", i, j, 
					(double)o->old_size/(1024.0*1024.0));
			}
			else if (isthorough && o->old_size > thresholdsizebytes)
			{
				/* candidate for compaction, since we can save at least thresholdsizebytes. */
				sv_array_append(archives_to_compact, o, 1);
				printf("%05x_%05x.tar, %.4fMb out of %.4fMb is no longer needed.\n", i, j, 
					(double)(o->old_size) / (1024.0 * 1024.0), 
					(double)(o->old_size + o->needed_size) / (1024.0 * 1024.0));
			}
			else
			{
				/* free memory */
				sv_array_close(&o->old_individual_files);
			}
		}
	}
}

check_result svdp_compact_removefilesfromarchives(const svdp_application* app, const svdp_backupgroup* group, 
	svdb_connection* db, const char* readydir, sv_array* archives_to_compact, bstrList* messages)
{
	sv_result currenterr = {};
	bstring archivepath = bstring_open();
	bstring getstderr = bstring_open();
	svdp_archiver arch = {};
	svdb_txn txn = {};
	check(svdp_archiver_open(&arch, cstr(app->path_app_data), cstr(group->groupname), 0, 
		group->approx_archive_size_bytes, cstr(group->encryption)));

	check(checkexternaltoolspresent(&arch, false));

	/* first, delete rows from the database. must be done in this order for transactional integrity. */
	check(svdb_txn_open(&txn, db));
	for (uint32_t i = 0; i < archives_to_compact->length; i++)
	{
		svdp_archive_info* o = (svdp_archive_info*)sv_array_at(archives_to_compact, i);
		bassignformat(archivepath, "%s%s%05x_%05x.tar", readydir, pathsep, 
			o->original_collection, o->archive_number);

		if (os_file_exists(cstr(archivepath)))
		{
			check(svdb_contents_bulk_delete(db, &o->old_individual_files, 0));
		}
	}

	check(svdb_txn_commit(&txn, db));

	/* next, modify the archives by removing some of files they contain */
	for (uint32_t i = 0; i < archives_to_compact->length; i++)
	{
		svdp_archive_info* o = (svdp_archive_info*)sv_array_at(archives_to_compact, i);
		bassignformat(archivepath, "%s%s%05x_%05x.tar", readydir, pathsep, o->original_collection, o->archive_number);
		if (os_file_exists(cstr(archivepath)))
		{
			check(svdp_archiver_delete_from_archive(
				&arch, cstr(archivepath), &o->old_individual_files, messages));
		}
		else
		{
			printf("Note: could not find the archive %s.\n", cstr(archivepath));
		}
	}

cleanup:
	svdb_txn_close(&txn, db);
	svdp_archiver_close(&arch);
	bdestroy(archivepath);
	bdestroy(getstderr);
	return currenterr;
}

check_result svdp_compact_removeoldarchives(svdb_connection* db, const char* readydir,
	sv_array* archives_to_remove, bstrList* messages)
{
	sv_result currenterr = {};
	bstring removedir = bformat("%s%s..%sreadytoremove", readydir, pathsep, pathsep);
	bstring src = bstring_open();
	bstring dest = bstring_open();
	bstring textfilepath = bstring_open();
	svdb_txn txn = {};
	check_b(os_create_dir(cstr(removedir)), "could not create directory.");
	bool somenotmoved = false;

	/* first, delete rows from the database. must be done in this order for transactional integrity. */
	check(svdb_txn_open(&txn, db));
	for (uint32_t i = 0; i < archives_to_remove->length; i++)
	{
		svdp_archive_info* o = (svdp_archive_info*)sv_array_at(archives_to_remove, i);
		check(svdb_contents_bulk_delete(db, &o->old_individual_files, 0));
	}
	check(svdb_txn_commit(&txn, db));

	/* next, move files on disk to a 'to-be-deleted' directory */
	for (uint32_t i = 0; i < archives_to_remove->length; i++)
	{
		svdp_archive_info* o = (svdp_archive_info*)sv_array_at(archives_to_remove, i);
		bassignformat(src, "%s%s%05x_%05x.tar", readydir, pathsep, o->original_collection, o->archive_number);
		bassignformat(dest, "%s%s%05x_%05x.tar", removedir, pathsep, o->original_collection, o->archive_number);
		if (!os_file_exists(cstr(src)))
		{
			/* write a little text file to show the user that this archive is safe to delete
			this happens for the case where the user only has the archives uploaded, not locally. */
			bassignformat(
				textfilepath, "%s%s%05x_%05x.tar.txt", cstr(removedir), pathsep, o->original_collection, o->archive_number);

			sv_result ignore = sv_file_writefile(cstr(textfilepath), "", "w");
			sv_result_close(&ignore);
			somenotmoved = true;
		}
		else
		{
			bool moved = os_tryuntil_move(cstr(src), cstr(dest), true);
			if (!moved)
			{
				bstring errmsg = bformat("Could not move %s to %s", cstr(src), cstr(dest));
				bstrListAppend(messages, errmsg);
				bdestroy(errmsg);
			}
		}
	}

	if (somenotmoved)
	{
		printf("Some archives weren't found, so we created text files in %s as a reminder of which archives can be safely deleted.\n",
			cstr(removedir));
	}
	else if (archives_to_remove->length)
	{
		printf("Completed successfully. The archives that are no longer needed were moved to %s.\n", cstr(removedir));
	}

cleanup:
	svdb_txn_close(&txn, db);
	bdestroy(removedir);
	bdestroy(src);
	bdestroy(dest);
	bdestroy(textfilepath);
	return currenterr;
}

void svdp_compact_state_close(svdp_compact_state* self)
{
	sv_2d_array_close(&self->archivenum_tostats);
}

check_result svdp_compact(const svdp_application* app, const svdp_backupgroup* group, svdb_connection* db)
{
	sv_result currenterr = {};
	svdb_txn txn = {};
	svdp_compact_state op;
	op.archivenum_tostats = sv_2d_array_open(sizeof32u(svdp_archive_info));
	sv_array archives_to_remove = sv_array_open(sizeof32u(svdp_archive_info), 0);
	sv_array archives_to_compact = sv_array_open(sizeof32u(svdp_archive_info), 0);
	bstrList* messages = bstrListCreate();
	bstring readytoupload = bformat("%s%suserdata%s%s%sreadytoupload", 
		cstr(app->path_app_data), pathsep, pathsep, cstr(group->groupname), pathsep);

	bstrList* choices = bstrListCreate();
	bstring msg = bformat("Compact data.\n\nWe're currently set up to mark files older than %d days as no longer "
		"needed, you can change this in Edit Group.\n",
		group->days_to_keep_prev_versions);
	bstrListSplitCstr(choices, "Find groups of files that are no longer needed (faster)|Go through each file and "
		"remove what is not needed (thorough, needs backup archives to be present locally)|Back", "|");
	int choice = ui_numbered_menu_pick_from_list(cstr(msg), choices, NULL, NULL, NULL);
	op.is_thorough = choice == 1;
	if (choice > 1)
	{
		goto cleanup;
	}

	check(svdb_txn_open(&txn, db));
	check(svdp_compact_getexpirationcutoff(db, group, &op.expiration_cutoff, time(NULL)));
	if (op.expiration_cutoff)
	{
		check(svdb_contentsiter(db, &op, &svdp_compact_maparchiveidtostats));
		svdp_compact_see_what_to_remove(&op.archivenum_tostats,
			group->compact_threshold_bytes, &archives_to_remove, &archives_to_compact, op.is_thorough);
		
		/* ensure db changes committed before moving files on disk */
		check(svdb_txn_commit(&txn, db));

		if (archives_to_remove.length + archives_to_compact.length == 0)
		{
			printf("Looks like our data is already compacted.");
		}
		else if (ask_user_yesno("As listed above, we found some old versions of files that can be safely removed. Continue?"))
		{
			check(svdp_compact_removeoldarchives(db, cstr(readytoupload), &archives_to_remove, messages));
			check(svdp_compact_removefilesfromarchives(app, group, db, cstr(readytoupload), &archives_to_compact, messages));
			if (messages->qty && ask_user_yesno("We encountered some minor warnings. Show details?"))
			{
				for (int i = 0; i < messages->qty; i++)
				{
					puts(bstrListViewAt(messages, i));
				}
				alertdialog("");
			}
		}
	}
	

cleanup:
	alertdialog("");
	bdestroy(readytoupload);
	bdestroy(msg);
	bstrListDestroy(choices);
	bstrListDestroy(messages);
	svdb_txn_close(&txn, db);

	for (uint32_t i = 0; i < archives_to_compact.length; i++)
	{
		svdp_archive_info* o = (svdp_archive_info*)sv_array_at(&archives_to_compact, i);
		sv_array_close(&o->old_individual_files);
	}
	for (uint32_t i = 0; i < archives_to_remove.length; i++)
	{
		svdp_archive_info* o = (svdp_archive_info*)sv_array_at(&archives_to_remove, i);
		sv_array_close(&o->old_individual_files);
	}
	sv_array_close(&archives_to_compact);
	sv_array_close(&archives_to_remove);
	svdp_compact_state_close(&op);
	return currenterr;
}

check_result svdp_choosecollectionid(svdb_connection* db, const char* readydir, bstring dbfilechosen, uint64_t* collectionidchosen)
{
	sv_result currenterr = {};
	bstrClear(dbfilechosen);
	*collectionidchosen = 0;
	bstring s = bstring_open();
	bstring dbpath = bstring_open();
	bstrList* choices = bstrListCreate();
	sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
	check(svdb_collectionsget(db, &arr, true));
	for (uint32_t i = 0; i < arr.length; i++)
	{
		svdb_collections_row* row = (svdb_collections_row*)sv_array_at(&arr, i);
		svdb_collectiontostring(row, true, false, s);
		bstrListAppend(choices, s);
	}

	if (choices->qty == 0)
	{
		printf("There are no collections to be backed up.\n");
		goto cleanup;
	}

	while (true)
	{
		int index = ui_numbered_menu_pick_from_long_list(choices, 10 /* show in groups of 10. */);
		if (index >= 0 && index < cast32u32s(arr.length))
		{
			svdb_collections_row* row = (svdb_collections_row*)sv_array_at(&arr, cast32s32u(index));
			bassignformat(dbpath, "%s%s%05x_index.db", readydir, pathsep, cast64u32u(row->id));
			if (os_file_exists(cstr(dbpath)))
			{
				printf("You chose to restore from %s.\n", bstrListViewAt(choices, index));
				if (ask_user_yesno("Continue?"))
				{
					bassign(dbfilechosen, dbpath);
					*collectionidchosen = row->id;
					break;
				}
			}
			else
			{
				printf("The file %s was not found. Please download it and try again, or pick another collection to restore from.\n",
					cstr(dbpath));
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
	bstrListDestroy(choices);
	sv_array_close(&arr);
	return currenterr;
}

void svdp_restore_state_close(svdp_restore_state* self)
{
	bdestroy(self->workingdir);
	bdestroy(self->destdir);
	bdestroy(self->scope);
	bdestroy(self->destfullpath);
	bdestroy(self->errmsg);
	bstrListDestroy(self->messages);
	svdp_archiver_close(&self->archiver);
	set_self_zero();
}

check_result svdp_restore_file(svdp_restore_state* op, const svdb_files_row* in_files_row, const bstring path)
{
	sv_result currenterr = {};
	svdb_contents_row contentsrow = {};
	bstring archivepath = bstring_open();
	check(svdb_contentsbyid(op->db, in_files_row->contents_id, &contentsrow));
	check_b(contentsrow.id != 0, "did not get correct contents row.");

	/* get src path */
	bassignformat(archivepath, "%s%s%05x_%05x.tar", cstr(op->archiver.readydir), pathsep,
		contentsrow.original_collection, contentsrow.archivenumber);

	/* get dest path */
	check_b(blength(path) >= 4, "path length is too short %s", cstr(path));
	const char* pathwithoutroot = cstr(path) + (islinux ? 1 : 3);
	bassignformat(op->destfullpath, "%s%s%s", cstr(op->destdir), pathsep, pathwithoutroot);
	check(test_hook_call_when_restoring_file(op->test_context, cstr(path), op->destfullpath));
	check(svdp_archiver_restore_from_archive(&op->archiver, cstr(archivepath), contentsrow.id, 
		cstr(op->archiver.workingdir), cstr(op->subworkingdir), cstr(op->destfullpath)));
	
	/* apply posix permissions */
	if (islinux)
	{
		log_errno(_, chmod(cstr(op->destfullpath), 
			cast64u32u(in_files_row->flags)), cstr(op->destfullpath));
	}

cleanup:
	bdestroy(archivepath);
	return currenterr;
}

check_result svdp_restore_cb(void* context, unusedboolptr(),
	const svdb_files_row* in_files_row, const bstring path)
{
	svdp_restore_state* op = (svdp_restore_state*)context;
	if (fnmatch_simple(cstr(op->scope), cstr(path)))
	{
		op->countfiles++;
		log_b(in_files_row->e_status != sv_filerowstatus_complete ||
			in_files_row->most_recent_collection != op->collectionidwanted,
			"%s, at the original time the backup was taken this file was not available, "
			"so we will recover a valid but previous version.", cstr(path));
		sv_result res = svdp_restore_file(op, in_files_row, path);

		if (res.code)
		{
			bassignformat(op->errmsg, "Could not restore %s\n\n%s", cstr(path), cstr(res.msg));
			bstrListAppend(op->messages, op->errmsg);
			sv_result_close(&res);
		}
	}
	return OK;
}

check_result svdp_restore(const svdp_application* app, const svdp_backupgroup* group, svdb_connection* db, int optype)
{
	sv_result currenterr = {};
	svdp_restore_state op = {};
	op.workingdir = bstring_open();
	op.subworkingdir = bstring_open();
	op.destdir = bstring_open();
	op.scope = bstring_open();
	op.destfullpath = bstring_open();
	op.errmsg = bstring_open();
	op.messages = bstrListCreate();
	
	bstring prev_dbfile = bstring_open();
	check(svdp_archiver_open(&op.archiver, cstr(app->path_app_data), cstr(group->groupname),
		0, group->approx_archive_size_bytes, cstr(group->encryption)));

	if (optype == svdp_op_restore_from_past)
	{
		check(svdp_choosecollectionid(db, cstr(op.archiver.readydir), prev_dbfile, &op.collectionidwanted));
		if (op.collectionidwanted && blength(prev_dbfile))
		{
			check(svdb_connection_disconnect(db));
			check(svdb_connection_open(db, cstr(prev_dbfile)));
		}
		else
		{
			goto cleanup;
		}
	}
	else
	{
		check(svdb_collectiongetlast(db, &op.collectionidwanted));
		if (!op.collectionidwanted)
		{
			printf("There are no collections to restore.\n");
			goto cleanup;
		}
	}
	
	printf("Which files should we restore? Enter '*' to restore all files, or a pattern like '*.mp3' or '%s', or 'q' to cancel.\n",
		islinux ? "/directory/path/*" : "C:\\myfiles\\path\\*");
	ask_user_prompt("", false, op.scope);
	if (s_equal(cstr(op.scope), "q"))
	{
		goto cleanup;
	}
	
	fnmatch_checkvalid(cstr(op.scope), op.errmsg);
	check_b(blength(op.errmsg) == 0, "This is not a valid pattern. %s", cstr(op.errmsg));
	ask_user_prompt("Please enter the full path to an output directory. It should be currently empty and "
		"have enough free hard drive space. Or enter 'q' to cancel.", false, op.destdir);
	if (s_equal(cstr(op.destdir), "q"))
	{
		goto cleanup;
	}
	else if (!isvalidchosendirectory(app, op.destdir) || !os_dir_empty(cstr(op.destdir)))
	{
		printf("Could not restore to this directory. %s",
			os_dir_empty(cstr(op.destdir)) ? "" :
			"Please first ensure that the directory is empty (including hidden files).");
		goto cleanup;
	}
	else if (!os_is_dir_writable(cstr(op.destdir)))
	{
		printf("It doesn't look like we have write access to %s.", cstr(op.destdir));
		goto cleanup;
	}

	bassign(op.workingdir, op.archiver.workingdir);
	bassignformat(op.subworkingdir, "%s%sout", cstr(op.workingdir), pathsep);
	check_b(os_create_dirs(cstr(op.subworkingdir)), "could not create %s", cstr(op.subworkingdir));
	op.db = db;
	check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
	if (op.messages->qty && ask_user_yesno("Some files could not be restored. Show details?"))
	{
		for (int i = 0; i < op.messages->qty; i++)
		{
			puts(bstrListViewAt(op.messages, i));
		}
	}
	
	printf("Restore complete. %lld files restored.", castull(op.countfiles));
	check(svdb_connection_disconnect(op.db));
cleanup:
	alertdialog("");
	bdestroy(prev_dbfile);
	svdp_restore_state_close(&op);
	return currenterr;
}
