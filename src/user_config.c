/*
user_config.c

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

check_result sv_app_load(sv_app *self, const char *dir, bool low_access)
{
    sv_result currenterr = {};
    bstring filenamedetectinstances = bstring_open();
    bstring logdir = bstring_open();
    set_self_zero();
    if (dir && dir[0])
    {
        self->path_app_data = bstring_open();
        bassigncstr(self->path_app_data, dir);
        check_b(os_isabspath(dir) && os_dir_exists(dir),
            "The directory %s is not accessible. %s", dir,
            low_access ? "You may have forgotten to grant Read/Write access "
                         "to this directory for this user account."
                       : "");
    }
    else
    {
        self->path_app_data = os_get_create_appdatadir();
        check_b(blength(self->path_app_data) != 0,
            "Could not start GlacialBackup because we were not able to "
            "create a folder in the appdata directory.\n\n%s\n\n You can "
            "specify a directory by giving the command-line argument "
            "glacial_backup -directory /path/to/dir, as long as the same "
            "directory is provided every time glacial_backup is started.",
            cstr(self->path_app_data));
    }

    if (low_access)
    {
        /* show this to allow the parent process to close
        its handles to logging before we start logging. */
        alert("Welcome to GlacialBackup.");
    }

    self->low_access = low_access;
    check_b(os_is_dir_writable(cstr(self->path_app_data)),
        "Could not start "
        "GlacialBackup because we do not have write access in the directory "
        "%s. %s",
        cstr(self->path_app_data),
        low_access
            ? "You may have forgotten to grant Read/Write access for "
              "this user account."
            : "You can specify a directory by giving the "
              "command-line argument glacial_backup -directory /path/to/dir, as "
              "long as the same directory is provided every time glacial_backup "
              "is started.");

    bassign(restrict_write_access, self->path_app_data);
    int lockcode = 0;
    bsetfmt(filenamedetectinstances, "%s%sdetect_instances.pid",
        cstr(self->path_app_data), pathsep);
    bool other_instance =
        os_detect_other_instances(cstr(filenamedetectinstances), &lockcode);
    check_b(!other_instance,
        "Could not start GlacialBackup because it "
        "appears another instance is currently running.\ncode=%d",
        lockcode);

    bsetfmt(logdir, "%s%slogs", cstr(self->path_app_data), pathsep);
    check_b(os_create_dir(cstr(logdir)),
        "Could not start GlacialBackup because we could not create or access "
        "the folder containing logs.");
    check(sv_log_open(&self->log, cstr(logdir)));
    sv_log_register_active_logger(&self->log);
    check_b(is_little_endian(),
        "Could not start GlacialBackup because it "
        "currently only supports little-endian architectures.");

    self->grp_names = bstrlist_open();
    self->path_temp_archived = bformat(
        "%s%stemp%sarchived", cstr(self->path_app_data), pathsep, pathsep);
    self->path_temp_unarchived = bformat(
        "%s%stemp%sunarchived", cstr(self->path_app_data), pathsep, pathsep);
    check_b(os_create_dirs(cstr(self->path_temp_archived)),
        "failed to create %s", cstr(self->path_temp_archived));
    check_b(os_create_dirs(cstr(self->path_temp_unarchived)),
        "failed to create %s", cstr(self->path_temp_unarchived));

cleanup:
    bdestroy(filenamedetectinstances);
    bdestroy(logdir);
    return currenterr;
}

check_result sv_app_findgroupnames(sv_app *self)
{
    sv_result currenterr = {};
    bstrlist_clear(self->grp_names);
    bstring path = bstring_open();
    bstring latestdbpath = bstring_open();
    bstring dbpath = bstring_open();
    bstring grpname = bstring_open();
    bstrlist *dirs = bstrlist_open();
    bstring userdatadir =
        bformat("%s%suserdata", cstr(self->path_app_data), pathsep);
    check_b(os_create_dir(cstr(userdatadir)),
        "Could not start GlacialBackup because we could not create or "
        "access the data folder. %s",
        cstr(userdatadir));

    check(os_listdirs(cstr(userdatadir), dirs, true));
    for (int i = 0; i < dirs->qty; i++)
    {
        os_get_filename(blist_view(dirs, i), grpname);
        sv_app_groupdbpathfromname(self, cstr(grpname), dbpath);
        if (os_file_exists(cstr(dbpath)))
        {
            bstrlist_append(self->grp_names, grpname);
        }
        else
        {
            bsetfmt(path, "%s%sreadytoupload", blist_view(dirs, i), pathsep);
            if (os_dir_exists(cstr(path)))
            {
                /* restoring from another machine, let's find the latest
                .db file and copy it into place */
                check(os_findlastfilewithextension(
                    cstr(path), ".db", latestdbpath));

                if (blength(latestdbpath) &&
                    os_copy(cstr(latestdbpath), cstr(dbpath), false))
                {
                    bstrlist_append(self->grp_names, grpname);
                }
            }
        }
    }

cleanup:
    bdestroy(dbpath);
    bdestroy(latestdbpath);
    bdestroy(grpname);
    bdestroy(path);
    bdestroy(userdatadir);
    bstrlist_close(dirs);
    return currenterr;
}

