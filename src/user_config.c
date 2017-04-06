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

check_result svdp_application_load(svdp_application *self, const char *dir, bool is_low_access)
{
    sv_result currenterr = {};
    bstring filenamedetectinstances = bstring_open(), logdir = bstring_open();
    set_self_zero();
    if (dir && dir[0])
    {
        self->path_app_data = bstring_open();
        bassigncstr(self->path_app_data, dir);
        check_b(os_isabspath(dir) && os_dir_exists(dir), "The directory %s is not accessible. %s", dir,
            is_low_access ? "You may have forgotten to grant Read/Write access to this directory for "
            "this user account." : "");
    }
    else
    {
        self->path_app_data = os_get_create_appdatadir();
        check_b(blength(self->path_app_data) != 0,
            "Could not start GlacialBackup because we were not able to create a folder "
            "in the appdata directory.\n\n%s\n\n "
            "You can specify a directory by giving the command-line argument glacial_backup "
            "-directory /path/to/dir, as long as "
            "the same directory is provided every time glacial_backup is started.",
            cstr(self->path_app_data));
    }

    if (!islinux && is_low_access)
    {
        /* show this to allow the parent process to close handles to logging before we start logging. */
        alertdialog("Welcome to GlacialBackup.");
    }

    self->is_low_access = is_low_access;
    check_b(os_is_dir_writable(cstr(self->path_app_data)), "Could not start GlacialBackup "
        "because we do not have write access in the directory %s. %s",
        cstr(self->path_app_data),
        is_low_access ? "You may have forgotten to grant Read/Write access for this user account." :
        "You can specify a directory by giving the command-line argument glacial_backup "
        "-directory /path/to/dir, as long as "
        "the same directory is provided every time glacial_backup is started.");

    bassign(restrict_write_access, self->path_app_data);
    int lockcode = 0;
    bassignformat(filenamedetectinstances, "%s%sdetect_instances.pid",
        cstr(self->path_app_data), pathsep);
    bool other_instance = os_lock_file_to_detect_other_instances(
        cstr(filenamedetectinstances), &lockcode);
    check_b(!other_instance, "Could not start GlacialBackup because it appears "
        "another instance is currently running.\ncode=%d", lockcode);

    bassignformat(logdir, "%s%slogs", cstr(self->path_app_data), pathsep);
    check_b(os_create_dir(cstr(logdir)),
        "Could not start GlacialBackup because we could not create or access "
        "the folder containing logs.");
    check(sv_log_open(&self->logging, cstr(logdir)));
    sv_log_register_active_logger(&self->logging);
    check_b(is_little_endian(), "Could not start GlacialBackup because it "
        "currently only supports little-endian architectures.");

    self->backup_group_names = bstrlist_open();
    self->path_temp_archived = bformat("%s%stemp%sarchived",
        cstr(self->path_app_data), pathsep, pathsep);
    self->path_temp_unarchived = bformat("%s%stemp%sunarchived",
        cstr(self->path_app_data), pathsep, pathsep);
    check_b(os_create_dirs(cstr(self->path_temp_archived)),
        "failed to create %s", cstr(self->path_temp_archived));
    check_b(os_create_dirs(cstr(self->path_temp_unarchived)),
        "failed to create %s", cstr(self->path_temp_unarchived));

cleanup:
    bdestroy(filenamedetectinstances);
    bdestroy(logdir);
    return currenterr;
}


check_result svdp_application_findgroupnames(svdp_application *self)
{
    sv_result currenterr = {};
    bstrlist_clear(self->backup_group_names);
    bstring path = bstring_open();
    bstring latestdbpath = bstring_open();
    bstring maindbpath = bstring_open();
    bstring groupname = bstring_open();
    bstring userdatadir = bformat("%s%suserdata", cstr(self->path_app_data), pathsep);
    bstrlist *subdirectories = bstrlist_open();
    check_b(os_create_dir(cstr(userdatadir)),
        "Could not start GlacialBackup because we could not create or "
        "access the data folder.");
    check(os_listdirs(cstr(userdatadir), subdirectories, true));
    for (int i = 0; i < subdirectories->qty; i++)
    {
        os_get_filename(bstrlist_view(subdirectories, i), groupname);
        svdp_application_groupdbpathfromname(self, cstr(groupname), maindbpath);
        if (os_file_exists(cstr(maindbpath)))
        {
            bstrlist_append(self->backup_group_names, groupname);
        }
        else
        {
            bassignformat(path, "%s%sreadytoupload",
                bstrlist_view(subdirectories, i), pathsep);

            if (os_dir_exists(cstr(path)))
            {
                /* restoring from another machine, let's find the latest .db file
                and copy it into place */
                check(os_findlastfilewithextension(cstr(path), ".db", latestdbpath));
                if (blength(latestdbpath) &&
                    os_copy(cstr(latestdbpath), cstr(maindbpath), false))
                {
                    bstrlist_append(self->backup_group_names, groupname);
                }
            }
        }
    }
cleanup:
    bdestroy(maindbpath);
    bdestroy(latestdbpath);
    bdestroy(groupname);
    bdestroy(path);
    bdestroy(userdatadir);
    bstrlist_close(subdirectories);
    return currenterr;
}

