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

#include "user_config.h"
#include "operations.h"

check_result svdp_application_load(svdp_application* self, const char* dir, bool is_low_access)
{
	sv_result currenterr = {};
	bstring filenamedetectinstances = bstring_open(), logdir = bstring_open();
	set_self_zero();
	if (dir && dir[0])
	{
		bassigncstr(self->path_app_data, dir);
		check_b(os_isabspath(dir) && os_dir_exists(dir), "The provided directory %s does not exist.%s",
			is_low_access ? "You may have forgotten to grant Read/Write access for this user account?" : "",
			dir);
	}
	else
	{
		self->path_app_data = os_get_create_appdatadir();
		check_b(blength(self->path_app_data) != 0,
			"Could not start GlacialBackup because we were not able to create a folder in "
			"the appdata directory.\n\n%s\n\n "
			"You can specify a directory by giving the command-line argument glacial_backup -directory "
			"/path/to/dir, as long as "
			"the same directory is provided every time glacial_backup is started.",
			cstr(self->path_app_data));
	}

	self->is_low_access = is_low_access;
	check_b(os_is_dir_writable(cstr(self->path_app_data)), "Could not start GlacialBackup because "
		"we do not have write access in the directory %s. %s",
		cstr(self->path_app_data),
		is_low_access ? "You may have forgotten to grant Read/Write access for this user account?" :
		"You can specify a directory by giving the command-line argument glacial_backup -directory /path/to/dir, "
		"as long as "
		"the same directory is provided every time glacial_backup is started.");

	bassignformat(logdir, "%s%slogs", cstr(self->path_app_data), pathsep);
	check_b(os_create_dir(cstr(logdir)),
		"Could not start GlacialBackup because we could not create or access the folder containing logs.");
	check(sv_log_open(&self->logging, cstr(logdir)));
	sv_log_register_active_logger(&self->logging);

	self->current_group_name_index = -1;
	check_b(is_little_endian(), "Could not start GlacialBackup because it currently only supports "
		"little-endian architectures.");

	int lockcode = 0;
	bassignformat(filenamedetectinstances, "%s%sdetect_instances.txt", cstr(self->path_app_data), pathsep);
	bool other_instance = os_lock_file_to_detect_other_instances(cstr(filenamedetectinstances), &lockcode);
	check_b(!other_instance, "Could not start GlacialBackup because it appears another instance "
		"is currently running.\ncode=%d", lockcode);

	self->backup_group_names = bstrListCreate();
cleanup:
	bdestroy(filenamedetectinstances);
	bdestroy(logdir);
	return currenterr;
}


check_result svdp_application_findgroupnames(svdp_application* self)
{
	sv_result currenterr = {};
	bstrListClear(self->backup_group_names);
	self->current_group_name_index = -1;
	bstring userdatadir = bformat("%s%suserdata", cstr(self->path_app_data), pathsep);
	bstring groupname = bstring_open();
	bstring groupdbpath = bstring_open();
	bstring groupreadytoupload = bformat("%s%suserdata%sreadytoupload", cstr(self->path_app_data), pathsep, pathsep);
	bstring groupreadytouploadpath = bstring_open();
	bstrList* listdirentries = bstrListCreate();
	check_b(os_create_dir(cstr(userdatadir)),
		"Could not start GlacialBackup because we could not create or access the data folder.");

	check(os_listdirs(cstr(userdatadir), listdirentries, true));
	for (int i = 0; i < listdirentries->qty; i++)
	{
		os_get_filename(bstrListViewAt(listdirentries, i), groupname);
		svdp_application_groupdbpathfromname(self, cstr(groupname), groupdbpath);
		if (os_file_exists(cstr(groupdbpath)))
		{
			bstrListAppend(self->backup_group_names, groupname);
		}
		else if (os_dir_exists(cstr(groupreadytoupload)))
		{
			/* restoring from another machine, let's find the latest .db file and copy it into place */
			check(os_findlastfilewithextension(cstr(groupreadytoupload), ".db", groupreadytouploadpath));
			if (blength(groupreadytouploadpath) &&
				os_copy(cstr(groupreadytouploadpath), cstr(groupdbpath), false))
			{
				bstrListAppend(self->backup_group_names, groupname);
			}
		}
	}

cleanup:
	bdestroy(userdatadir);
	bdestroy(groupname);
	bdestroy(groupdbpath);
	bdestroy(groupreadytoupload);
	bdestroy(groupreadytouploadpath);
	bstrListDestroy(listdirentries);
	return currenterr;
}