void sv_app_groupdbpathfromname(
    const sv_app *self, const char *grpname, bstring path)
{
    bsetfmt(path, "%s%suserdata%s%s%s%s_index.db", cstr(self->path_app_data),
        pathsep, pathsep, grpname, pathsep, grpname);
}

void sv_app_close(sv_app *self)
{
    if (self)
    {
        bdestroy(self->path_app_data);
        bdestroy(self->path_temp_archived);
        bdestroy(self->path_temp_unarchived);
        bstrlist_close(self->grp_names);
        sv_log_close(&self->log);
        sv_log_register_active_logger(NULL);
        set_self_zero();
    }
}

check_result sv_grp_load(svdb_db *db, sv_group *self, const char *grpname)
{
    sv_result currenterr = {};
    set_self_zero();
    self->grpname = bfromcstr(grpname);
    self->root_directories = bstrlist_open();
    self->exclusion_patterns = bstrlist_open();
    check(
        svdb_getlist(db, s_and_len("root_directories"), self->root_directories));
    check(svdb_getlist(
        db, s_and_len("exclusion_patterns"), self->exclusion_patterns));
    check(svdb_getint(db, s_and_len("days_to_keep_prev_versions"),
        &self->days_to_keep_prev_versions));
    check(svdb_getint(db, s_and_len("approx_archive_size_bytes"),
        &self->approx_archive_size_bytes));
    check(svdb_getint(db, s_and_len("compact_threshold_bytes"),
        &self->compact_threshold_bytes));
    check(svdb_getint(
        db, s_and_len("separate_metadata"), &self->separate_metadata));
    check(svdb_getint(
        db, s_and_len("pause_duration_seconds"), &self->pause_duration_seconds));

cleanup:
    return currenterr;
}

check_result sv_grp_persist(svdb_db *db, const sv_group *self)
{
    sv_result currenterr = {};
    check(
        svdb_setlist(db, s_and_len("root_directories"), self->root_directories));
    check(svdb_setlist(
        db, s_and_len("exclusion_patterns"), self->exclusion_patterns));
    check(svdb_setint(db, s_and_len("days_to_keep_prev_versions"),
        self->days_to_keep_prev_versions));
    check(svdb_setint(db, s_and_len("approx_archive_size_bytes"),
        self->approx_archive_size_bytes));
    check(svdb_setint(db, s_and_len("compact_threshold_bytes"),
        self->compact_threshold_bytes));
    check(svdb_setint(
        db, s_and_len("separate_metadata"), self->separate_metadata));
    check(svdb_setint(
        db, s_and_len("pause_duration_seconds"), self->pause_duration_seconds));

cleanup:
    return currenterr;
}

bool sv_grp_isincluded(
    const sv_group *self, const char *path, unused_ptr(bstrlist))
{
    for (int i = 0; i < self->exclusion_patterns->qty; i++)
    {
        if (fnmatch_simple(blist_view(self->exclusion_patterns, i), path))
        {
            return false;
        }
    }

    return true;
}

void sv_grp_close(sv_group *self)
{
    if (self)
    {
        bdestroy(self->grpname);
        bstrlist_close(self->root_directories);
        bstrlist_close(self->exclusion_patterns);
        set_self_zero();
    }
}