void svdp_application_groupdbpathfromname(const svdp_application *self,
    const char *groupname, bstring outpath)
{
    bassignformat(outpath, "%s%suserdata%s%s%s%s_index.db",
        cstr(self->path_app_data), pathsep, pathsep, groupname, pathsep, groupname);
}

void svdp_application_close(svdp_application *self)
{
    if (self)
    {
        bdestroy(self->path_app_data);
        bdestroy(self->path_temp_archived);
        bdestroy(self->path_temp_unarchived);
        bstrlist_close(self->backup_group_names);
        sv_log_close(&self->logging);
        sv_log_register_active_logger(NULL);
        set_self_zero();
    }
}

check_result svdp_backupgroup_load(svdb_connection *db, svdp_backupgroup *self, const char *groupname)
{
    sv_result currenterr = {};
    set_self_zero();
    self->groupname = bfromcstr(groupname);
    self->root_directories = bstrlist_open();
    self->exclusion_patterns = bstrlist_open();
    self->encryption = bstring_open();
    check(svdb_propertygetstrlist(db, str_and_len32s("root_directories"),
        self->root_directories));
    check(svdb_propertygetstrlist(db, str_and_len32s("exclusion_patterns"),
        self->exclusion_patterns));
    check(svdb_propertygetint(db, str_and_len32s("days_to_keep_prev_versions"),
        &self->days_to_keep_prev_versions));
    check(svdb_propertygetint(db, str_and_len32s("use_encryption"),
        &self->use_encryption));
    check(svdb_propertygetint(db, str_and_len32s("approx_archive_size_bytes"),
        &self->approx_archive_size_bytes));
    check(svdb_propertygetint(db, str_and_len32s("compact_threshold_bytes"),
        &self->compact_threshold_bytes));
    check(svdb_propertygetint(db, str_and_len32s("separate_metadata_enabled"),
        &self->separate_metadata_enabled));
    check(svdb_propertygetint(db, str_and_len32s("pause_duration_seconds"),
        &self->pause_duration_seconds));

cleanup:
    return currenterr;
}

check_result svdp_backupgroup_persist(svdb_connection *db, const svdp_backupgroup *self)
{
    sv_result currenterr = {};
    check(svdb_propertysetstrlist(db, str_and_len32s("root_directories"),
        self->root_directories));
    check(svdb_propertysetstrlist(db, str_and_len32s("exclusion_patterns"),
        self->exclusion_patterns));
    check(svdb_propertysetint(db, str_and_len32s("days_to_keep_prev_versions"),
        self->days_to_keep_prev_versions));
    check(svdb_propertysetint(db, str_and_len32s("use_encryption"),
        self->use_encryption));
    check(svdb_propertysetint(db, str_and_len32s("approx_archive_size_bytes"),
        self->approx_archive_size_bytes));
    check(svdb_propertysetint(db, str_and_len32s("compact_threshold_bytes"),
        self->compact_threshold_bytes));
    check(svdb_propertysetint(db, str_and_len32s("separate_metadata_enabled"),
        self->separate_metadata_enabled));
    check(svdb_propertysetint(db, str_and_len32s("pause_duration_seconds"),
        self->pause_duration_seconds));

cleanup:
    return currenterr;
}

static void ask_key(const char *prompt, bstring s)
{
    while (true)
    {
        ask_user_prompt(prompt, true, s);
        if (blength(s) && s_isalphanum_paren_or_underscore(cstr(s)))
        {
            break;
        }
        else if (blength(s))
        {
            alertdialog("We're sorry, we currently don't support passwords containing spaces or "
                "special characters.");
        }
        else
        {
            break;
        }
    }
}