check_result svdp_application_creategroup(svdp_application* self, unusedint())
{
	sv_result currenterr = {};
	bstring newgroupname = bstring_open();
	ask_user_prompt("Please enter a group name (cannot contain spaces or symbols):", false, newgroupname);
	bstring groupdir = bformat("%s%suserdata%s%s", cstr(self->path_app_data), pathsep, pathsep, cstr(newgroupname));
	if (os_file_or_dir_exists(cstr(groupdir)))
	{
		alertdialog("Group of this name already exists.\n\n");
	}
	else
	{
		check(svdp_application_creategroup_impl(self, cstr(newgroupname), cstr(groupdir)));
	}

cleanup:
	bdestroy(groupdir);
	bdestroy(newgroupname);
	return currenterr;
}

check_result svdp_application_creategroup_impl(svdp_application* self, const char* newgroupname, const char* groupdir)
{
	sv_result currenterr = {};
	bstring path = bstring_open();
	svdb_connection connection = {};
	svdp_backupgroup backupgroup = {};
	check_b(strlen(newgroupname) && s_isalphanum_paren_or_underscore(newgroupname),
		"Contains unsupported character, remember that names cannot contain spaces or symbols");

	check_b(strlen(newgroupname) <= 32, "Group name is too long");
	check_b(os_create_dirs(groupdir), "Could not create group directory.");
	bassignformat(path, "%s%sreadytoupload", groupdir, pathsep);
	check_b(os_create_dirs(cstr(path)), "Could not create readytoupload directory.");
	svdp_application_groupdbpathfromname(self, newgroupname, path);

	/* start a new db file, it's the presence of the db file that will let it show up in the list */
	check(svdb_connection_open(&connection, cstr(path)));

	/* add default settings */
	check(svdp_backupgroup_load(&connection, &backupgroup, newgroupname));
	add_default_exclusion_patterns(&backupgroup);
	check(svdp_backupgroup_persist(&connection, &backupgroup));
	check(svdb_connection_disconnect(&connection));
cleanup:
	bdestroy(path);
	svdb_connection_close(&connection);
	svdp_backupgroup_close(&backupgroup);
	return currenterr;
}

void svdp_application_groupdbpathfromname(const svdp_application* self, const char* groupname, bstring outpath)
{
	bassignformat(outpath, "%s%suserdata%s%s%s%s_index.db", cstr(self->path_app_data), 
		pathsep, pathsep, groupname, pathsep, groupname);
}

void svdp_application_close(svdp_application* self)
{
	bdestroy(self->path_app_data);
	bstrListDestroy(self->backup_group_names);
	sv_log_close(&self->logging);
	sv_log_register_active_logger(NULL);
	set_self_zero();
}

check_result svdp_backupgroup_load(svdb_connection* db, svdp_backupgroup* self, const char* groupname)
{
	sv_result currenterr = {};
	set_self_zero();
	self->groupname = bfromcstr(groupname);
	self->root_directories = bstrListCreate();
	self->exclusion_patterns = bstrListCreate();
	self->encryption = bstring_open();
	check(svdb_propertygetstrlist(db, str_and_len32s("root_directories"), self->root_directories));
	check(svdb_propertygetstrlist(db, str_and_len32s("exclusion_patterns"), self->exclusion_patterns));
	check(svdb_propertygetint(db, str_and_len32s("days_to_keep_prev_versions"), &self->days_to_keep_prev_versions));
	check(svdb_propertygetstr(db, str_and_len32s("encryption"), self->encryption));
	check(svdb_propertygetint(db, str_and_len32s("approx_archive_size_bytes"), &self->approx_archive_size_bytes));
	check(svdb_propertygetint(db, str_and_len32s("compact_threshold_bytes"), &self->compact_threshold_bytes));
	check(svdb_propertygetint(db, str_and_len32s("separate_metadata_enabled"), &self->separate_metadata_enabled));
cleanup:
	return currenterr;
}