check_result load_backup_group(const sv_app *app, sv_group *grp, svdb_db *db,
    const char *optional_groupname)
{
    sv_result currenterr = {};
    bstring dbpath = bstring_open();
    if (!optional_groupname)
    {
        if (app->grp_names->qty == 0)
        {
            alert("No backup groups were found. You must first create a "
                  "backup group.");
        }
        else
        {
            int chosen = menu_choose(
                "Please choose a group.", app->grp_names, NULL, "(Back)", NULL);
            if (chosen < app->grp_names->qty)
            {
                optional_groupname = blist_view(app->grp_names, chosen);
            }
        }
    }

    if (optional_groupname)
    {
        sv_app_groupdbpathfromname(app, optional_groupname, dbpath);
        check_b(os_file_exists(cstr(dbpath)),
            "database file does not "
            "exist, expected to see at %s",
            cstr(dbpath));
        check(svdb_connect(db, cstr(dbpath)));
        check(sv_grp_load(db, grp, optional_groupname));
    }

cleanup:
    bdestroy(dbpath);
    return currenterr;
}

check_result sv_app_viewinfo(const sv_group *grp, svdb_db *db)
{
    sv_result currenterr = {};
    bstring s = bstring_open();
    sv_array a = sv_array_open(sizeof32u(sv_collection_row), 0);
    check(svdb_collectionsget(db, &a, true));
    printf("Backup history for group %s:\n", cstr(grp->grpname));
    for (uint32_t i = 0; i < a.length; i++)
    {
        sv_collection_row *row = (sv_collection_row *)sv_array_at(&a, i);
        svdb_collectiontostring(row, true /* verbose */, false, s);
        puts(cstr(s));
    }

    alert("");

cleanup:
    bdestroy(s);
    sv_array_close(&a);
    return currenterr;
}

check_result sv_app_run_lowpriv(sv_app *app, unused(int))
{
    bstring processdir = os_getthisprocessdir();
    if (app->low_access)
    {
        alert("We already appear to be running with low access.");
    }
    else
    {
        printf("We support running GlacialBackup in a user account that "
               "has restricted privileges. This can be useful to minimize the "
               "impact of any security vulnerability in xz or ffmpeg. (ffmpeg "
               "is only used if you have enabled the skip-audio-metadata "
               "feature.)\n");

#ifdef __linux__
        printf("\n\nStep 1) Create a user account, give it 'Read' and "
               "'Execute' access to the files to be backed up, and to %s where "
               "glacial_backup is currently installed. \n\nStep 2) Give it "
               "'Write' access to the directory \n%s \n\nStep 3) Log in as "
               "this user, open a terminal, and run\n glacial_backup -directory "
               "\"%s\" -low\n",
            cstr(processdir), cstr(app->path_app_data),
            cstr(app->path_app_data));
        alert("");
#else
        printf("\n\nStep 1) Create a Windows user account, give it 'Read' "
               "and 'List folder contents' access to the files to be backed up, "
               "and to '%s' where glacial_backup is currently installed. "
               "\n\nStep 2) Give it 'Write' access to the directory \n%s "
               "\n\nStep 3) Log in as the new user, open a cmd line, and run\n"
               "glacial_backup.exe -directory \"%s\" -low\n",
            cstr(processdir), cstr(app->path_app_data),
            cstr(app->path_app_data));

        if (ask_user("\nIf steps 1 and 2 are complete, we can run step 3 "
                     "automatically. \nRun it now? (y/n)"))
        {
            check_warn(os_restart_as_other_user(cstr(app->path_app_data)),
                "Did not start successfully.", continue_on_err);
        }
#endif
    }

    bdestroy(processdir);
    return OK;
}

bool check_user_typed_dir(const sv_app *app, bstring newpath)
{
    if (newpath->data[newpath->slen - 1] == pathsep[0])
    {
        /* strip trailing slash character */
        btrunc(newpath, newpath->slen - 1);
    }
    if (blength(newpath) < 4)
    {
        printf("Full paths to a directory are expected to be more "
               "than 3 characters.\n");
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
        printf("GlacialBackup cannot backup its own working directory "
               "at %s.\n",
            cstr(app->path_app_data));
    }
    else
    {
        return true;
    }

    alert("");
    return false;
}

