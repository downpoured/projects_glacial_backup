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

#include "util_archiver.h"
const uint32_t limitfilesperarchive = 32000;

check_result svdp_archiver_advance_next_archive(svdp_archiver* self)
{
	sv_result currenterr = {};
	bstring friendlyfilenamespath = bformat("%s%sfilenames.txt", cstr(self->workingdir), pathsep);
	if (self->currentarchivenumber > 0 && self->currentarchivecountfiles > 0
		&& os_file_exists(cstr(friendlyfilenamespath))
		&& blength(self->encryption) == 0)
	{
		/* if there's no encryption, add filenames.txt to the archive */
		sv_file_close(&self->friendlyfilenamesfile);
		check(svdp_tar_add(self, cstr(self->currentarchive), cstr(friendlyfilenamespath)));
	}

	self->currentarchivenumber++;
	self->currentarchivecountfiles = 0;
	bassignformat(self->currentarchive, "%s%s%05x_%05x.tar", cstr(self->workingdir), pathsep,
		self->currentcollectionid, self->currentarchivenumber);

	/* make a new friendlynames file */
	sv_file_close(&self->friendlyfilenamesfile);
	check(sv_file_open(&self->friendlyfilenamesfile, cstr(friendlyfilenamespath), "w"));
cleanup:
	bdestroy(friendlyfilenamespath);
	return currenterr;
}

check_result svdp_archiver_open(svdp_archiver* self, const char* pathapp, 
	const char* groupname, uint32_t currentcollectionid, uint32_t archivesize, const char* encryption)
{
	set_self_zero();
	self->workingdir = bformat("%s%suserdata%s%s%stemp", pathapp, pathsep, pathsep, groupname, pathsep);
	self->readydir = bformat("%s%suserdata%s%s%sreadytoupload", pathapp, pathsep, pathsep, groupname, pathsep);
	self->currentcollectionid = currentcollectionid;
	self->target_archive_size_bytes = archivesize;
	self->currentarchive = bstring_open();
	self->encryption = bfromcstr(encryption);
	self->path_archiver_binary = bstring_open();
	self->path_audiometadata_binary = bstring_open();
	self->path_compression_binary = bstring_open();
	self->list = bstrListCreate();
	self->strings.useforargscombined = bstring_open();
	self->strings.errmsg = bstring_open();
	self->strings.paramEncryption = bstring_open();
	self->strings.paramOutput = bstring_open();
	self->strings.paramInput = bstring_open();
	self->strings.pathfilename = bstring_open();
	self->strings.pathparentdir = bstring_open();
	return OK;
}

check_result svdp_archiver_beginarchiving(svdp_archiver* self)
{
	sv_result currenterr = {};

	/* clear this group's tmp folder */
	check_b(os_create_dir(cstr(self->workingdir)), "could not create or access tmp dir %s", cstr(self->workingdir));
	check(os_tryuntil_deletefiles(cstr(self->workingdir), "*"));
	self->currentarchivenumber = 0;
	check(svdp_archiver_advance_next_archive(self));
cleanup:
	return currenterr;
}

check_result svdp_archiver_addfile(svdp_archiver* self, const char* pathinput, 
	bool iscompressed, uint64_t contentsid, uint32_t* archivenumber)
{
	sv_result currenterr = {};

	/* make a 7z file */
	char nameOf7zfile[PATH_MAX] = { 0 };
	snprintf(nameOf7zfile, countof(nameOf7zfile) - 1, "%s%s%08llX.7z",
		cstr(self->workingdir), pathsep, castull(contentsid));

	check_b(os_tryuntil_remove(nameOf7zfile), "could not delete %s", nameOf7zfile);
	check(svdp_7z_add(self, nameOf7zfile, pathinput, iscompressed));

	uint64_t currentarchivefilesize = os_getfilesize(cstr(self->currentarchive));
	if (currentarchivefilesize + os_getfilesize(nameOf7zfile) > self->target_archive_size_bytes ||
		self->currentarchivecountfiles > limitfilesperarchive)
	{
		check(svdp_archiver_advance_next_archive(self));
	}

	/* add to friendly names*/
	fprintf(self->friendlyfilenamesfile.file, "%08llX.7z\t%s\n", castull(contentsid), pathinput);

	/* add to the tar file*/
	*archivenumber = self->currentarchivenumber;
	check_b(os_file_exists(nameOf7zfile), "could not find %s", nameOf7zfile);
	check(svdp_tar_add(self, cstr(self->currentarchive), nameOf7zfile));
	check_b(os_tryuntil_remove(nameOf7zfile), "could not delete %s", nameOf7zfile);
	self->currentarchivecountfiles++;
cleanup:
	return currenterr;
}

