/*
tests_util_archiver.c

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
#include <math.h>

check_result get_tar_archive_parameter(const char *archive, bstring sout);
check_result create_test_tar(
    const char *tar, const char *tempdir, ar_util *ar, int nfiles);
check_result create_test_xz(
    const char *xz, const char *tempdir, ar_util *ar, bool large);

SV_BEGIN_TEST_SUITE(tests_tar)
{
    SV_TEST_LIN("formatting path to archive")
    {
        TEST_OPEN(bstring, s);
        check(get_tar_archive_parameter("/path/to/file", s));
        TestEqs("--file=/path/to/file", cstr(s));
    }

    SV_TEST_LIN("path to archive cannot be relative")
    {
        TEST_OPEN(bstring, s);
        expect_err_with_message(
            get_tar_archive_parameter("relative/path", s), "abs path");
    }

    SV_TEST_WIN("formatting path to archive")
    {
        TEST_OPEN(bstring, s);
        check(get_tar_archive_parameter("X:\\path\\to\\file", s));
        TestEqs("--file=/X/path/to/file", cstr(s));
    }

    SV_TEST_WIN("path to archive cannot be relative")
    {
        TEST_OPEN(bstring, s);
        expect_err_with_message(
            get_tar_archive_parameter("relative\\path", s), "abs path");
    }

    SV_TEST_WIN("path to archive cannot be relative to drive")
    {
        TEST_OPEN(bstring, s);
        expect_err_with_message(
            get_tar_archive_parameter("\\fromroot\\path", s), "abs path");
    }

    SV_TEST_WIN("path to archive cannot be net share")
    {
        TEST_OPEN(bstring, s);
        expect_err_with_message(
            get_tar_archive_parameter("\\\\uncshare\\path", s), "abs path");
    }

    SV_TEST_WIN("attempt to add file larger than max size")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN_EX(bstring, tar, bformat("%s%sa.tar", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        expect_err_with_message(ar_util_add(&ar, cstr(tar), cstr(path),
                                    "namewithin.txt", 1024ULL * 1024 * 1024),
            "files of this size");
    }

    SV_TEST("attempt adding missing file to new tar")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(
            bstring, path, bformat("%s%snot-exist.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        expect_err_with_message(
            ar_util_add(&ar, cstr(tar), cstr(path), "namewithin", 0),
            "Cannot stat: No such file");
    }

    SV_TEST("attempt adding missing file to existing tar")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(
            bstring, path, bformat("%s%snot-exist.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        uint64_t archive_size = os_getfilesize(cstr(tar));
        expect_err_with_message(
            ar_util_add(&ar, cstr(tar), cstr(path), "namewithin", 0),
            "Cannot stat: No such file");
        TestTrue(archive_size > 0);
        TestEqn(archive_size, os_getfilesize(cstr(tar)));
    }

    SV_TEST("delete_from_tar")
    {
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, ids, sv_array_open_u64());
        TEST_OPEN_EX(sv_array, sizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 4));
        sv_array_add64u(&ids, 0x1c8);
        sv_array_add64u(&ids, 0x315);
        check(tests_cleardir(cstr(tempsubdir)));
        check(ar_util_delete(&ar, cstr(tar), tempdir, cstr(tempsubdir), &ids));
        check(tests_tar_list(&ar, cstr(tar), list));
        sv_array_add64u(&sizes, strlen("file-contents1"));
        sv_array_add64u(&sizes, strlen("file-contents1111"));
        check(ar_util_verify(&ar, cstr(tar), list, &sizes));
        TestEqList("0000007b.file|00000316.file", list);
    }

    SV_TEST("attempt delete missing file from tar")
    {
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(sv_array, ids, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        uint64_t archive_size = os_getfilesize(cstr(tar));
        sv_array_add64u(&ids, 0x1);
        check(tests_cleardir(cstr(tempsubdir)));
        check(ar_util_delete(&ar, cstr(tar), tempdir, cstr(tempsubdir), &ids));
        TestTrue(archive_size > 0);
        TestEqn(archive_size, os_getfilesize(cstr(tar)));
    }

    SV_TEST("attempt to use missing tar")
    {
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN_EX(
            bstring, tar, bformat("%s%sdoes-not-exist--.tar", tempdir, pathsep));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, sizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        sv_array_add64u(&sizes, 0x1);
        check(checkbinarypaths(&ar, false, tempdir));
        expect_err_with_message(
            tests_tar_list(&ar, cstr(tar), list), "Cannot open: No such");
        expect_err_with_message(ar_util_verify(&ar, cstr(tar), list, &sizes),
            "Cannot open: No such");
        expect_err_with_message(
            ar_util_delete(&ar, cstr(tar), tempdir, cstr(tempsubdir), &sizes),
            "Cannot open: No such");
    }

    SV_TEST("validate tar containing 1 file")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(path), "file-a-contents", "wb"));
        check(ar_util_add(&ar, cstr(tar), cstr(path), "0000007b.file",
            os_getfilesize(cstr(path))));
        check(tests_tar_list(&ar, cstr(tar), list));
        sv_array_add64u(&arrsizes, os_getfilesize(cstr(path)));
        check(ar_util_verify(&ar, cstr(tar), list, &arrsizes));
        TestTrue(os_getfilesize(cstr(path)) > 0);
        TestTrue(os_getfilesize(cstr(tar)) > 0);
        TestEqList("0000007b.file", list);
    }

    SV_TEST("validate tar containing 2 files")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_tar_list(&ar, cstr(tar), list));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11"));
        check(ar_util_verify(&ar, cstr(tar), list, &arrsizes));
        TestEqList("0000007b.file|000001c8.file", list);
    }

    SV_TEST("validate tar containing 4 files")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 4));
        check(tests_tar_list(&ar, cstr(tar), list));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11"));
        sv_array_add64u(&arrsizes, strlen("file-contents111"));
        sv_array_add64u(&arrsizes, strlen("file-contents1111"));
        check(ar_util_verify(&ar, cstr(tar), list, &arrsizes));
        TestEqList(
            "0000007b.file|000001c8.file|00000315.file|00000316.file", list);
    }

    SV_TEST("validation should fail if incorrect filename given")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11"));
        bstrlist_splitcstr(list, "0000007b.file|000001c8.fil", '|');
        expect_err_with_message(ar_util_verify(&ar, cstr(tar), list, &arrsizes),
            "couldn't find entry for file");
    }

    SV_TEST("validation should fail if partial filename given")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11"));
        bstrlist_splitcstr(list, "0000007b.file|00001c8.file", '|');
        expect_err_with_message(ar_util_verify(&ar, cstr(tar), list, &arrsizes),
            "couldn't find entry for file");
    }

    SV_TEST("validation should fail if mismatching lengths given")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11"));
        sv_array_add64u(&arrsizes, strlen("file-contents111"));
        bstrlist_splitcstr(list, "0000007b.file|000001c8.file", '|');
        expect_err_with_message(
            ar_util_verify(&ar, cstr(tar), list, &arrsizes), "2 != 3");
    }

    SV_TEST("validation should fail if too many filenames given")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11"));
        sv_array_add64u(&arrsizes, strlen("file-contents111"));
        bstrlist_splitcstr(
            list, "0000007b.file|000001c8.file|000001c9.file", '|');
        expect_err_with_message(ar_util_verify(&ar, cstr(tar), list, &arrsizes),
            "couldn't find entry for filesize");
    }

    SV_TEST("validation should fail if incorrect filesize given")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        sv_array_add64u(&arrsizes, strlen("file-contents1"));
        sv_array_add64u(&arrsizes, strlen("file-contents11") * 10);
        bstrlist_splitcstr(list, "0000007b.file|000001c8.file", '|');
        expect_err_with_message(ar_util_verify(&ar, cstr(tar), list, &arrsizes),
            "couldn't find entry for filesize");
    }

    SV_TEST("extract first file")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s0000007b.file", cstr(tempsubdir), pathsep));
        TEST_OPEN2(bstring, restored_to, contents);
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_cleardir(cstr(tempsubdir)));
        check(ar_util_extract_overwrite(
            &ar, cstr(tar), "0000007b.file", cstr(tempsubdir), restored_to));
        check(sv_file_readfile(cstr(restored_to), contents));
        TestEqs("file-contents1", cstr(contents));
        TestEqs(cstr(restored_to), cstr(path));
    }

    SV_TEST("extract second file")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s000001c8.file", cstr(tempsubdir), pathsep));
        TEST_OPEN2(bstring, restored_to, contents);
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_cleardir(cstr(tempsubdir)));
        check(sv_file_writefile(cstr(path), "already exists", "wb"));
        check(ar_util_extract_overwrite(
            &ar, cstr(tar), "000001c8.file", cstr(tempsubdir), restored_to));
        check(sv_file_readfile(cstr(restored_to), contents));
        TestEqs("file-contents11", cstr(contents));
        TestEqs(cstr(restored_to), cstr(path));
    }

    SV_TEST("extracting from missing tar should fail")
    {
        TEST_OPEN_EX(
            bstring, tar, bformat("%s%snotexist.tar", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        TEST_OPEN(bstring, restored_to);
        check(checkbinarypaths(&ar, false, tempdir));
        expect_err_with_message(
            ar_util_extract_overwrite(&ar, cstr(tar), "*", tempdir, restored_to),
            "Cannot open: No such");
    }

    SV_TEST("extracting missing file from tar should fail")
    {
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, tmp, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        TEST_OPEN(bstring, restored_to);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_cleardir(cstr(tmp)));
        expect_err_with_message(ar_util_extract_overwrite(&ar, cstr(tar),
                                    "0000007b.xz", cstr(tmp), restored_to),
            "Not found in archive");
    }

    SV_TEST("cannot add to archive if src locked")
    {
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_lockfile(true, cstr(path), &handle));
        expect_err_with_message(ar_util_add(&ar, cstr(tar), cstr(path),
                                    "namewithin", os_getfilesize(cstr(path))),
            "Cannot open: Permission denied");
        check(tests_lockfile(false, cstr(path), &handle));
    }

    SV_TEST("cannot add to archive if dest locked")
    {
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(
            bstring, path, bformat("%s%s00000315.file", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_lockfile(true, cstr(tar), &handle));
        expect_err_with_message(ar_util_add(&ar, cstr(tar), cstr(path),
                                    "namewithin", os_getfilesize(cstr(path))),
            "Cannot read:");
        check(tests_lockfile(false, cstr(tar), &handle));
    }

    SV_TEST("cannot extract from archive if src locked")
    {
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_cleardir(cstr(tempsubdir)));
        check(tests_lockfile(true, cstr(tar), &handle));
        expect_err_with_message(ar_util_extract_overwrite(&ar, cstr(tar),
                                    "0000007b.file", cstr(tempsubdir), NULL),
            "Cannot open:");
        check(tests_lockfile(false, cstr(tar), &handle));
    }

    SV_TEST_WIN("cannot extract from archive if dest locked")
    {
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, tempsubdir, bformat("%s%ssub", tempdir, pathsep));
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s0000007b.file", cstr(tempsubdir), pathsep));
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_cleardir(cstr(tempsubdir)));
        check(sv_file_writefile(cstr(path), "abc", "wb"));
        check(tests_lockfile(true, cstr(path), &handle));
        expect_err_with_message(ar_util_extract_overwrite(&ar, cstr(tar),
                                    "0000007b.file", cstr(tempsubdir), NULL),
            "couldn't remove ");
        check(tests_lockfile(false, cstr(path), &handle));
    }

    SV_TEST("cannot validate if archive locked")
    {
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN_EX(bstring, tar,
            bformat("%s%s%s.tar", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN(ar_util, ar);
        check(create_test_tar(cstr(tar), tempdir, &ar, 2));
        check(tests_lockfile(true, cstr(tar), &handle));
        expect_err_with_message(
            ar_util_verify(&ar, cstr(tar), list, &arrsizes), "Cannot open:");
        expect_err_with_message(
            tests_tar_list(&ar, cstr(tar), list), "Cannot open:");
        check(tests_lockfile(false, cstr(tar), &handle));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_xz)
{
    SV_TEST("create and validate xz file from text")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(path), "abcdef", "wb"));
        check(ar_util_xz_add(&ar, cstr(path), cstr(xz)));
        check(ar_util_xz_verify(&ar, cstr(xz)));
        TestTrue(os_getfilesize(cstr(path)) > 0);
        TestTrue(os_getfilesize(cstr(xz)) > 0);
    }

    SV_TEST("create and validate xz file from binary")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, true));
        check(ar_util_xz_verify(&ar, cstr(xz)));
        TestTrue(os_getfilesize(cstr(xz)) > 0);
    }

    SV_TEST("create xz file overwrite existing xz")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(xz), "existing file", "wb"));
        check(sv_file_writefile(cstr(path), "abcdef", "wb"));
        check(ar_util_xz_add(&ar, cstr(path), cstr(xz)));
        check(ar_util_xz_verify(&ar, cstr(xz)));
        TestTrue(os_getfilesize(cstr(path)) > 0);
        TestTrue(os_getfilesize(cstr(xz)) > 0);
    }

    SV_TEST("extract from xz file overwrite existing file")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.txt", tempdir, pathsep, currentcontext));
        TEST_OPEN(bstring, contents);
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, false));
        check(sv_file_writefile(cstr(path), "existing text", "wb"));
        check(ar_util_xz_extract_overwrite(&ar, cstr(xz), cstr(path)));
        check(sv_file_readfile(cstr(path), contents));
        TestEqs("file-contents1", cstr(contents));
    }

    SV_TEST("validate should fail on corrupt file with no header")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(xz), "not-a-valid-xz-file", "wb"));
        expect_err_with_message(
            ar_util_xz_verify(&ar, cstr(xz)), "File format not recognized");
    }

    SV_TEST("validate should fail on corrupt file with added byte")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, false));
        uint32_t correct_length = cast64u32u(os_getfilesize(cstr(xz)));
        sv_file file = {};
        check(sv_file_open(&file, cstr(xz), "ab"));
        fputc('a', file.file);
        sv_file_close(&file);
        TestEqn(correct_length + 1, os_getfilesize(cstr(xz)));
        expect_err_with_message(
            ar_util_xz_verify(&ar, cstr(xz)), "end of input");
    }

    SV_TEST("validate should fail on corrupt file with changed byte")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, true));
        uint32_t correct_length = cast64u32u(os_getfilesize(cstr(xz)));
        sv_file file = {};
        check(sv_file_open(&file, cstr(xz), "rb+"));
        fseek(file.file, cast32u32s(correct_length - 100), SEEK_SET);
        fputc('X', file.file);
        sv_file_close(&file);
        TestEqn(correct_length, os_getfilesize(cstr(xz)));
        expect_err_with_message(
            ar_util_xz_verify(&ar, cstr(xz)), "data is corrupt");
    }

    SV_TEST("validate should fail on corrupt file with truncated data")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(bstring, contents);
        TEST_OPEN(ar_util, ar);
        sv_file file = {};
        check(create_test_xz(cstr(xz), tempdir, &ar, true));
        uint32_t correct_length = cast64u32u(os_getfilesize(cstr(xz)));
        check(sv_file_readfile(cstr(xz), contents));
        btrunc(contents, contents->slen - 16);
        check(sv_file_open(&file, cstr(xz), "wb"));
        fwrite(contents->data, 1, cast32s32u(contents->slen), file.file);
        sv_file_close(&file);
        bdestroy(contents);
        TestEqn(correct_length - 16, os_getfilesize(cstr(xz)));
        expect_err_with_message(
            ar_util_xz_verify(&ar, cstr(xz)), "end of input");
    }

    SV_TEST("validate should fail on empty xz file")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(xz), "", "wb"));
        expect_err_with_message(ar_util_xz_verify(&ar, cstr(xz)), "0 bytes");
    }

    SV_TEST("validate should fail on missing xz file")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        expect_err_with_message(ar_util_xz_verify(&ar, cstr(xz)),
            islinux ? "see short path" : "get short path");
    }

    SV_TEST("validate should fail on locked xz file")
    {
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, false));
        check(tests_lockfile(true, cstr(xz), &handle));
        expect_err_with_message(
            ar_util_xz_verify(&ar, cstr(xz)), "Permission denied");
        check(tests_lockfile(false, cstr(xz), &handle));
    }

    SV_TEST("create should fail if src is missing")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(
            bstring, path, bformat("%s%snot-exist.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        expect_err_with_message(ar_util_xz_add(&ar, cstr(path), cstr(xz)),
            islinux ? "see short path" : "get short path");
    }

    SV_TEST("create should fail if src is locked")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(path), "abcdef", "wb"));
        check(tests_lockfile(true, cstr(path), &handle));
        expect_err_with_message(
            ar_util_xz_add(&ar, cstr(path), cstr(xz)), "not be 0 bytes");
        check(tests_lockfile(false, cstr(path), &handle));
    }

    SV_TEST("create should fail if dest is locked")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        check(sv_file_writefile(cstr(path), "abcdef", "wb"));
        check(sv_file_writefile(cstr(xz), "", "wb"));
        check(tests_lockfile(true, cstr(xz), &handle));

        expect_err_with_message(
            ar_util_xz_add(&ar, cstr(path), cstr(xz)), "Permission denied(13)");
        check(tests_lockfile(false, cstr(xz), &handle));
    }

    SV_TEST("extract should fail if src is locked")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, false));
        check(tests_lockfile(true, cstr(xz), &handle));

        /* bug in xz: it does not return an error exit code.
        product code catches this by verifying checksum of expanded files. */
        check(ar_util_xz_extract_overwrite(&ar, cstr(xz), cstr(path)));
        check(tests_lockfile(false, cstr(xz), &handle));
    }

    SV_TEST("extract should fail if dest is locked")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN_EX(os_lockedfilehandle, handle, {});
        TEST_OPEN(ar_util, ar);
        check(create_test_xz(cstr(xz), tempdir, &ar, false));
        check(sv_file_writefile(cstr(path), "", "wb"));
        check(tests_lockfile(true, cstr(path), &handle));
        expect_err_with_message(
            ar_util_xz_extract_overwrite(&ar, cstr(xz), cstr(path)),
            "Permission denied(13)");
        check(tests_lockfile(false, cstr(path), &handle));
    }

    SV_TEST("extract should fail if archive is missing")
    {
        TEST_OPEN_EX(
            bstring, xz, bformat("%s%s%s.xz", tempdir, pathsep, currentcontext));
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        TEST_OPEN(ar_util, ar);
        check(checkbinarypaths(&ar, false, tempdir));
        expect_err_with_message(
            ar_util_xz_extract_overwrite(&ar, cstr(xz), cstr(path)),
            islinux ? "see short path" : "get short path");
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(whole_tests_archive_filenames)
{
    /* create input files */
    const char *dir = tempdir;
    TEST_OPEN4(bstring, contents_got, large, filename, expectedout);
    TEST_OPEN(bstring, archive);
    TEST_OPEN(ar_util, ar);
    TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
    TEST_OPEN_EX(sv_array, arrsizes, sv_array_open_u64());
    TEST_OPEN_EX(bstring, restoreto, bformat("%s%srestoreto", dir, pathsep));
    TEST_OPEN_EX(bstring, file_normal, bformat("%s%snormal.txt", dir, pathsep));
    TEST_OPEN_EX(bstring, file_large, bformat("%s%slarge.txt", dir, pathsep));
    TEST_OPEN_EX(bstring, file_empty, bformat("%s%s.empty.xz", dir, pathsep));
    TEST_OPEN_EX(bstring, file_utf,
        bformat("%s%sutf_\xF0\x9D\x84\x9E.tar", dir, pathsep));
    TEST_OPEN_EX(bstring, file_name1,
        bformat("%s%s%s", dir, pathsep, islinux ? "*.txt" : ".txt"));
    TEST_OPEN_EX(bstring, file_name2,
        bformat("%s%s%s", dir, pathsep,
            islinux ? ".\"\n::'a'\\n\\a[ab]?.txt" : ".'a'[ab].txt"));
    TEST_OPEN_EX(bstring, file_name3,
        bformat("%s%s%s", dir, pathsep, "-beginswithdash and has spaces"));
    bstr_fill(large, 'a', 250 * 1024);
    check(checkbinarypaths(&ar, false, NULL));
    check(tests_cleardir(cstr(restoreto)));
    check(sv_file_writefile(cstr(file_normal), cstr(file_normal), "wb"));
    check(sv_file_writefile(cstr(file_large), cstr(large), "wb"));
    check(sv_file_writefile(cstr(file_empty), "", "wb"));
    check(sv_file_writefile(cstr(file_utf), cstr(file_utf), "wb"));
    check(sv_file_writefile(cstr(file_name1), cstr(file_name1), "wb"));
    check(sv_file_writefile(cstr(file_name2), cstr(file_name2), "wb"));
    check(sv_file_writefile(cstr(file_name3), cstr(file_name3), "wb"));
    bstring inputs[] = {file_normal, file_large, file_empty, file_utf,
        file_name1, file_name2, file_name3};

    /* test creating tar files */
    for (int i = 0; i < countof(inputs); i++)
    {
        /* create .tar file */
        os_get_filename(cstr(inputs[i]), filename);
        bsetfmt(archive, "%s%s%s.tar", dir, pathsep, cstr(filename));
        check(ar_util_add(&ar, cstr(archive), cstr(inputs[i]), "namewithin.txt",
            os_getfilesize(cstr(inputs[i]))));
        TestTrue(
            os_getfilesize(cstr(archive)) > os_getfilesize(cstr(inputs[i])));

        /* extract and confirm contents */
        TestTrue(os_create_dirs(cstr(restoreto)));
        check(os_tryuntil_deletefiles(cstr(restoreto), "*"));
        check(ar_util_extract_overwrite(
            &ar, cstr(archive), "namewithin.txt", cstr(restoreto), filename));
        bsetfmt(expectedout, "%s%snamewithin.txt", cstr(restoreto), pathsep);
        TestEqs(cstr(expectedout), cstr(filename));
        check(sv_file_readfile(cstr(filename), contents_got));
        TestEqs(inputs[i] == file_large
                ? cstr(large)
                : inputs[i] == file_empty ? "" : cstr(inputs[i]),
            cstr(contents_got));

        /* only one file should have been extracted */
        check(os_listdirs(cstr(restoreto), list, false));
        TestEqn(0, list->qty);
        check(os_listfiles(cstr(restoreto), list, false));
        TestEqn(1, list->qty);

        /* test tar --list */
        check(tests_tar_list(&ar, cstr(archive), list));
        TestEqList("namewithin.txt", list);
        sv_array_truncatelength(&arrsizes, 0);
        sv_array_add64u(&arrsizes, os_getfilesize(cstr(inputs[i])));
        bstrlist_splitcstr(list, "namewithin.txt", '|');
        check(ar_util_verify(&ar, cstr(archive), list, &arrsizes));
    }

    /* test creating xz files */
    for (int i = 0; i < countof(inputs); i++)
    {
        /* create .xz file */
        os_get_filename(cstr(inputs[i]), filename);
        bsetfmt(archive, "%s%s%s.out.xz", dir, pathsep, cstr(filename));
        check(ar_util_xz_add(&ar, cstr(inputs[i]), cstr(archive)));
        TestTrue(os_getfilesize(cstr(archive)) > 0);

        /* count files */
        check(os_listfiles(dir, list, false));
        int countfiles = list->qty;

        /* extract and confirm contents */
        bsetfmt(expectedout, "%s%s%s.sentout", dir, pathsep, cstr(filename));
        check(
            ar_util_xz_extract_overwrite(&ar, cstr(archive), cstr(expectedout)));
        check(sv_file_readfile(cstr(expectedout), contents_got));
        TestEqs(inputs[i] == file_large
                ? cstr(large)
                : inputs[i] == file_empty ? "" : cstr(inputs[i]),
            cstr(contents_got));

        /* only one file should have been extracted */
        SV_TEST_()
        {
            check(os_listfiles(dir, list, false));
            TestEqn(countfiles + 1, list->qty);
        }
    }
}
SV_END_TEST_SUITE()

check_result tests_check_tar_contents(
    ar_util *ar, const char *archive, const char *expected, bool check_order)
{
    /* input in the format "file1.txt|file2.txt^12|14".
    first names, then sizes in bytes */
    sv_result currenterr = {};
    sv_array expect_nsizes = sv_array_open_u64();
    bstrlist *expect_split = bstrlist_open();
    bstrlist *got_names = bstrlist_open();
    bstrlist_splitcstr(expect_split, expected, '^');
    TestEqn(expect_split->qty, 2);
    bstrlist *expect_names = bsplit(expect_split->entry[0], '|');
    bstrlist *expect_sizes = bsplit(expect_split->entry[1], '|');

    /* from strings to ints */
    for (int i = 0; i < expect_sizes->qty; i++)
    {
        uint64_t n = 0;
        check_b(uintfromstr(blist_view(expect_sizes, i), &n), "");
        sv_array_add64u(&expect_nsizes, n);
    }

    TestEqn(expect_names->qty, expect_sizes->qty);
    TestEqn(expect_names->qty, expect_nsizes.length);
    check(tests_tar_list(ar, archive, got_names));
    bstrlist_sort(got_names);
    TestEqn(expect_names->qty, got_names->qty);
    TestEqList(cstr(expect_split->entry[0]), got_names);
    if (check_order || !islinux)
    {
        if (expect_names->qty &&
            s_equal(blist_view(expect_names, expect_names->qty - 1),
                "filenames.txt"))
        {
            bstrlist_remove_at(expect_sizes, expect_sizes->qty - 1);
            bstrlist_remove_at(expect_names, expect_names->qty - 1);
            sv_array_truncatelength(&expect_nsizes, expect_nsizes.length - 1);
        }

        check(ar_util_verify(ar, archive, expect_names, &expect_nsizes));
    }

cleanup:
    bstrlist_close(expect_split);
    bstrlist_close(got_names);
    bstrlist_close(expect_names);
    bstrlist_close(expect_sizes);
    sv_array_close(&expect_nsizes);
    return currenterr;
}

check_result tests_tar_list(
    ar_util *self, const char *tarpath, bstrlist *results)
{
    sv_result currenterr = {};
    check_b(s_endwith(tarpath, ".tar"), "%s does not end with tar", tarpath);
    check(get_tar_archive_parameter(tarpath, self->tmp_arg_tar));
    const char *args[] = {linuxonly(cstr(self->tar_binary)) "--list",
        cstr(self->tmp_arg_tar), NULL};

    bsetfmt(self->tmp_results, "Context: tar list %s", tarpath);
    check(os_tryuntil_run(cstr(self->tar_binary), args, self->tmp_results,
        self->tmp_combined, true, 0, 0));

    /* note: breaks if inner filenames contain newlines */
    bstr_replaceall(self->tmp_results, "\r\n", "\n");
    bstrlist_splitcstr(results, cstr(self->tmp_results), '\n');
    if (results->qty > 0 && blength(results->entry[results->qty - 1]) == 0)
    {
        /* remove one final empty entry */
        bstrlist_remove_at(results, results->qty - 1);
    }

cleanup:
    return currenterr;
}

check_result create_test_xz(
    const char *xz, const char *tempdir, ar_util *ar, bool large)
{
    sv_result currenterr = {};
    bstring path = bformat("%s%sinput.txt", tempdir, pathsep);
    check(checkbinarypaths(ar, false, tempdir));
    if (large)
    {
        sv_file file = {};
        check(sv_file_open(&file, cstr(path), "wb"));
        for (int i = 0; i < 1024 * 96; i++)
        {
            fputc(((byte)(250 * sin(i * i * i))), file.file);
        }

        sv_file_close(&file);
    }
    else
    {
        check(sv_file_writefile(cstr(path), "file-contents1", "wb"));
    }

    check(ar_util_xz_add(ar, cstr(path), xz));
    check(ar_util_xz_verify(ar, xz));
    TestTrue(os_getfilesize(cstr(path)) > 0);
    TestTrue(os_getfilesize(xz) > 0);

cleanup:
    bdestroy(path);
    return currenterr;
}

check_result create_test_tar(
    const char *tar, const char *tempdir, ar_util *ar, int nfiles)
{
    sv_result currenterr = {};
    bstring path1 = bformat("%s%sa.txt", tempdir, pathsep);
    bstring path2 = bformat("%s%sb.txt", tempdir, pathsep);
    bstring path3 = bformat("%s%sc.txt", tempdir, pathsep);
    bstring path4 = bformat("%s%sd.txt", tempdir, pathsep);
    check(checkbinarypaths(ar, false, tempdir));
    check(sv_file_writefile(cstr(path1), "file-contents1", "wb"));
    check(sv_file_writefile(cstr(path2), "file-contents11", "wb"));
    check(sv_file_writefile(cstr(path3), "file-contents111", "wb"));
    check(sv_file_writefile(cstr(path4), "file-contents1111", "wb"));
    check(ar_util_add(
        ar, tar, cstr(path1), "0000007b.file", os_getfilesize(cstr(path1))));
    check(ar_util_add(
        ar, tar, cstr(path2), "000001c8.file", os_getfilesize(cstr(path2))));
    check_b(nfiles == 2 || nfiles == 4, "");
    if (nfiles == 4)
    {
        check(ar_util_add(
            ar, tar, cstr(path3), "00000315.file", os_getfilesize(cstr(path3))));
        check(ar_util_add(
            ar, tar, cstr(path4), "00000316.file", os_getfilesize(cstr(path4))));
    }

cleanup:
    bdestroy(path1);
    bdestroy(path2);
    bdestroy(path3);
    bdestroy(path4);
    return currenterr;
}
