/*
util_archiver.c

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

check_result ar_manager_open(ar_manager *self, const char *pathapp,
    const char *grpname, uint32_t collectionid, uint32_t archivesize)
{
    set_self_zero();
    self->ar = ar_util_open();
    self->path_working =
        bformat("%s%stemp%sarchived", pathapp, pathsep, pathsep);
    self->path_staging = bformat("%s%suserdata%s%s%sstaging", pathapp, pathsep,
        pathsep, grpname, pathsep);
    self->path_readytoupload = bformat("%s%suserdata%s%s%sreadytoupload",
        pathapp, pathsep, pathsep, grpname, pathsep);

    self->limitperarchive = 25 * 1000;
    self->collectionid = collectionid;
    self->target_archive_size = archivesize;
    self->currentarchive = bstring_open();
    self->current_names = bstrlist_open();
    self->current_sizes = sv_array_open_u64();
    return OK;
}

check_result ar_manager_begin(ar_manager *self)
{
    sv_result currenterr = {};

    /* clear tmp folders */
    sv_log_fmt("ar_manager_begin, collection=%u", self->collectionid);
    check_b(os_create_dirs(cstr(self->path_working)),
        "couldn't create or access %s", cstr(self->path_working));
    check(os_tryuntil_deletefiles(cstr(self->path_working), "*"));
    check_b(os_create_dirs(cstr(self->path_staging)),
        "couldn't create or access %s", cstr(self->path_staging));
    check(os_tryuntil_deletefiles(cstr(self->path_staging), "*"));
    self->currentarchivenum = 0;
    check(ar_manager_advance_to_next(self));

cleanup:
    return currenterr;
}

check_result ar_manager_advance_to_next(ar_manager *self)
{
    sv_result currenterr = {};
    bstring namestextpath =
        bformat("%s%sfilenames.txt", cstr(self->path_working), pathsep);
    sv_log_fmt("finishing archive %s with %d files", cstr(self->currentarchive),
        self->current_names->qty);

    if (self->currentarchivenum > 0 && self->current_names->qty > 0)
    {
        /* add filenames.txt */
        if (os_file_exists(cstr(namestextpath)))
        {
            sv_file_close(&self->namestextfile);
            sv_log_fmt("adding %s", cstr(namestextpath));
            check(ar_util_add(&self->ar, cstr(self->currentarchive),
                cstr(namestextpath), "filenames.txt", 0));
        }

        check(ar_util_verify(&self->ar, cstr(self->currentarchive),
            self->current_names, &self->current_sizes));
    }

    self->currentarchivenum++;
    bstrlist_clear(self->current_names);
    sv_array_truncatelength(&self->current_sizes, 0);
    bsetfmt(self->currentarchive, "%s%s%05x_%05x.tar", cstr(self->path_staging),
        pathsep, self->collectionid, self->currentarchivenum);

    sv_log_writes("starting", cstr(self->currentarchive));
    sv_file_close(&self->namestextfile);
    check(sv_file_open(&self->namestextfile, cstr(namestextpath), "w"));

cleanup:
    bdestroy(namestextpath);
    return currenterr;
}

