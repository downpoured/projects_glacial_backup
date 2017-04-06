/*
tests_util_audio_tags.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <math.h>
#include "tests.h"

SV_BEGIN_TEST_SUITE(tests_read_md5_string)
{
    SV_TEST("assign zeros") {
        hash256 h = {};
        memset(&h, 1, sizeof(h));
        h = hash256zeros;
        TestEqn(0, h.data[0]);
        TestEqn(0, h.data[1]);
        TestEqn(0, h.data[2]);
        TestEqn(0, h.data[3]);
    }

    SV_TEST("read") {
        hash256 h = {};
        TestTrue(readhash("abc\nPress [q] to stop, [?] for "
            "help\nMD5=1122334455667788a1a2a3a4a5a6a7a8", &h));
        TestEqn(0x8877665544332211ULL, h.data[0]);
        TestEqn(0xa8a7a6a5a4a3a2a1ULL, h.data[1]);
        TestEqn(0, h.data[2]);
        TestEqn(0, h.data[3]);
    }

    SV_TEST("read, windows newlines") {
        hash256 h = {};
        TestTrue(readhash("abc\r\nPress [q] to stop, [?] for "
            "help\r\nMD5=1122334455667788a1a2a3a4a5a6a7a8", &h));
        TestEqn(0x8877665544332211ULL, h.data[0]);
        TestEqn(0xa8a7a6a5a4a3a2a1ULL, h.data[1]);
        TestEqn(0, h.data[2]);
        TestEqn(0, h.data[3]);
    }

    SV_TEST("read, intervening message") {
        hash256 h = {};
        TestTrue(readhash("abc\nPress [q] to stop, [?] for "
            "help\nNo more output streams to write to, finishing.\n"
            "MD5=1122334455667788a1a2a3a4a5a6a7a8\n0", &h));
        TestEqn(0x8877665544332211ULL, h.data[0]);
        TestEqn(0xa8a7a6a5a4a3a2a1ULL, h.data[1]);
        TestEqn(0, h.data[2]);
        TestEqn(0, h.data[3]);
    }

    SV_TEST("reading should skip over other text") {
        hash256 h = {};
        TestTrue(readhash("\nPress [q] to stop"
            "\nMMD5=0000004455667788a1a2a3a4a5a6a7a8"
            "\nPress [q] to stop, [?] for "
            "help\nMD5=1122334455667788a1a2a3a4a5a6a7a8"
            "other text", &h));
        TestEqn(0x8877665544332211ULL, h.data[0]);
        TestEqn(0xa8a7a6a5a4a3a2a1ULL, h.data[1]);
        TestEqn(0, h.data[2]);
        TestEqn(0, h.data[3]);
    }

    SV_TEST("reading empty string should return false") {
        hash256 h = {};
        TestTrue(!readhash("", &h));
    }

    SV_TEST("reading string with non hex chars should return false") {
        hash256 h = {};
        TestTrue(!readhash("\nPress [q] to stop, [?] for "
            "help\nMD5=1122334455667788a1a2a3a4a5a6a7x", &h));
    }

    SV_TEST("reading string that is too short should return false") {
        hash256 h = {};
        TestTrue(!readhash("\nPress [q] to stop, [?] for "
            "help\nMD5=\nother text", &h));
    }

    SV_TEST("reading string that is too short should return false") {
        hash256 h = {};
        TestTrue(!readhash("\nPress [q] to stop, [?] for "
            "help\nMD5=1122334455667788a1a2a3a4a5a6a7a", &h));
    }
}
SV_END_TEST_SUITE()

void get_tar_version_from_string(bstring s, double *outversion);

void get_hash(const char *path, const char *tempdir, bool sepmetadata,
    hash256 *h, uint32_t *crc)
{
    *h = hash256zeros;
    *crc = UINT32_MAX;
    ar_util ar = ar_util_open();
    os_lockedfilehandle handle = {};
    check_warn(os_lockedfilehandle_open(
        &handle, path, true, NULL), "", exit_on_err);

    check_warn(checkbinarypaths(&ar, !!sepmetadata, tempdir), "", exit_on_err);
    check_warn(hash_of_file(&handle,
        sepmetadata ? 1 : 0,
        sepmetadata ? filetype_mp3 : filetype_binary,
        sepmetadata ? cstr(ar.audiotag_binary) : "",
        h, crc), "", exit_on_err);

    os_lockedfilehandle_close(&handle);
    ar_util_close(&ar);
}

SV_BEGIN_TEST_SUITE(tests_get_version)
{
    SV_TEST("bytes to string for null buffer") {
        TEST_OPEN(bstring, s);
        bytes_to_string(NULL, 0, s);
        TestEqs("", cstr(s));
    }

    SV_TEST("bytes to string for zero-length buffer") {
        byte buf[] = { 0 };
        TEST_OPEN(bstring, s);
        bytes_to_string(buf, 0, s);
        TestEqs("", cstr(s));
    }

    SV_TEST("bytes to string for buffer length 6") {
        byte buf[] = { 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0x00 };
        TEST_OPEN(bstring, s);
        bytes_to_string(buf, 6, s);
        TestEqs("123456abcdef", cstr(s));
    }

    SV_TEST("bytes to string for buffer length 16") {
        byte buf[] = { 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0x00,
            0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xef, 0x00, 0x00 };
        TEST_OPEN(bstring, s);
        bytes_to_string(buf, 16, s);
        TestEqs("123456abcdef0000ffeeddccbbef0000", cstr(s));
    }

    SV_TEST("get tar version empty string") {
        tagbstring s = bstr_static("");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(0.0, d);
    }

    SV_TEST("get tar version not gnu") {
        tagbstring s = bstr_static("tar () 1.1");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(0.0, d);
    }

    SV_TEST("get tar version no number") {
        tagbstring s = bstr_static("tar (GNU tar) ");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(0.0, d);
    }

    SV_TEST("get tar version no period") {
        tagbstring s = bstr_static("tar (GNU tar) 12345");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(0.0, d);
    }

    SV_TEST("get tar version") {
        tagbstring s = bstr_static("tar (GNU tar) 0.2");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(0.2, d);
    }

    SV_TEST("get tar version") {
        tagbstring s = bstr_static("tar (GNU tar) 1.012");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(1.012, d);
    }

    SV_TEST("get tar version") {
        tagbstring s = bstr_static("tar (GNU tar) 1.1234");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(1.1234, d);
    }

    SV_TEST("get tar version") {
        tagbstring s = bstr_static("tar (GNU tar) 10.001");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(10.001, d);
    }

    SV_TEST("get tar version") {
        tagbstring s = bstr_static("tar (GNU tar) 10.9");
        double d = 0;
        get_tar_version_from_string(&s, &d);
        TestEqFloat(10.9, d);
    }
}
SV_END_TEST_SUITE()


SV_BEGIN_TEST_SUITE(tests_hash_audio)
{
    SV_TEST("hash of text file") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        check(sv_file_writefile(cstr(path), "abcde", "wb"));
        get_hash(cstr(path), tempdir, false, &h, &crc32);
        TestTrue(0x4d36bf2cf609ea58ULL == h.data[0] && 0x19b91b8a95e63aadULL == h.data[1]);
        TestTrue(0x1f1242b808bf0428ULL == h.data[2] && 0xf6539e0c067bcadaULL == h.data[3]);
        TestEqn(0x8587d865, crc32);
    }

    SV_TEST("hash of 0-byte file") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, path, bformat("%s%sa.txt", tempdir, pathsep));
        check(sv_file_writefile(cstr(path), "", "wb"));
        get_hash(cstr(path), tempdir, false, &h, &crc32);
        TestTrue(0x232706fc6bf50919ULL == h.data[0] && 0x8b72ee65b4e851c7ULL == h.data[1]);
        TestTrue(0x88d8e9628fb694aeULL == h.data[2] && 0x015c99660e766a98ULL == h.data[3]);
        TestEqn(0x00000000, crc32);
    }

    SV_TEST("hash of missing file") {
        hash256 h = {};
        uint32_t crc32 = 0;
        os_lockedfilehandle handle = {};
        TEST_OPEN_EX(bstring, path, bformat(
            "%s%sdoes-exist.txt", tempdir, pathsep));
        check(sv_file_writefile(cstr(path), "", "wb"));
        check(os_lockedfilehandle_open(&handle, cstr(path), true, NULL));
        bsetfmt(handle.loggingcontext, "%s%snot-exist.mp3", tempdir, pathsep);
        expect_err_with_message(hash_of_file(&handle, true,
            filetype_mp3, "",
            &h, &crc32), "not found");
        os_lockedfilehandle_close(&handle);
    }

    SV_TEST("hash of invalid handle with audio metadata enabled") {
        hash256 h = {};
        uint32_t crc32 = 0;
        os_lockedfilehandle handle = { bstring_open(), NULL, -1 };
        bsetfmt(handle.loggingcontext, "%s%sdoes-exist.txt", tempdir, pathsep);
        check(sv_file_writefile(cstr(handle.loggingcontext), "", "wb"));
        expect_err_with_message(hash_of_file(&handle, true,
            filetype_binary, "",
            &h, &crc32), "bad file handle");
        os_lockedfilehandle_close(&handle);
    }

    SV_TEST("hash of invalid handle with audio metadata disabled") {
        hash256 h = {};
        uint32_t crc32 = 0;
        os_lockedfilehandle handle = { bstring_open(), NULL, -1 };
        bsetfmt(handle.loggingcontext, "%s%sdoes-exist.txt", tempdir, pathsep);
        check(sv_file_writefile(cstr(handle.loggingcontext), "", "wb"));
        expect_err_with_message(hash_of_file(&handle, false,
            filetype_binary, "",
            &h, &crc32), "bad file handle");
        os_lockedfilehandle_close(&handle);
    }

    SV_TEST("hash of invalid mp3, get-metadata turned off") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, invalidmp3, bformat(
            "%s%si.mp3", tempdir, pathsep));
        check(sv_file_writefile(cstr(invalidmp3), "not-a-valid-mp3", "wb"));
        get_hash(cstr(invalidmp3), tempdir, false, &h, &crc32);
        TestTrue(0x2998bf76385ad9d0 == h.data[0] && 0xdda72b9b7ffb52ac == h.data[1]);
        TestTrue(0xeab56cf2bacbb5e2 == h.data[2] && 0x722db00c51c532f4 == h.data[3]);
        TestEqn(0x67e2ecb6, crc32);
    }

    SV_TEST("hash of invalid mp3, get-metadata turned on") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, invalidmp3, bformat(
            "%s%si.mp3", tempdir, pathsep));
        check(sv_file_writefile(cstr(invalidmp3), "not-a-valid-mp3", "wb"));
        quiet_warnings(true);
        get_hash(cstr(invalidmp3), tempdir, true, &h, &crc32);
        quiet_warnings(false);
        TestTrue(0x2998bf76385ad9d0 == h.data[0] && 0xdda72b9b7ffb52ac == h.data[1]);
        TestTrue(0xeab56cf2bacbb5e2 == h.data[2] && 0x722db00c51c532f4 == h.data[3]);
        TestEqn(0x67e2ecb6, crc32);
    }

    SV_TEST("hash of valid mp3: id3 unchanged, get-metadata turned off") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), false, false, false));
        get_hash(cstr(validmp3), tempdir, false, &h, &crc32);
        TestTrue(0x32892370d570e3ad == h.data[0] && 0x89b895f43501da14 == h.data[1]);
        TestTrue(0xdeb97c81dbb62a3b == h.data[2] && 0x6a796a89e0166c54 == h.data[3]);
        TestEqn(0xb6a3e71a, crc32);
    }

    SV_TEST("hash of valid mp3: id3 changed, get-metadata turned off") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), true, false, false));
        get_hash(cstr(validmp3), tempdir, false, &h, &crc32);
        TestTrue(0xaa23935ca198aa6d == h.data[0] && 0xb86c87c3d3cced09 == h.data[1]);
        TestTrue(0xa5e99d107ff95dbb == h.data[2] && 0x3c14348d05e0870b == h.data[3]);
        TestEqn(0xdcf3b99d, crc32);
    }

    SV_TEST("1 get-metadata on, id3 unchanged, len unchanged, data unchanged") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), false, false, false));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == h.data[0] && 0x1ffaa7d05b187124 == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0xb6a3e71a, crc32);
    }

    SV_TEST("2 get-metadata on, id3 changed, len unchanged, data unchanged") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), true, false, false));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == h.data[0] && 0x1ffaa7d05b187124 == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0xdcf3b99d, crc32);
    }

    SV_TEST("3 get-metadata on, id3 unchanged, len changed, data unchanged") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), false, true, false));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == h.data[0] && 0x1ffaa7d05b187124 == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0x15655751, crc32);
    }

    SV_TEST("4 get-metadata on, id3 changed, len changed, data unchanged") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), true, true, false));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == h.data[0] && 0x1ffaa7d05b187124 == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0x14150f7b, crc32);
    }

    SV_TEST("5 get-metadata on, id3 unchanged, len unchanged, data changed") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), false, false, true));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x7d49276b4e05c2bd == h.data[0] && 0x39f840313d6eeb4f == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0xc5679762, crc32);
    }

    SV_TEST("6 get-metadata on, id3 changed, len unchanged, data changed") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), true, false, true));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x7d49276b4e05c2bd == h.data[0] && 0x39f840313d6eeb4f == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0xaf37c9e5, crc32);
    }

    SV_TEST("7 get-metadata on, id3 unchanged, len changed, data changed") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), false, true, true));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x7d49276b4e05c2bd == h.data[0] && 0x39f840313d6eeb4f == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0x898585be, crc32);
    }

    SV_TEST("8 get-metadata on, id3 changed, len changed, data changed") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, validmp3, bformat("%s%sv.mp3", tempdir, pathsep));
        check(writevalidmp3(cstr(validmp3), true, true, true));
        get_hash(cstr(validmp3), tempdir, true, &h, &crc32);
        TestTrue(0x7d49276b4e05c2bd == h.data[0] && 0x39f840313d6eeb4f == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0x88f5dd94, crc32);
    }

    SV_TEST("valid mp3, interesting filename") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, path, bformat(
            "%s%shelp%sMMD5=1122334455667788a1a2a3a4a5a6a7a8.mp3",
            tempdir, pathsep, islinux ? "\n" : ""));
        check(writevalidmp3(cstr(path), true, true, true));
        get_hash(cstr(path), tempdir, true, &h, &crc32);
        TestTrue(0x7d49276b4e05c2bd == h.data[0] && 0x39f840313d6eeb4f == h.data[1]);
        TestTrue(0x0000000000000000 == h.data[2] && 0x0000000000000000 == h.data[3]);
        TestEqn(0x88f5dd94, crc32);
    }

    SV_TEST("invalid mp3, interesting filename") {
        hash256 h = {};
        uint32_t crc32 = 0;
        TEST_OPEN_EX(bstring, path, bformat(
            "%s%shelp%sMMD5=1122334455667788a1a2a3a4a5a6a7a8.mp3",
            tempdir, pathsep, islinux ? "\n" : ""));
        check(sv_file_writefile(cstr(path), "not-a-valid-mp3", "wb"));
        quiet_warnings(true);
        get_hash(cstr(path), tempdir, true, &h, &crc32);
        quiet_warnings(false);
        TestTrue(0x2998bf76385ad9d0 == h.data[0] && 0xdda72b9b7ffb52ac == h.data[1]);
        TestTrue(0xeab56cf2bacbb5e2 == h.data[2] && 0x722db00c51c532f4 == h.data[3]);
        TestEqn(0x67e2ecb6, crc32);
    }
}
SV_END_TEST_SUITE()
