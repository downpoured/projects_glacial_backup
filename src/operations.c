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
			Set the status to status-file-complete
	b) If database has this path, but the file was changed,
			Set the status to status-file-needs-work
	c) If the database does not have this path
			Add a row with this path, status is status-file-needs-work 

Loop through all database rows with status < status-file-complete
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
			Update row, setting filesize, archive id, and status to status-file-complete

Adding to an archive
	Rather than storing the entire path, names within archive are in the form 00000.file or 00000.7z
	number is the contentsid in hex.
		(Otherwise, we'd have to call 7z l to see what is in an archive, 
		which is unreliable due to 7z's limitations around char encodings)
		(the filenames wouldn't be accurate after a while anyways, since we re-use contents)
		We'll add a text file called filenames.txt to the archive though as a courtesy.
	We use a heuristic based on file extension; binary files like mp3 are stored with no 
	compression, which is much faster. If adding with no compression and no encryption, 
	we'll add the file directly to the .tar instead of via a .7z.

Make a copy of the database into the ready-to-upload directory

Restore
----------------------------------------------------------------
First, let the user decide which old database to connect to.
(The newest database only has filenames for the latest collection.)
Let the user choose a scope (a fnmatch pattern)
Let the user choose an output directory (must be empty)
Simply iterate through all fileslist rows, and if the path matches the scope, extract files.
Add any failures to a list of strings to be shown afterwards; do not halt progress on every error.

Compact
----------------------------------------------------------------
Summarizing archive information
	First, determine which collectionid is the 'cutoff point'. 
	For example, if user chooses that files expire after 30 days, 
	we would find the first collectionid before 30 days
	Create a datastructure that maps archiveid to struct counting how many expired and
	non-expired files are present.
		(archiveid is the collectionid + number for that archive; the filename 
		in the form 0000_0000.tar reflects this)
	we could have used a hashmap/trie would be suitable for this mapping, 
	but we'll use a 2D array where the 1st dimension is collectionid and 2nd dimension is number.
	(i.e. 00001_00001.tar maps to myarray[1][1] and so on)
	Go through every row in tblcontents.
		If the content is needed, increment the total in the struct.
		If the content is not needed, increment the total of not-needed files.
		Also, add the contentid to a list of contentids in this archive that are ok to be removed.

Removing entire archives
	If the struct shows that no needed-files are present in an archive, 
	we'll just remove the entire archive
	If we're doing this offline and don't have access to the archive itself, 
	create a text file in a 'ready-to-delete' directory as an indicator to the user
	1) Start transaction
	2) delete from contentids where archiveid=x
	3) Commit transaction
	4) Move the archive on disk into a 'readytodelete' directory.
	Moving the files must be done *after *database operation is complete and committed. 
	Otherwise we will have inconsistent state --
	remember that there's nothing stopping the contents here from being used again
	in case the same file re-appears. 