check_result ar_manager_add(ar_manager *self, const char *input,
    bool already_compressed, uint64_t contentid, uint32_t *archivenumber,
    uint64_t *compressedsize)
{
    sv_result currenterr = {};
    *compressedsize = 0;
    bool add_file_directly_to_tar = already_compressed;
    uint64_t archivesize = os_getfilesize(cstr(self->currentarchive));

    if (add_file_directly_to_tar)
    {
        *compressedsize = os_getfilesize(input);
        if (archivesize + *compressedsize > self->target_archive_size ||
            self->current_names->qty > self->limitperarchive)
        {
            check(ar_manager_advance_to_next(self));
        }

        /* add directly into the tar */
        char namewithinarchive[PATH_MAX] = {0};
        snprintf(namewithinarchive, countof(namewithinarchive) - 1,
            "%08llx.file", castull(contentid));
        check(ar_util_add(&self->ar, cstr(self->currentarchive), input,
            namewithinarchive, *compressedsize));
        bstrlist_appendcstr(self->current_names, namewithinarchive);
    }
    else
    {
        /* make a xz file */
        bsetfmt(self->ar.tmp_xz_name, "%s%s%08llx.xz", cstr(self->path_working),
            pathsep, castull(contentid));
        check_b(os_tryuntil_remove(cstr(self->ar.tmp_xz_name)),
            "couldn't delete %s", cstr(self->ar.tmp_xz_name));
        check(ar_util_xz_add(&self->ar, input, cstr(self->ar.tmp_xz_name)));

        *compressedsize = os_getfilesize(cstr(self->ar.tmp_xz_name));
        if (archivesize + *compressedsize > self->target_archive_size ||
            self->current_names->qty > self->limitperarchive)
        {
            check(ar_manager_advance_to_next(self));
        }

        /* add xz file to the tar */
        char namewithin[PATH_MAX] = {0};
        snprintf(namewithin, countof(namewithin) - 1, "%08llx.xz",
            castull(contentid));
        check_b(*compressedsize > 0, "file %s cannot have size 0",
            cstr(self->ar.tmp_xz_name));
        check(ar_util_add(&self->ar, cstr(self->currentarchive),
            cstr(self->ar.tmp_xz_name), namewithin, *compressedsize));
        log_b(os_tryuntil_remove(cstr(self->ar.tmp_xz_name)),
            "couldn't delete %s", cstr(self->ar.tmp_xz_name));
        os_get_filename(cstr(self->ar.tmp_xz_name), self->ar.tmp_filename);
        bstrlist_append(self->current_names, self->ar.tmp_filename);
    }

    *archivenumber = self->currentarchivenum;
    sv_array_add64u(&self->current_sizes, *compressedsize);
    fprintf(self->namestextfile.file, "%08llx\t%s\n", castull(contentid), input);

cleanup:
    return currenterr;
}

check_result ar_manager_finish(ar_manager *self)
{
    sv_result currenterr = {};
    bstring pattern = bstring_open();
    bstrlist *list = bstrlist_open();

    /* ensure that archive is complete. */
    sv_log_fmt("finish archiving %u", self->collectionid);
    check(ar_manager_advance_to_next(self));

    /* delete temporary files */
    bsetfmt(pattern, "%05llx_*.tar", castull(self->collectionid));
    check(
        os_tryuntil_deletefiles(cstr(self->path_readytoupload), cstr(pattern)));

    /* move files from staging to readytoupload */
    int moved = 0;
    check(os_tryuntil_movebypattern(cstr(self->path_staging), "*.tar",
        cstr(self->path_readytoupload), true, &moved));

cleanup:
    bdestroy(pattern);
    bstrlist_close(list);
    return currenterr;
}

check_result ar_manager_restore(ar_manager *self, const char *archive,
    uint64_t contentid, const char *working_dir_archived, const char *dest)
{
    sv_result currenterr = {};
    bstring destparent = bstring_open();
    bstring was_restrict_write_access = bstrcpy(restrict_write_access);
    bstring namewithin = bformat("%08llx.*", castull(contentid));
    bstring path_file = bformat(
        "%s%s%08llx.file", working_dir_archived, pathsep, castull(contentid));
    bstring path_xz = bformat(
        "%s%s%08llx.xz", working_dir_archived, pathsep, castull(contentid));

    sv_log_fmt("restore %s file %08llx to %s", archive, contentid, dest);
    check_b(os_isabspath(archive) && os_file_exists(archive),
        "couldn't find archive %s.", archive);
    check_b(os_file_exists(cstr(self->ar.xz_binary)), "couldn't find archiver.");
    check_b(contentid, "contentid cannot be 0.");
    check_b(os_tryuntil_remove(cstr(path_file)), "couldn't remove %s",
        cstr(path_file));
    check_b(
        os_tryuntil_remove(cstr(path_xz)), "couldn't remove %s", cstr(path_xz));

    check(ar_util_extract_overwrite(&self->ar, archive, cstr(namewithin),
        working_dir_archived, self->ar.tmp_results));

    if (os_file_exists(cstr(path_file)))
    {
        /* file wasn't compressed, no decompression needed */
    }
    else if (os_file_exists(cstr(path_xz)))
    {
        /* it's a .xz archive */
        check(ar_util_xz_extract_overwrite(
            &self->ar, cstr(path_xz), cstr(path_file)));
    }
    else
    {
        check_b(false, "nothing found for %s in archive %s", cstr(path_file),
            archive);
    }

    /* move to destination */
    os_get_parent(dest, destparent);
    check_b(os_isabspath(cstr(destparent)),
        "couldn't get parent of directory %s", dest);
    check_b(os_create_dirs(cstr(destparent)),
        "couldn't create directories for %s", cstr(destparent));
    bassigncstr(restrict_write_access, cstr(destparent));
    check_b(os_tryuntil_move(cstr(path_file), dest, true),
        "couldn't move %s to %s", cstr(path_file), dest);
    check_b(os_file_exists(dest), "expected to have moved file to %s", dest);

cleanup:
    bassign(restrict_write_access, was_restrict_write_access);
    bdestroy(was_restrict_write_access);
    bdestroy(destparent);
    bdestroy(namewithin);
    bdestroy(path_file);
    bdestroy(path_xz);
    return currenterr;
}