check_result svdp_backupgroup_persist(svdb_connection* db, const svdp_backupgroup* self)
{
	sv_result currenterr = {};
	check(svdb_propertysetstrlist(db, str_and_len32s("root_directories"), self->root_directories));
	check(svdb_propertysetstrlist(db, str_and_len32s("exclusion_patterns"), self->exclusion_patterns));
	check(svdb_propertysetint(db, str_and_len32s("days_to_keep_prev_versions"), self->days_to_keep_prev_versions));
	check(svdb_propertysetstr(db, str_and_len32s("encryption"), cstr(self->encryption)));
	check(svdb_propertysetint(db, str_and_len32s("approx_archive_size_bytes"), self->approx_archive_size_bytes));
	check(svdb_propertysetint(db, str_and_len32s("compact_threshold_bytes"), self->compact_threshold_bytes));
	check(svdb_propertysetint(db, str_and_len32s("separate_metadata_enabled"), self->separate_metadata_enabled));
cleanup:
	return currenterr;
}

bool svdp_backupgroup_isincluded(const svdp_backupgroup* self, const char* path)
{
	for (int i = 0; i < self->exclusion_patterns->qty; i++)
	{
		if (fnmatch_simple(bstrListViewAt(self->exclusion_patterns, i), path))
			return false;
	}

	return true;
}

void svdp_backupgroup_close(svdp_backupgroup* self)
{
	bdestroy(self->groupname);
	bstrListDestroy(self->root_directories);
	bstrListDestroy(self->exclusion_patterns);
	bdestroy(self->encryption);
	set_self_zero();
}

check_result loadbackupgroup(const svdp_application* app, svdp_backupgroup* grp, svdb_connection* connection)
{
	sv_result currenterr = {};
	bstring dbpath = bstring_open();
	check_b(app->backup_group_names->qty && app->current_group_name_index >= 0 && 
		app->current_group_name_index < app->backup_group_names->qty, "Not chosen");
	const char* name = bstrListViewAt(app->backup_group_names, app->current_group_name_index);
	svdp_application_groupdbpathfromname(app, name, dbpath);
	check_b(os_file_exists(cstr(dbpath)), "database file does not exist, expected to see at %s", cstr(dbpath));
	check(svdb_connection_open(connection, cstr(dbpath)));
	check(svdp_backupgroup_load(connection, grp, name));
cleanup:
	bdestroy(dbpath);
	return currenterr;
}

check_result svdp_application_viewinfo(svdp_application* app, unusedint())
{
	sv_result currenterr = {};
	svdp_backupgroup group = {};
	svdb_connection connection = {};
	bstring s = bstring_open();
	sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
	check(loadbackupgroup(app, &group, &connection));
	check(svdb_collectionsget(&connection, &arr, true));
	printf("Backup history for group %s:\n", cstr(group.groupname));
	for (uint32_t i = 0; i < arr.length; i++)
	{
		svdb_collections_row* row = (svdb_collections_row*)sv_array_at(&arr, i);
		svdb_collectiontostring(row, true, false, s);
		puts(cstr(s));
	}

	alertdialog("");

cleanup:
	bdestroy(s);
	sv_array_close(&arr);
	svdp_backupgroup_close(&group);
	svdb_connection_close(&connection);
	return currenterr;
}