check_result svdp_archiver_finisharchiving(svdp_archiver* self)
{
	sv_result currenterr = {};
	bstring pattern = bstring_open();
	bstrList* list = bstrListCreate();

	/* ensure that the archive is complete and that filenames.txt is written. */
	check(svdp_archiver_advance_next_archive(self));

	/* kill any remnants of canceled backup operations in readytoupload */
	bassignformat(pattern, "%05llx_*.tar", castull(self->currentcollectionid));
	check(os_tryuntil_deletefiles(cstr(self->readydir), cstr(pattern)));

	/* move files from tmp to readytoupload */
	int moved = 0;
	check(os_tryuntil_movebypattern(cstr(self->workingdir), "*.tar", cstr(self->readydir), true, &moved));
cleanup:
	bdestroy(pattern);
	bstrListDestroy(list);
	return currenterr;
}

check_result get_tar_archive_parameter(const char* archive, bstring sout)
{
	sv_result currenterr = {};
	check_b(os_isabspath(archive), "require absolute path but got %s", archive);
	if (islinux)
	{
		bassignformat(sout, "--file=%s", archive);
	}
	else
	{
		/* see betterlogic.com/roger/2009/01/tar-woes-with-windows/ another workaround appears to be
		using the -C switch, like tar -cvf test.tar -C //./X:/Source/Folder ./
		using a relative path works and even relative to base drive letter like \path\to\file works 
		but then we need to assume same drive... */
		check_b(archive[1] == ':' && archive[2] == '\\', "we currently require paths in the form x:\\ but got %s", archive);
		bassignStatic(sout, "--file=/");
		bconchar(sout, archive[0]);
		bcatcstr(sout, archive + 2);
		for (int i = 0; i < blength(sout); i++)
		{
			if (sout->data[i] == '\\')
				sout->data[i] = '/';
		}
	}
cleanup:
	return currenterr;
}


check_result svdp_archiver_delete_from_archive(svdp_archiver* self, const char* archive,
	const sv_array* contentids, bstrList* messages)
{
	sv_result currenterr = {};
	bstring getoutput = bstring_open();
	char_ptr_array_builder builder = char_ptr_array_builder_open(contentids->length);
	if (!contentids || !contentids->length)
	{
		goto cleanup;
	}

	// instead of deleting in batches, extract everything * to a subdirectory, then re-compress.
	// 1) deleting from the middle of a tar archive is not efficient, causes data to be rewritten
	// 2) even if we use a large batche size of 100, consider the case
	// where there are 2000 small files to delete (not unlikely). even with batchsize=100 this 
	// could write cause on the order of 20*64mb of diskwrites.
	// 3) don't need to worry about batchsize or cmd limits, or dynamically building an array for that matter.
	// 4) 7z d ignores non-present filenames, but tar --delete complains about them. minor point, though.
	(void)archive;
	(void)contentids;
	(void)self;
	(void)messages;
	check_fatal(0, "not yet implemented");

#if 0
	check_b(s_endswith(archive, ".tar"), "must be a tar but got %s", archive);
	check_b(os_isabspath(archive) && os_file_exists(archive), "could not find archive. %s", archive);
	check_b(os_file_exists(cstr(self->path_archiver_binary)), "could not find archiver. %s", cstr(self->path_archiver_binary));
	check(get_tar_archive_parameter(archive, self->strings.paramInput));
	batchsize = (batchsize == 0) ? 50 : batchsize;
	
	for (uint32_t i = 0; i < contentids->length; i++)
	{
		if (builder.offsets.length == 0)
		{
			char_ptr_array_builder_add(&builder, str_and_len32s("--delete"));
			char_ptr_array_builder_add(&builder, cstr(self->strings.paramInput), blength(self->strings.paramInput));
		}
	
		uint64_t contentsid = castull(sv_array_at64u(contentids, i));
		char namewithinarchive[PATH_MAX] = { 0 };
		int written = snprintf(namewithinarchive, countof(namewithinarchive) - 1, "%08llX.7z", castull(contentsid));
		char_ptr_array_builder_add(&builder, namewithinarchive, written);
		if (builder.offsets.length >= batchsize || i >= contentids->length - 1)
		{
			char_ptr_array_builder_finalize(&builder);
			const char** args = (const char**)sv_array_at(&builder.char_ptrs, 0);
			int retcode = os_tryuntil_run_process(cstr(self->path_archiver_binary), args, os_proc_stdout_stderr, 
				NULL, true, self->strings.useforargscombined, getoutput);

			char_ptr_array_builder_reset(&builder);
			if (retcode)
			{
				bformata(getoutput, " returned %d.", retcode);
				bstrListAppendCstr(messages, cstr(getoutput));
			}
		}
	}
#endif

cleanup:
	char_ptr_array_builder_close(&builder);
	bdestroy(getoutput);
	return currenterr;
}

