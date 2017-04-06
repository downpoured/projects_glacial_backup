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

check_result svdp_archiver_advance_next_archive(svdp_archiver *self)
{
    sv_result currenterr = {};
    bstring friendlyfilenamespath = bformat("%s%sfilenames.txt",
        cstr(self->path_intermediate_archives), pathsep);

    if (self->currentarchivenumber > 0 && self->currentarchive_contents->qty > 0)
    {
        /* if there's no encryption, add filenames.txt to the archive */
        if (os_file_exists(cstr(friendlyfilenamespath))
            && blength(self->encryption) == 0)
        {
            sv_file_close(&self->friendlyfilenamesfile);
            sv_log_writefmt("adding %s", cstr(friendlyfilenamespath));
            check(svdp_tar_add(self, cstr(self->currentarchive), cstr(friendlyfilenamespath)));
        }

        check(svdp_tar_verify_contents(self, cstr(self->currentarchive),
            self->currentarchive_contents, &self->currentarchive_sizes));
    }

    sv_log_writefmt("finishing %s with %d files",
        cstr(self->currentarchive), self->currentarchive_contents->qty);

    self->currentarchivenumber++;
    bstrlist_clear(self->currentarchive_contents);
    sv_array_clear(&self->currentarchive_sizes);
    bassignformat(self->currentarchive, "%s%s%05x_%05x.tar", cstr(self->path_staging), pathsep,
        self->collectionid, self->currentarchivenumber);
    sv_log_writelines("starting", cstr(self->currentarchive));

    /* make a new friendlynames file */
    sv_file_close(&self->friendlyfilenamesfile);
    check(sv_file_open(&self->friendlyfilenamesfile, cstr(friendlyfilenamespath), "w"));
cleanup:
    bdestroy(friendlyfilenamespath);
    return currenterr;
}

check_result svdp_archiver_open(svdp_archiver *self,
    const char *pathapp, const char *groupname, uint32_t currentcollectionid,
    uint32_t archivesize, const char *encryption)
{
    set_self_zero();
    self->path_intermediate_archives = bformat("%s%stemp%sarchived",
        pathapp, pathsep, pathsep);
    self->path_staging = bformat("%s%suserdata%s%s%sstaging",
        pathapp, pathsep, pathsep, groupname, pathsep);
    self->path_readytoupload = bformat("%s%suserdata%s%s%sreadytoupload",
        pathapp, pathsep, pathsep, groupname, pathsep);

    self->limitfilesperarchive = 25 * 1000;
    self->collectionid = currentcollectionid;
    self->target_archive_size_bytes = archivesize;
    self->currentarchive = bstring_open();
    self->encryption = bfromcstr(encryption);
    self->path_archiver_binary = bstring_open();
    self->path_audiometadata_binary = bstring_open();
    self->path_compression_binary = bstring_open();
    self->currentarchive_contents = bstrlist_open();
    self->currentarchive_sizes = sv_array_open(sizeof32u(uint64_t), 0);
    self->tmp_list = bstrlist_open();
    self->tmp_argscombined = bstring_open();
    self->tmp_resultstring = bstring_open();
    self->tmp_arg_encryption = bstring_open();
    self->tmp_arg_input = bstring_open();
    self->tmp_filename = bstring_open();
    self->tmp_parentdir = bstring_open();
    self->can_add_to_tar_directly = true;
    return OK;
}

check_result svdp_archiver_beginarchiving(svdp_archiver *self)
{
    sv_result currenterr = {};

    /* clear tmp folders */
    sv_log_writefmt("svdp_archiver_beginarchiving, collection=%u", self->collectionid);
    check_b(os_create_dirs(cstr(self->path_intermediate_archives)),
        "could not create or access %s", cstr(self->path_intermediate_archives));
    check(os_tryuntil_deletefiles(cstr(self->path_intermediate_archives), "*"));
    check_b(os_create_dirs(cstr(self->path_staging)), "could not create or access %s",
        cstr(self->path_staging));

    check(os_tryuntil_deletefiles(cstr(self->path_staging), "*"));
    self->currentarchivenumber = 0;
    check(svdp_archiver_advance_next_archive(self));

cleanup:
    return currenterr;
}