check_result app_menu_edit_grp_dirs_impl(sv_app *app, sv_group *grp, svdb_db *db)
{
    sv_result currenterr = {};
    bstring newpath = bstring_open();
    bstring message = bstr_fromstatic("Add/remove directory...\n\n");
    uint64_t lastcollectionid = 0;
    check(svdb_collectiongetlast(db, &lastcollectionid));
    if (grp->days_to_keep_prev_versions && lastcollectionid > 0)
    {
        /* no need to show this message if backup has not been run yet. */
        bformata(message,
            "If a directory is removed from this list and not "
            "re-added, the files will begin to age out after %d days.\n\n",
            grp->days_to_keep_prev_versions);
    }

    /* if there are no current directories, let's skip straight to the part
    where we add a directory. */
    int choice = menu_choose(cstr(message), grp->root_directories,
        "%d) Remove %s from list\n", "Add directory...", "Back");

    if (choice >= 0 && choice < grp->root_directories->qty)
    {
        bstrlist_remove_at(grp->root_directories, choice);
    }
    else if (choice == grp->root_directories->qty)
    {
        ask_user_str("Adding directory: please enter the full path to a "
                     "directory to add, or 'q' to cancel.",
            true, newpath);
        if (!blength(newpath))
        {
            goto cleanup;
        }

        if (check_user_typed_dir(app, newpath))
        {
            bool valid = true;
            for (int i = 0; valid && i < grp->root_directories->qty; i++)
            {
                if (os_issubdirof(
                        blist_view(grp->root_directories, i), cstr(newpath)))
                {
                    printf("Could not add directory because it is a "
                           "subdirectory of already added %s\n",
                        blist_view(grp->root_directories, i));
                    alert("");
                    valid = false;
                }

                if (os_issubdirof(
                        cstr(newpath), blist_view(grp->root_directories, i)))
                {
                    printf(
                        "This directory can be added, but first please "
                        "remove its child %s from the list.\n (It's ok to "
                        "remove a directory from the list, it will not remove "
                        "existing archives.)",
                        blist_view(grp->root_directories, i));
                    alert("");
                    valid = false;
                }
            }

            if (valid)
            {
                bstrlist_append(grp->root_directories, newpath);
            }
        }
    }
    else
    {
        goto cleanup;
    }

    check(sv_grp_persist(db, grp));
    check(app_menu_edit_grp_dirs_impl(app, grp, db));

cleanup:
    bdestroy(message);
    bdestroy(newpath);
    return currenterr;
}

check_result sv_app_creategroup(sv_app *self, unused(int))
{
    sv_result currenterr = {};
    sv_group grp = {};
    svdb_db db = {};
    bstring newgrpname = bstring_open();
    bstring newpath = bstring_open();
    ask_user_str("Please enter a group name (cannot contain spaces or "
                 "symbols), \nor enter 'q' to cancel:",
        true, newgrpname);
    bstring dir = bformat("%s%suserdata%s%s", cstr(self->path_app_data), pathsep,
        pathsep, cstr(newgrpname));

    if (blength(newgrpname))
    {
        check_b(blength(newgrpname) &&
                s_isalphanum_paren_or_underscore(cstr(newgrpname)),
            "Contains unsupported character, remember that names cannot "
            "contain spaces or symbols");
        check_b(blength(newgrpname) <= 32, "Group name is too long");

        if (os_dir_exists(cstr(dir)))
        {
            alert("Group of this name already exists.\n\n");
        }
        else
        {
            /* let's get a directory from the user before actually
            creating the group */
            while (true)
            {
                ask_user_str(
                    "Adding directory: please enter the full path "
                    "to a directory containing the files to be backed up in "
                    "this group, or 'q' to cancel.",
                    true, newpath);

                if (!blength(newpath))
                {
                    goto cleanup;
                }
                else if (!check_user_typed_dir(self, newpath))
                {
                    alert("");
                }
                else
                {
                    break;
                }
            }

            check(sv_app_creategroup_impl(self, cstr(newgrpname), cstr(dir)));
            check(load_backup_group(self, &grp, &db, cstr(newgrpname)));
            check_b(db.db, "failed to load group");
            bstrlist_append(grp.root_directories, newpath);
            check(sv_grp_persist(&db, &grp));
            check(svdb_disconnect(&db));
            alert("\n\nThe group has been created successfully. You can "
                  "now use 'Edit group' to change additional settings or "
                  "'Run backup' to begin backing up your files.");
        }
    }

cleanup:
    sv_grp_close(&grp);
    svdb_close(&db);
    bdestroy(dir);
    bdestroy(newgrpname);
    bdestroy(newpath);
    return currenterr;
}