ar_util ar_util_open(void)
{
    ar_util self = {};
    self.tar_binary = bstring_open();
    self.xz_binary = bstring_open();
    self.audiotag_binary = bstring_open();
    self.tmp_arg_tar = bstring_open();
    self.tmp_inner_name = bstring_open();
    self.tmp_combined = bstring_open();
    self.tmp_ascii = bstring_open();
    self.tmp_results = bstring_open();
    self.tmp_filename = bstring_open();
    self.tmp_xz_name = bstring_open();
    self.list = bstrlist_open();
    return self;
}

check_result get_tar_archive_parameter(const char *archive, bstring s)
{
    sv_result currenterr = {};
    check_b(os_isabspath(archive), "not abs path %s", archive);
    if (islinux)
    {
        bsetfmt(s, "--file=%s", archive);
    }
    else
    {
        /* from C:\dir\file.tar to --file=/C/dir/file.tar
        another possibility: -C //./C:/dir --file=file.tar
        another possibility, if on the same drive: --file=\dir\file.tar */
        check_b(archive[1] == ':' && archive[2] == '\\',
            "we currently require paths in the form x:\\ but got %s", archive);
        bstr_assignstatic(s, "--file=/");
        bconchar(s, archive[0]);
        bcatcstr(s, archive + 2);
        bstr_replaceall(s, "\\", "/");
    }

cleanup:
    return currenterr;
}

check_result ar_util_add(ar_util *self, const char *tarpath,
    const char *inputpath, const char *namewithin, uint64_t inputsize)
{
    sv_result currenterr = {};
    confirm_writable(tarpath);
    check_b(s_endwith(tarpath, ".tar"), "%s", tarpath);
    bsetfmt(self->tmp_inner_name, "--transform=s/.*/%s/", namewithin);
    bstr_replaceall(self->tmp_inner_name, "\\", "\\\\"); /* escape backslashes */
    check_b(
        !s_contains(namewithin, "/"), "name cannot contain / %s", namewithin);
    check_b(islinux || (inputsize < 1024ULL * 1024 * 1024),
        "tar on windows does not support files of this size. %s %llu", inputpath,
        castull(inputsize));

    check(get_tar_archive_parameter(tarpath, self->tmp_arg_tar));
    const char *args[] = {linuxonly(cstr(self->tar_binary)) "--append",
        "--dereference", /* add file symlink points to, not symlink. */
        "--no-wildcards", /* do not use wildcards */
        "--no-unquote", /* do not parse backslash escape sequences */
        "--format=gnu", cstr(self->tmp_inner_name), cstr(self->tmp_arg_tar),
        "--", inputpath, NULL};

    bsetfmt(self->tmp_results, "Context: tar add %s %s", tarpath, inputpath);
    check(os_tryuntil_run(cstr(self->tar_binary), args, self->tmp_results,
        self->tmp_combined, true, 1 /* exit code 1 is ok */, 0));

cleanup:
    return currenterr;
}