check_result svdp_archiver_restore_from_archive(svdp_archiver* self, const char* archive, 
	uint64_t contentid, const char* workingdir, const char* subworkingdir, const char* dest)
{
	sv_result currenterr = {};
	bstring firstdest = bstring_open();
	bstring destparent = bstring_open();

	check_b(os_isabspath(archive) && os_file_exists(archive), "could not find archive %s.", archive);
	check_b(os_file_exists(cstr(self->path_compression_binary)), "could not find archiver.");
	check_b(contentid, "contentid cannot be 0.");

	/* extract the 7z from the tar */
	char namewithinarchive[PATH_MAX] = { 0 };
	snprintf(namewithinarchive, countof(namewithinarchive) - 1, "%08llX.7z", castull(contentid));
	bassignformat(firstdest, "%s%s%s", workingdir, pathsep, namewithinarchive);
	check_b(os_tryuntil_remove(cstr(firstdest)), "Could not remove temporary file %s.", cstr(firstdest));
	check(svdp_7z_extract_overwrite_ok(self, archive, NULL, namewithinarchive, workingdir));
	check_b(os_file_exists(cstr(firstdest)), "did not find %s in archive %s", namewithinarchive, archive);

	/* extract the file from the 7z */
	check(os_tryuntil_deletefiles(subworkingdir, "*"));
	check(svdp_7z_extract_overwrite_ok(self, cstr(firstdest), cstr(self->encryption), "*", subworkingdir));
	check(os_listfiles(subworkingdir, self->list, false));
	check_b(self->list->qty > 0, "nothing found in archive %s to get %s", namewithinarchive, dest);

	/* move to destination */
	os_get_parent(dest, destparent);
	check_b(os_isabspath(cstr(destparent)), "could not get parent of directory %s", dest);
	check_b(os_create_dirs(cstr(destparent)), "could not create directories for %s", cstr(destparent));
	check_b(os_tryuntil_move(bstrListViewAt(self->list, 0), dest, true), "os_tryuntil_move failed");
	check_b(self->list->qty == 1, "more than one file in archive %s to get %s", namewithinarchive, dest);
cleanup:
	bdestroy(firstdest);
	bdestroy(destparent);
	return currenterr;
}