check_result svdp_archiver_addfile(svdp_archiver *self, const char *pathinput,
    bool iscompressed, uint64_t contentsid, uint32_t *archivenumber, uint64_t *compressedsize)
{
    sv_result currenterr = {};
    bool make7zfilefirst = !iscompressed || blength(self->encryption) || !self->can_add_to_tar_directly;
    *compressedsize = 0;
    uint64_t currentarchivefilesize = os_getfilesize(cstr(self->currentarchive));

    if (make7zfilefirst)
    {
        /* make a 7z file */
        char name7zfile[PATH_MAX] = { 0 };
        snprintf(name7zfile, countof(name7zfile) - 1, "%s%s%08llx.7z",
            cstr(self->path_intermediate_archives), pathsep, castull(contentsid));
        check_b(os_tryuntil_remove(name7zfile), "could not delete %s", name7zfile);
        check(svdp_7z_add(self, name7zfile, pathinput, iscompressed));

        *compressedsize = os_getfilesize(name7zfile);
        if (currentarchivefilesize + *compressedsize > self->target_archive_size_bytes ||
            self->currentarchive_contents->qty > self->limitfilesperarchive)
        {
            check(svdp_archiver_advance_next_archive(self));
        }

        /* add to the tar file*/
        check_b(*compressedsize > 0, "file %s cannot have size 0", name7zfile);
        check(svdp_tar_add(self, cstr(self->currentarchive), name7zfile));
        log_b(os_tryuntil_remove(name7zfile), "could not delete %s", name7zfile);
        os_get_filename(name7zfile, self->tmp_filename);
        bstrlist_append(self->currentarchive_contents, self->tmp_filename);
    }
    else
    {
        *compressedsize = os_getfilesize(pathinput);
        if (currentarchivefilesize + *compressedsize > self->target_archive_size_bytes ||
            self->currentarchive_contents->qty > self->limitfilesperarchive)
        {
            check(svdp_archiver_advance_next_archive(self));
        }

        /* add directly into the tar file*/
        char namewithinarchive[PATH_MAX] = { 0 };
        snprintf(namewithinarchive, countof(namewithinarchive) - 1, "%08llx.file",
            castull(contentsid));
        check(svdp_tar_add_with_custom_name(
            self, cstr(self->currentarchive), pathinput, namewithinarchive));
        bstrlist_appendcstr(self->currentarchive_contents, namewithinarchive);
    }

    /* add to friendly names*/
    fprintf(self->friendlyfilenamesfile.file, "%08llx\t%s\n", castull(contentsid), pathinput);
    sv_array_add64u(&self->currentarchive_sizes, *compressedsize);
    *archivenumber = self->currentarchivenumber;

cleanup:
    return currenterr;
}

check_result svdp_archiver_finisharchiving(svdp_archiver *self)
{
    sv_result currenterr = {};
    bstring pattern = bstring_open();
    bstrlist *list = bstrlist_open();

    /* ensure that the archive is complete and that filenames.txt is written. */
    sv_log_writefmt("finish archiving %u", self->collectionid);
    check(svdp_archiver_advance_next_archive(self));

    /* kill any remnants of canceled backup operations in readytoupload */
    bassignformat(pattern, "%05llx_*.tar", castull(self->collectionid));
    check(os_tryuntil_deletefiles(cstr(self->path_readytoupload), cstr(pattern)));

    /* move files from tmp to readytoupload */
    int moved = 0;
    check(os_tryuntil_movebypattern(cstr(self->path_staging), "*.tar",
        cstr(self->path_readytoupload), true, &moved));

cleanup:
    bdestroy(pattern);
    bstrlist_close(list);
    return currenterr;
}