check_result svdp_application_run_lowpriv(svdp_application* app, unusedint())
{
	sv_result currenterr = {};
	bstring processdir = os_getthisprocessdir();
	if (app->is_low_access)
	{
		alertdialog("We already appear to be running with low access.");
		goto cleanup;
	}

	printf("It's possible to run GlacialBackup with a user account that has restricted privileges. "
		"This can minimize the chance of being affected by any vulnerability in the 7z or ffmpeg tools. (ffmpeg is "
		"not used by default, but you can enable it by turning on the separate-audio-metadata feature.)\n");

#ifdef __linux__
	printf("\n\nStep 1) Create a user account, give it 'Read' and 'Execute' access to the files to be backed up, "
		"and to %s where GlacialBackup is currently installed. "
		"\n\nStep 2) Give it 'Write' access to the directory %s. "
		"\n\nStep 3) Log in as this user, open a terminal, and run\n glacial_backup -directory \"%s\" -low\n",
		cstr(processdir), cstr(app->path_app_data), cstr(app->path_app_data));
#else
	printf("\n\nStep 1) Create a Windows user account, give it 'Read' and 'List folder contents' access to the "
		"files to be backed up, "
		"and to %s where GlacialBackup is currently installed. "
		"\n\nStep 2) Give it 'Write' access to the directory %s. "
		"\n\nStep 3) Log in as this user, open a cmd line, and run\n glacial_backup.exe -directory \"%s\" -low\n",
		cstr(processdir), cstr(app->path_app_data), cstr(app->path_app_data));
	if (ask_user_yesno("Step 3 can be started automatically. Run it now?"))
	{
		check_warn(os_restart_as_other_user(cstr(app->path_app_data)), 
			"Did not start successfully.", continue_on_err);
	}
#endif
	alertdialog("");
cleanup:
	bdestroy(processdir);
	return currenterr;
}

int choose_to_add_or_remove(const char* msg, const char* addmsg, bstrList* list)
{
	int choice = ui_numbered_menu_pick_from_list(msg, list, "%d) Remove %s from list\n", addmsg, "Back");
	if (choice >= 0 && choice < list->qty)
	{
		bstrListRemoveAt(list, choice);
	}
	return choice;
}

bool isvalidchosendirectory(const svdp_application* app, bstring newpath)
{
	if (newpath->data[newpath->slen - 1] == pathsep[0])
	{
		btrunc(newpath, newpath->slen - 1); /* strip trailing slash character */
	}
	if (blength(newpath) < 4)
	{
		printf("Full paths to a directory are expected to be more than 3 characters.\n");
	}
	else if (!os_dir_exists(cstr(newpath)))
	{
		printf("The directory was not found.\n");
	}
	else if (!os_isabspath(cstr(newpath)))
	{
		printf("You must enter a full path.\n");
	}
	else if (os_issubdirof(cstr(newpath), cstr(app->path_app_data)) ||
		os_issubdirof(cstr(app->path_app_data), cstr(newpath)))
	{
		printf("GlacialBackup cannot backup its own working directory at %s.\n",
			cstr(app->path_app_data));
	}
	else
	{
		return true;
	}
	return false;
}

check_result ui_app_menu_edit_group_edit_dirs(svdp_application* app, unusedint())
{
	sv_result currenterr = {};
	svdp_backupgroup backupgroup = {};
	svdb_connection connection = {};
	bstring newpath = bstring_open();
	check(loadbackupgroup(app, &backupgroup, &connection));
	bassignformat(newpath, "Add/remove directory...\n\nRemoving a directory from this list is safe, its files "
		"will simply begin to age out after %d days \n\n",
		backupgroup.days_to_keep_prev_versions);
	const char*msg = backupgroup.days_to_keep_prev_versions ? cstr(newpath) : "Add/remove directory...";
	int choice = choose_to_add_or_remove(msg,
		"Add directory...", backupgroup.root_directories);
	if (choice == backupgroup.root_directories->qty)
	{
		ask_user_prompt("Adding directory: please enter the full path to a directory to add.", false, newpath);
		if (isvalidchosendirectory(app, newpath))
		{
			bool isvalid = true;
			for (int i = 0; isvalid && i < backupgroup.root_directories->qty; i++)
			{
				if (os_issubdirof(bstrListViewAt(backupgroup.root_directories, i), cstr(newpath)))
				{
					printf("Could not add directory because it is a subdirectory of already added %s\n", 
						bstrListViewAt(backupgroup.root_directories, i));
					isvalid = false;
				}
				if (os_issubdirof(cstr(newpath), bstrListViewAt(backupgroup.root_directories, i)))
				{
					printf("This directory can be added, but first please remove its child %s from the list.\n"
						"(It's ok to remove a directory from the list, it will not remove existing archives.)",
						bstrListViewAt(backupgroup.root_directories, i));
					isvalid = false;
				}
			}
			if (isvalid)
			{
				bstrListAppend(backupgroup.root_directories, newpath);
				check(svdp_backupgroup_persist(&connection, &backupgroup));
				printf("Added directory %s.\n", cstr(newpath));
			}
		}
	}

	alertdialog("");
	check(svdb_connection_disconnect(&connection));
cleanup:
	svdp_backupgroup_close(&backupgroup);
	svdb_connection_close(&connection);
	bdestroy(newpath);
	return currenterr;
}

