/*
user_interface.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "user_interface.h"

check_result ui_action_viewlogs(sv_app *app, unused(int))
{
    bstring dir = bformat("%s%slogs", cstr(app->path_app_data), pathsep);
    printf("Log files are saved to the directory \n%s\n\n", cstr(dir));
    if (ask_user("Open this directory in UI?"))
    {
        os_open_dir_ui(cstr(dir));
        alert("");
    }

    bdestroy(dir);
    return OK;
}

check_result ui_action_exit(unused_ptr(sv_app), unused(int))
{
    exit(0);
}

noreturn_start() check_result ui_action_tests(unused_ptr(sv_app), unused(int))
{
    sv_log_register_active_logger(NULL);
    run_all_tests();
    return OK;
}
noreturn_end()

    check_result ui_restorefromother(sv_app *app, unused(int))
{
    printf("GlacialBackup can restore files from backups \nthat were created "
           "on another computer.\n\n"
           "1) Collect the files with names like 00001_00001.tar and "
           "00001.db into one directory. \n\n"
           "2) Create a directory at \n    "
           "%s%suserdata%sexample%sreadytoupload\nand place all of the files "
           "here.\n\n"
           "3) Start GlacialBackup, choose to 'Restore', and then choose "
           "'example' from the list. The files will then be restored!\n",
        cstr(app->path_app_data), pathsep, pathsep, pathsep);
    alert("");
    return OK;
}

check_result ui_menu_about(unused_ptr(sv_app), unused(int))
{
    alert("GlacialBackup, by Ben Fisher, 2016."
          "\n\nFor documentation and more, refer to"
          "\nhttps://github.com/moltenform/projects-glacial-backup"
          "\n\nGlacialBackup is free software: you can redistribute it and / "
          "or modify\nit under the terms of the GNU General Public License "
          "as published by\nthe Free Software Foundation, either version 3 "
          "of the License, or\n(at your option) any later version.");

    alert("\n\n"
          "\nBstring, from Paul Hsieh"
          "\nSQLite, from SQLite team"
          "\nTar, from GNU Project"
          "\nThe SpookyHash hash algorithm is"
          "\nCopyright(c) 2014, Spooky Contributors"
          "\nBob Jenkins, Andi Kleen, Ziga Zupanec, Arno Wagner"
          "\nAll rights reserved."
          "\nFnmatch, from GNU C Library"
          "\nXZ Utils, from Tukaani Project"
          "\nos_clock_gettime(), by Asain Kujovic"
          "\nTHIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS"
          "\n\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT"
          "\nLIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS"
          "\nFOR A PARTICULAR PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE"
          "\nCOPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,"
          "\nINDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES"
          "\n(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR"
          "\nSERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)"
          "\nHOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,"
          "\nSTRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)"
          "\nARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED"
          "\nOF THE POSSIBILITY OF SUCH DAMAGE.\n");

    return OK;
}

check_result ui_menu_edit_grp(sv_app *app, unused(int))
{
    menu_action_entry entries[] = {
        {"Add/remove directories...", &app_menu_edit_grp_dirs},
        {"Add/remove exclusion patterns...", &app_menu_edit_grp_patterns},
        {"Set how long to keep previous file versions...", &app_edit_setting,
            sv_set_days_to_keep_prev_versions},
        {"Set approximate filesize for archive files...", &app_edit_setting,
            sv_set_approx_archive_size_bytes},
        {"Set strength of data compaction...", &app_edit_setting,
            sv_set_compact_threshold_bytes},
        {"Set pause duration when running backups...", &app_edit_setting,
            sv_set_pause_duration},
        {"Skip metadata changes...", &app_edit_setting,
            sv_set_separate_metadata_enabled},
        {"Back", NULL}, {NULL, NULL}};

    return menu_choose_action("Edit backup group...", entries, app, NULL);
}

check_result ui_menu_more(sv_app *app, unused(int))
{
    menu_action_entry entries[] = {
        {"View information", &sv_application_run, sv_run_viewinfo},
        {"View logs", &ui_action_viewlogs},
        {"Compact backup data to save disk space", &sv_application_run,
            sv_run_compact},
        {"Verify archive integrity", &sv_application_run, sv_run_verify},
        {"Run tests", &ui_action_tests},
        {"Run backups with low-privilege account...", &sv_app_run_lowpriv},
        {"Back", NULL}, {NULL, NULL}};

    return menu_choose_action("More actions...", entries, app, NULL);
}

menu_action_entry entries_full[] = {{"About GlacialBackup...", &ui_menu_about},
    {"Run backup...", &sv_application_run, sv_run_backup},
    {"Sync to cloud...", &sv_application_run, sv_run_sync_cloud},
    {"Restore file(s)...", &sv_application_run, sv_run_restore},
    {"Restore from previous revisions...", &sv_application_run,
        sv_run_restore_from_past},
    {"Restore from another computer...", &ui_restorefromother},
    {"Create backup group...", &sv_app_creategroup},
    {"Edit backup group...", &ui_menu_edit_grp}, {"More...", &ui_menu_more},
    {"Exit", &ui_action_exit}, {NULL, NULL}};

menu_action_entry entries_without_backup_group[] = {
    {"About GlacialBackup...", &ui_menu_about},
    {"Create backup group...", &sv_app_creategroup},
    {"Restore from another computer...", &ui_restorefromother},
    {"Run tests", &ui_action_tests}, {"Exit", &ui_action_exit}, {NULL, NULL}};

const menu_action_entry *ui_app_menu_mainmenu_get(sv_app *app)
{
    check_warn(sv_app_findgroupnames(app), NULL, exit_on_err);
    return app->grp_names->qty > 0 ? entries_full : entries_without_backup_group;
}

int mainsig
{
    bool low_access = false;
    sv_app app = {};
    os_init();
    SvdpHashSeed1 = make_u64(chars_to_uint32('s', 'e', 'v', 'e'),
        chars_to_uint32('n', 'a', 'r', 'c'));
    SvdpHashSeed2 = make_u64(chars_to_uint32('h', 'i', 'v', 'e'),
        chars_to_uint32('s', '7', '7', '7'));
    restrict_write_access = bstring_open();

    bstring dir_from_args = parse_cmd_line_args(argc, argv, &low_access);
    check_warn(
        sv_app_load(&app, cstr(dir_from_args), low_access), NULL, exit_on_err);
    check_warn(
        menu_choose_action(
            "Welcome to GlacialBackup.\n"
            "https://github.com/moltenform/projects-glacial-backup",
            ui_app_menu_mainmenu_get(&app), &app, &ui_app_menu_mainmenu_get),
        NULL, exit_on_err);
    sv_app_close(&app);
    bdestroy(dir_from_args);
    return 0;
}