/* this method is not used in the product, only by tests. */
/* does not support unicode (limitation of 7z, not us); */
/* does not support filenames containing newlines or other 'interesting' characters */
/* itemtype is 0) unknown 1) directory 2) file added without compression 3) compressed file  */
check_result svdp_archiver_7z_list_parse_testsonly(bstring text, bstrList* paths, sv_array* sizes,
	sv_array* crcs, sv_array* itemtypes)
{
	sv_result currenterr = {};
	sv_array_truncatelength(sizes, 0);
	sv_array_truncatelength(crcs, 0);
	sv_array_truncatelength(itemtypes, 0);
	bstrListClear(paths);

	struct tagbstring searchfor = bsStatic("\r\n");
	struct tagbstring replacewith = bsStatic("\n");
	bReplaceAll(text, &searchfor, &replacewith, 0);
	sv_pseudosplit split = sv_pseudosplit_open(cstr(text));
	sv_pseudosplit_split(&split, '\n');
	check_b(split.splitpoints.length > 4, "Expected to see many lines but only got %d", split.splitpoints.length);
	check_b(sizes->elementsize == sizeof(uint64_t), "wrong elementsize");
	check_b(crcs->elementsize == sizeof(uint64_t), "wrong elementsize");
	check_b(itemtypes->elementsize == sizeof(uint64_t), "wrong elementsize");

	bool started = false;
	for (uint32_t i = 0; i < split.splitpoints.length; i++)
	{
		if (s_startswith(sv_pseudosplit_viewat(&split, i), "----------"))
		{
			started = true;
		}
		else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Path = "))
		{
			bstrListAppendCstr(paths, sv_pseudosplit_viewat(&split, i) + strlen("Path = "));
			sv_array_add64u(sizes, 0);
			sv_array_add64u(crcs, 0);
			sv_array_add64u(itemtypes, 0);
		}
		else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Size = "))
		{
			uint64_t size = 0;
			check_b(uintfromstring(sv_pseudosplit_viewat(&split, i) + strlen("Size = "), &size), 
				"could not parse size %s", sv_pseudosplit_viewat(&split, i));
			*(uint64_t*)(sv_array_at(sizes, sizes->length - 1)) = size;
		}
		else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "CRC = "))
		{
			uint64_t crc = 0;
			check_b(uintfromstringhex(sv_pseudosplit_viewat(&split, i) + strlen("CRC = "), &crc),
				"could not parse crc %s", sv_pseudosplit_viewat(&split, i));
			*(uint64_t*)(sv_array_at(crcs, crcs->length - 1)) = crc;
		}
		else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Folder = +"))
		{
			*(uint64_t*)(sv_array_at(itemtypes, itemtypes->length - 1)) = 1;
		}
		else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Method = Copy") &&
			!sv_array_at64u(itemtypes, itemtypes->length - 1))
		{
			*(uint64_t*)(sv_array_at(itemtypes, itemtypes->length - 1)) = 2;
		}
		else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Method = ") &&
			!sv_array_at64u(itemtypes, itemtypes->length - 1))
		{
			*(uint64_t*)(sv_array_at(itemtypes, itemtypes->length - 1)) = 3;
		}
	}
cleanup:
	sv_pseudosplit_close(&split);
	return currenterr;
}

/* this method is not used in the product, only by tests. */
check_result svdp_archiver_7z_list_testsonly(svdp_archiver* self, const char* archive, bstrList* paths,
	sv_array* sizes, sv_array* crcs, sv_array* itemtypes)
{
	sv_result currenterr = {};
	check_b(os_isabspath(archive) && os_file_exists(archive), "could not find archive %s.", archive);
	check_b(os_file_exists(cstr(self->path_compression_binary)), "could not find archiver.");
	bassignformat(self->strings.paramEncryption, "-p%s", cstr(self->encryption));

	const char* args[] = { "l",
		"-slt", /* show technical information (we want to see the crcs) */
		archive,
		blength(self->encryption) ? cstr(self->strings.paramEncryption) : NULL,
		NULL
	};

	bstrClear(self->strings.errmsg);
	int retcode = os_tryuntil_run_process(cstr(self->path_compression_binary), args, false,
		self->strings.useforargscombined, self->strings.errmsg);
	check_b(retcode == 0, "Failed with (%d) to %s stderr=%s", retcode, 
		cstr(self->currentarchive), cstr(self->strings.errmsg));

	check(svdp_archiver_7z_list_parse_testsonly(self->strings.errmsg, paths, sizes, crcs, itemtypes));

cleanup:
	return currenterr;
}