check_result sv_app_creategroup_impl(
    sv_app *self, const char *grpname, const char *grpdir)
{
    sv_result currenterr = {};
    bstring path = bstring_open();
    svdb_db db = {};
    sv_group grp = {};
    bsetfmt(path, "%s%sreadytoupload", grpdir, pathsep);
    check_b(
        os_create_dirs(grpdir), "Could not create group directory. %s", grpdir);
    check_b(os_create_dirs(cstr(path)),
        "Could not create "
        "readytoupload directory. %s",
        cstr(path));
    sv_app_groupdbpathfromname(self, grpname, path);

    /* start a new db file and add default settings */
    check(svdb_connect(&db, cstr(path)));
    check(sv_grp_load(&db, &grp, grpname));
    add_default_group_settings(&grp);
    check(svdb_setstr(&db, s_and_len("os"), islinux ? "linux" : "win"));
    check(sv_grp_persist(&db, &grp));
    check(svdb_disconnect(&db));

cleanup:
    bdestroy(path);
    svdb_close(&db);
    sv_grp_close(&grp);
    return currenterr;
}

check_result app_menu_edit_grp_dirs(sv_app *app, unused(int))
{
    sv_result currenterr = {};
    sv_group grp = {};
    svdb_db db = {};

    check(load_backup_group(app, &grp, &db, NULL));
    if (db.db)
    {
        check(app_menu_edit_grp_dirs_impl(app, &grp, &db));
    }

    check(svdb_disconnect(&db));

cleanup:
    sv_grp_close(&grp);
    svdb_close(&db);
    return currenterr;
}

check_result app_menu_edit_grp_patterns(sv_app *app, unused(int))
{
    sv_result currenterr = {};
    sv_group grp = {};
    svdb_db db = {};
    bstring newpattern = bstring_open();
    bstring reason_pattern_notvalid = bstring_open();

    check(load_backup_group(app, &grp, &db, NULL));
    if (!db.db)
    {
        goto cleanup;
    }

    const char *msg =
        "Add/remove exclusion patterns\n\nAn exclusion pattern "
        "determines what file types do not need to be backed up. For example, "
        "if the pattern *.tmp is in the list, a file named test.tmp will be "
        "skipped. A pattern like /example/dir1/* means that all files under "
        "this directory will be skipped. A pattern like *abc *means that any "
        "file with 'abc' in its path will be skipped. \n\nWe've added some "
        "default exclusion patterns, feel free add your own!";
    int choice = menu_choose(msg, grp.exclusion_patterns,
        "%d) Stop excluding files that match %s\n", "Add exclusion pattern...",
        "Back");

    if (choice >= 0 && choice < grp.exclusion_patterns->qty)
    {
        bstrlist_remove_at(grp.exclusion_patterns, choice);
    }
    else if (choice == grp.exclusion_patterns->qty)
    {
        ask_user_str("Please enter an exclusion pattern, or enter "
                     "'q' to cancel:",
            true, newpattern);

        if (blength(newpattern))
        {
            fnmatch_isvalid(cstr(newpattern), reason_pattern_notvalid);
            if (blength(reason_pattern_notvalid))
            {
                alert(cstr(reason_pattern_notvalid));
            }
            else
            {
                bstrlist_append(grp.exclusion_patterns, newpattern);
            }
        }
    }
    else
    {
        goto cleanup;
    }

    check(sv_grp_persist(&db, &grp));
    check(svdb_disconnect(&db));
    check(app_menu_edit_grp_patterns(app, 0));

cleanup:
    bdestroy(newpattern);
    bdestroy(reason_pattern_notvalid);
    sv_grp_close(&grp);
    svdb_close(&db);
    return currenterr;
}