check_result ar_util_verify(ar_util *self, const char *tarpath,
    const bstrlist *expectedcontents, const sv_array *expectedsizes)
{
    sv_result currenterr = {};
    check_b(s_endwith(tarpath, ".tar"), "%s", tarpath);
    check(get_tar_archive_parameter(tarpath, self->tmp_arg_tar));
    const char *args[] = {linuxonly(cstr(self->tar_binary)) "--list",
        "--verbose", cstr(self->tmp_arg_tar), NULL};

    bsetfmt(self->tmp_results, "Context: tar verify %s", tarpath);
    check(os_tryuntil_run(cstr(self->tar_binary), args, self->tmp_results,
        self->tmp_combined, true, 0, 0));
    check_b(cast32s32u(expectedcontents->qty) == expectedsizes->length,
        "%d != %d", expectedcontents->qty, expectedsizes->length);

    /* walk through the tar file listing, checking filename and size. */
    int pos = 0;
    for (int i = 0; i < expectedcontents->qty; i++)
    {
        bsetfmt(self->tmp_filename, " %llu ",
            castull(sv_array_at64u(expectedsizes, cast32s32u(i))));
        pos = binstr(self->tmp_results, pos + 1, self->tmp_filename);
        check_b(pos != BSTR_ERR,
            "couldn't find entry for filesize %s "
            "in %s",
            cstr(self->tmp_filename), cstr(self->tmp_results));

        bsetfmt(self->tmp_filename, " %s\n", blist_view(expectedcontents, i));
        pos = binstr(self->tmp_results, pos + 1, self->tmp_filename);
        check_b(pos != BSTR_ERR, "couldn't find entry for file %s in %s",
            cstr(self->tmp_filename), cstr(self->tmp_results));
    }

cleanup:
    return currenterr;
}

check_result ar_util_xz_verify_impl(ar_util *self, const char *path)
{
    sv_result currenterr = {};
    check_b(os_getfilesize(path) > 0, "%s should not be 0 bytes", path);
    const char *args[] = {linuxonly(cstr(self->xz_binary)) "--test", "--verbose",
        "--", /* in case path begins with a - */
        path, NULL};

    bsetfmt(self->tmp_results, "Context: xz validate %s", path);
    check(os_tryuntil_run(cstr(self->xz_binary), args, self->tmp_results,
        self->tmp_combined, true, 1 /* exit code 1 is ok */, 0));

    if (s_contains(cstr(self->tmp_results), ".xz:") ||
        s_contains(cstr(self->tmp_results), ".XZ:"))
    {
        os_get_filename(path, self->tmp_filename);
        bstr_replaceall(self->tmp_results, path, "");
        bstr_replaceall(self->tmp_results, cstr(self->tmp_filename), "");
        int countcolons = 0;
        for (int i = 0; i < blength(self->tmp_results); i++)
        {
            if (cstr(self->tmp_results)[i] == ':')
            {
                countcolons++;
            }
        }

        check_b(countcolons == 1,
            "results from xz validate indicate error, %s %s", path,
            cstr(self->tmp_results));
    }

cleanup:
    return currenterr;
}

check_result ar_util_xz_add_impl(
    ar_util *self, const char *input, const char *dest)
{
    sv_result currenterr = {};
    const char *args[] = {linuxonly(cstr(self->xz_binary)) "--compress",
        "--keep", "--stdout",
        "--force", /* compress even if hardlink, setuid, etc */
        "--check=crc32", /* for added compatibility */
        "-6", /* compression strength 6 */
        "--", /* in case path begins with a - */
        input, NULL};

    /* redirect stdout; there is no flag to specify location. */
    confirm_writable(dest);
    bsetfmt(self->tmp_results, "Context: xz add %s", input);
    check(os_tryuntil_run(cstr(self->xz_binary), args, self->tmp_results,
        self->tmp_combined, true, 1 /* exit code 1 is ok */, dest));
    check_b(os_file_exists(dest), "couldn't find output %s", dest);

    /* xz sometimes returns exit code 1 (success) even on fatal errors.
    for example, hold an exclusive lock, it writes an error to stderr
    but returns 1. so, double-check that the file is good. */
    check(ar_util_xz_verify(self, dest));

cleanup:
    return currenterr;
}