check_result svdp_archiver_7z_list_string_testsonly(svdp_archiver* self, const char* archive, bstrList* results)
{
	sv_result currenterr = {};
	sv_array sizes = sv_array_open(sizeof32u(uint64_t), 0);
	sv_array crcs = sv_array_open(sizeof32u(uint64_t), 0);
	sv_array itemtypes = sv_array_open(sizeof32u(uint64_t), 0);
	check(svdp_archiver_7z_list_testsonly(self, archive, results, &sizes, &crcs, &itemtypes));
	for (uint32_t i = 0; i < cast32s32u(results->qty); i++)
	{
		bformata(results->entry[i], " contentlength=%llu crc=%llx itemtype=%llu",
			castull(sv_array_at64u(&sizes, i)), castull(sv_array_at64u(&crcs, i)), castull(sv_array_at64u(&itemtypes, i)));
	}

cleanup:
	sv_array_close(&sizes);
	sv_array_close(&crcs);
	sv_array_close(&itemtypes);
	return currenterr;
}

check_result svdp_archiver_tar_list_string_testsonly(svdp_archiver* self, const char* archive, 
	bstrList* results, const char* subworkingdir)
{
	sv_result currenterr = {};
	bstrList* listof7z = bstrListCreate();
	check(os_tryuntil_deletefiles(subworkingdir, "*"));
	check_b(os_create_dirs(subworkingdir), "%s", subworkingdir);
	check(svdp_7z_extract_overwrite_ok(self, archive, NULL, "*", subworkingdir));
	check(os_listdirs(subworkingdir, self->list, false));
	check_b(self->list->qty == 0, "the directory %s has subdirectories, which we don't expect. "
		"please remove them. or the archive %s has subdirs", subworkingdir, archive);
	
	bstrListClear(results);
	check(os_listfiles(subworkingdir, self->list, false));
	for (int i = 0; i < self->list->qty; i++)
	{
		os_get_filename(bstrListViewAt(self->list, i), self->strings.pathfilename);
		bstrListAppend(results, self->strings.pathfilename);
		if (s_endswith(cstr(self->strings.pathfilename), ".7z"))
		{
			check(svdp_archiver_7z_list_string_testsonly(self, bstrListViewAt(self->list, i), listof7z));
			bstrListAppendList(results, listof7z);
		}
	}

	check(os_tryuntil_deletefiles(subworkingdir, "*"));

cleanup:
	bstrListDestroy(listof7z);
	return currenterr;
}

check_result svdp_7z_extract_overwrite_ok(svdp_archiver* self, const char* archive, const char* encryption, 
	const char* internalarchivepath, const char* outputdir)
{
	sv_result currenterr = {};
	bstring paramOutdir = bstring_open();
	bassigncstr(paramOutdir, "-o");
	bcatcstr(paramOutdir, outputdir);
	check_b(os_file_exists(archive), "archive path %s not found", archive);
	check_b(os_dir_exists(outputdir), "output dir %s not found", outputdir);
	bassignformat(self->strings.paramEncryption, "-p%s", encryption ? encryption : "");

	const char* args[] = { "e", /* we'd use x if we wanted to preserve paths */
		archive,
		cstr(paramOutdir),
		internalarchivepath,
		"-aoa", /* replace existing files */
		encryption ? cstr(self->strings.paramEncryption) : NULL,
		NULL
	};

	int retcode = os_tryuntil_run_process(cstr(self->path_compression_binary), args, true,
		self->strings.useforargscombined, self->strings.errmsg);
	check_b(retcode == 0, "Failed with (%d) to %s stderr=%s", retcode, cstr(self->currentarchive), cstr(self->strings.errmsg));
cleanup:
	bdestroy(paramOutdir);
	return currenterr;
}



