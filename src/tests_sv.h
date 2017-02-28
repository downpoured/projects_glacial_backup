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

#ifndef TESTS_SV_H_INCLUDE
#define TESTS_SV_H_INCLUDE

#include "tests_utils.h"
void run_sv_tests(bool run_from_ui);

typedef struct operations_test_hook {
	os_exclusivefilehandle filelocks[10];
	uint64_t fakelastmodtimes[10];
	bstring filenames[10];
	bstring dirfakeuserfiles;
	bstring path_group;
	bstring path_readytoupload;
	bstring path_readytoremove;
	bstring path_restoreto;
	bstring path_untar;
	bstring tmp_path;
	bstrlist *allcontentrows;
	bstrlist *allfilelistrows;
	int stage_of_test;
} operations_test_hook;

#endif