check_result svdp_backupgroup_ask_key(svdp_backupgroup *self,
    const svdp_application *app, bool isbackup)
{
    sv_result currenterr = {};
    bstring dir = bstring_open();
    bstring compressed_db = bstring_open();
    svdp_archiver archiver = {};
    bstrclear(self->encryption);
    if (self->use_encryption)
    {
        check(svdp_archiver_open(&archiver, cstr(app->path_app_data),
            cstr(self->groupname), 0, 0, ""));
        check(checkexternaltoolspresent(&archiver, 0));
        bassignformat(compressed_db, "%s%suserdata%s%s%sreadytoupload%s00001_compressed.db",
            cstr(app->path_app_data), pathsep, pathsep, cstr(self->groupname), pathsep, pathsep);

        if (os_file_exists(cstr(compressed_db)))
        {
            while (true)
            {
                ask_key("Please enter the key for this backup group, or 'q' to cancel.",
                    self->encryption);
                check_b(blength(self->encryption),
                    "Chose to cancel while entering an encryption key.");
                sv_result res = svdp_archiver_verify_has_one_item(&archiver, cstr(compressed_db),
                    NULL, cstr(self->encryption));
                if (res.code)
                {
                    if (s_contains(cstr(res.msg), "Wrong password?"))
                    {
                        printf("This does not appear to be the correct password, because it could "
                            "not successfully decrypt the previous archive 00001_compressed.\n");
                    }
                    else
                    {
                        printf("Could not check. Please ensure that a recent version of 7zip is "
                            "installed successfully.\nDetails:%s", cstr(res.msg));
                    }

                    sv_result_close(&res);
                }
                else
                {
                    break;
                }
            }
        }
        else if (isbackup)
        {
            check_b(0, "Could not find the file \n%s\n, and so we are not able to check whether "
                "an entered key is correct or not. Please restore this file first.",
                cstr(compressed_db));
        }
        else
        {
            ask_key("Please enter the key for this backup group, or 'q' to cancel. Warnings will "
                "be shown if the key is incorrect.", self->encryption);
            check_b(blength(self->encryption), "Chose to cancel while entering an encryption key.");
        }
    }

cleanup:
    bdestroy(dir);
    bdestroy(compressed_db);
    svdp_archiver_close(&archiver);
    return currenterr;
}

bool svdp_backupgroup_isincluded(const svdp_backupgroup *self, const char *path, bstrlist *messages)
{
    for (int i = 0; i < self->exclusion_patterns->qty; i++)
    {
        if (fnmatch_simple(bstrlist_view(self->exclusion_patterns, i), path))
        {
            return false;
        }
    }

    bstring fname = bstring_open();
    os_get_filename(path, fname);
    if (s_contains(path, "*") || s_contains(path, "?") || s_startswith(cstr(fname), "-"))
    {
        bstring s = bformat("%s skipped due to unsupported character in path", path);
        bstrlist_append(messages, s);
        bdestroy(s);
        return false;
    }

    bdestroy(fname);
    return true;
}

void svdp_backupgroup_close(svdp_backupgroup *self)
{
    if (self)
    {
        bdestroy(self->groupname);
        bstrlist_close(self->root_directories);
        bstrlist_close(self->exclusion_patterns);
        bdestroy(self->encryption);
        set_self_zero();
    }
}

check_result loadbackupgroup(const svdp_application *app, svdp_backupgroup *grp,
    svdb_connection *connection, const char *optional_groupname)
{
    sv_result currenterr = {};
    bstring dbpath = bstring_open();
    bstring os = bstring_open();
    if (!optional_groupname)
    {
        if (app->backup_group_names->qty == 0)
        {
            alertdialog("No backup groups were found. You must first create a backup group.");
        }
        else
        {
            int chosen = ui_numbered_menu_pick_from_list("Please choose a group.",
                app->backup_group_names, NULL, "(Back)", NULL);
            if (chosen < app->backup_group_names->qty)
            {
                optional_groupname = bstrlist_view(app->backup_group_names, chosen);
            }
        }
    }

    if (optional_groupname)
    {
        svdp_application_groupdbpathfromname(app, optional_groupname, dbpath);
        check_b(os_file_exists(cstr(dbpath)), "database file does not exist, expected to see at %s",
            cstr(dbpath));
        check(svdb_connection_open(connection, cstr(dbpath)));
        check(svdp_backupgroup_load(connection, grp, optional_groupname));
        check(svdb_propertygetstr(connection, str_and_len32s("os"), os));
        check_b(!blength(os) || s_equal(cstr(os), islinux ? "linux" : "win"),
            "This backup group was created on a different operating system; we don't yet support "
            "opening it.");
    }

cleanup:
    bdestroy(dbpath);
    bdestroy(os);
    return currenterr;
}

check_result svdp_application_viewinfo(unused_ptr(const svdp_application),
    const svdp_backupgroup *group, svdb_connection *db)
{
    sv_result currenterr = {};
    bstring s = bstring_open();
    sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
    check(svdb_collectionsget(db, &arr, true));
    printf("Backup history for group %s:\n", cstr(group->groupname));
    printf("Encryption is %s.\n", group->use_encryption ? "enabled" : "disabled");
    for (uint32_t i = 0; i < arr.length; i++)
    {
        svdb_collections_row *row = (svdb_collections_row *)sv_array_at(&arr, i);
        svdb_collectiontostring(row, true, false, s);
        puts(cstr(s));
    }

    alertdialog("");

cleanup:
    bdestroy(s);
    sv_array_close(&arr);
    return currenterr;
}

