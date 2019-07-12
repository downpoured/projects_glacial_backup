/*
tests_os.c

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

SV_BEGIN_TEST_SUITE(tests_path_handling)
{
    SV_TEST("os_split_dir with many subdirs")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir(pathsep "test" pathsep "one" pathsep "two", dir, filename);
        TestEqs(pathsep "test" pathsep "one", cstr(dir));
        TestEqs("two", cstr(filename));
    }

    SV_TEST("os_split_dir with one subdir")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir(pathsep "test", dir, filename);
        TestEqs("", cstr(dir));
        TestEqs("test", cstr(filename));
    }

    SV_TEST("os_split_dir with no subdir")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir("test", dir, filename);
        TestEqs("", cstr(dir));
        TestEqs("test", cstr(filename));
    }

    SV_TEST("os_split_dir with trailing pathsep")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir(pathsep "test" pathsep, dir, filename);
        TestEqs(pathsep "test", cstr(dir));
        TestEqs("", cstr(filename));
    }

    SV_TEST("os_split_dir with no root")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir("dir" pathsep "file", dir, filename);
        TestEqs("dir", cstr(dir));
        TestEqs("file", cstr(filename));
    }

    SV_TEST("os_split_dir with no root, depth=2")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir("dir" pathsep "dir" pathsep "file", dir, filename);
        TestEqs("dir" pathsep "dir", cstr(dir));
        TestEqs("file", cstr(filename));
    }

    SV_TEST("os_split_dir with empty string")
    {
        TEST_OPEN2(bstring, dir, filename);
        os_split_dir("", dir, filename);
        TestEqs("", cstr(dir));
        TestEqs("", cstr(filename));
    }

    SV_TEST("os_get_filename")
    {
        TEST_OPEN(bstring, filename);
        os_get_filename(pathsep "dirname" pathsep "a.txt", filename);
        TestEqs("a.txt", cstr(filename));
    }

    SV_TEST("find on system path")
    {
        TEST_OPEN(bstring, path);
        const char *bin = islinux ? "grep" : "calc.exe";
        check(os_binarypath(bin, path));
        TestTrue(os_isabspath(cstr(path)));
        TestTrue(os_file_exists(cstr(path)));
    }

    SV_TEST("not found on system path")
    {
        TEST_OPEN(bstring, path);
        check(os_binarypath("^not--found^", path));
        TestEqs("", cstr(path));
    }

    SV_TEST_LIN_()
    {
        TestTrue(os_isabspath("/a"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(os_isabspath("/a/b/c"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(!os_isabspath(""));
    }

    SV_TEST_LIN_()
    {
        TestTrue(!os_isabspath("a"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(!os_isabspath("a/b/c"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(!os_isabspath("../a/b/c"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(!os_isabspath("\\a"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(!os_isabspath("\\a\\b\\c"));
    }

    SV_TEST_LIN_()
    {
        TestTrue(os_create_dir("/"));
    }

    SV_TEST_LIN_()
    {
        TestTrue((int)-1 == (int)(off_t)-1);
    }

    SV_TEST_WIN_()
    {
        TestTrue(os_isabspath("C:\\a"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(os_isabspath("C:\\a\\b\\c"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(os_isabspath("C:\\"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath(""));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("C:/a"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("a"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("C:"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("a\\b\\c"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("..\\a\\b\\c"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("\\\\fileshare"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(!os_isabspath("\\\\fileshare\\path"));
    }

    SV_TEST_WIN_()
    {
        TestTrue(os_create_dir("C:\\"));
    }

    SV_TEST_()
    {
        TestTrue(os_create_dir(tempdir));
    }

    SV_TEST_()
    {
        TestTrue(os_issubdirof(pathsep "a", pathsep "a" pathsep "test"));
    }

    SV_TEST_()
    {
        TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a"));
    }

    SV_TEST_()
    {
        TestTrue(!os_issubdirof(pathsep "a", pathsep "ab" pathsep "test"));
    }

    SV_TEST_()
    {
        TestTrue(!os_issubdirof(
            pathsep "a" pathsep "test", pathsep "a" pathsep "testnotsame"));
    }

    SV_TEST_()
    {
        TestTrue(!os_issubdirof(
            pathsep "a" pathsep "test", pathsep "a" pathsep "tes0"));
    }

    SV_TEST_()
    {
        TestTrue(os_issubdirof(
            pathsep "a" pathsep "test", pathsep "a" pathsep "test"));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_aligned_malloc)
{
    SV_TEST("fill and read from aligned buffer")
    {
        int total = 0;
        byte *buf = os_aligned_malloc(16 * 4096, 4096);
        for (int i = 0; i < 16 * 4096; i++)
        {
            buf[i] = (byte)i;
        }

        for (int i = 0; i < 16 * 4096; i++)
        {
            total += buf[i];
        }

        os_aligned_free(&buf);
        TestEqn(8355840, total);
    }
}
SV_END_TEST_SUITE()

check_result writetextfilehelper(
    const char *dir, const char *leaf, const char *contents, bstring outgetpath)
{
    sv_result currenterr = {};
    bstring fullpath = bformat("%s%s%s", dir, pathsep, leaf);
    check(sv_file_writefile(cstr(fullpath), contents, "wb"));
    if (outgetpath)
    {
        bassign(outgetpath, fullpath);
    }

cleanup:
    bdestroy(fullpath);
    return currenterr;
}

SV_BEGIN_TEST_SUITE(tests_write_text_file)
{
    SV_TEST("write text")
    {
        TEST_OPEN2(bstring, path, contents);
        check(tmpwritetextfile(tempdir, "write.txt", path, "contents"));
        check(sv_file_readfile(cstr(path), contents));
        TestEqn(strlen("contents"), contents->slen);
        TestEqs("contents", cstr(contents));
    }

    SV_TEST("write text with utf8 chars")
    {
        TEST_OPEN2(bstring, path, contents);
        check(tmpwritetextfile(
            tempdir, "write\xED\x95\x9C.txt", path, "contents\xED\x95\x9C"));
        check(sv_file_readfile(cstr(path), contents));
        TestEqn(strlen("contents\xED\x95\x9C"), contents->slen);
        TestEqs("contents\xED\x95\x9C", cstr(contents));
    }

    SV_TEST("write text with newline chars")
    {
        TEST_OPEN2(bstring, path, contents);
        check(tmpwritetextfile(
            tempdir, "writenewlines.txt", path, "a\n\n\n\nb\r\nc\r\r"));
        check(sv_file_readfile(cstr(path), contents));
        TestEqn(strlen("a\n\n\n\nb\r\nc\r\r"), contents->slen);
        TestEqs("a\n\n\n\nb\r\nc\r\r", cstr(contents));
    }

    SV_TEST("write binary data")
    {
        sv_file f = {};
        TEST_OPEN2(bstring, path, s);
        check(tmpwritetextfile(tempdir, "writedata\xED\x95\x9C.txt", path, ""));
        check(sv_file_open(&f, cstr(path), "wb"));
        fwrite("a\0b\0c", sizeof(char), 5, f.file);
        sv_file_close(&f);
        check(sv_file_readfile(cstr(path), s));
        TestEqn(5, s->slen);
        TestEqn(6, s->mlen);
        TestEqn('b', s->data[2]);
        TestEqn('\0', s->data[3]);
        TestEqn('c', s->data[4]);
        TestEqn('\0', s->data[5]);
    }

    SV_TEST("low-level io should replace existing contents")
    {
        TEST_OPEN2(bstring, path, contents);
        check(tmpwritetextfile(tempdir, "a.txt", path, "existing text"));
        int fd1 =
            open(cstr(path), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
        check_b(fd1 >= 0, "open()");
        int64_t w = write(fd1, "abc", strlen32u("abc"));
        close(fd1);
        check(sv_file_readfile(cstr(path), contents));
        TestTrue(w > 0);
        TestEqs("abc", cstr(contents));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_file_operations)
{
    SV_TEST("remove() existing file")
    {
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "a.txt", path, ""));
        TestTrue(os_file_exists(cstr(path)));
        TestTrue(os_remove(cstr(path)));
        TestTrue(!os_file_exists(cstr(path)));
    }

    SV_TEST("remove() existing directory")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%s%s", tempdir, pathsep, "a"));
        TestTrue(os_create_dir(cstr(path)));
        TestTrue(os_dir_exists(cstr(path)));
        TestTrue(os_remove(cstr(path)));
        TestTrue(!os_dir_exists(cstr(path)));
    }

    SV_TEST("remove() should fail non-empty directory")
    {
        TEST_OPEN_EX(bstring, d, bformat("%s%s%s", tempdir, pathsep, "a"));
        TEST_OPEN(bstring, f);
        check_b(os_create_dir(cstr(d)), "");
        check_b(os_dir_exists(cstr(d)), "");
        check(tmpwritetextfile(cstr(d), "f.txt", f, "f"));
        TestTrue(!os_remove(cstr(d)));
        TestTrue(os_dir_exists(cstr(d)));
        TestTrue(os_remove(cstr(f)));
        TestTrue(os_remove(cstr(d)));
        TestTrue(!os_dir_exists(cstr(d)));
    }

    SV_TEST("remove() non-existing file")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%s%s", tempdir, pathsep, "z"));
        TestTrue(os_remove(cstr(path)));
    }

    SV_TEST("remove() non-existing/invalid path")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%s%s", tempdir, pathsep, "||z"));
        TestTrue(os_remove(cstr(path)));
    }

    SV_TEST("attempt copy missing src")
    {
        TEST_OPEN_EX(bstring, path1, bformat("%s%sx", tempdir, pathsep));
        TEST_OPEN_EX(bstring, path2, bformat("%s%sz", tempdir, pathsep));
        quiet_warnings(true);
        TestTrue(!os_copy(cstr(path1), cstr(path2), true));
        quiet_warnings(false);
    }

    SV_TEST("attempt copy missing dest")
    {
        TEST_OPEN_EX(bstring, dest,
            bformat("%s%s%s%sa", tempdir, pathsep, "notexist", pathsep));
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "ccopy.txt", path, "a"));
        TestTrue(os_file_exists(cstr(path)));
        quiet_warnings(true);
        TestTrue(!os_copy(cstr(path), cstr(dest), true));
        quiet_warnings(false);
        TestTrue(!os_file_exists(cstr(dest)));
    }

    SV_TEST("attempt copy invalid src")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%s%s", tempdir, pathsep, "z"));
        quiet_warnings(true);
        TestTrue(!os_copy("||invalid", cstr(path), true));
        quiet_warnings(false);
    }

    SV_TEST("attempt copy invalid dest")
    {
        TEST_OPEN_EX(bstring, dest,
            islinux ? bfromcstr("") : bformat("%s%s||||", tempdir, pathsep));
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "ccopy.txt", path, "a"));
        TestTrue(os_file_exists(cstr(path)));
        quiet_warnings(true);
        TestTrue(!os_copy(cstr(path), cstr(dest), true));
        quiet_warnings(false);
        TestTrue(!os_file_exists(cstr(dest)));
    }

    SV_TEST("copy dest exists, overwrite disallowed")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr"));
        check(tmpwritetextfile(tempdir, "cdest.txt", dest, "!dest"));
        quiet_warnings(true);
        TestTrue(!os_copy(cstr(src), cstr(dest), false));
        quiet_warnings(false);
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!dest", cstr(s));
    }

    SV_TEST("copy dest exists, overwrite allowed 1")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr"));
        check(tmpwritetextfile(tempdir, "cdest.txt", dest, "!dest"));
        TestTrue(os_copy(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr", cstr(s));
    }

    SV_TEST("copy dest exists, overwrite allowed 2")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr longer"));
        check(tmpwritetextfile(tempdir, "cdest.txt", dest, "!dest"));
        TestTrue(os_copy(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr longer", cstr(s));
    }

    SV_TEST("normal copy, overwrite disallowed")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr1"));
        bsetfmt(dest, "%s%s%s", tempdir, pathsep, "cdest.txt");
        TestTrue(os_copy(cstr(src), cstr(dest), false));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr1", cstr(s));
    }

    SV_TEST("normal copy, overwrite allowed")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr2"));
        bsetfmt(dest, "%s%s%s", tempdir, pathsep, "cdest.txt");
        TestTrue(os_copy(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr2", cstr(s));
    }

    SV_TEST("normal copy, large file")
    {
        TEST_OPEN4(bstring, src, dest, s, text);
        bstr_fill(text, 'a', 65 * 1024);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, cstr(text)));
        bsetfmt(dest, "%s%s%s", tempdir, pathsep, "cdest.txt");
        TestTrue(os_copy(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs(cstr(text), cstr(s));
        TestEqn(cast64s64u(text->slen), os_getfilesize(cstr(dest)));
        TestTrue(os_file_exists(cstr(src)));
    }

    SV_TEST("copy onto itself")
    {
        TEST_OPEN2(bstring, src, s);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr3"));
        TestTrue(os_copy(cstr(src), cstr(src), false));
        check(sv_file_readfile(cstr(src), s));
        TestEqs("!sr3", cstr(s));
    }

    SV_TEST("attempt move missing src")
    {
        TEST_OPEN_EX(bstring, path1, bformat("%s%s%s", tempdir, pathsep, "x"));
        TEST_OPEN_EX(bstring, path2, bformat("%s%s%s", tempdir, pathsep, "z"));
        TestTrue(!os_move(cstr(path1), cstr(path2), true));
    }

    SV_TEST("attempt move missing dest")
    {
        TEST_OPEN_EX(bstring, dest,
            bformat("%s%s%s%sa", tempdir, pathsep, "notexist", pathsep));
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "cmove.txt", path, "a"));
        TestTrue(os_file_exists(cstr(path)));
        quiet_warnings(true);
        TestTrue(!os_move(cstr(path), cstr(dest), true));
        quiet_warnings(false);
        TestTrue(!os_file_exists(cstr(dest)));
        TestTrue(os_file_exists(cstr(path)));
    }

    SV_TEST("attempt move invalid src")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%s%s", tempdir, pathsep, "z"));
        quiet_warnings(true);
        TestTrue(!os_move("||invalid", cstr(path), true));
        quiet_warnings(false);
    }

    SV_TEST("attempt move invalid dest")
    {
        TEST_OPEN_EX(bstring, dest,
            islinux ? bfromcstr("") : bformat("%s%s||||", tempdir, pathsep));
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "cmove.txt", path, "a"));
        TestTrue(os_file_exists(cstr(path)));
        quiet_warnings(true);
        TestTrue(!os_move(cstr(path), cstr(dest), true));
        quiet_warnings(false);
        TestTrue(!os_file_exists(cstr(dest)));
        TestTrue(os_file_exists(cstr(path)));
    }

    SV_TEST("move dest exists, overwrite disallowed")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr"));
        check(tmpwritetextfile(tempdir, "cdest.txt", dest, "!dest"));
        quiet_warnings(true);
        TestTrue(!os_move(cstr(src), cstr(dest), false));
        quiet_warnings(false);
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!dest", cstr(s));
        TestTrue(os_file_exists(cstr(src)));
    }

    SV_TEST("move dest exists, overwrite allowed 1")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr"));
        check(tmpwritetextfile(tempdir, "cdest.txt", dest, "!dest"));
        TestTrue(os_move(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr", cstr(s));
        TestTrue(!os_file_exists(cstr(src)));
    }

    SV_TEST("move dest exists, overwrite allowed 2")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr longer"));
        check(tmpwritetextfile(tempdir, "cdest.txt", dest, "!dest"));
        TestTrue(os_move(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr longer", cstr(s));
        TestTrue(!os_file_exists(cstr(src)));
    }

    SV_TEST("normal move, overwrite disallowed")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr1"));
        bsetfmt(dest, "%s%s%s", tempdir, pathsep, "cdest.txt");
        TestTrue(os_move(cstr(src), cstr(dest), false));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr1", cstr(s));
        TestTrue(!os_file_exists(cstr(src)));
    }

    SV_TEST("normal move, overwrite allowed")
    {
        TEST_OPEN3(bstring, src, dest, s);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr2"));
        bsetfmt(dest, "%s%s%s", tempdir, pathsep, "cdest.txt");
        TestTrue(os_move(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs("!sr2", cstr(s));
        TestTrue(!os_file_exists(cstr(src)));
    }

    SV_TEST("normal move, large file")
    {
        TEST_OPEN4(bstring, src, dest, s, text);
        bstr_fill(text, 'a', 65 * 1024);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, cstr(text)));
        bsetfmt(dest, "%s%s%s", tempdir, pathsep, "cdest.txt");
        TestTrue(os_move(cstr(src), cstr(dest), true));
        check(sv_file_readfile(cstr(dest), s));
        TestEqs(cstr(text), cstr(s));
        TestEqn(cast64s64u(text->slen), os_getfilesize(cstr(dest)));
        TestTrue(!os_file_exists(cstr(src)));
    }

    SV_TEST("move onto itself")
    {
        TEST_OPEN2(bstring, src, s);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "csrc.txt", src, "!sr3"));
        TestTrue(os_move(cstr(src), cstr(src), false));
        check(sv_file_readfile(cstr(src), s));
        TestEqs("!sr3", cstr(s));
    }

    SV_TEST("get filesize of missing file")
    {
        TEST_OPEN_EX(bstring, path, bformat("%s%s%s", tempdir, pathsep, "z"));
        TestEqn(0, os_getfilesize(cstr(path)));
    }

    SV_TEST("get filesize of empty file")
    {
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "getsize.txt", path, ""));
        TestEqn(0, os_getfilesize(cstr(path)));
    }

    SV_TEST("get filesize of very small file")
    {
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "getsize.txt", path, "abc"));
        TestEqn(strlen("abc"), os_getfilesize(cstr(path)));
    }

    SV_TEST("get filesize of small file")
    {
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(
            tempdir, "getsize.txt", path, "123456789123456789123456789"));
        TestEqn(
            strlen("123456789123456789123456789"), os_getfilesize(cstr(path)));
    }

    SV_TEST("set and read modified time, first")
    {
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "lmt.txt", path, ""));
        check_b(
            os_setmodifiedtime_nearestsecond(cstr(path), 0x5f000000LL),
            "");
        long long timegot = (long long)os_getmodifiedtime(cstr(path));
        TestTrue(llabs(timegot - 0x5f000000LL) < 10);
    }

    SV_TEST("set and read modified time, second")
    {
        TEST_OPEN(bstring, path);
        check(tmpwritetextfile(tempdir, "lmt.txt", path, ""));
        check_b(
            os_setmodifiedtime_nearestsecond(cstr(path), 0x5f100000LL),
            "");
        long long timegot = (long long)os_getmodifiedtime(cstr(path));
        TestTrue(llabs(timegot - 0x5f100000LL) < 10);
    }

    SV_TEST_()
    {
        TestTrue(!os_file_exists(".."));
    }

    SV_TEST_()
    {
        TestTrue(!os_file_exists("||invalid"));
    }

    SV_TEST_()
    {
        TestTrue(!os_file_exists("//invalid"));
    }

    SV_TEST_()
    {
        TestTrue(!os_file_exists(tempdir));
    }

    SV_TEST_()
    {
        TestTrue(!os_dir_exists(""));
    }

    SV_TEST_()
    {
        TestTrue(!os_dir_exists("||invalid"));
    }

    SV_TEST_()
    {
        TestTrue(!os_dir_exists("//invalid"));
    }

    SV_TEST_()
    {
        TestTrue(os_dir_exists(tempdir));
    }
}
SV_END_TEST_SUITE()

static check_result add_file_to_list_callback(void *context,
    const bstring filepath, uint64_t modtime, uint64_t filesize,
    unused(const bstring))
{
    bstrlist *list = (bstrlist *)context;
    TestTrue(modtime != 0);
    TestTrue(
        os_dir_exists(cstr(filepath)) == os_recurse_is_dir(modtime, filesize));
    TestTrue(
        os_file_exists(cstr(filepath)) == !os_recurse_is_dir(modtime, filesize));
    bstring s = bstring_open();
    os_get_filename(cstr(filepath), s);
    bformata(s, ":%llx", (unsigned long long)filesize);
    bstrlist_append(list, s);
    bdestroy(s);
    return OK;
}

SV_BEGIN_TEST_SUITE(tests_bypattern)
{
    SV_TEST("delete matches no files")
    {
        TEST_OPEN_EX(bstring, d1, bformat("%s" pathsep "d1", tempdir));
        TEST_OPEN3(bstring, f1, f2, f3);
        check_b(os_create_dirs(cstr(d1)), "");
        check(tmpwritetextfile(cstr(d1), "f1txt", f1, "contents"));
        check(tmpwritetextfile(cstr(d1), "f2.txta", f2, "contents"));
        check(tmpwritetextfile(cstr(d1), "f3.TXT", f3, "contents"));
        check(os_tryuntil_deletefiles(cstr(d1), "*.txt"));
        TestTrue(os_file_exists(cstr(f1)));
        TestTrue(os_file_exists(cstr(f2)));
        TestTrue(os_file_exists(cstr(f3)));
    }

    SV_TEST("delete matches some files")
    {
        TEST_OPEN_EX(bstring, d2, bformat("%s" pathsep "d2", tempdir));
        TEST_OPEN3(bstring, f1, f2, f3);
        check_b(os_create_dirs(cstr(d2)), "");
        check(tmpwritetextfile(cstr(d2), "f1.doc.txt", f1, "contents"));
        check(tmpwritetextfile(cstr(d2), "f2.txt.doc", f2, "contents"));
        check(tmpwritetextfile(cstr(d2), "a.txt", f3, "contents"));
        check(os_tryuntil_deletefiles(cstr(d2), "*.txt"));
        TestTrue(!os_file_exists(cstr(f1)));
        TestTrue(os_file_exists(cstr(f2)));
        TestTrue(!os_file_exists(cstr(f3)));
    }

    SV_TEST("move matches no files")
    {
        int moved = 0;
        TEST_OPEN_EX(bstring, d3, bformat("%s" pathsep "d3", tempdir));
        TEST_OPEN3(bstring, f1, f2, f3);
        check_b(os_create_dirs(cstr(d3)), "");
        check(tmpwritetextfile(cstr(d3), "f1txt", f1, "contents"));
        check(tmpwritetextfile(cstr(d3), "f2.txta", f2, "contents"));
        check(tmpwritetextfile(cstr(d3), "f3.TXT", f3, "contents"));
        check(
            os_tryuntil_movebypattern(cstr(d3), "*.txt", tempdir, true, &moved));
        TestTrue(os_file_exists(cstr(f1)));
        TestTrue(os_file_exists(cstr(f2)));
        TestTrue(os_file_exists(cstr(f3)));
        TestEqn(0, moved);
    }

    SV_TEST("move matches some files")
    {
        int moved = 0;
        TEST_OPEN_EX(bstring, d4, bformat("%s" pathsep "d4", tempdir));
        TEST_OPEN4(bstring, contents1, contents2, contents3, dest);
        TEST_OPEN3(bstring, f1, f2, f3);
        check_b(os_create_dirs(cstr(d4)), "");
        check(tmpwritetextfile(cstr(d4), "f1.doc.txt", f1, "co1"));
        check(tmpwritetextfile(cstr(d4), "f2.txt.doc", f2, "co2"));
        check(tmpwritetextfile(cstr(d4), "a.txt", f3, "co3"));
        check(
            os_tryuntil_movebypattern(cstr(d4), "*.txt", tempdir, true, &moved));
        bsetfmt(dest, "%s" pathsep "f1.doc.txt", tempdir);
        check(sv_file_readfile(cstr(dest), contents1));
        bsetfmt(dest, "%s" pathsep "f2.txt.doc", cstr(d4));
        check(sv_file_readfile(cstr(dest), contents2));
        bsetfmt(dest, "%s" pathsep "a.txt", tempdir);
        check(sv_file_readfile(cstr(dest), contents3));
        TestTrue(!os_file_exists(cstr(f1)));
        TestTrue(os_file_exists(cstr(f2)));
        TestTrue(!os_file_exists(cstr(f3)));
        TestEqs("co1", cstr(contents1));
        TestEqs("co2", cstr(contents2));
        TestEqs("co3", cstr(contents3));
        TestEqn(2, moved);
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_file_locks)
{
    SV_TEST("os_lockedfilehandle_stat reads filesize")
    {
        TEST_OPEN2(bstring, f, perms);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        uint64_t expected_lmt = os_getmodifiedtime(cstr(f));
        uint64_t expected_size = strlen("contents");
        uint64_t got_lmt = 0, got_size = 0;
        os_lockedfilehandle handle = {};
        check(os_lockedfilehandle_open(&handle, cstr(f), true, NULL));
        check(os_lockedfilehandle_stat(&handle, &got_size, &got_lmt, perms));
        os_lockedfilehandle_close(&handle);
        TestTrue(fnmatch_simple(islinux ? "p*|g*|u*" : "*", cstr(perms)));
        TestEqn(expected_size, got_size);
        TestEqn(expected_lmt, got_lmt);
    }

#if __linux__
    if (geteuid() == 0)
    {
        puts("skipping test because process is running as root.");
        goto cleanup;
    }
#endif

    SV_TEST_LIN("can't get a exclusivefilehandle on file without access")
    {
        os_lockedfilehandle handle = {};
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check_b(chmod(cstr(f), 0111) == 0, "");
        expect_err_with_message(
            os_lockedfilehandle_open(&handle, cstr(f), true, NULL),
            "Permission denied");
        os_lockedfilehandle_close(&handle);
        check_b(chmod(cstr(f), 0777) == 0, "");
    }

    SV_TEST_WIN("RW exclusivefilehandle locks out a FILE write")
    {
        os_lockedfilehandle handle = {};
        sv_file filewrapper = {};
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(os_lockedfilehandle_open(&handle, cstr(f), false, NULL));
        expect_err_with_message(
            sv_file_open(&filewrapper, cstr(f), "w"), "failed to open ");
        os_lockedfilehandle_close(&handle);
    }

    SV_TEST_WIN("RW exclusivefilehandle locks out a FILE read")
    {
        os_lockedfilehandle handle = {};
        sv_file filewrapper = {};
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(os_lockedfilehandle_open(&handle, cstr(f), false, NULL));
        expect_err_with_message(
            sv_file_open(&filewrapper, cstr(f), "r"), "failed to open ");
        os_lockedfilehandle_close(&handle);
    }

    SV_TEST_WIN("R exclusivefilehandle locks out a FILE write")
    {
        os_lockedfilehandle handle = {};
        sv_file filewrapper = {};
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(os_lockedfilehandle_open(&handle, cstr(f), true, NULL));
        expect_err_with_message(
            sv_file_open(&filewrapper, cstr(f), "w"), "failed to open ");
        os_lockedfilehandle_close(&handle);
    }

    SV_TEST_WIN("R exclusivefilehandle allows a FILE read")
    {
        os_lockedfilehandle handle = {};
        TEST_OPEN2(bstring, f, contents);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(os_lockedfilehandle_open(&handle, cstr(f), true, NULL));
        check(sv_file_readfile(cstr(f), contents));
        os_lockedfilehandle_close(&handle);
        TestEqs("contents", cstr(contents));
    }

    SV_TEST_WIN("existing lock prevents exclusivefilehandle")
    {
        os_lockedfilehandle handle = {};
        sv_file filewrapper = {};
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(sv_file_open(&filewrapper, cstr(f), "ab"));
        expect_err_with_message(
            os_lockedfilehandle_open(&handle, cstr(f), true, NULL), "open() failed");
        os_lockedfilehandle_close(&handle);
        sv_file_close(&filewrapper);
    }

    SV_TEST("lock same file twice")
    {
        /* note that posix locks are released after the first close */
        os_lockedfilehandle h1 = {}, h2 = {};
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(os_lockedfilehandle_open(&h1, cstr(f), true, NULL));
        check(os_lockedfilehandle_open(&h2, cstr(f), true, NULL));
        os_lockedfilehandle_close(&h2);
        os_lockedfilehandle_close(&h1);
    }

    SV_TEST("open handle, path found, should not sleep")
    {
        os_lockedfilehandle handle = {};
        bool file_missing = true;
        sleep_between_tries = 10000;
        TEST_OPEN(bstring, f);
        check(tmpwritetextfile(tempdir, "a.txt", f, "contents"));
        check(os_lockedfilehandle_tryuntil_open(
            &handle, cstr(f), true, &file_missing));
        os_lockedfilehandle_close(&handle);
        TestTrue(!file_missing);
    }

    SV_TEST("open handle, path not found, should not sleep")
    {
        TEST_OPEN_EX(bstring, f, bformat("%s" pathsep "not-found.txt", tempdir));
        os_lockedfilehandle handle = {};
        bool file_missing = true;
        sleep_between_tries = 10000;
        check(os_lockedfilehandle_tryuntil_open(
            &handle, cstr(f), true, &file_missing));
        check_b(handle.fd <= 0, "");
        os_lockedfilehandle_close(&handle);
        TestTrue(file_missing);
    }

    SV_TEST("open handle, directory not found, should not sleep")
    {
        TEST_OPEN_EX(
            bstring, f, bformat("%s" pathsep "no-dir" pathsep "a", tempdir));
        os_lockedfilehandle handle = {};
        bool file_missing = true;
        sleep_between_tries = 10000;
        check(os_lockedfilehandle_tryuntil_open(
            &handle, cstr(f), true, &file_missing));
        check_b(handle.fd <= 0, "");
        os_lockedfilehandle_close(&handle);
        TestTrue(file_missing);
    }

    SV_TEST("this dir is writable")
    {
        TestTrue(os_is_dir_writable(tempdir));
    }

    SV_TEST("mimic non-writable dir")
    {
        TEST_OPEN(bstring, f);
        os_lockedfilehandle handle = {};
        check(tmpwritetextfile(tempdir, "$$testwrite$$.tmp", f, "contents"));
        check(tests_lockfile(true, cstr(f), &handle));
        quiet_warnings(true);
        bool is_writable = os_is_dir_writable(tempdir);
        quiet_warnings(false);
        check(tests_lockfile(false, cstr(f), &handle));
        os_lockedfilehandle_close(&handle);
        TestTrue(!is_writable);
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_recurse_corner_cases)
{
    void createsearchspec(const sv_wstr *dir, sv_wstr *buffer);

#ifdef _WIN32
    SV_TEST("test with invalid path")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, msg, bstrlist_open());
        os_recurse_params params = {
            list, "C:\\|||invalid|||", &add_file_to_list_callback, 0, msg};

        quiet_warnings(true);
        check(os_recurse(&params));
        quiet_warnings(false);
        TestEqn(0, list->qty);
        TestEqn(1, msg->qty);
        TestTrue(s_contains(blist_view(msg, 0), "filename, directory name, or"));
    }

    SV_TEST("make search spec for normal path")
    {
        TEST_OPENA(sv_wstr, input, 1);
        TEST_OPENA(sv_wstr, output, 1);
        sv_wstr_append(&input, L"C:\\a\\normal\\path");
        createsearchspec(&input, &output);
        TestTrue(ws_equal(L"C:\\a\\normal\\path\\*", wcstr(output)));
    }

    SV_TEST("make search spec for path with trailing backslash")
    {
        TEST_OPENA(sv_wstr, input, 1);
        TEST_OPENA(sv_wstr, output, 1);
        sv_wstr_append(&input, L"C:\\a\\normal\\trailing\\");
        createsearchspec(&input, &output);
        TestTrue(ws_equal(L"C:\\a\\normal\\trailing\\*", wcstr(output)));
    }

    SV_TEST("make search spec for root path")
    {
        TEST_OPENA(sv_wstr, input, 1);
        TEST_OPENA(sv_wstr, output, 1);
        sv_wstr_append(&input, L"D:\\");
        createsearchspec(&input, &output);
        TestTrue(ws_equal(L"D:\\*", wcstr(output)));
    }

    SV_TEST("win32 error to string when there is no error")
    {
        char buf[BUFSIZ] = "";
        os_win32err_to_buffer(0, buf, countof(buf));
        TestEqs("The operation completed successfully.\r\n", buf);
    }

    SV_TEST("win32 error to string for access denied")
    {
        char buf[BUFSIZ] = "";
        os_win32err_to_buffer(ERROR_ACCESS_DENIED, buf, countof(buf));
        TestEqs("Access is denied.\r\n", buf);
    }