check_result ui_app_menu_edit_group_edit_excl_patterns(svdp_application* app, unusedint())
{
	sv_result currenterr = {};
	svdp_backupgroup backupgroup = {};
	svdb_connection connection = {};
	bstring newpattern = bstring_open();
	bstring validpattern = bstring_open();
	check(loadbackupgroup(app, &backupgroup, &connection));
	alertdialog("An exclusion pattern determines what file types do not need to be backed up. "
		"For example, if the pattern *.tmp is in the list, a file named test.tmp will be skipped. "
		"A pattern like /example/dir1/* means that all files under this directory will be skipped. "
		"A pattern like *abc* means that any file with 'abc' in its path will be skipped. "
		"\n\nA way to see what files will be included is to run Preview Backup under More Actions. "
		"We've added some default exclusion patterns, feel free to edit these or add your own!");
	int choice = choose_to_add_or_remove("Add/remove exclusion patterns", "Add exclusion pattern...", 
		backupgroup.exclusion_patterns);

	if (choice == backupgroup.root_directories->qty)
	{
		ask_user_prompt("Please enter an exclusion pattern:", false, newpattern);
		fnmatch_checkvalid(cstr(newpattern), validpattern);
		if (blength(validpattern))
		{
			printf("%s\n", cstr(validpattern));
		}
		else
		{
			bstrListAppend(backupgroup.exclusion_patterns, newpattern);
			check(svdp_backupgroup_persist(&connection, &backupgroup));
			printf("Added pattern %s.\n", cstr(newpattern));
		}
	}
	alertdialog("");
	check(svdb_connection_disconnect(&connection));
cleanup:
	bdestroy(newpattern);
	bdestroy(validpattern);
	svdp_backupgroup_close(&backupgroup);
	svdb_connection_close(&connection);
	return currenterr;
}

check_result ui_app_menu_edit_group_edit_encryption(svdp_application* app, unusedint())
{
	sv_result currenterr = {};
	svdp_backupgroup backupgroup = {};
	svdb_connection connection = {};
	bstring newkey = bstring_open();
	bstring keyshow = bfromcstr("*");

	check(loadbackupgroup(app, &backupgroup, &connection));
	bFill(keyshow, '*', blength(backupgroup.encryption));
	printf("Current encryption key (characters hidden): %s\n", blength(keyshow) ? cstr(keyshow) : "(none set)");
	ask_user_prompt("Please enter a new key, or 'q' to cancel, or 'no_key' to not have a key.", false, newkey);
	if (!s_equal("q", cstr(newkey)))
	{
		if (s_equal("no_key", cstr(newkey)))
		{
			bassignStatic(newkey, "");
		}

		uint64_t lastcollectionid = 0;
		check(svdb_collectiongetlast(&connection, &lastcollectionid));
		bool have_existing_backups = lastcollectionid > 0;
		printf("Set the key to %s?\n", cstr(newkey));
		if (have_existing_backups && !s_equal("", cstr(backupgroup.encryption)) && !s_equal(cstr(newkey), 
			cstr(backupgroup.encryption)))
		{
			printf("The key cannot be changed when there are existing encrypted backup archives. You can "
				"create a new backup group with this key as a workaround.");
		}
		else if (ask_user_yesno(""))
		{
			bassign(backupgroup.encryption, newkey);
			check(svdp_backupgroup_persist(&connection, &backupgroup));
			alertdialog("Successfully changed key.");
		}
	}
	check(svdb_connection_disconnect(&connection));
cleanup:
	bdestroy(newkey);
	bdestroy(keyshow);
	svdp_backupgroup_close(&backupgroup);
	svdb_connection_close(&connection);
	return currenterr;
}