check_result svdp_application_run_lowpriv(svdp_application *app, unused(int))
{
    bstring processdir = os_getthisprocessdir();
    if (app->is_low_access)
    {
        alertdialog("We already appear to be running with low access.");
    }
    else
    {
        printf("We support running GlacialBackup in a user account that has restricted privileges. "
            "This can be useful to minimize the impact of any security vulnerability in 7z or ffmpeg. "
            "(Note that ffmpeg is "
            "only used if you have enabled the skip-audio-metadata feature.)\n");

#ifdef __linux__
        printf("\n\nStep 1) Create a user account, give it 'Read' and 'Execute' access to the files to "
            "be backed up, and to %s where glacial_backup is currently installed. "
            "\n\nStep 2) Give it 'Write' access to the directory \n%s "
            "\n\nStep 3) Log in as this user, open a terminal, and run\n glacial_backup -directory "
            "\"%s\" -low\n",
            cstr(processdir), cstr(app->path_app_data), cstr(app->path_app_data));
        alertdialog("");
#else
        printf("\n\nStep 1) Create a Windows user account, give it 'Read' and 'List folder contents' "
            "access to the files to be backed up, "
            "and to '%s' where glacial_backup is currently installed. "
            "\n\nStep 2) Give it 'Write' access to the directory \n%s "
            "\n\nStep 3) Log in as the new user, open a cmd line, and run\nglacial_backup.exe "
            "-directory \"%s\" -low\n",
            cstr(processdir), cstr(app->path_app_data), cstr(app->path_app_data));

        if (ask_user_yesno("\nIf steps 1 and 2 are complete, we can run step 3 automatically. \nRun "
            "it now? (y/n)"))
        {
            check_warn(os_restart_as_other_user(cstr(app->path_app_data)),
                "Did not start successfully.", continue_on_err);
        }
#endif
    }

    bdestroy(processdir);
    return OK;
}