check_result app_edit_setting(sv_app *app, int setting)
{
    sv_result currenterr = {};
    sv_group grp = {};
    svdb_db db = {};
    bstring input = bstring_open();
    const char *prompt = "";
    int valmin = 0, valmax = 100000;
    uint32_t *ptr = NULL;
    uint32_t scalefactor = 1;
    check(load_backup_group(app, &grp, &db, NULL));
    if (!db.db)
    {
        goto cleanup;
    }

    switch (setting)
    {
    case sv_set_days_to_keep_prev_versions:
        prompt = "Set how long to keep previous file versions.\n\n"
                 "By default, GlacialBackup keeps previous versions of a file "
                 "for 30 days.\nAfter this point, the file becomes eligible for "
                 "removal when Compact is run.\nA value of 0 is possible, which "
                 "means that previous versions do not need to be kept.\nYou can "
                 "enter a number of days and press Enter. The current value is "
                 "%d days.\n";
        ptr = &grp.days_to_keep_prev_versions;
        break;
    case sv_set_separate_metadata_enabled:
        prompt = "Skip metadata changes...\n\n"
                 "When this feature is enabled, changes in audio files that are "
                 "solely metadata changes will not cause the entire file to be "
                 "archived again. For example, if you frequently change the ID3 "
                 "tags on your mp3 music, this can take up a lot of space "
                 "because "
                 "the entire mp3 is added each time backups are run. With this "
                 "feature enabled, only changes to the actual audio data are "
                 "archived. We support this feature for mp3, ogg, m4a, and flac "
                 "files, and use ffmpeg to perform the computation.\nEnter 1 to "
                 "enable and 0 to disable. The setting is currently %d.";
        ptr = &grp.separate_metadata;
        valmax = 1;
        break;
    case sv_set_approx_archive_size_bytes:
        prompt = "Set approximate filesize for archive files...\n\n"
                 "GlacialBackup adds files to separate archives rather than "
                 "a single archive file split into volumes. Please enter a "
                 "number for an approximate size of each archive file, by "
                 "default "
                 "64 megabytes. The current value is %d Mb.";
        ptr = &grp.approx_archive_size_bytes;
        scalefactor = 1024 * 1024;
        valmin = 1;
        valmax = 500;
        break;
    case sv_set_compact_threshold_bytes:
        prompt =
            "Set strength of data compaction...\n\n"
            "As an example, if this is set to '10', then when Compact is "
            "run, for each archive if at least 10 Mb of old versions can be "
            "removed, then the old data will be removed. In effect, if this "
            "number is smaller, more space will likely be recovered, at the "
            "expense of time taken and further writes to disk. The current "
            "value is %d Mb.";
        ptr = &grp.compact_threshold_bytes;
        scalefactor = 1024 * 1024;
        valmin = 1;
        valmax = 500;
        break;
    case sv_set_pause_duration:
        prompt = "Set pause duration when running backups...\n\nWhen backing "
                 "up files, we'll pause every thirty seconds in order to let "
                 "other processes have a chance to run. How long should we "
                 "pause? "
                 "The current value is %d seconds.";
        ptr = &grp.pause_duration_seconds;
        valmin = 0;
        valmax = 500;
    default:
        break;
    }

    if (ptr)
    {
        os_clr_console();
        printf(prompt, *ptr / scalefactor);
        *ptr =
            ask_user_int(" Please enter a value:", valmin, valmax) * scalefactor;
        check(sv_grp_persist(&db, &grp));
    }

    check(svdb_disconnect(&db));

cleanup:
    bdestroy(input);
    sv_grp_close(&grp);
    svdb_close(&db);
    return currenterr;
}

check_result write_archive_checksum(svdb_db *db, const char *filepath,
    uint64_t compaction_cutoff, bool stillneeded)
{
    sv_result currenterr = {};
    bstring filename = bstring_open();
    bstring checksum = bstring_open();
    os_get_filename(filepath, filename);
    uint32_t idhigher = 0, idlower = 0;
    int matched = sscanf(cstr(filename), "%5x_%5x.tar", &idhigher, &idlower);
    check_b(matched == 2, "couldn't get archiveid from file %s", filepath);
    if (!stillneeded)
    {
        bstr_assignstatic(checksum, "no_longer_needed");
    }
    else
    {
        check(get_file_checksum_string(filepath, checksum));
    }

    check(svdb_archives_write_checksum(db, make_u64(idhigher, idlower),
        (uint64_t)time(NULL), compaction_cutoff, cstr(checksum), filepath));

cleanup:
    bdestroy(checksum);
    bdestroy(filename);
    return currenterr;
}

static bool verify_archive_checksum(const char *filename, const char *checksum,
    bstrlist *filenames, bstrlist *checksums)
{
    /* the list of filenames and checksums can contain different versions,
    e.g. 001_001.tar (original hash); 001_001.tar (hash after compact).
    this is because we should accept either version as valid. */
    for (int i = 0; i < filenames->qty; i++)
    {
        if (s_equal(filename, blist_view(filenames, i)) &&
            s_equal(checksum, blist_view(checksums, i)))
        {
            return true;
        }
    }

    return false;
}

static bool archive_needed(
    const char *filename, bstrlist *filenames, bstrlist *checksums)
{
    for (int i = 0; i < filenames->qty; i++)
    {
        if (s_equal(filename, blist_view(filenames, i)) &&
            s_equal(blist_view(checksums, i), "no_longer_needed"))
        {
            return false;
        }
    }

    return true;
}

