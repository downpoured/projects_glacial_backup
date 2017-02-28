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

#include "main.h"

check_result ui_app_actions_tests(unusedapp(), unusedint())
{
	sv_log_register_active_logger(NULL);
	run_sv_tests();
	alertdialog("Tests complete. GlacialBackup will now exit.");
	exit(0);
}

check_result ui_app_actions_viewlogs(svdp_application* app, unusedint())
{
	bstring logdir = bformat("%s%slogs", cstr(app->path_app_data), pathsep);
	printf("Log files are saved to the directory %s.", cstr(logdir));
	if (ask_user_yesno("Open this directory in UI?"))
	{
		os_open_dir_ui(cstr(logdir));
	}
	
	bdestroy(logdir);
	return OK;
}

check_result ui_app_actions_exit(unusedapp(), unusedint())
{
	exit(0);
}

check_result ui_app_menu_restorefromother(svdp_application* app, unusedint())
{
	printf("Let's say you are restoring a group named 'example'."
		"\n(There are many files with names like example_0001_0001.7z)"
		"\nCreate a folder \n%s%sexample%sreadytoupload\n and place all files into it."
		"\nThen start GlacialBackup and select Restore.\n", 
		cstr(app->path_app_data), pathsep, pathsep);
	alertdialog("");
	return OK;
}

check_result ui_app_menu_about(unusedapp(), unusedint())
{
	alertdialog("Downpoured GlacialBackup, by Ben Fisher, 2016."
		"\n\nGlacialBackup is free software : you can redistribute it and / or modify"
		"\nit under the terms of the GNU General Public License as published by"
		"\nthe Free Software Foundation, either version 3 of the License, or"
		"\n(at your option) any later version.");
	alertdialog("\n\nLibraries:"
		"\nBstring, by Paul Hsieh"
		"\n7Zip, by Igor Pavlov"
		"\nSQLite, by SQLite team"
		"\nFFmpeg, by FFmpeg team (disabled by default)"
		"\nFnmatch implementation, from The GNU C Library"
		"\nThe SpookyHash hashing implementation is"
		"\nCopyright(c) 2014, Spooky Contributors"
		"\nBob Jenkins, Andi Kleen, Ziga Zupanec, Arno Wagner"
		"\nAll rights reserved."
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

check_result ui_app_menu_openwebsite(unusedapp(), unusedint())
{
	os_open_glacial_backup_website();
	return OK;
}

check_result ui_app_menu_edit_group(svdp_application* app, unusedint())
{
	pickgroup(app);
	if (app->current_group_name_index >= 0)
	{
		ui_numbered_menu_spec_entry entries[] = {
			{ "Add/remove directories...", &ui_app_menu_edit_group_edit_dirs },
			{ "Add/remove exclusion patterns...", &ui_app_menu_edit_group_edit_excl_patterns },
			{ "Set how long to keep previous file versions...", &ui_app_edit_setting, svdp_setting_days_to_keep_prev_versions },
			{ "Set encryption...", &ui_app_menu_edit_group_edit_encryption },
			{ "Set approximate filesize for archive files...",  &ui_app_edit_setting, svdp_setting_approx_archive_size_bytes },
			{ "Set strength of data compaction...",  &ui_app_edit_setting, svdp_setting_compact_threshold_bytes },
			{ "Separate audio metadata changes...",  &ui_app_edit_setting, svdp_setting_separate_metadata_enabled },
			{ "Back to main menu", NULL },
			{ NULL, NULL } };

		return ui_numbered_menu_show("Edit backup group...", entries, app, NULL);
	}
	return OK;
}

check_result ui_app_menu_more_actions(svdp_application* app, unusedint())
{
	ui_numbered_menu_spec_entry entries[] = {
		{ "View information", &svdp_application_viewinfo },
		{ "View logs", &ui_app_actions_viewlogs },
		{ "Compact backup data to save disk space", &svdp_application_run, svdp_op_compact },
		{ "Run tests", &ui_app_actions_tests },
		{ "Run backups with low-privilege account...", &svdp_application_run_lowpriv },
		{ "Back to main menu", NULL },
		{ NULL, NULL } };

	return ui_numbered_menu_show("More actions...", entries, app, NULL);
}

ui_numbered_menu_spec_entry entriesFull[] = {
	{ "Run backup", &svdp_application_run, svdp_op_backup },
	{ "Restore file(s)...", &svdp_application_run, svdp_op_restore},
	{ "Restore file(s) from the past...", &svdp_application_run, svdp_op_restore_from_past },
	{ "Restore from another computer...", &ui_app_menu_restorefromother },
	{ "Create backup group...", &svdp_application_creategroup },
	{ "Edit backup group...", &ui_app_menu_edit_group },
	{ "More...", &ui_app_menu_more_actions },
	{ "Exit", &ui_app_actions_exit },
	{ NULL, NULL } };

ui_numbered_menu_spec_entry entriesWithoutBackupGroup[] = {
	{ "Create backup group...", &svdp_application_creategroup },
	{ "Restore from another computer...", &ui_app_menu_restorefromother },
	{ "Exit", &ui_app_actions_exit },
	{ NULL, NULL } };

const ui_numbered_menu_spec_entry* ui_app_menu_mainmenu_get(svdp_application* app)
{
	check_warn(svdp_application_findgroupnames(app), NULL, exit_on_err);
	return app->backup_group_names->qty > 0 ?
		entriesFull : entriesWithoutBackupGroup;
}

check_result ui_app_menu_mainmenu(svdp_application* app, unusedint())
{
	return ui_numbered_menu_show("Main menu.", ui_app_menu_mainmenu_get(app), app, &ui_app_menu_mainmenu_get);
}

check_result ui_app_menu_intro(svdp_application* app, unusedint())
{
	ui_numbered_menu_spec_entry entries[] = {
		{ "Main menu", &ui_app_menu_mainmenu },
		{ "About GlacialBackup Software", &ui_app_menu_about },
		{ "Open GlacialBackup website", &ui_app_menu_openwebsite },
		{ "Exit", NULL },
		{ NULL, NULL } };

	return ui_numbered_menu_show("Welcome to GlacialBackup.", entries, app, NULL);
}

int mainsig
{
	svdp_application app = {};
	os_init();
	run_sv_tests();
	bool is_low_access = false;
	bstring dir_from_args = parse_cmd_line_args(argc, argv, &is_low_access);
	check_warn(svdp_application_load(&app, cstr(dir_from_args), is_low_access), NULL, exit_on_err);
	check_warn(ui_app_menu_intro(&app, svdp_op_none), NULL, exit_on_err);
	svdp_application_close(&app);
	bdestroy(dir_from_args);
	return 0;
}