check_result ar_util_xz_extract_overwrite_impl(
    ar_util *self, const char *archive, const char *dest)
{
    sv_result currenterr = {};
    const char *args[] = {linuxonly(cstr(self->xz_binary)) "--decompress",
        "--keep", "--stdout", "--quiet",
        "--quiet", /* add --quiet twice to suppress messages in stderr */
        "--force", /* decompress even if setuid, etc */
        "--", /* in case path begins with a - */
        archive, NULL};

    /* redirect stdout; there is no flag to specify location. */
    confirm_writable(dest);
    bsetfmt(self->tmp_results, "Context: xz extract %s", archive);
    check(os_tryuntil_run(cstr(self->xz_binary), args, self->tmp_results,
        self->tmp_combined, true, 1 /* exit code 1 is ok */, dest));
    check_b(os_file_exists(dest), "couldn't find output %s", dest);

cleanup:
    return currenterr;
}

check_result ar_util_xz_add(ar_util *self, const char *input, const char *dest)
{
    /* xz-utils on windows does not support unicode names, so use the
    8.3 short path as a workaround. */
    sv_result currenterr = {};
    check_b(os_get_short_path(input, self->tmp_ascii),
        "couldn't get short path %s", input);
    check_b(os_file_exists(cstr(self->tmp_ascii)),
        "couldn't see short path %s %s", input, cstr(self->tmp_ascii));
    check(ar_util_xz_add_impl(self, cstr(self->tmp_ascii), dest));

cleanup:
    return currenterr;
}

check_result ar_util_xz_extract_overwrite(
    ar_util *self, const char *path, const char *dest)
{
    sv_result currenterr = {};
    check_b(os_get_short_path(path, self->tmp_ascii),
        "couldn't get short path %s", path);
    check_b(os_file_exists(cstr(self->tmp_ascii)),
        "couldn't see short path %s %s", path, cstr(self->tmp_ascii));
    check(ar_util_xz_extract_overwrite_impl(self, cstr(self->tmp_ascii), dest));

cleanup:
    return currenterr;
}

check_result ar_util_xz_verify(ar_util *self, const char *path)
{
    sv_result currenterr = {};
    check_b(os_get_short_path(path, self->tmp_ascii),
        "couldn't get short path %s", path);
    check_b(os_file_exists(cstr(self->tmp_ascii)),
        "couldn't see short path %s %s", path, cstr(self->tmp_ascii));
    check(ar_util_xz_verify_impl(self, cstr(self->tmp_ascii)));

cleanup:
    return currenterr;
}

check_result ar_util_extract_overwrite(ar_util *self, const char *archive,
    const char *namewithin, const char *tmpdir, bstring saved_to)
{
    sv_result currenterr = {};
    bsetfmt(self->tmp_filename, "%s%s%s", tmpdir, pathsep, namewithin);
    check_b(os_tryuntil_remove(cstr(self->tmp_filename)), "couldn't remove %s",
        cstr(self->tmp_filename));
    check(get_tar_archive_parameter(archive, self->tmp_arg_tar));
    const char *args[] = {linuxonly(cstr(self->tar_binary)) "--extract",
        "--no-same-owner", /* save as current user, we'll chown later */
        "--no-same-permissions", /* save as default mode, we'll chmod later */
        "--wildcards", /* extracting '*' will extract all files */
        "--unlink-first", /* overwrite any existing file */
        cstr(self->tmp_arg_tar), "--", namewithin, NULL};

    confirm_writable(tmpdir);
    check_b(os_setcwd(tmpdir), "failed to set wd %s", tmpdir);
    bsetfmt(
        self->tmp_results, "Context: tar extract %s %s", archive, namewithin);
    check(os_tryuntil_run(cstr(self->tar_binary), args, self->tmp_results,
        self->tmp_combined, true, 0, 0));
    check_b(
        s_endwith(namewithin, "*") || os_file_exists(cstr(self->tmp_filename)),
        "file not found at %s", cstr(self->tmp_filename));
    bassign(saved_to, self->tmp_filename);

cleanup:
    return currenterr;
}