check_result svdp_7z_add(svdp_archiver* self, const char* path7zfile, const char* inputfilename, bool is_compressed)
{
	sv_result currenterr = {};
	/*	batch size is currently hard-coded to one.
	we would otherwise have to maintain the file lock for the lifetime of the batch
	(we must not allow the file to be altered between reading its metadata and adding it to the archive)
	it would also be more difficult, if 7z returned an error exit status, to know which of the files failed.
	The downside that we create more processes, which takes more time.

	not using 7z's "volumes" mode.
	1) complicated to add files to archive without 7z error "the file exists".
	2) unlike winrar, does not add headers to each part; corruption in one volume often corrupts all.
	3) we already are splitting into multiple archives. unnecessary complexity to have two different splitting methods.

	we now are creating a .tar containing .7z files
		we don't just make a .7z containing the user's files because appending to a 7z, even with solidmode=off, 
		7z re-writes the entire archive
		add in two steps, user-file -> 000001.file.7z -> 00001_00001.tar
		we don't yet add files directly to the tar (would not support encryption, further code complexity), 
		although that would save disk writes
		we used to have to take the file data and pass it via stdin to 7z, mainly so that we could specify our own filename
		now that we don't need to specify a filename, because it's the .7z within the .tar that can have a numeric 
		filename, we don't need to pass through stdin.
			why we used to provide data through stdin:
				1) allows us to specify the name within the archive, without using symlinks or hardlinks 
				(hardlinks not commonly used; wouldn't work on FAT systems)
				2) "7za l" doesn't work with unicode, so listing contents would be difficult... 
				but we no longer need to list because each 7z only contains one file
				3) don't need to work around filenames that contain shell metacharacters... 
				but execv should be safe
				4) ensure data added to the archive is the data we got the contents-hash of... 
				but we are still holding flock, so we are no less safe
	*/

	os_get_parent(inputfilename, self->strings.pathparentdir);
	os_get_filename(inputfilename, self->strings.pathfilename);
	os_setcwd(cstr(self->strings.pathparentdir));

	check_b(!os_file_exists(path7zfile), "file should not exist yet %s", path7zfile);
	bassignformat(self->strings.paramInput, "%s", cstr(self->strings.pathfilename));
	bassignformat(self->strings.paramEncryption, "-p%s", cstr(self->encryption));
	bool is_encrypted = blength(self->encryption) > 0;
	const char* params[] = {
		"a",
		path7zfile,
		cstr(self->strings.pathfilename),
		"-ms=off", /* turn off solid mode */
		is_compressed ? "-mx0" : "-mx3", /* add this file without compression */
		is_encrypted ? "-mhe" : NULL,
		is_encrypted ? cstr(self->strings.paramEncryption) : NULL,
		NULL
	};

	int retcode = os_tryuntil_run_process(cstr(self->path_compression_binary), params, true,
		self->strings.useforargscombined, self->strings.errmsg);
	check_b(retcode == 0, "Failed with %s(%d) to %s (%s)", get7zerr(retcode), retcode, path7zfile,
		cstr(self->strings.errmsg));

cleanup:
	return currenterr;
}

check_result svdp_tar_add(svdp_archiver* self, const char* tarname, const char* inputfile)
{
	sv_result currenterr = {};
	os_get_parent(inputfile, self->strings.pathparentdir);
	os_get_filename(inputfile, self->strings.pathfilename);
	os_setcwd(cstr(self->strings.pathparentdir));
	check(get_tar_archive_parameter(tarname, self->strings.paramInput));
	const char* params[] = {
		"--append",
		cstr(self->strings.paramInput),
		cstr(self->strings.pathfilename),
		NULL
	};

	int retcode = os_tryuntil_run_process(cstr(self->path_archiver_binary), params, true,
		self->strings.useforargscombined, self->strings.errmsg);
	check_b(retcode == 0, "Failed with %d to %s %s (%s)", retcode, tarname, inputfile, cstr(self->strings.errmsg));

cleanup:
	return currenterr;
}

void svdp_archiver_close(svdp_archiver* self)
{
	bdestroy(self->workingdir);
	bdestroy(self->readydir);
	bdestroy(self->currentarchive);
	bdestroy(self->encryption);
	bdestroy(self->path_compression_binary);
	bdestroy(self->path_archiver_binary);
	bdestroy(self->path_audiometadata_binary);
	bdestroy(self->strings.useforargscombined);
	bdestroy(self->strings.errmsg);
	bdestroy(self->strings.paramEncryption);
	bdestroy(self->strings.paramOutput);
	bdestroy(self->strings.paramInput);
	bdestroy(self->strings.pathfilename);
	bdestroy(self->strings.pathparentdir);
	bstrListDestroy(self->list);
	sv_file_close(&self->friendlyfilenamesfile);
	set_self_zero();
}