bool isvalidchosendirectory(const svdp_application *app, bstring newpath)
{
    if (newpath->data[newpath->slen - 1] == pathsep[0])
    {
        btrunc(newpath, newpath->slen - 1); /* strip trailing slash character */
    }
    if (blength(newpath) < 4)
    {
        printf("Full paths to a directory are expected to be more than 3 characters.\n");
    }
    else if (!os_isabspath(cstr(newpath)))
    {
        printf("You must enter a full path.\n");
    }
    else if (!os_dir_exists(cstr(newpath)))
    {
        printf("The directory was not found.\n");
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

    alertdialog("");
    return false;
}

check_result ui_app_menu_edit_group_edit_dirs_impl(svdp_application *app, svdp_backupgroup *grp,
    svdb_connection *connection)
{
    sv_result currenterr = {};
    bstring newpath = bstring_open();
    bstring message = bstr_fromstatic("Add/remove directory...\n\n");
    uint64_t lastcollectionid = 0;
    check(svdb_collectiongetlast(connection, &lastcollectionid));
    if (grp->days_to_keep_prev_versions && lastcollectionid > 0)
    {
        /* no need to show this message if backup has not been run yet. */
        bformata(message, "If a directory is removed from this list and not re-added, the files "
            "will begin to age out after %d days.\n\n",
            grp->days_to_keep_prev_versions);
    }

    /* if there are no current directories, let's skip straight to the part where we add a directory. */
    int choice = ui_numbered_menu_pick_from_list(cstr(message), grp->root_directories, "%d) Remove "
        "%s from list\n", "Add directory...", "Back");
    if (choice >= 0 && choice < grp->root_directories->qty)
    {
        bstrlist_remove_at(grp->root_directories, choice);
    }
    else if (choice == grp->root_directories->qty)
    {
        ask_user_prompt("Adding directory: please enter the full path to a directory to add, or 'q' "
            "to cancel.", true, newpath);
        if (!blength(newpath))
        {
            goto cleanup;
        }

        if (isvalidchosendirectory(app, newpath))
        {
            bool isvalid = true;
            for (int i = 0; isvalid && i < grp->root_directories->qty; i++)
            {
                if (os_issubdirof(bstrlist_view(grp->root_directories, i), cstr(newpath)))
                {
                    printf("Could not add directory because it is a subdirectory of already added "
                        "%s\n", bstrlist_view(grp->root_directories, i));
                    alertdialog("");
                    isvalid = false;
                }
                if (os_issubdirof(cstr(newpath), bstrlist_view(grp->root_directories, i)))
                {
                    printf("This directory can be added, but first please remove its child %s from "
                        "the list.\n(It's ok to remove a directory from the list, it will not remove "
                        "existing archives.)",
                        bstrlist_view(grp->root_directories, i));
                    alertdialog("");
                    isvalid = false;
                }
            }

            if (isvalid)
            {
                bstrlist_append(grp->root_directories, newpath);
            }
        }
    }
    else
    {
        goto cleanup;
    }

    check(svdp_backupgroup_persist(connection, grp));
    check(ui_app_menu_edit_group_edit_dirs_impl(app, grp, connection));
cleanup:
    bdestroy(message);
    bdestroy(newpath);
    return currenterr;
}

/* in addition to being a backup copy of the first database, also use this to check the encryption key */
check_result svdp_application_creategroup_createinitialdb(
    const svdp_application *self, svdp_backupgroup *group)
{
    sv_result currenterr = {};
    svdp_archiver archiver = {};
    bstring src = bstring_open();
    svdp_application_groupdbpathfromname(self, cstr(group->groupname), src);
    bstring dest = bformat("%s%suserdata%s%s%sreadytoupload%s00001_compressed.db",
        cstr(self->path_app_data), pathsep, pathsep, cstr(group->groupname), pathsep, pathsep);
    check_b(os_tryuntil_remove(cstr(dest)), "could not remove previous %s",
        cstr(dest));
    check(svdp_archiver_open(&archiver, cstr(self->path_app_data), cstr(group->groupname),
        0, 0, cstr(group->encryption)));
    check(checkexternaltoolspresent(&archiver, 0));
    check(svdp_7z_add(&archiver, cstr(dest), cstr(src), false));
    check_b(os_file_exists(cstr(dest)), "%s not created", cstr(dest));

cleanup:
    svdp_archiver_close(&archiver);
    bdestroy(src);
    bdestroy(dest);
    return currenterr;
}

check_result svdp_application_creategroup(svdp_application *self, unused(int))
{
    sv_result currenterr = {};
    svdp_backupgroup backupgroup = {};
    svdb_connection connection = {};
    bstring newgroupname = bstring_open();
    bstring newpath = bstring_open();
    ask_user_prompt("Please enter a group name (cannot contain spaces or symbols), \nor enter "
        "'q' to cancel:", true, newgroupname);
    bstring groupdir = bformat("%s%suserdata%s%s", cstr(self->path_app_data), pathsep, pathsep,
        cstr(newgroupname));
    if (blength(newgroupname))
    {
        check_b(blength(newgroupname) && s_isalphanum_paren_or_underscore(cstr(newgroupname)),
            "Contains unsupported character, remember that names cannot contain spaces or symbols");
        check_b(blength(newgroupname) <= 32, "Group name is too long");
        if (os_dir_exists(cstr(groupdir)))
        {
            alertdialog("Group of this name already exists.\n\n");
        }
        else
        {
            /* let's get a directory from the user before actually creating the group */
            while (true)
            {
                ask_user_prompt("Adding directory: please enter the full path to a directory "
                    "containing the files to be backed up in this group, or 'q' to cancel.",
                    true, newpath);
                if (!blength(newpath))
                {
                    goto cleanup;
                }
                else if (!isvalidchosendirectory(self, newpath))
                {
                    alertdialog("");
                }
                else
                {
                    break;
                }
            }

            bool encrypt = ask_user_yesno("Use encryption for this group? y/n");
            check(svdp_application_creategroup_impl(self, cstr(newgroupname), cstr(groupdir)));
            check(loadbackupgroup(self, &backupgroup, &connection, cstr(newgroupname)));
            bstrlist_append(backupgroup.root_directories, newpath);
            backupgroup.use_encryption = encrypt ? 1 : 0;
            check(svdp_backupgroup_persist(&connection, &backupgroup));

            if (encrypt)
            {
                ask_key("We are now setting an encryption key for this group. Please be careful, "
                    "this key is never stored on disk, and so if the key is forgotten, the archives "
                    "cannot be accessed. Please enter a key, or 'q' to cancel.", backupgroup.encryption);
                check_b(blength(backupgroup.encryption),
                    "Chose to cancel while entering an encryption key.");
            }

            check(svdb_connection_disconnect(&connection));
            check(svdp_application_creategroup_createinitialdb(self, &backupgroup));
            alertdialog("\n\nThe group has been created successfully. You can now use 'Edit group' "
                "to change additional settings or 'Run backup' to begin backing up your files.");
        }
    }

cleanup:
    svdp_backupgroup_close(&backupgroup);
    svdb_connection_close(&connection);
    bdestroy(groupdir);
    bdestroy(newgroupname);
    bdestroy(newpath);
    return currenterr;
}

check_result svdp_application_creategroup_impl(svdp_application *self,
    const char *newgroupname, const char *groupdir)
{
    sv_result currenterr = {};
    bstring path = bstring_open();
    svdb_connection connection = {};
    svdp_backupgroup backupgroup = {};
    check_b(os_create_dirs(groupdir), "Could not create group directory.");
    bassignformat(path, "%s%sreadytoupload", groupdir, pathsep);
    check_b(os_create_dirs(cstr(path)), "Could not create readytoupload directory.");
    svdp_application_groupdbpathfromname(self, newgroupname, path);

    /* start a new db file, it's the presence of the db file that will let it show up in the list */
    check(svdb_connection_open(&connection, cstr(path)));

    /* add default settings */
    check(svdp_backupgroup_load(&connection, &backupgroup, newgroupname));
    add_default_group_settings(&backupgroup);
    check(svdb_propertysetstr(&connection, str_and_len32s("os"), islinux ? "linux" : "win"));
    check(svdp_backupgroup_persist(&connection, &backupgroup));
    check(svdb_connection_disconnect(&connection));
cleanup:
    bdestroy(path);
    svdb_connection_close(&connection);
    svdp_backupgroup_close(&backupgroup);
    return currenterr;
}

check_result ui_app_menu_edit_group_edit_dirs(svdp_application *app, unused(int))
{
    sv_result currenterr = {};
    svdp_backupgroup backupgroup = {};
    svdb_connection connection = {};

    check(loadbackupgroup(app, &backupgroup, &connection, NULL));
    if (connection.db)
    {
        check(ui_app_menu_edit_group_edit_dirs_impl(app, &backupgroup, &connection));
    }

    check(svdb_connection_disconnect(&connection));
cleanup:
    svdp_backupgroup_close(&backupgroup);
    svdb_connection_close(&connection);
    return currenterr;
}

check_result ui_app_menu_edit_group_edit_excl_patterns(svdp_application *app, unused(int))
{
    sv_result currenterr = {};
    svdp_backupgroup backupgroup = {};
    svdb_connection connection = {};
    bstring newpattern = bstring_open();
    bstring reason_pattern_notvalid = bstring_open();

    check(loadbackupgroup(app, &backupgroup, &connection, NULL));
    if (!connection.db)
    {
        goto cleanup;
    }

    const char *msg = "Add/remove exclusion patterns\n\nAn exclusion pattern determines what file "
        "types do not need to be backed up. "
        "For example, if the pattern *.tmp is in the list, a file named test.tmp will be skipped. "
        "A pattern like /example/dir1/* means that all files under this directory will be skipped. "
        "A pattern like *abc *means that any file with 'abc' in its path will be skipped. "
        "\n\nWe've added some default exclusion patterns, feel free add your own!";
    int choice = ui_numbered_menu_pick_from_list(msg, backupgroup.exclusion_patterns, "%d) Stop "
        "excluding files that match %s\n", "Add exclusion pattern...", "Back");
    if (choice >= 0 && choice < backupgroup.exclusion_patterns->qty)
    {
        bstrlist_remove_at(backupgroup.exclusion_patterns, choice);
    }
    else if (choice == backupgroup.exclusion_patterns->qty)
    {
        ask_user_prompt("Please enter an exclusion pattern, or enter 'q' to cancel:", true, newpattern);
        if (blength(newpattern))
        {
            fnmatch_checkvalid(cstr(newpattern), reason_pattern_notvalid);
            if (blength(reason_pattern_notvalid))
            {
                alertdialog(cstr(reason_pattern_notvalid));
            }
            else
            {
                bstrlist_append(backupgroup.exclusion_patterns, newpattern);
            }
        }
    }
    else
    {
        goto cleanup;
    }

    check(svdp_backupgroup_persist(&connection, &backupgroup));
    check(svdb_connection_disconnect(&connection));
    check(ui_app_menu_edit_group_edit_excl_patterns(app, 0));
cleanup:
    bdestroy(newpattern);
    bdestroy(reason_pattern_notvalid);
    svdp_backupgroup_close(&backupgroup);
    svdb_connection_close(&connection);
    return currenterr;
}

check_result ui_app_edit_setting(svdp_application *app, int whichsetting)
{
    sv_result currenterr = {};
    svdp_backupgroup backupgroup = {};
    svdb_connection connection = {};
    bstring input = bstring_open();
    const char *prompt = "";
    int valmin = 0, valmax = 100000;
    uint32_t *ptr = NULL;
    uint32_t scalefactor = 1;
    check(loadbackupgroup(app, &backupgroup, &connection, NULL));
    if (!connection.db)
    {
        goto cleanup;
    }

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
        prompt = "Skip metadata changes...\n\n"
            "When this feature is enabled, changes in audio files that are solely metadata changes "
            "will not cause the entire file "
            "to be archived again. For example, if you frequently change the ID3 tags on your mp3 "
            "music, this can take up a lot of space because the entire mp3 is added each time backups "
            "are run. "
            "With this feature enabled, only changes to the actual audio data are archived. We "
            "support this feature for mp3, ogg, m4a, and flac files, and use ffmpeg to perform "
            "the computation.\n"
            "Enter 1 to enable and 0 to disable. The setting is currently %d.";
        ptr = &backupgroup.separate_metadata_enabled;
        valmax = 1;
        break;
    case svdp_setting_approx_archive_size_bytes:
        prompt = "Set approximate filesize for archive files...\n\n"
            "GlacialBackup uses 7zip for compression, and adds files to separate "
            "archives rather than a single archive file split into volumes. Please enter a number "
            "for an approximate size of each archive file, by default 64 megabytes. The current "
            "value is %d Mb.";
        ptr = &backupgroup.approx_archive_size_bytes;
        scalefactor = 1024 * 1024;
        valmin = 1;
        valmax = 500;
        break;
    case svdp_setting_compact_threshold_bytes:
        prompt = "Set strength of data compaction...\n\n"
            "As an example, if this is set to '10', then when Compact is run, for each archive "
            "if at least 10 Mb of old versions can be removed, "
            "then the old data will be removed. In effect, if this number is smaller, more space "
            "will likely be recovered, at the expense "
            "of time taken and further writes to disk. The current value is %d Mb.";
        ptr = &backupgroup.compact_threshold_bytes;
        scalefactor = 1024 * 1024;
        valmin = 1;
        valmax = 500;
        break;
    case svdp_setting_pause_duration:
        prompt = "Set pause duration when running backups...\n\n"
            "When backing up files, we'll pause every thirty seconds in order to let other "
            "processes have a chance to run. How long should we pause? "
            "The current value is %d seconds.";
        ptr = &backupgroup.pause_duration_seconds;
        valmin = 0;
        valmax = 500;
    default:
        break;
    }

    if (ptr)
    {
        os_clr_console();
        printf(prompt, *ptr / scalefactor);
        *ptr = ask_user_int(" Please enter a value:", valmin, valmax) * scalefactor;
        check(svdp_backupgroup_persist(&connection, &backupgroup));
    }

    check(svdb_connection_disconnect(&connection));
cleanup:
    bdestroy(input);
    svdp_backupgroup_close(&backupgroup);
    svdb_connection_close(&connection);
    return currenterr;
}

