/*
tests_userconfig.c

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

SV_BEGIN_TEST_SUITE(tests_get_file_extension_category)
{
    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len("")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len(".")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len("..")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len("...")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len(".mp3")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len(".xz")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len("a.xz")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_binary == get_file_extension_info(s_and_len("aa.xz")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_mp3 == get_file_extension_info(s_and_len("a.mp3")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none ==
            get_file_extension_info(s_and_len("wrong case.Mp3")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none ==
            get_file_extension_info(s_and_len("wrong case.MP3")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none ==
            get_file_extension_info(s_and_len("wrong case.jpG")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_none == get_file_extension_info(s_and_len("test.xzz")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_none == get_file_extension_info(s_and_len("test.xz.doc")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_none == get_file_extension_info(s_and_len("test.xz.")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_none == get_file_extension_info(s_and_len("test.m4a.")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_none == get_file_extension_info(s_and_len("test.")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_binary == get_file_extension_info(s_and_len("test.xz")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_binary == get_file_extension_info(s_and_len("test..xz")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_binary == get_file_extension_info(s_and_len("test...xz")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_binary == get_file_extension_info(s_and_len("test....xz")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_ogg == get_file_extension_info(s_and_len("test.ogg")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_ogg == get_file_extension_info(s_and_len("test..ogg")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_ogg == get_file_extension_info(s_and_len("test...ogg")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_ogg == get_file_extension_info(s_and_len("test....ogg")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_flac == get_file_extension_info(s_and_len("test.flac")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_flac == get_file_extension_info(s_and_len("test..flac")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_flac == get_file_extension_info(s_and_len("test...flac")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_flac == get_file_extension_info(s_and_len("test....flac")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_binary ==
            get_file_extension_info(s_and_len("test.doc.xz")));
    }

    SV_TEST_()
    {
        TestTrue(
            filetype_m4a == get_file_extension_info(s_and_len("test.doc.m4a")));
    }

    SV_TEST_()
    {
        TestTrue(filetype_binary ==
            get_file_extension_info(s_and_len("test.doc.webm")));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_persist_user_configs)
{
    SV_TEST("persist and read")
    {
        svdb_db db = {};
        sv_group grp = {};
        TEST_OPEN_EX(sv_group, groupgot, {});
        TEST_OPEN_EX(bstring, path, bformat("%s%sconfig.db", tempdir, pathsep));
        check(svdb_connect(&db, cstr(path)));
        grp.approx_archive_size_bytes = 111;
        grp.compact_threshold_bytes = 222;
        grp.days_to_keep_prev_versions = 333;
        grp.exclusion_patterns = bstrlist_open();
        grp.root_directories = bstrlist_open();
        grp.separate_metadata = 555;
        grp.pause_duration_seconds = 666;
        grp.grpname = bfromcstr("name");
        bstrlist_splitcstr(grp.exclusion_patterns, "*.aaa|*.bbb|*.ccc", '|');
        bstrlist_splitcstr(grp.root_directories, "/path/1|/path/2", '|');
        check(sv_grp_persist(&db, &grp));
        check(sv_grp_load(&db, &groupgot, "name"));
        svdb_close(&db);
        sv_grp_close(&grp);

        TestEqn(111, groupgot.approx_archive_size_bytes);
        TestEqn(222, groupgot.compact_threshold_bytes);
        TestEqn(333, groupgot.days_to_keep_prev_versions);
        TestEqList("*.aaa|*.bbb|*.ccc", groupgot.exclusion_patterns);
        TestEqList("/path/1|/path/2", groupgot.root_directories);
        TestEqn(555, groupgot.separate_metadata);
        TestEqn(666, groupgot.pause_duration_seconds);
        TestEqs("name", cstr(groupgot.grpname));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_find_groups)
{
    SV_TEST("do recognize group with expected .db file location")
    {
        TEST_OPEN_EX(bstring, userdata, tests_make_subdir(tempdir, "userdata"));
        TEST_OPEN_EX(sv_app, test_app, {});
        TEST_OPEN(bstring, path);
        bsetfmt(path, "%s%sexpected%sreadytoupload", cstr(userdata), pathsep,
            pathsep);
        TestTrue(os_create_dirs(cstr(path)));
        bsetfmt(path, "%s%sexpected%sexpected_index.db", cstr(userdata), pathsep,
            pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        bsetfmt(path, "%s%sexpected%sreadytoupload%s00001_00001.tar",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        bsetfmt(path, "%s%sexpected%sreadytoupload%s00001_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        test_app.grp_names = bstrlist_open();
        test_app.path_app_data = bfromcstr(tempdir);
        check(sv_app_findgroupnames(&test_app));
        TestEqList("expected", test_app.grp_names);
    }

    SV_TEST("do not recognize group with no .db file")
    {
        TEST_OPEN_EX(bstring, userdata, tests_make_subdir(tempdir, "userdata"));
        TEST_OPEN_EX(sv_app, test_app, {});
        TEST_OPEN(bstring, path);
        bsetfmt(path, "%s%sno_db_files%sreadytoupload", cstr(userdata), pathsep,
            pathsep);
        TestTrue(os_create_dirs(cstr(path)));
        bsetfmt(path, "%s%sno_db_files%sreadytoupload%s00001_00001.tar",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        bsetfmt(path, "%s%sno_db_files%sreadytoupload%s00001_00002.tar",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        test_app.grp_names = bstrlist_open();
        test_app.path_app_data = bfromcstr(tempdir);
        check(sv_app_findgroupnames(&test_app));
        TestEqList("expected", test_app.grp_names);
    }

    SV_TEST("incomplete group with .db files, get the latest")
    {
        TEST_OPEN_EX(bstring, userdata, tests_make_subdir(tempdir, "userdata"));
        TEST_OPEN_EX(sv_app, test_app, {});
        TEST_OPEN2(bstring, path, contents);
        bsetfmt(path, "%s%sno_main_db%sreadytoupload", cstr(userdata), pathsep,
            pathsep);
        TestTrue(os_create_dirs(cstr(path)));
        bsetfmt(path, "%s%sno_main_db%sreadytoupload%s00001_00001.tar",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        bsetfmt(path, "%s%sno_main_db%sreadytoupload%s00001_00002.tar",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "", "wb"));
        bsetfmt(path, "%s%sno_main_db%sreadytoupload%s00001_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "i1", "wb"));
        bsetfmt(path, "%s%sno_main_db%sreadytoupload%s00002_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "i2", "wb"));
        bsetfmt(path, "%s%sno_main_db%sreadytoupload%s00003_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "i3", "wb"));
        bsetfmt(path, "%s%sno_main_db", cstr(userdata), pathsep);
        check(os_tryuntil_deletefiles(cstr(path), "*"));
        bsetfmt(path, "%s%sno_main_db%sno_main_db_index.db", cstr(userdata),
            pathsep, pathsep);
        TestTrue(!os_file_exists(cstr(path)));
        test_app.grp_names = bstrlist_open();
        test_app.path_app_data = bfromcstr(tempdir);
        check(sv_app_findgroupnames(&test_app));
        bstrlist_sort(test_app.grp_names);
        bsetfmt(path, "%s%sno_main_db%sno_main_db_index.db", cstr(userdata),
            pathsep, pathsep);
        check(sv_file_readfile(cstr(path), contents));
        TestEqList("expected|no_main_db", test_app.grp_names);
        TestEqs("i3", cstr(contents));
    }

    SV_TEST("incomplete group with .db files, get the latest")
    {
        TEST_OPEN_EX(bstring, userdata, tests_make_subdir(tempdir, "userdata"));
        TEST_OPEN_EX(sv_app, test_app, {});
        TEST_OPEN2(bstring, path, contents);
        bsetfmt(path, "%s%sno_main_db_hex%sreadytoupload", cstr(userdata),
            pathsep, pathsep);
        TestTrue(os_create_dirs(cstr(path)));
        bsetfmt(path, "%s%sno_main_db_hex%sreadytoupload%s00001_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "1", "wb"));
        bsetfmt(path, "%s%sno_main_db_hex%sreadytoupload%s00002_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "2", "wb"));
        bsetfmt(path, "%s%sno_main_db_hex%sreadytoupload%s0000a_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "a", "wb"));
        bsetfmt(path, "%s%sno_main_db_hex%sreadytoupload%s0000e_index.db",
            cstr(userdata), pathsep, pathsep, pathsep);
        check(sv_file_writefile(cstr(path), "e", "wb"));
        bsetfmt(path, "%s%sno_main_db_hex", cstr(userdata), pathsep);
        check(os_tryuntil_deletefiles(cstr(path), "*"));
        bsetfmt(path, "%s%sno_main_db_hex%sno_main_db_hex_index.db",
            cstr(userdata), pathsep, pathsep);
        TestTrue(!os_file_exists(cstr(path)));
        test_app.grp_names = bstrlist_open();
        test_app.path_app_data = bfromcstr(tempdir);
        check(sv_app_findgroupnames(&test_app));
        bstrlist_sort(test_app.grp_names);
        bsetfmt(path, "%s%sno_main_db_hex%sno_main_db_hex_index.db",
            cstr(userdata), pathsep, pathsep);
        check(sv_file_readfile(cstr(path), contents));
        TestEqs("e", cstr(contents));
        TestEqList("expected|no_main_db|no_main_db_hex", test_app.grp_names);
    }

    SV_TEST("cleanup")
    {
        TEST_OPEN(bstring, path);
        bsetfmt(path, "%s%suserdata%sexpected", tempdir, pathsep, pathsep);
        tests_cleardir(cstr(path));
        bsetfmt(path, "%s%suserdata%sno_db_files", tempdir, pathsep, pathsep);
        tests_cleardir(cstr(path));
        bsetfmt(path, "%s%suserdata%sno_main_db", tempdir, pathsep, pathsep);
        tests_cleardir(cstr(path));
        bsetfmt(path, "%s%suserdata%sno_main_db_hex", tempdir, pathsep, pathsep);
        tests_cleardir(cstr(path));
        bsetfmt(path, "%s%suserdata", tempdir, pathsep);
        tests_cleardir(cstr(path));
    }
}

SV_END_TEST_SUITE()