check_result ar_util_delete(ar_util *self, const char *archive,
    const char *tmpdir_tar, const char *tmpdir, const sv_array *contentids)
{
    sv_result currenterr = {};
    bstring out = bformat("%s%souttmp.tar", tmpdir_tar, pathsep);
    bstring innername = bstring_open();
    confirm_writable(tmpdir);
    confirm_writable(cstr(out));
    if (!contentids || !contentids->length)
    {
        goto cleanup;
    }

    /* create a "output_tar" before overwriting the archive for resiliency.
    why create a new archive instead of using tar --delete ?
    1) consider deleting 2000 small files from an archive.
    even in batches of 100, this is 20 * 64Mb of disk writes, inefficient.
    2) avoid the need to be concerned with cmd limits and building arg list.
    3) tar --delete fails if any of the names aren't within the archive. */
    check_b(os_create_dirs(tmpdir), "couldn't create %s", tmpdir);
    check(os_tryuntil_deletefiles(tmpdir, "*"));
    check(ar_util_extract_overwrite(self, archive, "*", tmpdir, NULL));
    sv_log_fmt("delete_from_archive %s tmpdir=%s "
               "tmp=%s",
        archive, tmpdir, cstr(out));

    for (uint32_t i = 0; i < contentids->length; i++)
    {
        sv_log_fmt("del:%08llx", castull(sv_array_at64u(contentids, i)));
        bsetfmt(self->tmp_filename, "%s%s%08llx.file", tmpdir, pathsep,
            castull(sv_array_at64u(contentids, i)));
        check_b(os_remove(cstr(self->tmp_filename)), "couldn't remove %s",
            cstr(self->tmp_filename));
        bsetfmt(self->tmp_filename, "%s%s%08llx.xz", tmpdir, pathsep,
            castull(sv_array_at64u(contentids, i)));
        check_b(os_remove(cstr(self->tmp_filename)), "couldn't remove %s",
            cstr(self->tmp_filename));
    }

    /* create a new archive */
    check_b(os_remove(cstr(out)), "couldn't remove %s", cstr(out));
    check(os_listfiles(tmpdir, self->list, true)); /* should be sorted */
    for (int i = 0; i < self->list->qty; i++)
    {
        os_get_filename(blist_view(self->list, i), innername);
        check(ar_util_add(
            self, cstr(out), blist_view(self->list, i), cstr(innername), 0));
    }

    check(os_tryuntil_deletefiles(tmpdir, "*"));
    check_b(os_tryuntil_move(cstr(out), archive, true),
        "couldn't move %s overwriting %s", cstr(out), archive);

cleanup:
    bdestroy(innername);
    bdestroy(out);
    return currenterr;
}

void ar_manager_close(ar_manager *self)
{
    if (self)
    {
        bdestroy(self->path_working);
        bdestroy(self->path_staging);
        bdestroy(self->path_readytoupload);
        bdestroy(self->currentarchive);
        sv_array_close(&self->current_sizes);
        sv_file_close(&self->namestextfile);
        ar_util_close(&self->ar);
        set_self_zero();
    }
}

void ar_util_close(ar_util *self)
{
    if (self)
    {
        bdestroy(self->xz_binary);
        bdestroy(self->tar_binary);
        bdestroy(self->audiotag_binary);
        bdestroy(self->tmp_filename);
        bdestroy(self->tmp_arg_tar);
        bdestroy(self->tmp_inner_name);
        bdestroy(self->tmp_combined);
        bdestroy(self->tmp_ascii);
        bdestroy(self->tmp_results);
        bdestroy(self->tmp_xz_name);
        bstrlist_close(self->list);
        set_self_zero();
    }
}