check_result write_archive_checksum(svdb_connection *db, const char *filepath,
    uint64_t compaction_cutoff, bool nolongerneeded)
{
    sv_result currenterr = {};
    bstring filename = bstring_open();
    bstring checksum = bstring_open();
    os_get_filename(filepath, filename);
    uint32_t idhigher = 0, idlower = 0;
    int matched = sscanf(cstr(filename), "%5x_%5x.tar", &idhigher, &idlower);
    check_b(matched == 2, "could not get archiveid from filename %s", filepath);
    if (nolongerneeded)
    {
        bstr_assignstatic(checksum, "no_longer_needed");
    }
    else
    {
        check(get_file_checksum_string(filepath, checksum));
    }

    check(svdb_archives_write_checksum(db, make_uint64(idhigher, idlower), (uint64_t)time(NULL),
        compaction_cutoff, cstr(checksum), filepath));
cleanup:
    bdestroy(checksum);
    bdestroy(filename);
    return currenterr;
}

static bool verify_archive_checksum(const char *filename,
    const char *checksum, bstrlist *filenames, bstrlist *checksums)
{
    /* an archive gets compacted and its checksum changes, but we'll still accept an
    older version of an archive as valid because it contains a strict superset of the files needed. */
    for (int i = 0; i < filenames->qty; i++)
    {
        if (s_equal(filename, bstrlist_view(filenames, i)) &&
            s_equal(checksum, bstrlist_view(checksums, i)))
        {
            return true;
        }
    }

    return false;
}