check_result get_tar_archive_parameter(const char *archive, bstring sout)
{
    sv_result currenterr = {};
    check_b(os_isabspath(archive), "require absolute path but got %s", archive);
    if (islinux)
    {
        bassignformat(sout, "--file=%s", archive);
    }
    else
    {
        check_b(archive[1] == ':' && archive[2] == '\\', "we currently require paths "
            "in the form x:\\ but got %s", archive);
        bstr_assignstatic(sout, "--file=/");
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

/* like a transaction, this writes to a temporary file before overwriting the main archive. */
check_result svdp_archiver_delete_from_archive(svdp_archiver *self, const char *archive,
    const char *dir_tmp_unarchived, const sv_array *contentids)
{
    sv_result currenterr = {};
    bstring getoutput = bstring_open();
    bstring output_tar = bformat("%s%souttmp.tar",
        cstr(self->path_intermediate_archives), pathsep);

    confirm_writable(dir_tmp_unarchived);
    confirm_writable(cstr(output_tar));
    if (!contentids || !contentids->length)
    {
        goto cleanup;
    }

    /* instead of deleting in batches, extract everything * to a subdirectory, then re-compress.
    1) deleting from the middle of a tar archive is not efficient, causes data to be rewritten
    2) even if we use a large batche size of 100, consider the case
    where there are 2000 small files to delete (not unlikely). even with batchsize=100 this
    could write cause on the order of 20*64mb of diskwrites.
    3) don't need to worry about batchsize or cmd limits, or dynamically building an array
    for that matter. */
    check_b(os_create_dirs(dir_tmp_unarchived), "could not create %s", dir_tmp_unarchived);
    check(os_tryuntil_deletefiles(dir_tmp_unarchived, "*"));
    check(svdp_7z_extract_overwrite_ok(self, archive, NULL, "*", dir_tmp_unarchived));
    sv_log_writefmt("delete_from_archive %s dir_tmp_unarchived=%s tmp=%s",
        archive, dir_tmp_unarchived, cstr(output_tar));
    for (uint32_t i = 0; i < contentids->length; i++)
    {
        sv_log_writefmt("del:%08llx", castull(sv_array_at64u(contentids, i)));
        bassignformat(self->tmp_filename, "%s%s%08llx.7z",
            dir_tmp_unarchived, pathsep, castull(sv_array_at64u(contentids, i)));
        check_b(os_remove(cstr(self->tmp_filename)), "could not remove %s",
            cstr(self->tmp_filename));
        bassignformat(self->tmp_filename, "%s%s%08llx.file",
            dir_tmp_unarchived, pathsep, castull(sv_array_at64u(contentids, i)));
        check_b(os_remove(cstr(self->tmp_filename)), "could not remove %s",
            cstr(self->tmp_filename));
    }

    /* create a new archive */
    check_b(os_remove(cstr(output_tar)), "could not remove %s", cstr(output_tar));
    check(os_listfiles(dir_tmp_unarchived, self->tmp_list, false));
    for (int i = 0; i < self->tmp_list->qty; i++)
    {
        check(svdp_tar_add(self, cstr(output_tar), bstrlist_view(self->tmp_list, i)));
    }

    check(os_tryuntil_deletefiles(dir_tmp_unarchived, "*"));
    check_b(os_tryuntil_move(cstr(output_tar), archive, true), "could not move %s overwriting %s",
        cstr(output_tar), archive);
cleanup:
    bdestroy(getoutput);
    bdestroy(output_tar);
    return currenterr;
}

check_result svdp_archiver_restore_from_archive(svdp_archiver *self,
    const char *archive, uint64_t contentid, const char *working_dir_archived,
    const char *working_dir_unarchived, const char *dest)
{
    sv_result currenterr = {};
    bstring firstdest = bstring_open();
    bstring destparent = bstring_open();
    bstring was_restrict_write_access = bstrcpy(restrict_write_access);
    confirm_writable(working_dir_archived);
    confirm_writable(working_dir_unarchived);

    sv_log_writefmt("restore %s file %08llx to %s", archive, contentid, dest);
    check_b(os_isabspath(archive) && os_file_exists(archive), "could not find archive %s.", archive);
    check_b(os_file_exists(cstr(self->path_compression_binary)), "could not find archiver.");
    check_b(contentid, "contentid cannot be 0.");

    /* extract the 7z from the tar */
    char namewithinarchive[PATH_MAX] = { 0 };
    snprintf(namewithinarchive, countof(namewithinarchive) - 1, "%08llx.7z", castull(contentid));
    bassignformat(firstdest, "%s%s%s", working_dir_archived, pathsep, namewithinarchive);
    check_b(os_tryuntil_remove(cstr(firstdest)), "Could not remove temporary file %s.", cstr(firstdest));
    check(svdp_7z_extract_overwrite_ok(self, archive, NULL, namewithinarchive, working_dir_archived));

    if (os_file_exists(cstr(firstdest)))
    {
        /* extract the file from the 7z */
        check(os_tryuntil_deletefiles(working_dir_unarchived, "*"));
        check(os_listfiles(working_dir_unarchived, self->tmp_list, false));
        check_b(self->tmp_list->qty == 0, "expected %s to be empty", working_dir_unarchived);
        check(svdp_7z_extract_overwrite_ok(self,
            cstr(firstdest), cstr(self->encryption), "*", working_dir_unarchived));

        check(os_listfiles(working_dir_unarchived, self->tmp_list, false));
        check_b(self->tmp_list->qty > 0, "nothing found in archive %s to get %s", namewithinarchive, dest);
    }
    else
    {
        /* look for a .file instead of a .7z and use it instead */
        snprintf(namewithinarchive, countof(namewithinarchive) - 1, "%08llx.file", castull(contentid));
        bassignformat(firstdest, "%s%s%s", working_dir_archived, pathsep, namewithinarchive);
        check_b(os_tryuntil_remove(cstr(firstdest)), "Could not remove temporary file %s.", cstr(firstdest));
        check(svdp_7z_extract_overwrite_ok(self, archive, NULL, namewithinarchive, working_dir_archived));
        check_b(os_file_exists(cstr(firstdest)), "did not find %s in archive %s", namewithinarchive, archive);
        bstrlist_clear(self->tmp_list);
        bstrlist_append(self->tmp_list, firstdest);
    }

    /* move to destination */
    os_get_parent(dest, destparent);
    check_b(os_isabspath(cstr(destparent)), "could not get parent of directory %s", dest);
    check_b(os_create_dirs(cstr(destparent)), "could not create directories for %s", cstr(destparent));
    bassigncstr(restrict_write_access, cstr(destparent));
    check_b(os_tryuntil_move(bstrlist_view(self->tmp_list, 0), dest, true),
        "could not move %s to %s", bstrlist_view(self->tmp_list, 0), dest);
    bassign(restrict_write_access, was_restrict_write_access);
    check_b(os_file_exists(dest), "expected to have moved file to %s", dest);
    check_b(self->tmp_list->qty == 1, "more than one file in archive %s to get %s", namewithinarchive, dest);
    log_b(os_tryuntil_remove(cstr(firstdest)), "Could not remove temporary file %s.", cstr(firstdest));

cleanup:
    bassign(restrict_write_access, was_restrict_write_access);
    bdestroy(was_restrict_write_access);
    bdestroy(firstdest);
    bdestroy(destparent);
    return currenterr;
}

/* this method is not used in the product, only by tests. */
/* does not support unicode (limitation of 7z, not us); does not support filenames containing newlines
or other 'interesting' characters */
/* itemtype is 0) unknown 1) directory 2) file added without compression 3) compressed file  */
check_result svdp_archiver_7z_list_parse_testsonly(bstring text, bstrlist *paths, sv_array *sizes,
    sv_array *crcs, sv_array *itemtypes)
{
    sv_result currenterr = {};
    sv_array_truncatelength(sizes, 0);
    sv_array_truncatelength(crcs, 0);
    sv_array_truncatelength(itemtypes, 0);
    bstrlist_clear(paths);

    bstr_replaceall(text, "\r\n", "\n");
    sv_pseudosplit split = sv_pseudosplit_open(cstr(text));
    sv_pseudosplit_split(&split, '\n');
    check_b(split.splitpoints.length > 4, "Expected to see many lines but only got %d",
        split.splitpoints.length);

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
            bstrlist_appendcstr(paths, sv_pseudosplit_viewat(&split, i) + strlen("Path = "));
            sv_array_add64u(sizes, 0);
            sv_array_add64u(crcs, 0);
            sv_array_add64u(itemtypes, 0);
        }
        else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Size = "))
        {
            uint64_t size = 0;
            check_b(uintfromstring(sv_pseudosplit_viewat(&split, i) + strlen("Size = "), &size),
                "could not parse size %s", sv_pseudosplit_viewat(&split, i));
            *(uint64_t *)(sv_array_at(sizes, sizes->length - 1)) = size;
        }
        else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "CRC = "))
        {
            uint64_t crc = 0;
            check_b(uintfromstringhex(sv_pseudosplit_viewat(&split, i) + strlen("CRC = "), &crc),
                "could not parse crc %s", sv_pseudosplit_viewat(&split, i));
            *(uint64_t *)(sv_array_at(crcs, crcs->length - 1)) = crc;
        }
        else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Folder = +"))
        {
            *(uint64_t *)(sv_array_at(itemtypes, itemtypes->length - 1)) = 1;
        }
        else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Method = Copy") &&
            !sv_array_at64u(itemtypes, itemtypes->length - 1))
        {
            *(uint64_t *)(sv_array_at(itemtypes, itemtypes->length - 1)) = 2;
        }
        else if (started && s_startswith(sv_pseudosplit_viewat(&split, i), "Method = ") &&
            !sv_array_at64u(itemtypes, itemtypes->length - 1))
        {
            *(uint64_t *)(sv_array_at(itemtypes, itemtypes->length - 1)) = 3;
        }
    }