check_result sv_verify_archives(
    const sv_app *app, const sv_group *grp, svdb_db *db, int *countmismatches)
{
    sv_result currenterr = {};
    bstring path = bstring_open();
    bstring checksum_got = bstring_open();
    bstring shown_previously = bstring_open();
    bstring dir = bformat("%s%suserdata%s%s%sreadytoupload",
        cstr(app->path_app_data), pathsep, pathsep, cstr(grp->grpname), pathsep);

    bstrlist *names = bstrlist_open();
    bstrlist *sums = bstrlist_open();
    check(svdb_archives_get_checksums(db, names, sums));
    check_b(names->qty == sums->qty, "should be same number. got %d, %d",
        names->qty, sums->qty);

    if (names->qty == 0)
    {
        printf("No archives to verify, backup hasn't been run yet.\n");
    }
    else
    {
        for (int i = 0; i < names->qty; i++)
        {
            /* we've already shown this, we don't need to show it again */
            bool dupe = s_equal(cstr(shown_previously), blist_view(names, i));
            bool needed = archive_needed(blist_view(names, i), names, sums);
            if (dupe || !needed)
            {
                continue;
            }

            bassign(shown_previously, names->entry[i]);
            bsetfmt(path, "%s%s%s", cstr(dir), pathsep, blist_view(names, i));
            if (os_file_exists(cstr(path)))
            {
                check(get_file_checksum_string(cstr(path), checksum_got));
                if (verify_archive_checksum(
                        blist_view(names, i), cstr(checksum_got), names, sums))
                {
                    printf("%s: contents verified.\n", blist_view(names, i));
                }
                else
                {
                    if (countmismatches)
                    {
                        (*countmismatches)++;
                    }
                    else
                    {
                        printf("%s: WARNING, checksum does not match, file "
                               "is from a newer version or is damaged. Current "
                               "checksum is %s\n",
                            blist_view(names, i), cstr(checksum_got));
                    }
                }
            }
            else
            {
                if (countmismatches)
                {
                    (*countmismatches)++;
                }
                else
                {
                    printf("%s: file not present on local disk.\n",
                        blist_view(names, i));
                }
            }
        }
    }

    if (!countmismatches)
    {
        alert("");
    }

cleanup:
    bdestroy(path);
    bdestroy(dir);
    bdestroy(checksum_got);
    bdestroy(shown_previously);
    bstrlist_close(names);
    bstrlist_close(sums);
    return currenterr;
}

void add_default_group_settings(sv_group *grp)
{
    grp->approx_archive_size_bytes = 64 * 1024 * 1024;
    grp->compact_threshold_bytes = 32 * 1024 * 1024;
    grp->days_to_keep_prev_versions = 30;
    grp->pause_duration_seconds = 30;
    grp->separate_metadata = 0;

    bstrlist_appendcstr(grp->exclusion_patterns, "*.tmp");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pyc");
#ifdef __linux__
    /* from c++.gitignore */
    bstrlist_appendcstr(grp->exclusion_patterns, "*.slo");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.lo");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.o");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.obj");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.gch");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pch");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.so");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.dylib");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.lai");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.la");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.lib");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.ko");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.elf");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.ilk");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.map");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.exp");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.so");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.idb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pdb");
#else
    /* from visualstudio.gitignore */
    bstrlist_appendcstr(grp->exclusion_patterns, "*.ilk");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.meta");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.obj");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pch");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pdb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pgc");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pgd");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.rsp");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.sbr");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.tlb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.tli");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.tmp_proj");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.vspscc");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.vssscc");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.pidb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.svclog");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.scc");

    /* from visualstudio.gitignore cache files, profiler */
    bstrlist_appendcstr(grp->exclusion_patterns, "*.aps");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.ncb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.sdf");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.cachefile");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.VC.db");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.psess");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.vsp");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.vspx");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.sap");

    /*  other msvc */
    bstrlist_appendcstr(grp->exclusion_patterns, "*.ipch");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.iobj");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.ipdb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.idb");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.tlog");
    bstrlist_appendcstr(grp->exclusion_patterns, "*.exp");

    /* windows */
    bstrlist_appendcstr(grp->exclusion_patterns, "*\\Thumbs.db");
    bstrlist_appendcstr(grp->exclusion_patterns, "*\\ehthumbs.db");
#endif
}