static bool is_archive_needed(const char *filename, bstrlist *filenames, bstrlist *checksums)
{
    for (int i = 0; i < filenames->qty; i++)
    {
        if (s_equal(filename, bstrlist_view(filenames, i)) &&
            s_equal(bstrlist_view(checksums, i), "no_longer_needed"))
        {
            return false;
        }
    }

    return true;
}

check_result svdp_verify_archive_checksums(const svdp_application *app,
    const svdp_backupgroup *group, svdb_connection *db, int *countmismatches)
{
    sv_result currenterr = {};
    bstring path = bstring_open();
    bstring checksum_got = bstring_open();
    bstring filename_shown_previously = bstring_open();
    bstring directory = bformat("%s%suserdata%s%s%sreadytoupload",
        cstr(app->path_app_data), pathsep, pathsep, cstr(group->groupname), pathsep);

    bstrlist *filenames = bstrlist_open();
    bstrlist *checksums = bstrlist_open();
    check(svdb_archives_get_checksums(db, filenames, checksums));
    check_b(filenames->qty == checksums->qty, "should be same number but got %d, %d",
        filenames->qty, checksums->qty);

    if (filenames->qty == 0)
    {
        printf("No archives to verify, backup hasn't been run yet.\n");
    }
    else
    {
        for (int i = 0; i < filenames->qty; i++)
        {
            if (s_equal(cstr(filename_shown_previously), bstrlist_view(filenames, i)) ||
                !is_archive_needed(bstrlist_view(filenames, i), filenames, checksums))
            {
                /* we've already shown this, we don't need to show it again */
                continue;
            }

            bassign(filename_shown_previously, filenames->entry[i]);
            bassignformat(path, "%s%s%s", cstr(directory), pathsep, bstrlist_view(filenames, i));
            if (os_file_exists(cstr(path)))
            {
                check(get_file_checksum_string(cstr(path), checksum_got));
                if (verify_archive_checksum(bstrlist_view(filenames, i), cstr(checksum_got),
                    filenames, checksums))
                {
                    printf("%s: contents verified.\n", bstrlist_view(filenames, i));
                }
                else
                {
                    printf("%s: WARNING, checksum does not match, the file is from a newer version "
                        "or is damaged. Current checksum is %s\n",
                        bstrlist_view(filenames, i), cstr(checksum_got));
                    if (countmismatches)
                    {
                        (*countmismatches)++;
                    }
                }
            }
            else
            {
                printf("%s: file not present on local disk.\n", bstrlist_view(filenames, i));
                if (countmismatches)
                {
                    (*countmismatches)++;
                }
            }
        }
    }

    if (!countmismatches)
    {
        alertdialog("");
    }

cleanup:
    bdestroy(path);
    bdestroy(directory);
    bdestroy(checksum_got);
    bdestroy(filename_shown_previously);
    bstrlist_close(filenames);
    bstrlist_close(checksums);
    return currenterr;
}