cleanup:
    sv_pseudosplit_close(&split);
    return currenterr;
}

/* this method is not used in the product, only by tests. */
check_result svdp_archiver_7z_list_testsonly(svdp_archiver *self, const char *archive,
    bstrlist *paths, sv_array *sizes,
    sv_array *crcs, sv_array *itemtypes)
{
    sv_result currenterr = {};
    check_b(os_isabspath(archive) && os_file_exists(archive), "could not find archive %s.", archive);
    check_b(os_file_exists(cstr(self->path_compression_binary)), "could not find archiver.");
    bassignformat(self->tmp_arg_encryption, "-p%s", cstr(self->encryption));

    const char *args[] = {
        linuxonly(cstr(self->path_compression_binary))
        linuxonly("-no-utf16")
        "l",
        "-slt", /* show technical information (we want to see the crcs) */
        archive,
        blength(self->encryption) ? cstr(self->tmp_arg_encryption) : NULL,
        NULL
    };

    bstrclear(self->tmp_resultstring);
    int retcode = os_tryuntil_run_process(cstr(self->path_compression_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring);
    check_b(retcode == 0, "Failed with (%d) to %s stderr=%s", retcode, cstr(self->currentarchive),
        cstr(self->tmp_resultstring));
    check(svdp_archiver_7z_list_parse_testsonly(self->tmp_resultstring, paths, sizes, crcs, itemtypes));

cleanup:
    return currenterr;
}

check_result svdp_archiver_7z_list_string_testsonly(svdp_archiver *self, const char *archive,
    bstrlist *results)
{
    sv_result currenterr = {};
    sv_array sizes = sv_array_open(sizeof32u(uint64_t), 0);
    sv_array crcs = sv_array_open(sizeof32u(uint64_t), 0);
    sv_array itemtypes = sv_array_open(sizeof32u(uint64_t), 0);
    check(svdp_archiver_7z_list_testsonly(self, archive, results, &sizes, &crcs, &itemtypes));
    for (uint32_t i = 0; i < cast32s32u(results->qty); i++)
    {
        bformata(results->entry[i], " contentlength=%llu crc=%llx itemtype=%llu",
            castull(sv_array_at64u(&sizes, i)), castull(sv_array_at64u(&crcs, i)),
            castull(sv_array_at64u(&itemtypes, i)));
    }
cleanup:
    sv_array_close(&sizes);
    sv_array_close(&crcs);
    sv_array_close(&itemtypes);
    return currenterr;
}

check_result svdp_archiver_tar_list_string_testsonly(svdp_archiver *self,
    const char *archive, bstrlist *results, const char *subworkingdir)
{
    sv_result currenterr = {};
    bstrlist *listof7z = bstrlist_open();
    if (!os_file_exists(archive))
    {
        bstrlist_splitcstr(results, "not_found", "|");
        goto cleanup;
    }

    confirm_writable(subworkingdir);
    check(os_tryuntil_deletefiles(subworkingdir, "*"));
    check_b(os_create_dirs(subworkingdir), "%s", subworkingdir);
    check(svdp_7z_extract_overwrite_ok(self, archive, NULL, "*", subworkingdir));
    check(os_listdirs(subworkingdir, self->tmp_list, false));
    check_b(self->tmp_list->qty == 0, "the directory %s has subdirectories, which we don't "
        "expect. please remove them. or the archive %s has subdirs", subworkingdir, archive);

    bstrlist_clear(results);
    check(os_listfiles(subworkingdir, self->tmp_list, islinux /* need to sort */));
    for (int i = 0; i < self->tmp_list->qty; i++)
    {
        os_get_filename(bstrlist_view(self->tmp_list, i), self->tmp_filename);
        bstrlist_append(results, self->tmp_filename);
        if (s_endswith(cstr(self->tmp_filename), ".7z"))
        {
            check(svdp_archiver_7z_list_string_testsonly(self,
                bstrlist_view(self->tmp_list, i), listof7z));

            bstrlist_concat(results, listof7z);
        }
    }

    check(os_tryuntil_deletefiles(subworkingdir, "*"));

cleanup:
    bstrlist_close(listof7z);
    return currenterr;
}

check_result svdp_7z_extract_overwrite_ok(svdp_archiver *self, const char *archive,
    const char *encryption, const char *internalarchivepath, const char *outputdir)
{
    sv_result currenterr = {};
    bstring param_outdir = bstring_open();
    bassigncstr(param_outdir, "-o");
    bcatcstr(param_outdir, outputdir);
    check_b(os_file_exists(archive), "archive path %s not found", archive);
    check_b(os_dir_exists(outputdir), "output dir %s not found", outputdir);
    bassignformat(self->tmp_arg_encryption, "-p%s", encryption ? encryption : "");
    confirm_writable(outputdir);

    const char *args[] = {
        linuxonly(cstr(self->path_compression_binary))
        linuxonly("-no-utf16")
        "e", /* we'd use x if we wanted to preserve paths */
        archive,
        cstr(param_outdir),
        internalarchivepath,
        "-aoa", /* replace existing files */
        encryption ? cstr(self->tmp_arg_encryption) : NULL,
        NULL
    };

    int retcode = os_tryuntil_run_process(cstr(self->path_compression_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring);
    check_b(retcode == 0, "Failed with (%d) to %s stderr=%s", retcode, cstr(self->currentarchive),
        cstr(self->tmp_resultstring));

cleanup:
    bdestroy(param_outdir);
    return currenterr;
}

check_result svdp_tar_verify_contents(svdp_archiver *self, const char *archive,
    bstrlist *expectedcontents, sv_array *expectedsizes)
{
    sv_result currenterr = {};
    bassignformat(self->tmp_arg_encryption, "-p%s", cstr(self->encryption));
    bool is_encrypted = blength(self->encryption) > 0;
    const char *args[] = {
        linuxonly(cstr(self->path_compression_binary))
        linuxonly("-no-utf16")
        "l",
        "-slt", /* show details */
        archive,
        is_encrypted ? cstr(self->tmp_arg_encryption) : NULL,
        NULL
    };

    int retcode = -1;
    check(os_run_process(cstr(self->path_compression_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring, &retcode));
    check_b(retcode == 0, "Failed with %d to %s (%s)", retcode, archive,
        cstr(self->tmp_resultstring));

    /* the files were added in order, and so we can make the search faster by
    moving the search-start position forward after each match */
    int pos = 0;
    check_b(cast32s32u(expectedcontents->qty) == expectedsizes->length, "expected same length");
    for (int i = 0; i < expectedcontents->qty; i++)
    {
        bassignformat(self->tmp_argscombined, nativenewline "Path = %s" nativenewline,
            bstrlist_view(expectedcontents, i));
        pos = binstr(self->tmp_resultstring, pos + blength(self->tmp_argscombined),
            self->tmp_argscombined);
        check_b(pos != BSTR_ERR, "could not find entry for file %s in %s",
            cstr(self->tmp_argscombined), cstr(self->tmp_resultstring));

        bassignformat(self->tmp_argscombined, nativenewline "Size = %llu" nativenewline,
            castull(sv_array_at64u(expectedsizes, cast32s32u(i))));
        pos = binstr(self->tmp_resultstring, pos + blength(self->tmp_argscombined),
            self->tmp_argscombined);
        check_b(pos != BSTR_ERR, "could not find entry for file %s in %s",
            cstr(self->tmp_argscombined), cstr(self->tmp_resultstring));
    }

cleanup:
    return currenterr;
}

check_result svdp_archiver_verify_has_one_item(svdp_archiver *self, const char *path7zfile,
    const char *inputfilename, const char *encryption)
{
    sv_result currenterr = {};
    bassignformat(self->tmp_arg_encryption, "-p%s", encryption ? encryption : "");
    const char *args[] = {
        linuxonly(cstr(self->path_compression_binary))
        linuxonly("-no-utf16")
        "l",
        "-slt", /* show details */
        path7zfile,
        encryption ? cstr(self->tmp_arg_encryption) : NULL,
        NULL
    };

    int retcode = -1;
    check(os_run_process(cstr(self->path_compression_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring, &retcode));
    check_b(retcode == 0, "Failed with %d to %s (%s)", retcode, path7zfile,
        cstr(self->tmp_resultstring));

    bstrlist_splitcstr(self->tmp_list, cstr(self->tmp_resultstring), nativenewline "Path = ");
    if (!inputfilename || !s_contains(inputfilename, "\nPath = "))
    {
        check_b(self->tmp_list->qty == 3, "expected two Path = entries but got %s",
            cstr(self->tmp_resultstring));
    }

    if (inputfilename)
    {
        bassignformat(self->tmp_argscombined, nativenewline "Size = %llu" nativenewline,
            castull(os_getfilesize(inputfilename)));
        bool foundcorrectsize = binstr(self->tmp_resultstring, 0,
            self->tmp_argscombined) != BSTR_ERR;
        check_b(foundcorrectsize, "expected correct size %s but got %s",
            cstr(self->tmp_argscombined), cstr(self->tmp_resultstring));
    }

cleanup:
    return currenterr;
}

check_result svdp_7z_add(svdp_archiver *self, const char *path7zfile, const char *inputfilepath, bool is_compressed)
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
    3) we already are splitting into multiple archives. unnecessary complexity to have two
    different splitting methods.

    we now are creating a .tar containing .7z files
        we don't just make a .7z containing the user's files because appending to a 7z, even with
        solidmode=off, 7z re-writes the entire archive
        add in two steps, user-file -> 000001.file.7z -> 00001_00001.tar
        we used to have to take the file data and pass it via stdin to 7z, mainly so that we could
        specify our own filename
    */

    os_get_parent(inputfilepath, self->tmp_parentdir);
    os_get_filename(inputfilepath, self->tmp_filename);
    os_setcwd(cstr(self->tmp_parentdir));
    confirm_writable(path7zfile);

    check_b(!os_file_exists(path7zfile), "file should not exist yet %s", path7zfile);
    bassignformat(self->tmp_arg_encryption, "-p%s", cstr(self->encryption));
    bool is_encrypted = blength(self->encryption) > 0;
    const char *args[] = {
        linuxonly(cstr(self->path_compression_binary))
        linuxonly("-no-utf16")
        "a",
        path7zfile,
        cstr(self->tmp_filename),
        "-ms=off", /* turn off solid mode */
        is_compressed ? "-mx0" : "-mx3", /* add this file without compression */
        is_encrypted ? "-mhe" : NULL,
        is_encrypted ? cstr(self->tmp_arg_encryption) : NULL,
        NULL
    };

    int retcode = os_tryuntil_run_process(cstr(self->path_compression_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring);
    check_b(retcode == 0, "Failed with %d to %s (%s)", retcode, path7zfile,
        cstr(self->tmp_resultstring));
    check(svdp_archiver_verify_has_one_item(self, path7zfile, inputfilepath,
        is_encrypted ? cstr(self->encryption) : NULL));
cleanup:
    return currenterr;
}

check_result svdp_tar_add(svdp_archiver *self, const char *tarname, const char *inputfile)
{
    sv_result currenterr = {};
    os_get_parent(inputfile, self->tmp_parentdir);
    os_get_filename(inputfile, self->tmp_filename);
    os_setcwd(cstr(self->tmp_parentdir));
    confirm_writable(tarname);

    check(get_tar_archive_parameter(tarname, self->tmp_arg_input));
    const char *args[] = {
        linuxonly(cstr(self->path_archiver_binary))
        "--append",
        "--dereference", /* add the file that a symlink points to, not the symlink. */
        "--no-wildcards", /* no parameters use wildcards */
        "--no-unquote", /* otherwise, backslash escape sequences are parsed */
        "--format=gnu",
        cstr(self->tmp_arg_input),
        cstr(self->tmp_filename),
        NULL
    };

    int retcode = os_tryuntil_run_process(cstr(self->path_archiver_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring);
    check_b(retcode == 0, "Failed with %d to %s %s (%s)",
        retcode, tarname, inputfile, cstr(self->tmp_resultstring));

cleanup:
    return currenterr;
}

check_result svdp_tar_add_with_custom_name(svdp_archiver *self, const char *tarname,
    const char *inputfile, const char *namewithinarchive)
{
    sv_result currenterr = {};
    os_get_parent(inputfile, self->tmp_parentdir);
    os_get_filename(inputfile, self->tmp_filename);
    os_setcwd(cstr(self->tmp_parentdir));
    confirm_writable(tarname);
    bstring transformfilename = bformat("--transform=s/.*/%s/", namewithinarchive);
    bstr_replaceall(transformfilename, "\\", "\\\\");
    /* otherwise, backslash escape sequences are parsed */
    check_b(!s_contains(namewithinarchive, "/"), "name cannot contain / %s", namewithinarchive);

    check(get_tar_archive_parameter(tarname, self->tmp_arg_input));
    const char *args[] = {
        linuxonly(cstr(self->path_archiver_binary))
        "--append",
        "--dereference", /* add the file that a symlink points to, not the symlink. */
        "--no-wildcards", /* no parameters use wildcards */
        "--no-unquote", /* otherwise, backslash escape sequences are parsed */
        "--format=gnu",
        cstr(transformfilename),
        cstr(self->tmp_arg_input),
        cstr(self->tmp_filename),
        NULL
    };

    int retcode = os_tryuntil_run_process(cstr(self->path_archiver_binary), args, true,
        self->tmp_argscombined, self->tmp_resultstring);
    check_b(retcode == 0, "Failed with %d to %s %s (%s)", retcode, tarname,
        inputfile, cstr(self->tmp_resultstring));

cleanup:
    bdestroy(transformfilename);
    return currenterr;
}

const hash256 hash256zeros = {};
extern inline void hash256tostr(const hash256 *h1, bstring s);
extern inline bool hash256eq(const hash256 *h1, const hash256 *h2);

void svdp_archiver_close(svdp_archiver *self)
{
    if (self)
    {
        bdestroy(self->path_intermediate_archives);
        bdestroy(self->path_staging);
        bdestroy(self->path_readytoupload);
        bdestroy(self->currentarchive);
        bdestroy(self->encryption);
        bdestroy(self->path_compression_binary);
        bdestroy(self->path_archiver_binary);
        bdestroy(self->path_audiometadata_binary);
        bdestroy(self->tmp_argscombined);
        bdestroy(self->tmp_resultstring);
        bdestroy(self->tmp_arg_encryption);
        bdestroy(self->tmp_arg_input);
        bdestroy(self->tmp_filename);
        bdestroy(self->tmp_parentdir);
        bstrlist_close(self->tmp_list);
        sv_array_close(&self->currentarchive_sizes);
        sv_file_close(&self->friendlyfilenamesfile);
        set_self_zero();
    }
}