check_result ui_app_edit_setting(svdp_application* app, int whichsetting)
{
	sv_result currenterr = {};
	svdp_backupgroup backupgroup = {};
	svdb_connection connection = {};
	bstring input = bstring_open();
	const char* prompt = "";
	int valmin = 0, valmax = INT_MAX;
	uint32_t* ptr = NULL;
	uint32_t scalefactor = 1;
	check(loadbackupgroup(app, &backupgroup, &connection));
	switch (whichsetting)
	{
	case svdp_setting_days_to_keep_prev_versions:
		prompt = "Set how long to keep previous file versions.\n\n"
			"By default, GlacialBackup keeps previous versions of a file for 30 days.\n"
			"After this point, the file becomes eligible for removal when Compact is run.\n"
			"A value of 0 is possible, which means that previous versions do not need to be kept.\n"
			"You can enter a number of days and press Enter. The current value is %d days.\n";
		ptr = &backupgroup.days_to_keep_prev_versions;
		break;
	case svdp_setting_separate_metadata_enabled:
		prompt = "Separate audio metadata changes...\n\n"
			"When this feature is enabled, metadata changes in audio files will not cause the entire file "
			"to be added to the archive again. For example, if you frequently change the ID3 tags on your mp3 "
			"music, this can take up a lot of backup space because the entire mp3 is added each time. "
			"We support this feature for mp3, ogg, m4a, and flac files, and use ffmpeg to perform the computation.\n"
			"Enter 1 to enable and 0 to disable. The setting is currently %d.";
		ptr = &backupgroup.separate_metadata_enabled;
		break;
	case svdp_setting_approx_archive_size_bytes:
		prompt = "Set approximate filesize for archive files...\n\n"
			"GlacialBackup uses 7zip for compression, and adds files to separate archives rather than "
			"a single archive file split into volumes. Please enter a number for an approximate size of each "
			"archive file, by default 64 megabytes. The current value is %d Mb.";
		ptr = &backupgroup.approx_archive_size_bytes;
		scalefactor = 1024 * 1024;
		valmin = 1;
		valmax = 500;
		break;
	case svdp_setting_compact_threshold_bytes:
		prompt = "Set strength of data compaction...\n\n"
			"As an example, if this is set to '10', then when Compact is run, for each archive if at least 10 Mb of "
			"old versions can be removed, "
			"then the old data will be removed. In effect, if this number is smaller, more space will likely be "
			"recovered, at the expense "
			"of time taken and further writes to disk. The current value is %d Mb.";
		ptr = &backupgroup.compact_threshold_bytes;
		scalefactor = 1024 * 1024;
		valmin = 1;
		valmax = 500;
		break;
	default:
		break;
	}

	if (ptr)
	{
		printf(prompt, *ptr / scalefactor);
		*ptr = ask_user_int("Please enter a value:", valmin, valmax) * scalefactor;
		check(svdp_backupgroup_persist(&connection, &backupgroup));
	}

	check(svdb_connection_disconnect(&connection));
cleanup:
	bdestroy(input);
	svdp_backupgroup_close(&backupgroup);
	svdb_connection_close(&connection);
	return currenterr;
}