void add_default_group_settings(svdp_backupgroup *group)
{
    group->approx_archive_size_bytes = 64 * 1024 * 1024;
    group->compact_threshold_bytes = 32 * 1024 * 1024;
    group->days_to_keep_prev_versions = 30;
    group->pause_duration_seconds = 30;
    group->separate_metadata_enabled = 0;

    bstrlist_appendcstr(group->exclusion_patterns, "*.tmp");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pyc");
#ifdef __linux__
    /* from c++.gitignore */
    bstrlist_appendcstr(group->exclusion_patterns, "*.slo");
    bstrlist_appendcstr(group->exclusion_patterns, "*.lo");
    bstrlist_appendcstr(group->exclusion_patterns, "*.o");
    bstrlist_appendcstr(group->exclusion_patterns, "*.obj");
    bstrlist_appendcstr(group->exclusion_patterns, "*.gch");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pch");
    bstrlist_appendcstr(group->exclusion_patterns, "*.so");
    bstrlist_appendcstr(group->exclusion_patterns, "*.dylib");
    bstrlist_appendcstr(group->exclusion_patterns, "*.lai");
    bstrlist_appendcstr(group->exclusion_patterns, "*.la");
    bstrlist_appendcstr(group->exclusion_patterns, "*.lib");
    bstrlist_appendcstr(group->exclusion_patterns, "*.ko");
    bstrlist_appendcstr(group->exclusion_patterns, "*.elf");
    bstrlist_appendcstr(group->exclusion_patterns, "*.ilk");
    bstrlist_appendcstr(group->exclusion_patterns, "*.map");
    bstrlist_appendcstr(group->exclusion_patterns, "*.exp");
    bstrlist_appendcstr(group->exclusion_patterns, "*.so");
    bstrlist_appendcstr(group->exclusion_patterns, "*.idb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pdb");
#else
    /* from visualstudio.gitignore */
    bstrlist_appendcstr(group->exclusion_patterns, "*.ilk");
    bstrlist_appendcstr(group->exclusion_patterns, "*.meta");
    bstrlist_appendcstr(group->exclusion_patterns, "*.obj");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pch");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pdb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pgc");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pgd");
    bstrlist_appendcstr(group->exclusion_patterns, "*.rsp");
    bstrlist_appendcstr(group->exclusion_patterns, "*.sbr");
    bstrlist_appendcstr(group->exclusion_patterns, "*.tlb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.tli");
    bstrlist_appendcstr(group->exclusion_patterns, "*.tmp_proj");
    bstrlist_appendcstr(group->exclusion_patterns, "*.vspscc");
    bstrlist_appendcstr(group->exclusion_patterns, "*.vssscc");
    bstrlist_appendcstr(group->exclusion_patterns, "*.pidb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.svclog");
    bstrlist_appendcstr(group->exclusion_patterns, "*.scc");
    /* from visualstudio.gitignore cache files, profiler */
    bstrlist_appendcstr(group->exclusion_patterns, "*.aps");
    bstrlist_appendcstr(group->exclusion_patterns, "*.ncb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.sdf");
    bstrlist_appendcstr(group->exclusion_patterns, "*.cachefile");
    bstrlist_appendcstr(group->exclusion_patterns, "*.VC.db");
    bstrlist_appendcstr(group->exclusion_patterns, "*.psess");
    bstrlist_appendcstr(group->exclusion_patterns, "*.vsp");
    bstrlist_appendcstr(group->exclusion_patterns, "*.vspx");
    bstrlist_appendcstr(group->exclusion_patterns, "*.sap");
    /*  other msvc */
    bstrlist_appendcstr(group->exclusion_patterns, "*.ipch");
    bstrlist_appendcstr(group->exclusion_patterns, "*.iobj");
    bstrlist_appendcstr(group->exclusion_patterns, "*.ipdb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.idb");
    bstrlist_appendcstr(group->exclusion_patterns, "*.tlog");
    bstrlist_appendcstr(group->exclusion_patterns, "*.exp");
    /* windows */
    bstrlist_appendcstr(group->exclusion_patterns, "*\\Thumbs.db");
    bstrlist_appendcstr(group->exclusion_patterns, "*\\ehthumbs.db");
#endif
}
