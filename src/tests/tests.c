/*
tests.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "tests.h"

void run_all_tests()
{
    puts("running tests...");
    bstring dir = os_get_tmpdir("tmpglacial_backup");
    bstring btempdir = tests_make_subdir(cstr(dir), "tests");
    bassign(restrict_write_access, dir);
    check_fatal(blength(btempdir), "could not get test dir at %s",
        cstr(btempdir));

    const char *tempdir = cstr(btempdir);
    tests_sv_array(tempdir);
    tests_arithmetic(tempdir);
    tests_2d_array(tempdir);
    tests_open_db_connection(tempdir);
    tests_path_handling(tempdir);
    tests_aligned_malloc(tempdir);
    tests_write_text_file(tempdir);
    tests_file_operations(tempdir);
    tests_bypattern(tempdir);
    tests_file_locks(tempdir);
    tests_recurse_corner_cases(tempdir);
    tests_startprocess(tempdir);
    tests_incrementing_filenames(tempdir);
    tests_recurse_dir(tempdir);
    tests_logging(tempdir);
    tests_get_file_extension_category(tempdir);
    tests_persist_user_configs(tempdir);
    tests_find_groups(tempdir);
    tests_c_string(tempdir);
    tests_fnmatch(tempdir);
    tests_string_bstr(tempdir);
    tests_string_bstr_extensions(tempdir);
    tests_string_conversions(tempdir);
    tests_string_list(tempdir);
    tests_string_pseudosplit(tempdir);
    tests_widestrings(tempdir);
    tests_tar(tempdir);
    tests_xz(tempdir);
    whole_tests_archive_filenames(tempdir);
    tests_read_md5_string(tempdir);
    tests_get_version(tempdir);
    tests_hash_audio(tempdir);
    whole_tests_db(tempdir);
    whole_tests_operations(tempdir);

    bdestroy(dir);
    bdestroy(btempdir);
    puts("tests complete");
    alert("ran tests");
    exit(0);
}

bool testintimpl(int line,
    unsigned long long n1,
    unsigned long long n2)
{
    if (n1 == n2)
    {
        return true;
    }

    bstring s = bformat("line %d, expected %lld, got %lld", line, n1, n2);
    alert(cstr(s));
    bdestroy(s);
    return false;
}

bool teststrimpl(int line,
    const char *s1,
    const char *s2)
{
    if (s_equal(s1, s2))
    {
        return true;
    }

    bstring s = bformat("line %d, expected \n\n%s, got \n\n%s", line, s1, s2);
    alert(cstr(s));
    bdestroy(s);
    return false;
}

bool testlistimpl(int lineno,
    const char *expected,
    const bstrlist *list)
{
    bstring joined = bjoin_static(list, "|");
    if (s_equal(expected, cstr(joined)))
    {
        bdestroy(joined);
        return true;
    }

    bstring s = bformat("line %d, expected \n\n%s, got \n\n%s",
        lineno, expected, cstr(joined));

    alert(cstr(s));
    bdestroy(s);
    bdestroy(joined);
    return false;
}

bool testpslistimpl(int lineno,
    const char *expected,
    sv_pseudosplit *list)
{
    bstring joined = bstring_open();
    for (uint32_t i = 0; i < list->splitpoints.length; i++)
    {
        bcatcstr(joined, sv_pseudosplit_viewat(list, i));
        bcatcstr(joined, "|");
    }

    if (blength(joined))
    {
        /* strip the final | character */
        btrunc(joined, blength(joined) - 1);
    }

    if (s_equal(expected, cstr(joined)))
    {
        bdestroy(joined);
        return true;
    }

    bstring s = bformat("line %d, expected \n\n%s, got \n\n%s",
        lineno, expected, cstr(joined));

    alert(cstr(s));
    bdestroy(s);
    bdestroy(joined);
    return false;
}

void expect_err_with_message_impl(sv_result res,
    const char *msgcontains)
{
    TestTrue(res.code != 0);
    if (!s_contains(cstr(res.msg), msgcontains))
    {
        printf("message \n%s\n did not contain '%s'",
            cstr(res.msg), msgcontains);

        TestTrue(false);
    }

    sv_result_close(&res);
}