void add_default_exclusion_patterns(svdp_backupgroup* group)
{
	group->approx_archive_size_bytes = 64 * 1024 * 1024;
	group->compact_threshold_bytes = 32 * 1024 * 1024;
	group->days_to_keep_prev_versions = 30;
	group->separate_metadata_enabled = false;

	bstrListAppendCstr(group->exclusion_patterns, "*.tmp");
	bstrListAppendCstr(group->exclusion_patterns, "*.pyc");
#ifdef __linux__
	/* from c++.gitignore */
	bstrListAppendCstr(group->exclusion_patterns, "*.slo");
	bstrListAppendCstr(group->exclusion_patterns, "*.lo");
	bstrListAppendCstr(group->exclusion_patterns, "*.o");
	bstrListAppendCstr(group->exclusion_patterns, "*.obj");
	bstrListAppendCstr(group->exclusion_patterns, "*.gch");
	bstrListAppendCstr(group->exclusion_patterns, "*.pch");
	bstrListAppendCstr(group->exclusion_patterns, "*.so");
	bstrListAppendCstr(group->exclusion_patterns, "*.dylib");
	bstrListAppendCstr(group->exclusion_patterns, "*.lai");
	bstrListAppendCstr(group->exclusion_patterns, "*.la");
	bstrListAppendCstr(group->exclusion_patterns, "*.lib");
	bstrListAppendCstr(group->exclusion_patterns, "*.ko");
	bstrListAppendCstr(group->exclusion_patterns, "*.elf");
	bstrListAppendCstr(group->exclusion_patterns, "*.ilk");
	bstrListAppendCstr(group->exclusion_patterns, "*.map");
	bstrListAppendCstr(group->exclusion_patterns, "*.exp");
	bstrListAppendCstr(group->exclusion_patterns, "*.so");
	bstrListAppendCstr(group->exclusion_patterns, "*.idb");
	bstrListAppendCstr(group->exclusion_patterns, "*.pdb");
#else
	/* from visualstudio.gitignore */
	bstrListAppendCstr(group->exclusion_patterns, "*.ilk");
	bstrListAppendCstr(group->exclusion_patterns, "*.meta");
	bstrListAppendCstr(group->exclusion_patterns, "*.obj");
	bstrListAppendCstr(group->exclusion_patterns, "*.pch");
	bstrListAppendCstr(group->exclusion_patterns, "*.pdb");
	bstrListAppendCstr(group->exclusion_patterns, "*.pgc");
	bstrListAppendCstr(group->exclusion_patterns, "*.pgd");
	bstrListAppendCstr(group->exclusion_patterns, "*.rsp");
	bstrListAppendCstr(group->exclusion_patterns, "*.sbr");
	bstrListAppendCstr(group->exclusion_patterns, "*.tlb");
	bstrListAppendCstr(group->exclusion_patterns, "*.tli");
	bstrListAppendCstr(group->exclusion_patterns, "*.tmp_proj");
	bstrListAppendCstr(group->exclusion_patterns, "*.vspscc");
	bstrListAppendCstr(group->exclusion_patterns, "*.vssscc");
	bstrListAppendCstr(group->exclusion_patterns, "*.pidb");
	bstrListAppendCstr(group->exclusion_patterns, "*.svclog");
	bstrListAppendCstr(group->exclusion_patterns, "*.scc");
	/* from visualstudio.gitignore cache files, profiler */
	bstrListAppendCstr(group->exclusion_patterns, "*.aps");
	bstrListAppendCstr(group->exclusion_patterns, "*.ncb");
	bstrListAppendCstr(group->exclusion_patterns, "*.sdf");
	bstrListAppendCstr(group->exclusion_patterns, "*.cachefile");
	bstrListAppendCstr(group->exclusion_patterns, "*.VC.db");
	bstrListAppendCstr(group->exclusion_patterns, "*.psess");
	bstrListAppendCstr(group->exclusion_patterns, "*.vsp");
	bstrListAppendCstr(group->exclusion_patterns, "*.vspx");
	bstrListAppendCstr(group->exclusion_patterns, "*.sap");
	/*  other msvc */
	bstrListAppendCstr(group->exclusion_patterns, "*.ipch");
	bstrListAppendCstr(group->exclusion_patterns, "*.iobj");
	bstrListAppendCstr(group->exclusion_patterns, "*.ipdb");
	bstrListAppendCstr(group->exclusion_patterns, "*.idb");
	bstrListAppendCstr(group->exclusion_patterns, "*.tlog");
	bstrListAppendCstr(group->exclusion_patterns, "*.exp");
	/* windows */
	bstrListAppendCstr(group->exclusion_patterns, "*\\Thumbs.db");
	bstrListAppendCstr(group->exclusion_patterns, "*\\ehthumbs.db");
#endif
}

void pickgroup(svdp_application* app)
{
	app->current_group_name_index = -1;
	if (app->backup_group_names->qty == 0)
	{
		alertdialog("No backup groups were found. You must first create a backup group.");
	}
	else
	{
		app->current_group_name_index = ui_numbered_menu_pick_from_list("Please choose a group.",
			app->backup_group_names, NULL, NULL, NULL);
	}
}