#endif
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_startprocess)
{
    const char *sh = islinux ? "/bin/sh" : getenv("comspec");
    SV_TEST("building normal arguments")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, true));
        TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2\"", cstr(cmd));
    }

    SV_TEST("building args with spaces and interesting characters")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"  arg 1  ",
            "\\a\\/|? "
            "'`~!@#$%^&*()",
            NULL};
        TestTrue(argvquote("C:\\path\\the binary", args, cmd, true));
        TestEqs("\"C:\\path\\the binary\" \"  arg 1  "
                "\" \"\\a\\/|? '`~!@#$%^&*()\"",
            cstr(cmd));
    }

    SV_TEST("reject args containing double quotes, "
            "on windows this requires 'thorough argument join'")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2with\"quote", NULL};
        TestTrue(!argvquote("C:\\path\\binary", args, cmd, true));
    }

    SV_TEST("reject args containing trailing backslash, "
            "on windows this requires 'thorough argument join'")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2with\\", NULL};
        TestTrue(!argvquote("C:\\path\\binary", args, cmd, true));
    }

    SV_TEST("thorough arg join, building normal arguments")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2\"", cstr(cmd));
    }

    SV_TEST("thorough arg join, building args with interesting chars")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"  arg 1  ", "\\a\\/|? '`~!@#$%^&*()", NULL};
        TestTrue(argvquote("C:\\path\\the binary", args, cmd, false));
        TestEqs("\"C:\\path\\the binary\" \"  arg 1  "
                "\" \"\\a\\/|? '`~!@#$%^&*()\"",
            cstr(cmd));
    }

    SV_TEST("thorough arg join, containing double quotes")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2with\"quote", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs("\"C:\\path\\binary\" \"arg1\" "
                "\"arg2with\\\"quote\"",
            cstr(cmd));
    }

    SV_TEST("thorough arg join, containing trailing backslash")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2with\\", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs("\"C:\\path\\binary\" \"arg1\" "
                "\"arg2with\\\\\"",
            cstr(cmd));
    }

    SV_TEST("thorough arg join, containing just two double quotes")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "\"\"", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs("\"C:\\path\\binary\" \"arg1\" \"\\\"\\\"\"", cstr(cmd));
    }

    SV_TEST("thorough arg join, containing just three double quotes")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "\"\"\"", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs("\"C:\\path\\binary\" \"arg1\" \"\\\"\\\"\\\"\"", cstr(cmd));
    }

    SV_TEST("thorough arg join, backslash before double quotes")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2with\\\\\"quote", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs("\"C:\\path\\binary\" \"arg1\" "
                "\"arg2with\\\\\\\\\\\"quote\"",
            cstr(cmd));
    }

    SV_TEST("thorough arg join, containing trailing backslashes")
    {
        TEST_OPEN(bstring, cmd);
        const char *args[] = {"arg1", "arg2with\\\\\\", NULL};
        TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
        TestEqs(
            "\"C:\\path\\binary\" \"arg1\" \"arg2with\\\\\\\\\\\\\"", cstr(cmd));
    }

    SV_TEST_WIN("read stdin")
    {
        TEST_OPEN4(bstring, path, pathsrc, pathdest, tmp);
        int retcode = 0;
        os_lockedfilehandle handle = {};
        check(tmpwritetextfile(tempdir, "src.txt", pathsrc, "abcde\r\nline"));
        check(tmpwritetextfile(tempdir, "dest.txt", pathdest, ""));
        check(tmpwritetextfile(tempdir, "s.bat", path,
            "@echo off\r\n"
            "set /p var=Enter:\r\n"
            "echo val is \"%var%\" > dest.txt"));

        const char *args[] = {"/c", cstr(path), NULL};
        check_b(os_setcwd(tempdir), "");
        check(os_lockedfilehandle_open(&handle, cstr(pathsrc), true, NULL));
        check(os_run_process(sh, args, NULL, tmp, true, 0, &handle, &retcode));
        check(sv_file_readfile(cstr(pathdest), tmp));
        os_lockedfilehandle_close(&handle);
        TestEqn(0, retcode);
        TestEqs("val is \"abcde\" \r\n", cstr(tmp));
    }

    SV_TEST_WIN("process exits with code zero")
    {
        TEST_OPEN3(bstring, path, combargs, out);
        int retcode = 0;
        check(tmpwritetextfile(tempdir, "s.bat", path,
            "@echo off\r\n"
            "echo s1s && echo s2s 1>&2 && echo s3s"));

        const char *args[] = {"/c", cstr(path), NULL};
        check(os_run_process(sh, args, out, combargs, true, 0, 0, &retcode));
        TestEqn(0, retcode);
        TestEqs("s1s \r\ns2s  \r\ns3s\r\n", cstr(out));
    }

    SV_TEST_WIN("process exits with code nonzero")
    {
        TEST_OPEN3(bstring, path, combargs, out);
        int retcode = 0;
        check(tmpwritetextfile(tempdir, "s.bat", path,
            "@echo off\r\n"
            "echo s3s && echo s4s 1>&2 && exit /b 123"));

        const char *args[] = {"/c", cstr(path), NULL};
        check(os_run_process(sh, args, out, combargs, true, 0, 0, &retcode));
        TestEqn(123, retcode);
        TestEqs("s3s \r\ns4s  \r\n", cstr(out));
    }

    SV_TEST_WIN("process echos a parameter")
    {
        TEST_OPEN3(bstring, path, combargs, out);
        int retcode = 0;
        check(tmpwritetextfile(tempdir, "s.bat", path,
            "@echo off\r\n"
            "echo s6s && echo %1"));

        const char *args[] = {"/c", cstr(path), "giveparam", NULL};
        check(os_run_process(sh, args, out, combargs, true, 0, 0, &retcode));
        TestEqn(0, retcode);
        TestEqs("s6s \r\n\"giveparam\r\n", cstr(out));
    }

    SV_TEST_LIN("process exits with code zero")
    {
        TEST_OPEN3(bstring, path, combargs, out);
        int retcode = 0;
        check(tmpwritetextfile(tempdir, "s.sh", path,
            "echo 's1s' && echo 's2s' 1>&2 && echo 's3s'"));

        const char *args[] = {sh, cstr(path), NULL};
        check(os_run_process(sh, args, out, combargs, true, 0, 0, &retcode));
        TestEqn(0, retcode);
        TestEqs("s1s\ns2s\ns3s\n", cstr(out));
    }

    SV_TEST_LIN("process exits with code nonzero")
    {
        TEST_OPEN3(bstring, path, combargs, out);
        int retcode = 0;
        check(tmpwritetextfile(
            tempdir, "s.sh", path, "echo 's3s' && echo 's4s' 1>&2 && exit 123"));

        const char *args[] = {sh, cstr(path), NULL};
        check(os_run_process(sh, args, out, combargs, true, 0, 0, &retcode));
        TestEqn(123, retcode);
        TestEqs("s3s\ns4s\n", cstr(out));
    }

    SV_TEST_LIN("process echos a parameter")
    {
        TEST_OPEN3(bstring, path, combargs, out);
        int retcode = 0;
        check(tmpwritetextfile(tempdir, "s.sh", path, "echo 's6s' && echo $1"));

        const char *args[] = {sh, cstr(path), "giveparam", NULL};
        check(os_run_process(sh, args, out, combargs, true, 0, 0, &retcode));
        TestEqn(0, retcode);
        TestEqs("s6s\ngiveparam\n", cstr(out));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_incrementing_filenames)
{
    SV_TEST("no matching files, latest number is 0")
    {
        uint32_t latestnumber = UINT32_MAX;
        check(readlatestnumberfromfilename(
            tempdir, "testlog", ".txt", &latestnumber));
        TestEqn(0, latestnumber);
    }

    SV_TEST("third file present, should return 3")
    {
        uint32_t latestnumber = UINT32_MAX;
        TEST_OPEN(bstring, file);
        check(tmpwritetextfile(tempdir, "testlog00003.txt", file, ""));
        check(readlatestnumberfromfilename(
            tempdir, "testlog", ".txt", &latestnumber));
        TestEqn(3, latestnumber);
    }

    SV_TEST("first, third file present, should return 3")
    {
        uint32_t latestnumber = UINT32_MAX;
        TEST_OPEN(bstring, file);
        check(tmpwritetextfile(tempdir, "testlog00001.txt", file, ""));
        check(readlatestnumberfromfilename(
            tempdir, "testlog", ".txt", &latestnumber));
        TestEqn(3, latestnumber);
    }

    SV_TEST("first, second, third file present, should return 3")
    {
        uint32_t latestnumber = UINT32_MAX;
        TEST_OPEN(bstring, file);
        check(tmpwritetextfile(tempdir, "testlog02.txt", file, ""));
        check(readlatestnumberfromfilename(
            tempdir, "testlog", ".txt", &latestnumber));
        TestEqn(3, latestnumber);
    }

    SV_TEST("high number file present")
    {
        uint32_t latestnumber = UINT32_MAX;
        TEST_OPEN(bstring, file);
        check(tmpwritetextfile(tempdir, "testlog000123456.txt", file, ""));
        check(readlatestnumberfromfilename(
            tempdir, "testlog", ".txt", &latestnumber));
        TestEqn(123456, latestnumber);
    }

    SV_TEST("first file present, should return 1.")
    {
        uint32_t latestnumber = UINT32_MAX;
        TEST_OPEN(bstring, file);
        check(os_tryuntil_deletefiles(tempdir, "*"));
        check(tmpwritetextfile(tempdir, "testlog01.txt", file, ""));
        check(readlatestnumberfromfilename(
            tempdir, "testlog", ".txt", &latestnumber));
        TestEqn(1, latestnumber);
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, file);
        appendnumbertofilename("/path/dir", "prefix", ".txt", 1, file);
        TestEqs("/path/dir" pathsep "prefix00001.txt", cstr(file));
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, file);
        appendnumbertofilename("/path/dir", "prefix", ".txt", 123456, file);
        TestEqs("/path/dir" pathsep "prefix123456.txt", cstr(file));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "/path/file01.txt2"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "/2path/file01.txt"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "2/path/file01.txt"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "/path/fileA01.txt"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "/path/fileA01A.txt"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "/path/file01A.txt"));
    }

    SV_TEST_()
    {
        TestEqn(
            0, readnumberfromfilename("/path/file", ".txt", "/path/file01txt"));
    }

    SV_TEST_()
    {
        TestEqn(
            0, readnumberfromfilename("/path/file", ".txt", "/path/file.txt"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename("/path/file", ".txt", "/path/file12A34.txt"));
    }

    SV_TEST_()
    {
        TestEqn(0,
            readnumberfromfilename(
                "/path/file", ".txt", "/path/file01\xCC.txt"));
    }

    SV_TEST_()
    {
        TestEqn(
            0, readnumberfromfilename("/path/file", ".txt", "/path/file00.txt"));
    }

    SV_TEST_()
    {
        TestEqn(
            1, readnumberfromfilename("/path/file", ".txt", "/path/file1.txt"));
    }

    SV_TEST_()
    {
        TestEqn(1,
            readnumberfromfilename("/path/file", ".txt", "/path/file00001.txt"));
    }

    SV_TEST_()
    {
        TestEqn(12345,
            readnumberfromfilename("/path/file", ".txt", "/path/file12345.txt"));
    }

    SV_TEST_()
    {
        TestEqn(123456,
            readnumberfromfilename(
                "/path/file", ".txt", "/path/file123456.txt"));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_recurse_dir)
{
    TEST_OPEN_EX(bstring, d1, bformat("%s" pathsep "d1 \xED\x95\x9C", tempdir));
    TEST_OPEN_EX(bstring, d2, bformat("%s" pathsep "d2 \xED\x95\x9C", tempdir));
    TEST_OPEN_EX(bstring, d3, bformat("%s" pathsep "d3", cstr(d1)));
    TEST_OPEN_EX(bstring, d4, bformat("%s" pathsep "d4", cstr(d3)));
    TEST_OPEN3(bstring, f1, f2, f3);
    check_b(os_create_dir(cstr(d1)), "");
    check_b(os_create_dir(cstr(d2)), "");
    check_b(os_create_dir(cstr(d3)), "");
    check_b(os_create_dir(cstr(d4)), "");
    check(tmpwritetextfile(cstr(d1), "f1.txt", f1, "I"));
    check(tmpwritetextfile(cstr(d1), "f2.txt", f2, "II"));
    check(tmpwritetextfile(cstr(d2), "f3.txt", f3, "III"));

    SV_TEST("list files, not recursive")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        check(os_listfiles(cstr(d1), list, true));
        TestEqn(2, list->qty);
        TestTrue(s_endwith(blist_view(list, 0), pathsep "f1.txt"));
        TestTrue(s_endwith(blist_view(list, 1), pathsep "f2.txt"));
    }

    SV_TEST("list dirs, not recursive")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        check(os_listdirs(tempdir, list, true));
        TestEqn(2, list->qty);
        TestTrue(s_endwith(blist_view(list, 0), pathsep "d1 \xED\x95\x9C"));
        TestTrue(s_endwith(blist_view(list, 1), pathsep "d2 \xED\x95\x9C"));
    }

    SV_TEST("recurse files and dirs")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        os_recurse_params params = {
            list, tempdir, &add_file_to_list_callback, INT_MAX};
        check(os_recurse(&params));
        bstrlist_sort(list);
        TestEqList("d1 \xED\x95\x9C:ffffffffffffffff|"
                   "d2 \xED\x95\x9C:ffffffffffffffff|"
                   "d3:ffffffffffffffff|d4:ffffffffffffffff|"
                   "f1.txt:1|f2.txt:2|f3.txt:3",
            list);
    }

    SV_TEST("recurse files and dirs, hit recursion limit")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        os_recurse_params params = {list, tempdir, &add_file_to_list_callback};
        params.max_recursion_depth = 1;
        expect_err_with_message(os_recurse(&params), "recursion limit");
    }

    SV_TEST("recurse on missing path")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, msg, bstrlist_open());
        TEST_OPEN_EX(bstring, path, bformat("%s%snotexist", tempdir, pathsep));
        os_recurse_params params = {
            list, cstr(path), &add_file_to_list_callback, 0, msg};
        check(os_recurse(&params));
        TestEqn(0, list->qty);
        TestEqn(0, msg->qty);
    }

    SV_TEST("recurse on invalid path should fail")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        os_recurse_params params = {list, "", &add_file_to_list_callback};
        expect_err_with_message(os_recurse(&params), "expected full path");
        TestEqn(0, list->qty);
    }

    check(os_tryuntil_deletefiles(cstr(d4), "*"));
    check(os_tryuntil_deletefiles(cstr(d3), "*"));
    check(os_tryuntil_deletefiles(cstr(d2), "*"));
    check(os_tryuntil_deletefiles(cstr(d1), "*"));
    check_b(os_remove(cstr(d4)), "");
    check_b(os_remove(cstr(d3)), "");
    check_b(os_remove(cstr(d2)), "");
    check_b(os_remove(cstr(d1)), "");
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_logging)
{
    SV_TEST("logging silently ignored if nothing is registered")
    {
        sv_log_register_active_logger(NULL);
        sv_log_write("test");
    }

    SV_TEST("test time formatting")
    {
        TEST_OPEN_EX(
            bstring, path, bformat("%s%s%s", tempdir, pathsep, "time.txt"));
        TEST_OPEN(bstring, s);
        sv_file f = {};
        check(sv_file_open(&f, cstr(path), "wb"));
        sv_log_addnewlinetime(f.file, 0, 123, 100);
        sv_file_close(&f);
        check(sv_file_readfile(cstr(path), s));
        TestEqs(nativenewline "00:02:03:100 ", cstr(s));
    }

    SV_TEST("test time formatting morning of actual day")
    {
        TEST_OPEN_EX(
            bstring, path, bformat("%s%s%s", tempdir, pathsep, "time.txt"));
        TEST_OPEN(bstring, s);
        sv_file f = {};
        check(sv_file_open(&f, cstr(path), "wb"));
        int64_t day_start_sep_27 = 1474934400ULL;
        int64_t morning_sep_27 = 1474949106ULL;
        sv_log_addnewlinetime(f.file, day_start_sep_27, morning_sep_27, 1);
        sv_file_close(&f);
        check(sv_file_readfile(cstr(path), s));
        TestEqs(nativenewline "04:05:06:001 ", cstr(s));
    }

    SV_TEST("test time formatting evening of actual day")
    {
        TEST_OPEN_EX(
            bstring, path, bformat("%s%s%s", tempdir, pathsep, "time.txt"));
        TEST_OPEN(bstring, s);
        sv_file f = {};
        check(sv_file_open(&f, cstr(path), "wb"));
        int64_t day_start_sep_27 = 1474934400ULL;
        int64_t evening_sep_27 = 1475007777ULL;
        sv_log_addnewlinetime(f.file, day_start_sep_27, evening_sep_27, 10);
        sv_file_close(&f);
        check(sv_file_readfile(cstr(path), s));
        TestEqs(nativenewline "20:22:57:010 ", cstr(s));
    }

    SV_TEST("write formatted log entries")
    {
        TEST_OPEN_EX(bstring, logpathfirst,
            bformat("%s%s%s", tempdir, pathsep, "log00001.txt"));
        TEST_OPEN(bstring, s);
        sv_log testlogger = {};
        check(sv_log_open(&testlogger, tempdir));
        sv_log_register_active_logger(&testlogger);
        sv_log_write("");
        sv_log_write("abcd");
        sv_log_fmt("s %s int %d", "abc", 123);
        sv_log_register_active_logger(NULL);
        sv_log_close(&testlogger);

        check(sv_file_readfile(cstr(logpathfirst), s));
        const char *pattern = nativenewline
            "????"
            "/"
            "??"
            "/"
            "??" nativenewline "??:??:??:??? " nativenewline
            "??:??:??:??? abcd" nativenewline "??:??:??:??? s abc int 123";
        TestTrue(fnmatch_simple(pattern, cstr(s)));
    }

    SV_TEST("write enough entries for second file")
    {
        TEST_OPEN_EX(bstring, logpathsecond,
            bformat("%s%s%s", tempdir, pathsep, "log00002.txt"));
        TestTrue(!os_file_exists(cstr(logpathsecond)));
        sv_log testlogger = {};
        check(sv_log_open(&testlogger, tempdir));
        testlogger.cap_filesize = 4;
        testlogger.counter = sv_log_check_size_period - 1;
        sv_log_register_active_logger(&testlogger);
        sv_log_write("abcd");
        sv_log_register_active_logger(NULL);
        sv_log_close(&testlogger);
        TestTrue(os_file_exists(cstr(logpathsecond)));
    }
}
SV_END_TEST_SUITE()