Removing individual files from archives
	This can be done to save disk space, but note that it is slow, causing a rewrite of the
	rest of the archive (would be even slower if we didn't disable 'solid' mode).
	And so we don't remove individual files from archives unless a threshold is met. The
	compact_threshold_bytes setting.
	For example, if compact_threshold_bytes is 10*1024*1024, we won't remove files from
	an archive unless we know that 10mb(uncompressed at least) can be saved
	And we can remove the files in bulk by sending many command-line parameters to 7z.

*/

check_result svdp_backup_addtoqueue_cb(void *context, const bstring filepath, 
	uint64_t lmt_from_disk, uint64_t actual_size_from_disk, const bstring permissions)
{
	sv_result currenterr = {};
	svdp_backup_state *op = (svdp_backup_state *)context;
	uint64_t size_from_disk = actual_size_from_disk;
	show_status_update_queue(op);
	if (!os_recurse_is_dir(lmt_from_disk, size_from_disk) &&
		svdp_backupgroup_isincluded(op->group, cstr(filepath), op->messages))
	{
		svdb_files_row row = { 0 };
		check(svdb_filesbypath(&op->db, filepath, &row));
		adjustfilesize_if_audio_file(op->group->separate_metadata_enabled,
			get_file_extension_info(cstr(filepath), blength(filepath)), size_from_disk, &size_from_disk);

		if (row.id != 0 && row.contents_length == size_from_disk && row.last_write_time == lmt_from_disk &&
			row.most_recent_collection == op->collectionid - 1 && row.e_status == sv_filerowstatus_complete)
		{
			/* the file hasn't been changed. */
			sv_log_writefmt("queue-same %s %llx", cstr(filepath), castull(row.id));
			row.most_recent_collection = op->collectionid;
			row.e_status = sv_filerowstatus_complete;
			check(svdb_filesupdate(&op->db, &row, permissions));
			check(svdb_contents_setlastreferenced(&op->db, row.contents_id, op->collectionid));
		}
		else if (row.id != 0)
		{
			/* the file has been changed. */
			sv_log_writefmt("queue-changed %s %llx", cstr(filepath), castull(row.id));
			row.most_recent_collection = op->collectionid;
			row.e_status = sv_filerowstatus_queued;
			check(svdb_filesupdate(&op->db, &row, permissions));
			op->count.approx_count_items_in_queue += 1;
		}
		else
		{
			/* it's a new file. */
			sv_log_writelines("queue-new", cstr(filepath));
			check(svdb_filesinsert(&op->db, filepath, 
				row.most_recent_collection, sv_filerowstatus_queued, NULL));

			op->count.approx_count_items_in_queue += 1;
		}
	}
cleanup:
	return currenterr;
}

check_result svdp_backup_addtoqueue(svdp_backup_state *op)
{
	sv_result currenterr = {};
	bstring tmp = bstring_open();
	check(test_hook_provide_file_list(op->test_context, op, &svdp_backup_addtoqueue_cb));
	sv_log_writefmt("starting svdp_backup_addtoqueue collection=%llu", op->collectionid);
	for (int i = 0; i < op->group->root_directories->qty; i++)
	{
		const char *dir = bstrlist_view(op->group->root_directories, i);
		if (os_dir_exists(dir))
		{
			sv_log_writelines("svdp_backup_addtoqueue", dir);
			printf("Searching %s...\n", dir);
			os_recurse_params params = { op, dir, &svdp_backup_addtoqueue_cb, PATH_MAX /*max depth*/,
				op->messages };
			check(os_recurse(&params));
		}
		else
		{
			log_b(0, "Note: starting directory \n%s\nwas not found.", dir);
			printf("\nNote: starting directory \n%s\nwas not found. You can go to 'Edit Group', "
				"'Add/Remove Directories' to remove this directory from the list.\n", dir);
		}
	}

cleanup:
	bdestroy(tmp);
	return currenterr;
}

check_result svdp_backup_usemanualfilelist_onefile(svdp_backup_state *op, sv_pseudosplit *split, 
	bstring permissions, uint32_t i)
{
	sv_result currenterr = {};
	os_exclusivefilehandle handle = {};
	bool filenotfound = false;
	check(os_exclusivefilehandle_tryuntil_open(
		&handle, sv_pseudosplit_viewat(split, i), true, &filenotfound));

	if (filenotfound)
	{
		printf("not found %s\n", sv_pseudosplit_viewat(split, i));
		check_b(ask_user_yesno("continue?"), "user chose not to continue.");
	}
	else
	{
		uint64_t lmt_from_disk = 0, actual_size_from_disk = 0;
		check(os_exclusivefilehandle_stat(&handle, &actual_size_from_disk, 
			&lmt_from_disk, permissions));

		check(svdp_backup_addtoqueue_cb(op, split->currentline, lmt_from_disk,
			actual_size_from_disk, permissions));
	}

cleanup:
	os_exclusivefilehandle_close(&handle);
	return currenterr;
}


check_result svdp_backup_usemanualfilelist(
	svdp_backup_state *op, const char *appdir, const char *groupname)
{
	sv_result currenterr = {};
	sv_pseudosplit split = sv_pseudosplit_open("");
	bstring filelistpath = bformat("%s%suserdata%s%s%smanual-file-list.txt", 
		appdir, pathsep, pathsep, groupname, pathsep);

	bstring permissions = bstring_open();
	if (os_file_exists(cstr(filelistpath)))
	{
		check(sv_file_readfile(cstr(filelistpath), split.text));
		bstr_replaceall(split.text, "\r", "");
		sv_pseudosplit_split(&split, '\n');
		printf("reading manual file list...%d entries...\n", split.splitpoints.length);
		sv_log_writefmt("reading manual file list %s...%d entries...\n", 
			cstr(filelistpath), split.splitpoints.length);

		for (uint32_t i = 0; i < split.splitpoints.length; i++)
		{
			if (sv_pseudosplit_viewat(&split, i)[0])
			{
				check(svdp_backup_usemanualfilelist_onefile(op, &split, permissions, i));
			}
		}
	}

cleanup:
	sv_pseudosplit_close(&split);
	bdestroy(filelistpath);
	bdestroy(permissions);
	return currenterr;
}


void svdp_backup_state_close(svdp_backup_state *self)
{
	if (self)
	{
		bstrlist_close(self->messages);
		bdestroy(self->tmp_resultstring);
		bdestroy(self->count.summary_current_dir);
		svdp_archiver_close(&self->archiver);
		sv_array_close(&self->dbrows_to_delete);
		svdb_connection_close(&self->db);
		set_self_zero();
	}
}

void show_status_update_queue(svdp_backup_state *op)
{
	time_t now = time(NULL);
	if (!op->test_context && now - op->time_since_showing_update > 4)
	{
		fputs(".", stdout);
		op->time_since_showing_update = now;
	}
}

void pause_if_requested(svdp_backup_state *op)
{
	time_t now = time(NULL);
	if (!op->test_context && now - op->time_since_showing_update > 30)
	{
		for (uint64_t i = 0; i < op->pause_duration_seconds / 2; i++)
		{
			os_sleep(2 * 1000);
			fputs(".", stdout);
		}

		op->time_since_showing_update = time(NULL);
		fputs("\n", stdout);
	}
}

void show_status_update(svdp_backup_state *op, unused_ptr(const char))
{
	if (op->count.approx_count_items_in_queue != 0)
	{
		uint64_t percentage_done = (100 * (op->count.count_moved_path + op->count.count_new_path))
			/ op->count.approx_count_items_in_queue;
		percentage_done = MIN(percentage_done, 99); /* never show something like "104%" */
		if (!op->test_context && percentage_done != op->prev_percent_shown)
		{
			printf("%llu%% complete...\n", castull(percentage_done));
			op->prev_percent_shown = percentage_done;
		}
	}
}

check_result svdp_backup_addfile(svdp_backup_state *op, os_exclusivefilehandle *handle, 
	const bstring path, uint64_t rowid)
{
	sv_result currenterr = {};
	svdb_files_row newfilesrow = {};
	svdb_contents_row contentsrow = {};

	bstring permissions = bstring_open();
	uint64_t contentslength = 0, rawcontentslength = 0, modtimeondisk = 0;
	hash256 hash = {};
	uint32_t crc32 = 0;
	knownfileextension ext = get_file_extension_info(cstr(path), blength(path));
	check(os_exclusivefilehandle_stat(handle, &rawcontentslength, &modtimeondisk, permissions));
	check(test_hook_get_file_info(op->test_context, handle, 
		&rawcontentslength, &modtimeondisk, permissions));
	adjustfilesize_if_audio_file(op->group->separate_metadata_enabled, 
		ext, rawcontentslength, &contentslength);
	newfilesrow.last_write_time = modtimeondisk;
	newfilesrow.contents_length = contentslength;
	check(hash_of_file(handle, op->group->separate_metadata_enabled, 
		ext, cstr(op->archiver.path_audiometadata_binary), &hash, &crc32));
	
	/* is this file already in an archive? */
	check(svdb_contentsbyhash(&op->db, &hash, newfilesrow.contents_length, &contentsrow));
	if (contentsrow.id)
	{
		sv_log_writefmt("process-addupdate %s fid=%llx cid=%llx", cstr(path), castull(rowid), 
			castull(contentsrow.id));

		check(svdb_contents_setlastreferenced(&op->db, contentsrow.id, op->collectionid));
		newfilesrow.contents_id = contentsrow.id;
	}
	else
	{
		/* get the latest contentrowid */
		svdb_contents_row newcontentsrow = { 0 };
		check(svdb_contentsinsert(&op->db, &newcontentsrow.id));

		/* add to an archive on disk */
		bool iscompressed = ext != knownfileextension_none;
		sv_log_writefmt("process-addnew %s fid=%llx cid=%llx", cstr(path), castull(rowid), 
			castull(newcontentsrow.id));
		check(svdp_archiver_addfile(&op->archiver, cstr(path), iscompressed, newcontentsrow.id, 
			&newcontentsrow.archivenumber, &newcontentsrow.compressed_contents_length));

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

	/* update the filesrow */
	newfilesrow.id = rowid;
	newfilesrow.most_recent_collection = op->collectionid;
	newfilesrow.e_status = sv_filerowstatus_complete;
	check(svdb_filesupdate(&op->db, &newfilesrow, permissions));

cleanup:
	bdestroy(permissions);
	return currenterr;
}

check_result svdp_backup_processqueue_cb(void *context, unused_ptr(bool),
	const svdb_files_row *in_files_row, const bstring path, unused(const bstring))
{
	sv_result currenterr = {};
	os_exclusivefilehandle handle = {};
	svdp_backup_state *op = (svdp_backup_state *)context;
	pause_if_requested(op);
	show_status_update(op, cstr(path));
	bool filenotfound = false;

	sv_result result_openhandle = os_exclusivefilehandle_tryuntil_open(
		&handle, cstr(path), true, &filenotfound);

	if (result_openhandle.code && in_files_row->contents_id == 0)
	{
		/* can't access the file, but this is a file we haven't backed up yet */
		sv_log_writefmt("process-newfilenotaccess %s %llx", cstr(path), castull(in_files_row->id));
		sv_array_add64u(&op->dbrows_to_delete, in_files_row->id);
	}
	else if (result_openhandle.code)
	{
		/* can't access the file: pretend like the file exists and has not changed.  */
		sv_log_writefmt("process-notaccess %s %llx", cstr(path), castull(in_files_row->id));
		bassignformat(op->tmp_resultstring, "Could not access %s", cstr(path));
		bstrlist_append(op->messages, op->tmp_resultstring);
		check(svdb_contents_setlastreferenced(&op->db, in_files_row->contents_id, op->collectionid));
	}
	else if (!result_openhandle.code && filenotfound)
	{
		/* path no longer exists */
		sv_log_writefmt("process-notfound %s %llx", cstr(path), castull(in_files_row->id));
		sv_array_add64u(&op->dbrows_to_delete, in_files_row->id);
		op->count.count_moved_path++;
	}
	else
	{
		/* can access the file */
		check(svdp_backup_addfile(op, &handle, path, in_files_row->id));
		op->count.count_new_path++;
	}

cleanup:
	sv_result_close(&result_openhandle);
	os_exclusivefilehandle_close(&handle);
	return currenterr;
}

check_result svdp_backup_makecopyofdb(svdp_backup_state *op, 
	const svdp_backupgroup *group, const char *appdir)
{
	bstring src = bformat("%s%suserdata%s%s%s%s_index.db", 
		appdir, pathsep, pathsep, cstr(group->groupname), pathsep, cstr(group->groupname));

	bstring dest = bformat("%s%suserdata%s%s%sreadytoupload%s%05x_index.db",
		appdir, pathsep, pathsep, cstr(group->groupname), pathsep, pathsep, cast64u32u(op->collectionid));
	
	/* make a copy of it to the upload dir, ok if this extra copy fails. */
	/* future feature might be to call drop index and vacuum on the copy, to save disk space */
	sv_log_writefmt("copy %s to %s", cstr(src), cstr(dest));
	os_sleep(op->test_context ? 1 : 250);
	log_b(os_tryuntil_copy(cstr(src), cstr(dest), true), "could not copy db.");
	bdestroy(src);
	bdestroy(dest);
	return OK;
}

void svdp_backup_compute_preview_on_new_file(svdp_backup_state *op,
	const char *path, uint64_t rawcontentslength)
{
	const int noteworthysize = 20 * 1024 * 1024;
	os_get_parent(path, op->tmp_resultstring);
	if (!bstr_equal(op->count.summary_current_dir, op->tmp_resultstring))
	{
		/* recurse-through-directory will make files in the same directory adjacent to each other,
		and so when the directory changes we're done with this directory. */
		if (op->count.summary_current_dir_size > noteworthysize)
		{
			printf("\n%s -- %.3fMb of new data\n", cstr(op->count.summary_current_dir), 
				(double)op->count.summary_current_dir_size / (1024.0*1024.0));
		}

		op->count.summary_current_dir_size = 0;
		bassign(op->count.summary_current_dir, op->tmp_resultstring);
	}

	op->count.count_new_bytes += rawcontentslength;
	op->count.summary_current_dir_size += rawcontentslength;
}

check_result svdp_backup_compute_preview_cb(void *context, unused_ptr(bool),
	unused_ptr(const svdb_files_row), const bstring path, unused(const bstring))
{
	sv_result currenterr = {};
	os_exclusivefilehandle handle = {};
	svdp_backup_state *op = (svdp_backup_state *)context;
	bool filenotfound = false;
	sv_result result_openhandle = os_exclusivefilehandle_open(&handle, cstr(path), true, &filenotfound);
	if (!filenotfound && !result_openhandle.code)
	{
		hash256 hash = {};
		uint32_t crc32 = 0;
		uint64_t contentslength = 0, rawcontentslength = 0, modtimeondisk = 0;
		knownfileextension ext = get_file_extension_info(cstr(path), blength(path));
		check(os_exclusivefilehandle_stat(&handle, &rawcontentslength, &modtimeondisk, NULL));
		adjustfilesize_if_audio_file(op->group->separate_metadata_enabled, ext, rawcontentslength, &contentslength);
		bool file_is_new = op->collectionid == 1; /* on the first run, no need for a db lookup */
		if (!file_is_new)
		{
			check(hash_of_file(&handle, op->group->separate_metadata_enabled,
				ext, cstr(op->archiver.path_audiometadata_binary), &hash, &crc32));

			svdb_contents_row found = {};
			check(svdb_contentsbyhash(&op->db, &hash, contentslength, &found));
			if (!found.id)
			{
				file_is_new = true;
			}
		}

		if (file_is_new)
		{
			svdp_backup_compute_preview_on_new_file(op, cstr(path), rawcontentslength);
		}
	}

cleanup:
	sv_result_close(&result_openhandle);
	os_exclusivefilehandle_close(&handle);
	return currenterr;
}

void svdp_backup_compute_preview(svdp_backup_state *op)
{
	if (ask_user_yesno("Show preview? y/n"))
	{
		op->count.summary_current_dir = bstring_open();
		op->count.count_new_bytes = 0;
		check_warn(svdb_files_in_queue(&op->db, sv_makestatus(op->collectionid,
			sv_filerowstatus_complete), op, svdp_backup_compute_preview_cb), "", continue_on_err);

		svdp_backup_compute_preview_on_new_file(op, "", 0);
		printf("\nNew files were seen, for a total of %.3fMb new data.\n", 
			(double)op->count.count_new_bytes / (1024.0*1024.0));
		op->count.count_new_bytes = 0;
		alertdialog("");
	}
}

check_result svdp_backup_write_archive_checksums(svdp_backup_state *op)
{
	sv_result currenterr = {};
	bstring filename = bstring_open();
	bstring filenamestartswith = bformat("%05llx_", castull(op->collectionid));
	const char *dir_readytoupload = cstr(op->archiver.path_readytoupload);
	bstrlist *files = bstrlist_open();
	check(os_listfiles(dir_readytoupload, files, false));
	for (int i = 0; i < files->qty; i++)
	{
		os_get_filename(bstrlist_view(files, i), filename);
		if (s_startswith(cstr(filename), cstr(filenamestartswith)) &&
			!s_contains(cstr(filename), " ") &&
			s_endswith(cstr(filename), ".tar"))
		{
			check(write_archive_checksum(&op->db, bstrlist_view(files, i), 0, false));
			fputs(".", stdout);
		}
	}

cleanup:
	bdestroy(filename);
	bdestroy(filenamestartswith);
	return currenterr;
}

check_result svdp_backup_recordcollectionstats(svdp_backup_state *op)
{
	sv_result currenterr = {};
	time_t timecompleted = time(NULL);
	svdb_collections_row updatedrow = { 0 };
	updatedrow.id = op->collectionid;
	updatedrow.time_finished = cast64s64u(timecompleted);
	check(svdb_filesgetcount(&op->db, &updatedrow.count_total_files));
	updatedrow.count_new_contents = op->count.count_new_files;
	updatedrow.count_new_contents_bytes = op->count.count_new_bytes;
	check(svdb_collectionupdate(&op->db, &updatedrow));
	svdb_collectiontostring(&updatedrow, true, true, op->tmp_resultstring);
	sv_log_writelines("Writing collection stats", cstr(op->tmp_resultstring));
	if (!op->test_context)
	{
		printf("   Total number of backed up files: %llu\n\n", castull(updatedrow.count_total_files));
	}
cleanup:
	return currenterr;
}

check_result svdp_backup(const svdp_application *app, const svdp_backupgroup *group, 
	svdb_connection *db, void *test_context)
{
	sv_result currenterr = {};
	svdp_backup_state op = {};
	svdb_txn txn = {};
	bstring dbpath = bstring_open();
	op.app = app;
	op.group = group;
	op.test_context = test_context;
	op.messages = bstrlist_open();
	op.dbrows_to_delete = sv_array_open(sizeof32u(uint64_t), 0);
	op.prev_percent_shown = UINT64_MAX;
	op.pause_duration_seconds = group->pause_duration_seconds;
	op.tmp_resultstring = bstring_open();
	os_clr_console();
	if (!test_context && group->root_directories->qty == 0)
	{
		alertdialog("\nThere are currently no directories to be backed up for this group. "
			"To add a directory, go to 'Edit Group' and choose 'Add/remove directories'.");
		goto cleanup;
	}
	
	svdp_application_groupdbpathfromname(app, cstr(group->groupname), dbpath);
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(&op.db, cstr(dbpath)));
	check(svdb_txn_open(&txn, &op.db));
	check(svdb_collectioninsert(&op.db, (uint64_t)time(NULL), &op.collectionid));
	check(svdp_archiver_open(&op.archiver, cstr(op.app->path_app_data), cstr(op.group->groupname),
		cast64u32u(op.collectionid), op.group->approx_archive_size_bytes, cstr(op.group->encryption)));
	check(checkexternaltoolspresent(&op.archiver, op.group->separate_metadata_enabled));
	check(svdp_backup_addtoqueue(&op));
	check(svdp_backup_usemanualfilelist(&op, cstr(op.app->path_app_data), cstr(op.group->groupname)));
	check(test_hook_call_before_processing_queue(op.test_context, &op.db, &op.archiver));
	check_b(op.collectionid, "must have valid collectionid");
	if (!test_context)
	{
		printf("\nThere %s %llu added, changed, or renamed file%s.\n\n", 
			op.count.approx_count_items_in_queue == 1 ? "is" : "are",
			castull(op.count.approx_count_items_in_queue),
			op.count.approx_count_items_in_queue == 1 ? "" : "s");
		
		svdp_backup_compute_preview(&op);
	}
	
	check(svdp_archiver_beginarchiving(&op.archiver));
	check(svdb_files_in_queue(&op.db, sv_makestatus(op.collectionid, sv_filerowstatus_complete), 
		&op, svdp_backup_processqueue_cb));

	if (!test_context)
	{
		printf("100%% complete...");
		printf("\n%llu new file%s (%0.3f Mb) archived.\n ",
			castull(op.count.count_new_files),
			op.count.count_new_files == 1 ? "" : "s",
			(double)op.count.count_new_bytes / (1024.0 * 1024));
	}

	check(svdb_files_bulk_delete(&op.db, &op.dbrows_to_delete, 0));
	check(svdp_backup_recordcollectionstats(&op));
	check(svdp_archiver_finisharchiving(&op.archiver));
	check(svdp_backup_write_archive_checksums(&op));
	check(svdb_txn_commit(&txn, &op.db));
	check(svdb_connection_disconnect(&op.db));
	check(svdp_backup_makecopyofdb(&op, group, cstr(app->path_app_data)));
	if (op.messages->qty && !op.test_context)
	{
		puts("\nNotes:");
		for (int i = 0; i < op.messages->qty; i++)
		{
			printf("%s\n", bstrlist_view(op.messages, i));
		}
	}

	if (!op.test_context)
	{
		printf("\n\nBackups saved successfully to\n%s\n\nYou can now copy the contents "
			"of this directory to an external drive, or upload to online storage.",
			cstr(op.archiver.path_readytoupload));
		alertdialog("");
	}

cleanup:
	svdb_txn_close(&txn, &op.db);
	svdb_connection_close(&op.db);
	svdp_backup_state_close(&op);
	bdestroy(dbpath);
	return currenterr;
}

check_result svdp_application_run(svdp_application *app, int optype)
{
	sv_result currenterr = {};
	svdp_backupgroup backupgroup = {};
	svdb_connection connection = {};
	bstring newpath = bstring_open();
	check(loadbackupgroup(app, &backupgroup, &connection, NULL));
	check(svdp_backupgroup_ask_key(&backupgroup, app, optype == svdp_op_backup));
	if (connection.db)
	{
		switch (optype)
		{
		case svdp_op_compact:
			check(svdp_compact(app, &backupgroup, &connection));
			break;
		case svdp_op_backup:
			check(svdp_backup(app, &backupgroup, &connection, NULL));
			break;
		case svdp_op_viewinfo:
			check(svdp_application_viewinfo(app, &backupgroup, &connection));
			break;
		case svdp_op_verify:
			check(svdp_verify_archive_checksums(app, &backupgroup, &connection, NULL));
			break;
		case svdp_op_restore: /* fallthrough */
		case svdp_op_restore_from_past:
			check(svdp_restore(app, &backupgroup, &connection, optype));
			break;
		}
	}

	check(svdb_connection_disconnect(&connection));
cleanup:
	svdp_backupgroup_close(&backupgroup);
	svdb_connection_close(&connection);
	bdestroy(newpath);
	return currenterr;
}

/* Look for files that are too old - with latest_seen_collectionid <= collectionid_to_expire. */
check_result svdp_compact_getexpirationcutoff(svdb_connection *db, 
	const svdp_backupgroup *group, uint64_t *collectionid_to_expire, time_t now)
{
	sv_result currenterr = {};
	sv_array collectionrows = sv_array_open(sizeof32u(svdb_collections_row), 0);
	check(svdb_collectionsget(db, &collectionrows, true));
	*collectionid_to_expire = 0;
	if (collectionrows.length == 0 || collectionrows.length == 1)
	{
		sv_log_writeline("Nothing to compact, there is no old data.");
		printf("Nothing to compact, there is no old data.\n");
	}
	else
	{
		/* the collections returned are sorted in order from newest to oldest. 
		by starting the loop at 1 we intentionally skip over the newest collection. */
		time_t days_duration_seconds = group->days_to_keep_prev_versions * 60 * 60 * 24;
		for (uint32_t i = 1; i < collectionrows.length; i++)
		{
			svdb_collections_row *row = (svdb_collections_row *)sv_array_at(&collectionrows, i);
			sv_log_writefmt("found collection %llu that finished at %llu", row->id, row->time_finished);
			if (now - cast64u64s(row->time_finished) > days_duration_seconds)
			{
				sv_log_writefmt("choosing collection %llu because %llu > %llu", row->id, 
					now - cast64u64s(row->time_finished), days_duration_seconds);
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
check_result svdp_compact_getarchivestats(void *context, unused_ptr(bool), const svdb_contents_row *row)
{
	svdp_compact_state *op = (svdp_compact_state *)context;
	check_fatal(row->original_collection < 1000 * 1000 && row->archivenumber < 1000 * 1000,
		"too large, archivenamept1=%d, archivenamept2=%d", row->original_collection, row->archivenumber);

	sv_2d_array_ensure_space(&op->archive_statistics, row->original_collection, row->archivenumber);
	svdp_archive_stats *summary = (svdp_archive_stats *)sv_2d_array_at(
		&op->archive_statistics, row->original_collection, row->archivenumber);

	summary->original_collection = row->original_collection;
	summary->archive_number = row->archivenumber;
	if (row->most_recent_collection > op->expiration_cutoff)
	{
		summary->needed_entries += 1;
		summary->needed_size += row->compressed_contents_length;
	}
	else
	{
		summary->old_entries += 1;
		summary->old_size += row->compressed_contents_length;

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

void svdp_compact_see_what_to_remove(svdp_compact_state *op, uint64_t thresholdsizebytes)
{
	sv_log_writefmt("svdp_compact_see_what_to_remove, cutoff=%llu", op->expiration_cutoff);
	op->archives_to_remove = sv_array_open(sizeof32u(svdp_archive_stats), 0);
	op->archives_to_removefiles = sv_array_open(sizeof32u(svdp_archive_stats), 0);

	for (uint32_t i = 0; i < op->archive_statistics.arr.length; i++)
	{
		sv_array *subarray = (sv_array *)sv_array_at(&op->archive_statistics.arr, i);
		for (uint32_t j = 0; j < subarray->length; j++)
		{
			svdp_archive_stats *archive = (svdp_archive_stats *)sv_array_at(subarray, j);
			if (archive->old_entries > 0 && archive->needed_entries == 0)
			{
				/* clear candidate for removal -- everything there is old */
				sv_array_append(&op->archives_to_remove, archive, 1);
				if (!op->test_context)
				{
					printf("%05x_%05x.tar, all %.3fMb no longer needed.\n", i, j, 
						(double)archive->old_size / (1024.0 * 1024.0));
				}
			}
			else if (op->is_thorough && archive->old_size > thresholdsizebytes)
			{
				/* candidate for compaction, since we can save at least thresholdsizebytes. */
				sv_array_append(&op->archives_to_removefiles, archive, 1);
				if (!op->test_context)
				{
					printf("%05x_%05x.tar, %.3fMb out of %.3fMb is no longer needed.\n", i, j, 
						(double)archive->old_size / (1024.0*1024.0), 
						(double)(archive->old_size + archive->needed_size) / (1024.0*1024.0));
				}
			}
			else
			{
				/* free memory */
				sv_array_close(&archive->old_individual_files);
			}
		}
	}
}

check_result svdp_compact_removeoldfilesfromarchives(svdp_compact_state *op, 
	const svdp_application *app, const svdp_backupgroup *group, svdb_connection *db, 
	const char *readydir, bstrlist *messages)
{
	sv_result currenterr = {};
	bstring archivepath = bstring_open();
	bstring getstderr = bstring_open();
	svdp_archiver arch = {};
	svdb_txn txn = {};
	check(svdp_archiver_open(&arch, cstr(app->path_app_data), cstr(group->groupname), 
		0, group->approx_archive_size_bytes, cstr(group->encryption)));
	check(checkexternaltoolspresent(&arch, false));

	/* first, delete rows from the database. must be done in this order for transactional integrity. */
	check(svdb_txn_open(&txn, db));
	for (uint32_t i = 0; i < op->archives_to_removefiles.length; i++)
	{
		svdp_archive_stats *o = (svdp_archive_stats *)sv_array_at(&op->archives_to_removefiles, i);
		bassignformat(archivepath, "%s%s%05x_%05x.tar", 
			readydir, pathsep, o->original_collection, o->archive_number);
		sv_log_writefmt("removeoldfilesfromarchives from db, cutoff=%llu, archive=%s, rowstodelete=%d", 
			op->expiration_cutoff, cstr(archivepath), o->old_individual_files.length);
		if (os_file_exists(cstr(archivepath)))
		{
			check(svdb_contents_bulk_delete(db, &o->old_individual_files, 0));
		}
		else
		{
			bassignformat(getstderr, "archive not seen %s", cstr(archivepath));
			bstrlist_append(messages, getstderr);
		}
	}

	check(svdb_txn_commit(&txn, db));

	/* next, modify the archives by removing some of files they contain */
	for (uint32_t i = 0; i < op->archives_to_removefiles.length; i++)
	{
		svdp_archive_stats *o = (svdp_archive_stats *)sv_array_at(&op->archives_to_removefiles, i);
		bassignformat(archivepath, "%s%s%05x_%05x.tar",
			readydir, pathsep, o->original_collection, o->archive_number);
		sv_log_writefmt("removeoldfilesfromarchives from disk, cutoff=%llu, archive=%s, rowstodelete=%d",
			op->expiration_cutoff, cstr(archivepath), o->old_individual_files.length);
		if (os_file_exists(cstr(archivepath)))
		{
			sv_result result = svdp_archiver_delete_from_archive(&arch,
				cstr(archivepath), cstr(op->working_dir_unarchived), &o->old_individual_files);

			if (result.code)
			{
				bstrlist_append(messages, result.msg);
				sv_result_close(&result);
			}
			else
			{
				/* record the new archive checksum. intentionally done outside of the transaction above. */
				check(write_archive_checksum(db, cstr(archivepath), op->expiration_cutoff, false));
			}
		}
	}

cleanup:
	svdb_txn_close(&txn, db);
	svdp_archiver_close(&arch);
	bdestroy(archivepath);
	bdestroy(getstderr);
	return currenterr;
}

check_result svdp_compact_removeoldarchives(svdp_compact_state *op, svdb_connection *db,
	const char *appdatadir, const char *groupname, const char *readydir, bstrlist *messages)
{
	sv_result currenterr = {};
	bstring removedir = bformat("%s%suserdata%s%s%sreadytoremove",
		appdatadir, pathsep, pathsep, groupname, pathsep);

	bstring src = bstring_open();
	bstring dest = bstring_open();
	bstring textfilepath = bstring_open();
	svdb_txn txn = {};
	check_b(os_create_dir(cstr(removedir)), "could not create directory.");
	bool somenotmoved = false;

	/* first, delete rows from the database. must be done in this order for transactional integrity. */
	check(svdb_txn_open(&txn, db));
	for (uint32_t i = 0; i < op->archives_to_remove.length; i++)
	{
		svdp_archive_stats *o = (svdp_archive_stats *)sv_array_at(&op->archives_to_remove, i);
		sv_log_writefmt("removeoldarchives from db, cutoff=%llu, archive=%08x_%08x, rowstodelete=%d",
			op->expiration_cutoff, o->original_collection, o->archive_number, o->old_individual_files.length);
		check(svdb_contents_bulk_delete(db, &o->old_individual_files, 0));

		/* record that we no longer need this archive */
		bassignformat(src, "%s%s%05x_%05x.tar", readydir, pathsep, o->original_collection, o->archive_number);
		check(write_archive_checksum(db, cstr(src), op->expiration_cutoff, true));
	}

	check(svdb_txn_commit(&txn, db));

	/* next, move files on disk to a 'to-be-deleted' directory */
	for (uint32_t i = 0; i < op->archives_to_remove.length; i++)
	{
		svdp_archive_stats *o = (svdp_archive_stats *)sv_array_at(&op->archives_to_remove, i);
		bassignformat(src, "%s%s%05x_%05x.tar",
			readydir, pathsep, o->original_collection, o->archive_number);

		bassignformat(dest, "%s%s%05x_%05x.tar",
			cstr(removedir), pathsep, o->original_collection, o->archive_number);

		sv_log_writefmt("removeoldarchives from disk, cutoff=%llu, archive=%08x_%08x, rowstodelete=%d",
			op->expiration_cutoff, o->original_collection, o->archive_number, o->old_individual_files.length);
		if (!os_file_exists(cstr(src)))
		{
			/* write a little text file to show the user that this archive is safe to delete
			this happens for the case where the user only has the archives uploaded, not locally. */
			bassignformat(textfilepath, "%s%s%05x_%05x.tar.txt",
				cstr(removedir), pathsep, o->original_collection, o->archive_number);

			sv_result ignore = sv_file_writefile(cstr(textfilepath), "", "w");
			sv_log_writefmt("wrote a text file. details=%s", ignore.msg ? cstr(ignore.msg) : "");
			sv_result_close(&ignore);
			somenotmoved = true;
		}
		else
		{
			sv_log_writefmt("moving %s to %s", cstr(src), cstr(dest));
			bool moved = os_tryuntil_move(cstr(src), cstr(dest), true);
			if (!moved)
			{
				bstring errmsg = bformat("Could not move %s to %s", cstr(src), cstr(dest));
				bstrlist_append(messages, errmsg);
				bdestroy(errmsg);
			}
		}
	}

	if (somenotmoved && !op->test_context)
	{
		printf("Some archives weren't found, so we created text files in %s as a reminder of which "
			"archives can be safely deleted.\n", cstr(removedir));
		alertdialog("");
	}
	else if (op->archives_to_remove.length && !op->test_context)
	{
		printf("The archives that are no longer needed were moved to %s.\n", cstr(removedir));
		alertdialog("");
	}

cleanup:
	svdb_txn_close(&txn, db);
	bdestroy(removedir);
	bdestroy(src);
	bdestroy(dest);
	bdestroy(textfilepath);
	return currenterr;
}

void svdp_compact_state_close(svdp_compact_state *self)
{
	if (self)
	{
		sv_2d_array_close(&self->archive_statistics);
		for (uint32_t i = 0; i < self->archives_to_removefiles.length; i++)
		{
			svdp_archive_stats *o = (svdp_archive_stats *)sv_array_at(&self->archives_to_removefiles, i);
			sv_array_close(&o->old_individual_files);
		}
		for (uint32_t i = 0; i < self->archives_to_remove.length; i++)
		{
			svdp_archive_stats *o = (svdp_archive_stats *)sv_array_at(&self->archives_to_remove, i);
			sv_array_close(&o->old_individual_files);
		}

		sv_array_close(&self->archives_to_removefiles);
		sv_array_close(&self->archives_to_remove);
		bdestroy(self->working_dir_archived);
		bdestroy(self->working_dir_unarchived);
		set_self_zero();
	}
}

check_result svdp_compact(const svdp_application *app, const svdp_backupgroup *group, svdb_connection *db)
{
	sv_result currenterr = {};
	svdb_txn txn = {};
	svdp_compact_state op = {};
	op.working_dir_archived = bstrcpy(app->path_temp_archived);
	op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
	op.archive_statistics = sv_2d_array_open(sizeof32u(svdp_archive_stats));
	bstrlist *messages = bstrlist_open();
	bstrlist *choices = bstrlist_open();
	bstring readytoupload = bformat("%s%suserdata%s%s%sreadytoupload", 
		cstr(app->path_app_data), pathsep, pathsep, cstr(group->groupname), pathsep);
	
	bstring msg = bfromcstr("Compact data.\n\nRunning compaction is a good way to reduce the disk space "
		"used by GlacialBackup.\n\n");
	
	if (group->days_to_keep_prev_versions)
	{
		bformata(msg, "Files that you've deleted %d days ago are eligible to be removed from the backup "
			"archives when compaction is run. (This number of days can be changed in 'Edit Group'). ", 
			group->days_to_keep_prev_versions);
	}
	else
	{
		bstr_catstatic(msg, "Files that you've deleted are eligible to be removed from the backup "
			"archives when compaction is run. (A number of days to wait before removal can be enabled "
			"in 'Edit Group'). ");
	}

	bstr_catstatic(msg, "\n\nPlease choose a level of compaction to run:");
	bstrlist_splitcstr(choices, "Quick Compaction. Determine and show a list of .tar archives that are "
		"no longer needed.|Thorough Compaction. Search within each .tar archive and remove data that is "
		"no longer needed.|Back", "|");
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
		check(svdb_contentsiter(db, &op, &svdp_compact_getarchivestats));
		svdp_compact_see_what_to_remove(&op, group->compact_threshold_bytes);
		svdp_compact_archivestats_to_string(&op, false, msg);
		sv_log_writeline(cstr(msg));

		/* ensure db changes committed before moving files on disk */
		check(svdb_txn_commit(&txn, db));

		if (op.archives_to_remove.length + op.archives_to_removefiles.length == 0)
		{
			alertdialog("We did not find data that can be compacted.");
		}
		else
		{
			alertdialog("As listed above, we found some data that can be compacted.");
			check(svdp_compact_removeoldarchives(&op, db, cstr(app->path_app_data), cstr(group->groupname), 
				cstr(readytoupload), messages));

			check(svdp_compact_removeoldfilesfromarchives(&op, app, group, db, cstr(readytoupload), messages));
			if (messages->qty)
			{
				puts("Notes:");
				for (int i = 0; i < messages->qty; i++)
				{
					puts(bstrlist_view(messages, i));
				}
			}

			alertdialog("\nCompact complete.");
		}
	}

cleanup:
	bdestroy(readytoupload);
	bdestroy(msg);
	bstrlist_close(choices);
	bstrlist_close(messages);
	svdb_txn_close(&txn, db);
	svdp_compact_state_close(&op);
	return currenterr;
}

check_result svdp_choosecollectionid(svdb_connection *db, const char *readydir, bstring dbfilechosen, 
	uint64_t *collectionidchosen)
{
	sv_result currenterr = {};
	bstrclear(dbfilechosen);
	*collectionidchosen = 0;
	bstring s = bstring_open();
	bstring dbpath = bstring_open();
	bstrlist *choices = bstrlist_open();
	sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
	check(svdb_collectionsget(db, &arr, true));
	for (uint32_t i = 0; i < arr.length; i++)
	{
		svdb_collections_row *row = (svdb_collections_row *)sv_array_at(&arr, i);
		svdb_collectiontostring(row, true, false, s);
		bstrlist_append(choices, s);
	}

	if (choices->qty == 0)
	{
		alertdialog("There are no previous collections.\n");
		goto cleanup;
	}

	while (true)
	{
		int index = ui_numbered_menu_pick_from_long_list(choices, 10 /* show in groups of 10. */);
		if (index >= 0 && index < cast32u32s(arr.length))
		{
			svdb_collections_row *row = (svdb_collections_row *)sv_array_at(&arr, cast32s32u(index));
			bassignformat(dbpath, "%s%s%05x_index.db", readydir, pathsep, cast64u32u(row->id));
			if (os_file_exists(cstr(dbpath)))
			{
				os_clr_console();
				printf("%s\n\n", bstrlist_view(choices, index));
				bassign(dbfilechosen, dbpath);
				*collectionidchosen = row->id;
				break;
			}
			else
			{
				printf("The file \n%s\n was not found. Please download it and try again, or pick "
					"another collection to restore from.\n", cstr(dbpath));

				alertdialog("");
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

void svdp_restore_state_close(svdp_restore_state *self)
{
	if (self)
	{
		bdestroy(self->working_dir_archived);
		bdestroy(self->working_dir_unarchived);
		bdestroy(self->destdir);
		bdestroy(self->scope);
		bdestroy(self->destfullpath);
		bdestroy(self->tmp_resultstring);
		bstrlist_close(self->messages);
		svdp_archiver_close(&self->archiver);
		set_self_zero();
	}
}

check_result svdp_restore_file(svdp_restore_state *op, const svdb_files_row *in_files_row, 
	const bstring path, const bstring permissions)
{
	sv_result currenterr = {};
	svdb_contents_row contentsrow = {};
	os_exclusivefilehandle handle = {};
	bstring archivepath = bstring_open();
	bstring hashexpected = bstring_open();
	bstring hashgot = bstring_open();
	hash256 hash = {};
	check(svdb_contentsbyid(op->db, in_files_row->contents_id, &contentsrow));
	check_b(contentsrow.id != 0, "did not get correct contents row.");

	/* get src path */
	bassignformat(archivepath, "%s%s%05x_%05x.tar", cstr(op->archiver.path_readytoupload), pathsep,
		contentsrow.original_collection, contentsrow.archivenumber);

	/* get dest path */
	check_b(blength(path) >= 4, "path length is too short %s", cstr(path));
	const char *pathwithoutroot = cstr(path) + (islinux ? 1 : 3);
	bassignformat(op->destfullpath, "%s%s%s", cstr(op->destdir), pathsep, pathwithoutroot);
	check_b(islinux || blength(op->destfullpath) < PATH_MAX, "The length of the resulting path would "
		"have been too long, please choose a shorter destination directory.");
	check(test_hook_call_when_restoring_file(op->test_context, cstr(path), op->destfullpath));
	check(svdp_archiver_restore_from_archive(&op->archiver, cstr(archivepath), contentsrow.id, 
		cstr(op->working_dir_archived), cstr(op->working_dir_unarchived), cstr(op->destfullpath)));
	
	/* apply lmt */
	log_b(os_setmodifiedtime_nearestsecond(cstr(op->destfullpath), in_files_row->last_write_time), "%s %llu",
		cstr(op->destfullpath), castull(in_files_row->last_write_time));

	/* confirm filesize */
	knownfileextension ext = get_file_extension_info(cstr(op->destfullpath), blength(op->destfullpath));
	bool is_separate_audio = op->separate_metadata_enabled && ext != knownfileextension_otherbinary &&
		ext != knownfileextension_none;

	if (!is_separate_audio)
	{
		check_b(contentsrow.contents_length == os_getfilesize(cstr(op->destfullpath)),
			"restoring %08llx to %s expected size %llu but got %llu",
			castull(contentsrow.id), cstr(op->destfullpath),
			castull(contentsrow.contents_length), castull(os_getfilesize(cstr(op->destfullpath))));
	}

	/* confirm hash */
	if (!is_separate_audio || blength(op->archiver.path_audiometadata_binary))
	{
		/* don't compare crc; in the separate_metadata case it is expected not to match */
		uint32_t crc = 0;
		log_b(os_try_set_readable(cstr(op->destfullpath)), "%s", cstr(op->destfullpath));
		check(os_exclusivefilehandle_open(&handle, cstr(op->destfullpath), true, NULL));
		check(hash_of_file(&handle, cast64u32u(op->separate_metadata_enabled),
			ext, cstr(op->archiver.path_audiometadata_binary), &hash, &crc));
		hash256tostr(&contentsrow.hash, hashexpected);
		hash256tostr(&hash, hashgot);
		check_b(s_equal(cstr(hashexpected), cstr(hashgot)),
			"restoring %08llx to %s expected hash %s but got %s",
			castull(contentsrow.id), cstr(op->destfullpath),
			cstr(hashexpected), cstr(hashgot));
	}
	
	/* apply posix permissions */
	if (islinux && op->restore_owners && !op->test_context)
	{
		sv_result result = os_set_permissions(cstr(op->destfullpath), permissions);
		if (result.code)
		{
			bcatcstr(result.msg, cstr(op->destfullpath));
			check(result);
		}
	}

cleanup:
	os_exclusivefilehandle_close(&handle);
	bdestroy(archivepath);
	bdestroy(hashexpected);
	bdestroy(hashgot);
	return currenterr;
}

check_result svdp_restore_cb(void *context, unused_ptr(bool),
	const svdb_files_row *in_files_row, const bstring path, const bstring permissions)
{
	svdp_restore_state *op = (svdp_restore_state *)context;
	if (fnmatch_simple(cstr(op->scope), cstr(path)))
	{
		op->countfilesmatch++;
		op->countfilescomplete++;
		log_b(in_files_row->e_status == sv_filerowstatus_complete &&
			in_files_row->most_recent_collection == op->collectionidwanted,
			"%s, at the original time the backup was taken this file was not available, so we will "
			"recover a valid but previous version. %d %llu %llu",
			cstr(path), in_files_row->e_status, castull(in_files_row->most_recent_collection), 
			castull(op->collectionidwanted));

		sv_result res = svdp_restore_file(op, in_files_row, path, permissions);
		if (res.code)
		{
			bassignformat(op->tmp_resultstring, "%s: %s", cstr(path), cstr(res.msg));
			bstrlist_append(op->messages, op->tmp_resultstring);
			sv_result_close(&res);
			op->countfilescomplete--;
		}
	}
	return OK;
}

check_result svdp_restore(const svdp_application *app, const svdp_backupgroup *group, 
	svdb_connection *db, int optype)
{
	sv_result currenterr = {};
	svdp_restore_state op = {};
	svdb_txn txn = {};
	op.working_dir_archived = bstrcpy(app->path_temp_archived);
	op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
	op.separate_metadata_enabled = group->separate_metadata_enabled;
	op.destdir = bstring_open();
	op.scope = bstring_open();
	op.destfullpath = bstring_open();
	op.tmp_resultstring = bstring_open();
	op.messages = bstrlist_open();
	op.db = db;

	bstring prev_dbfile = bstring_open();
	check(svdp_archiver_open(&op.archiver, cstr(app->path_app_data), cstr(group->groupname),
		0, group->approx_archive_size_bytes, cstr(group->encryption)));
	sv_result result = checkexternaltoolspresent(&op.archiver, group->separate_metadata_enabled);
	if (result.code)
	{
		if (group->separate_metadata_enabled && s_contains(cstr(result.msg), "ffmpeg") && 
			ask_user_yesno("We were not able to find ffmpeg. Please try to install ffmpeg first.\n"
				"Should we continue without installing ffmpeg (files can be restored successfully, "
				"but file content verification will be skipped)? y/n"))
		{
			sv_result_close(&result);
			check(checkexternaltoolspresent(&op.archiver, 0));
			bstr_assignstatic(op.archiver.path_audiometadata_binary, "");
		}

		check(result);
	}

	if (optype == svdp_op_restore_from_past)
	{
		check(svdp_choosecollectionid(db, cstr(op.archiver.path_readytoupload),
			prev_dbfile, &op.collectionidwanted));

		if (op.collectionidwanted && blength(prev_dbfile))
		{
			check(svdb_connection_disconnect(db));
			check(svdb_connection_open(db, cstr(prev_dbfile)));
			check(svdb_txn_open(&txn, db));
		}
		else
		{
			goto cleanup;
		}
	}
	else
	{
		os_clr_console();
		check(svdb_txn_open(&txn, db));
		check(svdb_collectiongetlast(db, &op.collectionidwanted));
		if (!op.collectionidwanted)
		{
			alertdialog("There are no collections to restore.\n");
			goto cleanup;
		}
	}
	
	printf("Which files should we restore? Enter '*' to restore all files, or a pattern like '*.mp3' "
		"or '%s', or 'q' to cancel.\n",
		islinux ? "/directory/path/*" : "C:\\myfiles\\path\\*");
	ask_user_prompt("", true, op.scope);
	if (!blength(op.scope))
	{
		goto cleanup;
	}
	
	fnmatch_checkvalid(cstr(op.scope), op.tmp_resultstring);
	check_b(blength(op.tmp_resultstring) == 0, "This is not a valid pattern. %s", cstr(op.tmp_resultstring));
	os_clr_console();
	ask_user_prompt("Please enter the full path to an output directory. It should be currently empty "
		"and have enough free hard drive space. Or enter 'q' to cancel.", true, op.destdir);
	if (!blength(op.destdir) || !isvalidchosendirectory(app, op.destdir))
	{
		goto cleanup;
	}
	else if (!os_dir_empty(cstr(op.destdir)))
	{
		alertdialog("This directory does not appear to be empty. Please first ensure that the directory "
			"is empty.");
		goto cleanup;
	}
	else if (!os_is_dir_writable(cstr(op.destdir)))
	{
		alertdialog("It doesn't look like we have write access to this directory.");
		goto cleanup;
	}
	
	if (islinux && ask_user_yesno("\nRestore users, groups, and permissions (requires root access)? y/n"))
	{
		op.restore_owners = true;
	}
	
	check_b(os_create_dirs(cstr(op.working_dir_archived)), "could not create %s",
		cstr(op.working_dir_archived));
	check_b(os_create_dirs(cstr(op.working_dir_unarchived)), "could not create %s",
		cstr(op.working_dir_unarchived));

	check(os_tryuntil_deletefiles(cstr(op.working_dir_archived), "*"));
	check(os_tryuntil_deletefiles(cstr(op.working_dir_unarchived), "*"));
	sv_log_writefmt("restoring collection id %llu scope=%s output=%s", op.collectionidwanted, 
		cstr(op.scope), cstr(op.destdir));
	check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
	if (op.messages->qty)
	{
		printf("Remember that if the .tar files have been have been uploaded but are no longer present "
			"locally, the tar files must be moved back to \n%s\n\n",
			cstr(op.archiver.path_readytoupload));
		if (optype == svdp_op_restore_from_past && group->days_to_keep_prev_versions)
		{
			printf("Also, remember that when Compaction is run, some data that is older than %d days "
				"is made unavailable.\n\n",
				group->days_to_keep_prev_versions);
		}
		else if (optype == svdp_op_restore_from_past)
		{
			printf("Also, remember that when Compaction is run, some data from previous versions is "
				"made unavailable.\n\n");
		}

		alertdialog("");
		puts("Notes:");
		for (int i = 0; i < op.messages->qty; i++)
		{
			puts(bstrlist_view(op.messages, i));
		}
	}
	
	/* rollback since we made no db changes (the txn is only used for better performance) */
	check(svdb_txn_rollback(&txn, db));
	check(svdb_connection_disconnect(db));
	printf("Restore complete for %lld/%lld files.", castull(op.countfilescomplete), castull(op.countfilesmatch));
	alertdialog("");
cleanup:
	svdb_txn_close(&txn, db);
	bdestroy(prev_dbfile);
	svdp_restore_state_close(&op);
	return currenterr;
}

/*
hidden features:
	manual-file-list.txt

for future investigation:
	don't encrypt within archives, just encrypt the entire .tar file.
		allows use of .xz format
		erase all 'encryption' code in the product, except for 
		an option to create .encrypted versions alongside the .tar files in readytoupload
		it could use last-modified-times to see if the .encrypted was up to date. (and set the .encrypted lmt 
		for that reason)

	pass command-line flags for automated backups, or at least batch backups.
	need to store more permissions metadata in linux
	use utimensat to set last-modified-times more precisely during restore
	use xz instead of 7z, use something like openssl to encrypt files
	add support for opening a database made on another operating system. the difference is 7z's use-utf16.
	add translation support
	add a better bin-packing algorithm, although the current approach is fine for most files.
*/