check_result tmpwritetextfile(const char *dir,
    const char *leaf,
    bstring fullpath,
    const char *contents)
{
    sv_result currenterr = {};
    bsetfmt(fullpath, "%s%s%s", dir, pathsep, leaf);
    check(sv_file_writefile(cstr(fullpath), contents, "wb"));

cleanup:
    return currenterr;
}

sv_result tests_lockfile(bool lock,
    const char *s,
    os_lockedfilehandle *handle)
{
    sv_result currenterr = {};

    /* runtime locks can't be used on linux because file locks are advisory */
    if (islinux)
    {
        check_errno(_, chmod(s, lock ? 0111 : 0644));
    }
    else if (lock)
    {
        check(os_lockedfilehandle_open(handle,
            s, false /* don't allow read */, NULL));
    }
    else
    {
        os_lockedfilehandle_close(handle);
    }

cleanup:
    return currenterr;
}

bstring tests_make_subdir(const char *parent, const char *leaf)
{
    bstring ret = bstring_open();
    check_fatal(os_isabspath(parent), "must specify absolute path");
    bsetfmt(ret, "%s%s%s", parent, pathsep, leaf);
    if (os_create_dir(cstr(ret)))
    {
        return ret;
    }
    else
    {
        bstrclear(ret);
        return ret;
    }
}

void sv_test_hook_close(sv_test_hook *self)
{
    if (self)
    {
        for (size_t i = 0; i < countof(self->filenames); i++)
        {
            bdestroy(self->filenames[i]);
        }

        bdestroy(self->path_userfiles);
        bdestroy(self->path_group);
        bdestroy(self->path_readytoupload);
        bdestroy(self->path_readytoremove);
        bdestroy(self->path_restoreto);
        bdestroy(self->path_untar);
        bdestroy(self->path_tmp);
        bstrlist_close(self->allcontentrows);
        bstrlist_close(self->allfilelistrows);
        bstrlist_close(self->listexpectcontentrows);
        bstrlist_close(self->listexpectfilelistrows);
        set_self_zero();
    }
}

sv_test_hook sv_test_hook_open(const char *dir)
{
    sv_test_hook self = {};
    self.path_group = bformat("%s%suserdata%stest",
        dir, pathsep, pathsep);
    self.path_readytoupload = bformat("%s%suserdata%stest%sreadytoupload",
        dir, pathsep, pathsep, pathsep);
    self.path_readytoremove = bformat("%s%suserdata%stest%sreadytoremove",
        dir, pathsep, pathsep, pathsep);
    self.path_restoreto = bformat("%s%srestoreto",
        dir, pathsep);
    self.path_untar = bformat("%s%suntar",
        dir, pathsep);
    self.path_userfiles = bformat("%s%sfakeuserfiles",
        dir, pathsep);

    self.path_tmp = bstring_open();
    self.allcontentrows = bstrlist_open();
    self.allfilelistrows = bstrlist_open();
    self.listexpectcontentrows = bstrlist_open();
    self.listexpectfilelistrows = bstrlist_open();
    return self;
}

sv_result tests_cleardir_impl(const char *tempdir)
{
    sv_result currenterr = {};
    bstrlist *list = bstrlist_open();
    check_b(os_create_dirs(tempdir), "");
    check_b(os_setcwd(tempdir), "");

    /* delete one level of subdirs*/
    check(os_listdirs(tempdir, list, false));
    for (int i = 0; i < list->qty; i++)
    {
        check(os_tryuntil_deletefiles(
            blist_view(list, i), "*"));
        check_b(os_remove(blist_view(list, i)),
            "test left too many remaining subdirectories at %s",
            blist_view(list, i));
    }

    /* delete files at root level */
    check(os_tryuntil_deletefiles(tempdir, "*"));

cleanup:
    bstrlist_close(list);
    return currenterr;
}

sv_result tests_cleardir(const char *tempdir)
{
    quiet_warnings(true);
    sv_result ret = tests_cleardir_impl(tempdir);
    quiet_warnings(false);
    if (ret.code)
    {
        printf("There are files left from a previous failed run. "
            "Please delete all files and directories in \n%s\n and "
            "try again. \n\n\nDetails: %s", tempdir, cstr(ret.msg));
        alert("");
    }

    return ret;
}
