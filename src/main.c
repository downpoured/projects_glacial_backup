/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "util.h"
#include "util_os.h"
#include "sphash.h"
#include "sqlite3.h"
#include "user_config.h"
#include "dbaccess.h"
#include "util_archiver.h"
#include "operations.h"
#include "tests.h"

bc_log g_log = {};

void svdp_menumore()
{
	clearscreen();
	int choice = ask_user_int("More... \n\n\n"
		"1) View upload staging area\n"
		"2) View logs\n"
		"3) View archive info\n"
		"4) View a file's history\n"
		"5) (Testing: Database to text)\n"
		"6) (Testing: Run tests)\n"
		"7) Back to main menu\n", 1, 6);

	switch (choice)
	{
	case 1:
		svdp_viewlocation(false /*logs or staging*/);
		break;
	case 2:
		svdp_viewlocation(true /*logs or staging*/);
		break;
	case 3:
		svdp_runoperation(svdpeoperations_trim, true /*preview*/);
		break;
	case 4:
		svdp_runoperation(svdpeoperations_filehistory, false /*preview*/);
		break;
	case 5:
		svdp_runoperation(svdpeoperations_totxt, false /*preview*/);
		break;
	case 6:
		runtests();
		break;
	default:
		break;
	}
}

void svdp_menumain()
{
	bool fcontinue = true;
	while (fcontinue)
	{
		clearscreen();
		int choice = ask_user_int("welcome to glacial_backup. \n\n\n"
			"1) Configure locations...\n"
			"2) Preview backup\n"
			"3) Run backup\n"
			"4) Preview restore\n"
			"5) Run restore\n"
			"6) Trim obsolete versions...\n"
			"7) More...\n"
			"8) Exit\n", 1, 8);

		switch (choice)
		{
		case 1:
			svdp_configs_in_editor();
			break;
		case 2:
			svdp_runoperation(svdpeoperations_backup, true /*preview*/);
			break;
		case 3:
			svdp_runoperation(svdpeoperations_backup, false /*preview*/);
			break;
		case 4:
			svdp_runoperation(svdpeoperations_restore, true /*preview*/);
			break;
		case 5:
			svdp_runoperation(svdpeoperations_restore, false /*preview*/);
			break;
		case 6:
			svdp_runoperation(svdpeoperations_trim, false /*preview*/);
			break;
		case 7:
			svdp_menumore();
			break;
		case 8:
			fcontinue = false;
			break;
		default:
			fcontinue = false;
			break;
		}
	}
}

static void printsupportedargs(bool sayunrecognized)
{
	if (sayunrecognized)
		puts("Unrecognized parameter.\n");

	puts("syntax is \nglacial_backup " cmdswitch "backup {groupname}[" cmdswitch "preview]\n\n");
}

static bool getargs(int argc, char *argv[], const char** groupname, bool* ispreview, enum svdpeoperations* op)
{
	if (argc < 3 || argc > 4)
	{
		return false;
	}

	if (argc >= 4 && s_equal(argv[3], cmdswitch "preview"))
	{
		*ispreview = true;
	}
	else
	{
		return false;
	}

	if (s_startswith(argv[2], "group"))
	{
		*groupname = argv[2];
	}
	else
	{
		return false;
	}

	if (s_equal(argv[1], cmdswitch "backup"))
	{
		*op = svdpeoperations_backup;
	}
	else
	{
		return false;
	}

	return true;
}

int svdpmain(int argc, char *argv[])
{
	Result currenterr = {};
	int retcode = 0;
	check(svdp_check_other_instances());
	check(svdp_startgloballogging(&g_log));
	if (argc <= 1)
	{
		alertdialog("Starting interactive session, run with -h to see parameters.");
		svdp_menumain();
	}
	else if (s_equal(argv[1], "-h") || s_equal(argv[1], "--help") || s_equal(argv[1], "/h")
		|| s_equal(argv[1], "/?") || s_equal(argv[1], "-?"))
	{
		printsupportedargs(false);
	}
	else
	{
		g_log.suppress_dialogs = true;
		bool ispreview = false;
		const char* groupname = NULL;
		enum svdpeoperations op = svdpeoperations_none;
		if (!getargs(argc, argv, &groupname, &ispreview, &op))
		{
			printsupportedargs(false);
			retcode = 1;
		}
		else
		{
			retcode = svdp_runoperation_groupname(op, ispreview, groupname);
		}
	}

cleanup:
	retcode |= currenterr.errortag;
	bc_log_close(&g_log);
	print_error_if_ocurred("", &currenterr);
	g_log.suppress_dialogs = false;
	return retcode;
}


int main(int argc, char *argv[])
{
	int retcode = 0;
	runtests();

#if 0
	retcode = svdpmain(argc, argv);
#endif

	return retcode;
}
