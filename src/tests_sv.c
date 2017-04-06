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

#include "tests_sv.h"
#include <math.h>

check_result test_archiver_file_extension_info()
{
    /* test strings that are too short */
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s(".")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("..")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("...")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s(".mp3")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s(".7z")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("a.7z")));
    /* test strings that are short but usable */
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("aa.7z")));
    TestTrue(knownfileextension_mp3 == get_file_extension_info(str_and_len32s("a.mp3")));
    /* case matters */
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("wrong case.Mp3")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("wrong case.MP3")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("wrong case.jpG")));
    /* strings that shouldn't match */
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("test.7zz")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("test.7z.doc")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("test.7z.")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("test.m4a.")));
    TestTrue(knownfileextension_none == get_file_extension_info(str_and_len32s("test.")));
    /* strings that should match */
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("test.7z")));
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("test..7z")));
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("test...7z")));
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("test....7z")));
    TestTrue(knownfileextension_ogg == get_file_extension_info(str_and_len32s("test.ogg")));
    TestTrue(knownfileextension_ogg == get_file_extension_info(str_and_len32s("test..ogg")));
    TestTrue(knownfileextension_ogg == get_file_extension_info(str_and_len32s("test...ogg")));
    TestTrue(knownfileextension_ogg == get_file_extension_info(str_and_len32s("test....ogg")));
    TestTrue(knownfileextension_flac == get_file_extension_info(str_and_len32s("test.flac")));
    TestTrue(knownfileextension_flac == get_file_extension_info(str_and_len32s("test..flac")));
    TestTrue(knownfileextension_flac == get_file_extension_info(str_and_len32s("test...flac")));
    TestTrue(knownfileextension_flac == get_file_extension_info(str_and_len32s("test....flac")));
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("test.doc.7z")));
    TestTrue(knownfileextension_m4a == get_file_extension_info(str_and_len32s("test.doc.m4a")));
    TestTrue(knownfileextension_otherbinary == get_file_extension_info(str_and_len32s("test.doc.webm")));
    return OK;
}

check_result svdb_connection_openhandle(svdb_connection *self);
check_result get_tar_archive_parameter(const char *archive, bstring sout);
check_result svdp_restore_cb(void *context, bool *,
    const svdb_files_row *in_files_row, const bstring path, const bstring permissions);
check_result svdp_compact_getexpirationcutoff(svdb_connection *db,
    const svdp_backupgroup *group, uint64_t *collectionid_to_expire, time_t now);
check_result svdb_runsql(svdb_connection *self, const char *sql, int lensql);
void get_tar_version_from_string(bstring s, double *outversion);

check_result test_util_audio_tags(unused_ptr(const char))
{
    bstring path = bstring_open();
    bstring s = bstring_open();

    { /* test aligned_malloc */
        byte *buf = os_aligned_malloc(16 * 4096, 4096);

        /* fill the buffer with meaningless values, every element should be accessible */
        int total = 0;
        for (int i = 0; i < 4096; i++)
        {
            buf[i] = (byte)(i * i);
        }
        for (int i = 0; i < 4096; i++)
        {
            total += buf[i];
        }

        TestEqn(432128, total);
        os_aligned_free(&buf);
    }
    { /* parse md5 */
        hash256 hash = {};
        tagbstring input = bstr_static("test\nMD5=1122334455667788a1a2a3a4a5a6a7a8");
        TestTrue(readhash(&input, &hash));
        TestEqn(0x8877665544332211ULL, hash.data[0]);
        TestEqn(0xa8a7a6a5a4a3a2a1ULL, hash.data[1]);
        TestEqn(0, hash.data[2]);
        TestEqn(0, hash.data[3]);
    }
    { /* parse md5, empty string */
        hash256 hash = {};
        tagbstring input = bstr_static("");
        TestTrue(!readhash(&input, &hash));
    }
    { /* parse md5, no hash */
        hash256 hash = {};
        tagbstring input = bstr_static("\nMD5=11");
        TestTrue(!readhash(&input, &hash));
    }
    { /* parse md5, skip over partial entries */
        hash256 hash = {};
        tagbstring input = bstr_static("test\nMD5=11223344\nMD5=11nocharacters\nothertext\n"
            "MD5=a1a2a3a4a5a6a7a81122334455667788\ntest");
        TestTrue(readhash(&input, &hash));
        TestEqn(0xa8a7a6a5a4a3a2a1ULL, hash.data[0]);
        TestEqn(0x8877665544332211ULL, hash.data[1]);
        TestEqn(0, hash.data[2]);
        TestEqn(0, hash.data[3]);
    }
    { /* write bytes */
        byte buf[] = { 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef };
        bytes_to_string(buf, 0, s);
        TestEqs("", cstr(s));
        bytes_to_string(buf, 3, s);
        TestEqs("123456", cstr(s));
        bytes_to_string(buf, countof(buf), s);
        TestEqs("123456abcdef", cstr(s));
    }
    { /* get_tar_version_from_string on invalid strings */
        double d = 0;
        bstr_assignstatic(s, "");
        get_tar_version_from_string(s, &d);
        TestTrue(d == 0.0);
        bstr_assignstatic(s, "tar () 1.1");
        get_tar_version_from_string(s, &d);
        TestTrue(d == 0.0);
        bstr_assignstatic(s, "tar (GNU tar) ");
        get_tar_version_from_string(s, &d);
        TestTrue(d == 0.0);
        bstr_assignstatic(s, "tar (GNU tar) 12345");
        get_tar_version_from_string(s, &d);
        TestTrue(d == 0.0);
    }
    { /* get_tar_version_from_string on valid strings */
        double d = 0;
        bstr_assignstatic(s, "tar (GNU tar) 0.2");
        get_tar_version_from_string(s, &d);
        TestTrue(fabs(d - 0.2) < 0.001);
        bstr_assignstatic(s, "tar (GNU tar) 1.012");
        get_tar_version_from_string(s, &d);
        TestTrue(fabs(d - 1.012) < 0.001);
        bstr_assignstatic(s, "tar (GNU tar) 1.1234");
        get_tar_version_from_string(s, &d);
        TestTrue(fabs(d - 1.1234) < 0.001);
        bstr_assignstatic(s, "tar (GNU tar) 10.001");
        get_tar_version_from_string(s, &d);
        TestTrue(fabs(d - 10.001) < 0.001);
        bstr_assignstatic(s, "tar (GNU tar) 10.9");
        get_tar_version_from_string(s, &d);
        TestTrue(fabs(d - 10.9) < 0.001);
    }

    bdestroy(s);
    bdestroy(path);
    return OK;
}

check_result test_db_basics(const char *dir)
{
    sv_result currenterr = {};
    svdb_connection db = {};
    bstring dbpathreopen = bformat("%s%stestrepoen.db", dir, pathsep);
    bstring dbpathutf8 = bformat("%s%s\xED\x95\x9C test.db", dir, pathsep);
    bstring dbpathtestempty = bformat("%s%stestempty.db", dir, pathsep);
    bstring str_got = bstring_open();
    check(os_tryuntil_deletefiles(dir, "*"));

    /* create new db and open existing db. pathname has utf8 chars */
    check(svdb_connection_open(&db, cstr(dbpathutf8)));
    check(svdb_connection_disconnect(&db));
    check(svdb_connection_open(&db, cstr(dbpathutf8)));
    uint32_t schema_version = 0;
    check(svdb_propertygetint(&db, str_and_len32s("SchemaVersion"), &schema_version));
    TestEqn(1, schema_version);
    check(svdb_connection_disconnect(&db));

    /* create new db and add 2 rows */
    check(svdb_connection_open(&db, cstr(dbpathreopen)));
    check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 123));
    check(svdb_propertysetstr(&db, str_and_len32s("TestStr"), "hello"));
    check(svdb_connection_disconnect(&db));

    /* connect to existing db and read the 2 rows */
    check(svdb_connection_open(&db, cstr(dbpathreopen)));
    uint32_t int_got = 0;
    check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &int_got));
    check(svdb_propertygetstr(&db, str_and_len32s("TestStr"), str_got));
    TestEqn(123, int_got);
    TestEqs("hello", cstr(str_got));
    check(svdb_connection_disconnect(&db));

    { /* create an empty db file (no schema),
      and make sure we can recover */
        svdb_connection conn_test_empty = {};
        conn_test_empty.path = bstrcpy(dbpathtestempty);
        check(svdb_connection_openhandle(&conn_test_empty));
        check(svdb_connection_disconnect(&conn_test_empty));
        set_debugbreaks_enabled(false);
        check(svdb_connection_open(&conn_test_empty, cstr(dbpathtestempty)));
        set_debugbreaks_enabled(true);
        schema_version = 0;
        check(svdb_propertygetint(&conn_test_empty,
            str_and_len32s("SchemaVersion"),&schema_version));

        TestEqn(1, schema_version);
        check(svdb_connection_disconnect(&conn_test_empty));
        svdb_connection_close(&conn_test_empty);
    }

    { /* data is kept if transaction committed */
        check(svdb_connection_open(&db, cstr(dbpathutf8)));
        check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 1));
        svdb_txn txn = {};
        check(svdb_txn_open(&txn, &db));
        check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 2));
        uint32_t ngot = 0;
        check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &ngot));
        TestEqn(2, ngot);
        check(svdb_txn_commit(&txn, &db));
        ngot = 0;
        check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &ngot));
        TestEqn(2, ngot);

        /* data is not kept if transaction rolled back */
        svdb_txn_close(&txn, &db);
        check(svdb_txn_open(&txn, &db));
        check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 3));
        ngot = 0;
        check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &ngot));
        TestEqn(3, ngot);
        check(svdb_txn_rollback(&txn, &db));
        ngot = 0;
        check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &ngot));
        TestEqn(2, ngot);
        check(svdb_connection_disconnect(&db));
    }

cleanup:
    svdb_connection_close(&db);
    bdestroy(dbpathutf8);
    bdestroy(dbpathtestempty);
    bdestroy(str_got);
    bdestroy(dbpathreopen);
    return currenterr;
}

check_result test_tbl_properties(svdb_connection *db)
{
    sv_result currenterr = {};
    bstrlist *list = bstrlist_open();
    bstrlist *list_got = bstrlist_open();
    bstring str_got = bstring_open();
    svdb_txn txn = {};
    check(svdb_txn_open(&txn, db));

    /* reading from non-existent properties should return default value */
    uint32_t int_got = UINT32_MAX;
    bassigncstr(str_got, "text");
    bstrlist_appendcstr(list_got, "text");
    check(svdb_propertygetint(db, str_and_len32s("NonExistInt"), &int_got));
    check(svdb_propertygetstr(db, str_and_len32s("NonExistStr"), str_got));
    check(svdb_propertygetstrlist(db, str_and_len32s("NonExistStrList"), list_got));
    TestEqn(0, int_got);
    TestEqs("", cstr(str_got));
    TestEqList("", list_got);

    /* item values can be replaced */
    check(svdb_propertysetint(db, str_and_len32s("TestInt"), 123));
    check(svdb_propertysetint(db, str_and_len32s("TestInt"), 456));
    check(svdb_propertysetstr(db, str_and_len32s("TestStr"), "hello"));
    check(svdb_propertysetstr(db, str_and_len32s("TestStr"), "otherstring"));
    check(svdb_propertygetint(db, str_and_len32s("TestInt"), &int_got));
    check(svdb_propertygetstr(db, str_and_len32s("TestStr"), str_got));
    TestEqn(456, int_got);
    TestEqs("otherstring", cstr(str_got));

    /* there should be 3 rows now */
    uint64_t countrows = 0;
    check(svdb_propertygetcount(db, &countrows));
    TestEqn(3, countrows);

    /* set and get list with 3 items */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "item1");
    bstrlist_appendcstr(list, "item2");
    bstrlist_appendcstr(list, "item3");
    check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
    bstrlist_clear(list_got);
    check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), list_got));
    TestEqList("item1|item2|item3", list_got);

    /* set and get list with 1 item */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "item1");
    check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
    bstrlist_clear(list_got);
    check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), list_got));
    TestEqList("item1", list_got);

    /* set and get list with 0 items */
    bstrlist_clear(list);
    check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
    bstrlist_clear(list_got);
    bstrlist_appendcstr(list_got, "text");
    check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), list_got));
    TestEqn(0, list_got->qty);

    /* set and get list with item with pipe char */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "pipechar|isok");
    check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
    bstrlist_clear(list_got);
    check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), list_got));
    TestEqn(1, list_got->qty);
    TestEqs("pipechar|isok", bstrlist_view(list_got, 0));

    /* setting invalid string into list should fail */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "item1");
    bstrlist_appendcstr(list, "invalid|||||invalid");
    set_debugbreaks_enabled(false);
    expect_err_with_message(svdb_propertysetstrlist(db,
        str_and_len32s("TestList"), list), "cannot include");
    set_debugbreaks_enabled(true);

    /* there should only be 4 rows now */
    check(svdb_propertygetcount(db, &countrows));
    TestEqn(4, countrows);
    check(svdb_txn_rollback(&txn, db));
cleanup:
    svdb_txn_close(&txn, db);
    bdestroy(str_got);
    bstrlist_close(list);
    bstrlist_close(list_got);
    return currenterr;
}

check_result files_addallrowstolistofstrings(void *context, unused_ptr(bool),
    const svdb_files_row *row, const bstring path, const bstring permissions)
{
    bstrlist *list = (bstrlist *)context;
    bstring s = bstring_open();
    svdb_files_row_string(row, cstr(path), cstr(permissions), s);
    bstrlist_append(list, s);
    bdestroy(s);
    return OK;
}

check_result contents_addallrowstolistofstrings(void *context, unused_ptr(bool),
    const svdb_contents_row *row)
{
    bstrlist *list = (bstrlist *)context;
    bstring s = bstring_open();
    svdb_contents_row_string(row, s);
    bstrlist_append(list, s);
    bdestroy(s);
    return OK;
}

check_result test_tbl_fileslist(svdb_connection *db)
{
    sv_result currenterr = {};
    bstring path1 = bstring_open();
    bstring path2 = bstring_open();
    bstring path3 = bstring_open();
    bstring permissions = bstring_open();
    bstring srowexpect = bstring_open();
    bstring srowgot = bstring_open();
    svdb_txn txn = {};

    svdb_files_row row1 = { 0,
        5 /*contents_id*/, 5000ULL * 1024 * 1024, /*contents_length*/
        555 /* last_write_time*/,
        1111 /*most_recent_collection*/, sv_filerowstatus_complete };
    svdb_files_row row2 = { 0,
        6 /*contents_id*/, 6000ULL * 1024 * 1024, /*contents_length*/
        666 /* last_write_time*/,
        1111 /*most_recent_collection*/, sv_filerowstatus_queued };
    svdb_files_row row3 = { 0,
        7 /*contents_id*/, 7000ULL * 1024 * 1024, /*contents_length*/
        777 /* last_write_time*/,
        1000 /*most_recent_collection*/, sv_filerowstatus_queued };

    check(svdb_txn_open(&txn, db));
    uint64_t rowid0;

    { /* unique index should prevent duplicate paths. */
        bassigncstr(path1, "/test/path/1");
        bassigncstr(path2, islinux ? "/test/path/1" : "/TEST/PATH/1");
        check(svdb_filesinsert(db, path1, 8888, sv_filerowstatus_queued, &rowid0));
        set_debugbreaks_enabled(false);
        expect_err_with_message(svdb_filesinsert(
            db, path2, 9999, sv_filerowstatus_queued, NULL), "UNIQUE constraint failed");
        set_debugbreaks_enabled(true);
        uint64_t filescount = 0;
        check(svdb_filesgetcount(db, &filescount));
        TestEqn(1, filescount);
    }
    { /* add 3 rows to the database */
        bassigncstr(path1, "/test/addrows/1");
        bassigncstr(path2, "/test/addrows/2");
        bassigncstr(path3, "/test/addrows/3");
        bassigncstr(permissions, "pm1");
        check(svdb_filesinsert(db, path1, 1, sv_filerowstatus_queued, &row1.id));
        check(svdb_filesupdate(db, &row1, permissions));
        bassigncstr(permissions, "pm2");
        check(svdb_filesinsert(db, path2, 2, sv_filerowstatus_queued, &row2.id));
        check(svdb_filesupdate(db, &row2, permissions));
        bassigncstr(permissions, "pm3");
        check(svdb_filesinsert(db, path3, 3, sv_filerowstatus_queued, &row3.id));
        check(svdb_filesupdate(db, &row3, permissions));
    }
    { /* get by path */
        svdb_files_row rowgot = { 0 };
        /* I'd use memcmp to compare, but padding after the struct apparently throws it off */
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_filesbypath(db, path1, &rowgot));
        svdb_files_row_string(&row1, "", "", srowexpect);
        svdb_files_row_string(&rowgot, "", "", srowgot);
        TestEqs(cstr(srowexpect), cstr(srowgot));
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_filesbypath(db, path2, &rowgot));
        svdb_files_row_string(&row2, "", "", srowexpect);
        svdb_files_row_string(&rowgot, "", "", srowgot);
        TestEqs(cstr(srowexpect), cstr(srowgot));
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_filesbypath(db, path3, &rowgot));
        svdb_files_row_string(&row3, "", "", srowexpect);
        svdb_files_row_string(&rowgot, "", "", srowgot);
        TestEqs(cstr(srowexpect), cstr(srowgot));
    }
    { /* try to get by nonexistant path */
        svdb_files_row rowgot = { 0 };
        memset(&rowgot, 1, sizeof(rowgot));
        bassigncstr(srowexpect, "/test/addrows/");
        check(svdb_filesbypath(db, srowexpect, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* try to update nonexistant row */
        svdb_files_row rowtest = { 1234 /* bogus rowid */, 5, 5, 5 };
        set_debugbreaks_enabled(false);
        expect_err_with_message(svdb_filesupdate(db, &rowtest, permissions), "changed no rows");
        set_debugbreaks_enabled(true);
    }
    { /* query status less than */
        bstrlist *list = bstrlist_open();
        check(svdb_files_in_queue(db, sv_makestatus(1111, sv_filerowstatus_complete),
            list, &files_addallrowstolistofstrings));
        TestEqn(list->qty, 2);
        svdb_files_row_string(&row2, cstr(path2), "pm2", srowexpect);
        TestEqs(cstr(srowexpect), bstrlist_view(list, 0));
        svdb_files_row_string(&row3, cstr(path3), "pm3", srowexpect);
        TestEqs(cstr(srowexpect), bstrlist_view(list, 1));
        bstrlist_close(list);
    }
    { /* query status less than, matches no rows */
        bstrlist *list = bstrlist_open();
        check(svdb_files_in_queue(db, sv_makestatus(1, sv_filerowstatus_complete),
            list, &files_addallrowstolistofstrings));
        TestEqn(list->qty, 0);
        bstrlist_close(list);
    }
    { /* query status less than, matches all rows */
        bstrlist *list = bstrlist_open();
        check(svdb_files_in_queue(db, svdb_files_in_queue_get_all,
            list, &files_addallrowstolistofstrings));
        TestEqn(list->qty, 4);
        bstrlist_close(list);
    }
    { /* ok to batch delete 0 rows */
        uint64_t filescount = 0;
        check(svdb_filesgetcount(db, &filescount));
        TestEqn(4, filescount);
        sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
        check(svdb_files_bulk_delete(db, &arr, 0));
        check(svdb_filesgetcount(db, &filescount));
        TestEqn(4, filescount);
        sv_array_close(&arr);
    }
    { /* batch delete */
        uint64_t filescount = 0;
        check(svdb_filesgetcount(db, &filescount));
        TestEqn(4, filescount);
        sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
        sv_array_add64u(&arr, rowid0);
        sv_array_add64u(&arr, row2.id);
        sv_array_add64u(&arr, row3.id);

        /* deleting 3 rows with batchsize 2 covers both the full-batch and partial-batch cases*/
        check(svdb_files_bulk_delete(db, &arr, 2));

        /* row 1 should still exist */
        check(svdb_filesgetcount(db, &filescount));
        TestEqn(1, filescount);
        svdb_files_row rowgot = { 0 };
        check(svdb_filesbypath(db, path1, &rowgot));
        svdb_files_row_string(&row1, "", "", srowexpect);
        svdb_files_row_string(&rowgot, "", "", srowgot);
        TestEqs(cstr(srowexpect), cstr(srowgot));

        /* row 2 should now be gone */
        memset(&rowgot, 1, sizeof(rowgot));
        check(svdb_filesbypath(db, path2, &rowgot));
        TestEqn(0, rowgot.id);
        sv_array_close(&arr);
    }
    { /* test row-to-string */
        svdb_files_row_string(&row1, "path", "flags", srowexpect);
        TestEqs("contents_length=5, contents_id=5242880000, last_write_time=555, flags=flags, "
            "most_recent_collection=1111, e_status=3, id=2path", cstr(srowexpect));
    }

    check(svdb_txn_rollback(&txn, db));
cleanup:
    bdestroy(path1);
    bdestroy(path2);
    bdestroy(path3);
    bdestroy(permissions);
    bdestroy(srowexpect);
    bdestroy(srowgot);
    svdb_txn_close(&txn, db);
    return currenterr;
}

check_result test_tbl_collections(svdb_connection *db)
{
    sv_result currenterr = {};
    bstring srowexpected = bstring_open();
    bstring srowgot = bstring_open();
    svdb_txn txn = {};

    svdb_collections_row row1 = { 0, 0 /*time*/, 11 /*time_finished*/,
        111 /* count_total_files */, 1111 /*count_new_contents*/,
        1000ULL * 1024 * 1024 /* count_new_contents_bytes*/ };
    svdb_collections_row row2 = { 0, 0 /*time*/, 22 /*time_finished*/,
        222 /* count_total_files */, 2222 /*count_new_contents*/,
        2000ULL * 1024 * 1024 /* count_new_contents_bytes*/ };
    svdb_collections_row row3 = { 0, 0 /*time*/, 33 /*time_finished*/,
        333 /* count_total_files */, 3333 /*count_new_contents*/,
        3000ULL * 1024 * 1024 /* count_new_contents_bytes*/ };

    check(svdb_txn_open(&txn, db));

    { /* get latest on empty db */
        uint64_t rowid_got = UINT64_MAX;
        check(svdb_collectiongetlast(db, &rowid_got));
        TestEqn(0, rowid_got);
    }
    { /* read collections on empty db */
        sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
        check(svdb_collectionsget(db, &arr, true));
        TestEqn(0, arr.length);
        sv_array_close(&arr);
    }
    { /* add 3 rows to the database */
        check(svdb_collectioninsert(db, 5, &row1.id));
        check(svdb_collectionupdate(db, &row1));
        row1.time = 5;

        check(svdb_collectioninsert(db, 6, &row2.id));
        check(svdb_collectionupdate(db, &row2));
        row2.time = 6;

        check(svdb_collectioninsert(db, 7, &row3.id));
        check(svdb_collectionupdate(db, &row3));
        row3.time = 7;
    }
    { /* should not be able to set timestarted */
        svdb_collections_row rowtest = { row1.id, 9999 /* change timestarted */ };
        set_debugbreaks_enabled(false);
        sv_result res = svdb_collectionupdate(db, &rowtest);
        TestTrue(res.code != 0);
        TestTrue(s_contains(cstr(res.msg), "cannot set time started"));
        set_debugbreaks_enabled(true);
    }
    { /* try to update non-existant row */
        svdb_collections_row rowtest = { 1234 /* bogus rowid */, 0, 0 };
        set_debugbreaks_enabled(false);
        sv_result res = svdb_collectionupdate(db, &rowtest);
        TestTrue(res.code != 0);
        TestTrue(s_contains(cstr(res.msg), "changed no rows"));
        sv_result_close(&res);
        set_debugbreaks_enabled(true);
    }
    { /* get latest id */
        uint64_t rowid_got = 0;
        check(svdb_collectiongetlast(db, &rowid_got));
        TestEqn(row3.id, rowid_got);
    }
    { /* read collections */
        sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
        check(svdb_collectionsget(db, &arr, true));
        TestEqn(3, arr.length);
        /* remember that the results should be sorted in reverse order */
        svdb_collections_row *row_got = (svdb_collections_row *)sv_array_at(&arr, 0);
        svdb_collectiontostring(&row3, true, true, srowexpected);
        svdb_collectiontostring(row_got, true, true, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        row_got = (svdb_collections_row *)sv_array_at(&arr, 1);
        svdb_collectiontostring(&row2, true, true, srowexpected);
        svdb_collectiontostring(row_got, true, true, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        row_got = (svdb_collections_row *)sv_array_at(&arr, 2);
        svdb_collectiontostring(&row1, true, true, srowexpected);
        svdb_collectiontostring(row_got, true, true, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        sv_array_close(&arr);
    }
    { /* test row-to-string */
        svdb_collectiontostring(&row1, true, true, srowexpected);
        TestEqs("time=5, time_finished=11, count_total_files=111, count_new_contents=1111, "
            "count_new_contents_bytes=1048576000, id=1", cstr(srowexpected));
    }

    check(svdb_txn_rollback(&txn, db));
cleanup:
    bdestroy(srowexpected);
    bdestroy(srowgot);
    svdb_txn_close(&txn, db);
    return currenterr;
}

check_result test_tbl_contents(svdb_connection *db)
{
    sv_result currenterr = {};
    bstring srowexpected = bstring_open();
    bstring srowgot = bstring_open();
    svdb_txn txn = {};

    svdb_contents_row rowgot = { 0 };
    svdb_contents_row row1 = { 0,
        1000ULL * 1024 * 1024 /*contents_length*/,
        1001ULL * 1024 * 1024 /* compressed_contents_length */,
        1 /* most_recent_collection */, 11 /* original_collection */,
        111 /* archivenumber */,
            {{ 0x1111111111111111ULL, 0x2222222222222222ULL,
            0x3333333333333333ULL, 0x4444444444444444ULL }}, /* hash */
        0x11111111 /* crc32 */ };
    svdb_contents_row row2 = { 0,
        2000ULL * 1024 * 1024 /*contents_length*/,
        2002ULL * 1024 * 1024 /* compressed_contents_length */,
        2 /* most_recent_collection */, 22 /*original_collection*/,
        222 /* archivenumber*/,
            {{ 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555555ULL, }}, /*hash*/
        0x22222222 /* crc32 */ };
    svdb_contents_row row3 = { 0,
        3000ULL * 1024 * 1024 /*contents_length*/,
        3003ULL * 1024 * 1024 /* compressed_contents_length */,
        3 /* most_recent_collection */, 33 /*original_collection*/,
        333 /* archivenumber*/,
            {{ 0x3333333333333333ULL, 0x4444444444444444ULL,
            0x5555555555555555ULL, 0x6666666666666666ULL }}, /*hash*/
        0x33333333 /* crc32 */ };

    check(svdb_txn_open(&txn, db));

    { /* test row iteration when empty */
        bstrlist *list = bstrlist_open();
        check(svdb_contentsiter(db, list, &contents_addallrowstolistofstrings));
        TestEqn(0, list->qty);
        bstrlist_close(list);
    }
    { /* add 3 rows to the database */
        check(svdb_contentsinsert(db, &row1.id));
        check(svdb_contentsupdate(db, &row1));
        check(svdb_contentsinsert(db, &row2.id));
        check(svdb_contentsupdate(db, &row2));
        check(svdb_contentsinsert(db, &row3.id));
        check(svdb_contentsupdate(db, &row3));
    }
    { /* get contents by id */
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_contentsbyid(db, row1.id, &rowgot));
        svdb_contents_row_string(&row1, srowexpected);
        svdb_contents_row_string(&rowgot, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_contentsbyid(db, row2.id, &rowgot));
        svdb_contents_row_string(&row2, srowexpected);
        svdb_contents_row_string(&rowgot, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_contentsbyid(db, row3.id, &rowgot));
        svdb_contents_row_string(&row3, srowexpected);
        svdb_contents_row_string(&rowgot, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
    }
    { /* get contents by non-existant id */
        memset(&rowgot, 1, sizeof(rowgot));
        check(svdb_contentsbyid(db, 999, &rowgot));
        TestEqn(0, rowgot.id);
        TestTrue(row1.id != 0 && row2.id != 0 && row3.id != 0);
    }
    { /* get contents by hash */
        memset(&rowgot, 0, sizeof(rowgot));
        hash256 hash = {{ 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555555ULL }};
        check(svdb_contentsbyhash(db, &hash, 2000ULL * 1024 * 1024, &rowgot));
        svdb_contents_row_string(&row2, srowexpected);
        svdb_contents_row_string(&rowgot, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
    }
    { /* right hash but wrong length */
        memset(&rowgot, 1, sizeof(rowgot));
        hash256 hash = {{ 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555555ULL }};
        check(svdb_contentsbyhash(db, &hash, 1234, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* right length but wrong hash */
        memset(&rowgot, 1, sizeof(rowgot));
        hash256 hash = {{ 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555556ULL /* note final 6 */ }};
        check(svdb_contentsbyhash(db, &hash, 2000ULL * 1024 * 1024, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* wrong length and wrong hash */
        memset(&rowgot, 1, sizeof(rowgot));
        hash256 hash = {{ 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555556ULL /* note final 6 */ }};
        check(svdb_contentsbyhash(db, &hash, 1234, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* test row iteration */
        bstrlist *list = bstrlist_open();
        check(svdb_contentsiter(db, list, &contents_addallrowstolistofstrings));
        TestEqn(3, list->qty);
        svdb_contents_row_string(&row1, srowexpected);
        TestEqs(cstr(srowexpected), bstrlist_view(list, 0));
        svdb_contents_row_string(&row2, srowexpected);
        TestEqs(cstr(srowexpected), bstrlist_view(list, 1));
        svdb_contents_row_string(&row3, srowexpected);
        TestEqs(cstr(srowexpected), bstrlist_view(list, 2));
        bstrlist_close(list);
    }
    { /* test row-to-string */
        svdb_contents_row_string(&row1, srowexpected);
        TestEqs("hash=1111111111111111 2222222222222222 3333333333333333 4444444444444444, "
            "crc32=11111111, contents_length=1048576000,1049624576, most_recent_collection=1, "
            "original_collection=11, archivenumber=111, id=1", cstr(srowexpected));
    }
    { /* set last referenced */
        check(svdb_contents_setlastreferenced(db, row3.id, 99));
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_contentsbyid(db, row3.id, &rowgot));
        TestEqn(rowgot.most_recent_collection, 99);
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_contentsbyid(db, row1.id, &rowgot));
        TestEqn(rowgot.most_recent_collection, 1);
        check(svdb_contents_setlastreferenced(db, row3.id, 3));
    }
    { /* set last referenced on missing row */
        set_debugbreaks_enabled(false);
        sv_result res = svdb_contents_setlastreferenced(db, 1234 /*bogus rowid*/, 99);
        TestTrue(res.code != 0);
        TestTrue(s_contains(cstr(res.msg), "changed no rows"));
        sv_result_close(&res);
        set_debugbreaks_enabled(true);
    }
    { /* test batch delete of nothing */
        uint64_t count = 0;
        check(svdb_contentsgetcount(db, &count));
        TestEqn(3, count);
        sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
        check(svdb_contents_bulk_delete(db, &arr, 0));
        check(svdb_contentsgetcount(db, &count));
        TestEqn(3, count);
        sv_array_close(&arr);
    }
    { /* test batch delete of nonexistant ids */
        uint64_t count = 0;
        check(svdb_contentsgetcount(db, &count));
        TestEqn(3, count);
        sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
        sv_array_add64u(&arr, 12345 /* bogus rowid */);
        sv_array_add64u(&arr, 123456 /* bogus rowid */);
        check(svdb_contents_bulk_delete(db, &arr, 0));
        check(svdb_contentsgetcount(db, &count));
        TestEqn(3, count);
        sv_array_close(&arr);
    }
    { /* test batch delete */
        uint64_t count = 0;
        check(svdb_contentsgetcount(db, &count));
        TestEqn(3, count);
        sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
        sv_array_add64u(&arr, row1.id);
        sv_array_add64u(&arr, row3.id);
        check(svdb_contents_bulk_delete(db, &arr, 2));

        /* row 2 should still exist */
        check(svdb_contentsgetcount(db, &count));
        TestEqn(1, count);
        memset(&rowgot, 0, sizeof(rowgot));
        check(svdb_contentsbyid(db, row2.id, &rowgot));
        svdb_contents_row_string(&row2, srowexpected);
        svdb_contents_row_string(&rowgot, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));

        /* row 1 should now be gone */
        memset(&rowgot, 1, sizeof(rowgot));
        check(svdb_contentsbyid(db, row1.id, &rowgot));
        TestEqn(0, rowgot.id);
        sv_array_close(&arr);
    }

    check(svdb_txn_rollback(&txn, db));
cleanup:
    bdestroy(srowexpected);
    bdestroy(srowgot);
    svdb_txn_close(&txn, db);
    return currenterr;
}

check_result test_usergroup_persist(svdb_connection *db)
{
    sv_result currenterr = {};
    svdp_backupgroup group = {};
    svdp_backupgroup groupgot = {};

    /* save a group to the database */
    group.approx_archive_size_bytes = 111;
    group.compact_threshold_bytes = 222;
    group.days_to_keep_prev_versions = 333;
    group.encryption = bfromcstr("key-not-persisted");
    group.use_encryption = 444;
    group.exclusion_patterns = bstrlist_open();
    group.root_directories = bstrlist_open();
    group.separate_metadata_enabled = 555;
    group.pause_duration_seconds = 666;
    group.groupname = bfromcstr("testgroup");
    bstrlist_splitcstr(group.exclusion_patterns, "*.aaa|*.bbb|*.ccc", "|");
    bstrlist_splitcstr(group.root_directories, "/path/1|/path/2", "|");
    check(svdp_backupgroup_persist(db, &group));

    /* read group from the database and make sure that it matches */
    check(svdp_backupgroup_load(db, &groupgot, "testgroup"));
    TestEqn(111, groupgot.approx_archive_size_bytes);
    TestEqn(222, groupgot.compact_threshold_bytes);
    TestEqn(333, groupgot.days_to_keep_prev_versions);
    TestEqn(444, groupgot.use_encryption);
    TestEqs("", cstr(groupgot.encryption));
    TestEqList("*.aaa|*.bbb|*.ccc", groupgot.exclusion_patterns);
    TestEqList("/path/1|/path/2", groupgot.root_directories);
    TestEqn(555, groupgot.separate_metadata_enabled);
    TestEqn(666, groupgot.pause_duration_seconds);
    TestEqs("testgroup", cstr(groupgot.groupname));
cleanup:
    svdp_backupgroup_close(&group);
    svdp_backupgroup_close(&groupgot);
    return currenterr;
}

check_result test_db_rows(const char *dir)
{
    sv_result currenterr = {};
    svdb_connection db = {};
    bstring dbpath = bformat("%s%stestrows.db", dir, pathsep);
    check(os_tryuntil_deletefiles(dir, "*"));
    check(svdb_connection_open(&db, cstr(dbpath)));
    check(test_tbl_properties(&db));
    check(test_tbl_fileslist(&db));
    check(test_tbl_collections(&db));
    check(test_tbl_contents(&db));
    check(test_usergroup_persist(&db));
    check(svdb_connection_disconnect(&db));
cleanup:
    svdb_connection_close(&db);
    bdestroy(dbpath);
    return currenterr;
}

void get_hash_helper(const char *path, svdp_archiver *archiver, bool sepmetadata,
    hash256 *hash, uint32_t *crc)
{
    *hash = hash256zeros;
    *crc = UINT32_MAX;
    os_exclusivefilehandle handle = {};
    check_warn(os_exclusivefilehandle_open(&handle, path, true, NULL), "", exit_on_err);
    check_warn(hash_of_file(&handle,
        sepmetadata ? 1 : 0,
        sepmetadata ? knownfileextension_mp3 : knownfileextension_otherbinary,
        archiver ? cstr(archiver->path_audiometadata_binary) : "",
        hash, crc), "", exit_on_err);
    os_exclusivefilehandle_close(&handle);
}

check_result writevalidmp3(const char *path, bool changeid3, bool changeid3length, bool changedata)
{
    sv_result currenterr = {};
    byte firstdata[] = { 'I', 'D', '3', 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 'x', 'T', 'A', 'L', 'B', 0x00, 0x00, 0x00,
        0x0d, 0x00, 0x00, 0x01, 0xff, 0xfe, 'A', 0x00, 'l', 0x00, 'b', 0x00, 'u', 0x00, 'm', 0x00, 'T', 'P', 'E', '1', 0x00,
        0x00, 0x00, 0x0f, 0x00, 0x00, 0x01, 0xff, 0xfe, 'A', 0x00, 'r', 0x00, 't', 0x00, 'i', 0x00, 's', 0x00, 't', 0x00,
        'C', 'O', 'M', 'M', 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x01, 'e', 'n', 'g', 0xff, 0xfe, 0x00, 0x00, 0xff, 0xfe, 'C',
        0x00, 'o', 0x00, 'm', 0x00, 'm', 0x00, 'e', 0x00, 'n', 0x00, 't', 0x00, 'T', 'I', 'T', '2', 0x00, 0x00, 0x00, 0x0d,
        0x00, 0x00, 0x01, 0xff, 0xfe, 'T', 0x00, 'i', 0x00, 't', 0x00, 'l', 0x00, 'e', 0x00, 'T', 'Y', 'E', 'R', 0x00, 0x00,
        0x00, 0x05, 0x00, 0x00, 0x00, 'Y', 'e', 'a', 'r', };
    byte seconddata[] = { 0xff, 0xfb, 0x92, 0x60, 0xb7, 0x80, 0x05, 'd', 0x3b, 'T', 0xd3, 'X', 0xc2, 0xdc, 0x1b, 0xa0,
        0x0a, 0xf3, 0x00, 0x00, 0x01, 0x94, 'D', 0xe3, 'Q', 'M', 0xe7, 0x0b, 'p', 'l', 0x80, 'l', 0x0c, 0x10, 0x00, 0x06,
        0xaf, 0xff, 0xff, 0xff, 0xff, 0xfa, 0xc6, 0xaa, 'l', 'h', 0x95, '7', 0x3c, 'p', 'u', 0xc4, 'a', 0x95, 0xa9, 0x2c, 'u', 'r',
        0xc0, 'Q', 'J', 0xa9, '7', 0x24, 'i', 0x09, 0xe3, 0xb0, 0x25, 0x1d, 'G', '0', 0xd1, '4', 0x3b, 0xa5, 0x29, 0xad, 0x8b,
        0x1a, 0x60, 0x18, 0x88, 0x08, 0xcd, 0xc5, 'Q', 'q', 0xca, 'l', 0xab, 0x21, 0x2b, 0x09, 0x83, 'r', 0x0a, 'D', 0x9e, 0xbd,
        'm', 0xd6, 0x85, 0xb2, 0x26, 0xf9, 'q', 0x23, 0xea, 0xfd, 'h', '8', 0xc2, 0xcd, 'c', 0x00, 0xf1, 0xa1, 0x95, 0x1c, '1', 0x2f,
        'e', 0x0c, 0xc4, 0x1c, 'V', 0x1f, 0x2a, 'K', 0xf9, 'C', 0xb6, 0xc8, 0xdb, 0xbb, 0x08, 0x81, '3', 'N', 0x19, 0x14, 'F', 0x25,
        'j', 0x3b, 'Q', 0xc6, 0x96, 'L', 0x3b, 'm', 0x99, 'M', 0xdb, 0x12, 0xc4, 0xcd, 'i', 0xa4, 0xdc, 0x0d, '3', 0x2a, 0x80, 0xa0,
        0x8a, 0x93, 0xd2, 0x9c, 0xe5, 't', 0xb4, 0x7f, 0xac, 'u', '3', 'n', 0x9e, 0xde, '2', 0xaa, 0x1a, 0x1d, 'G', 0xff, 0x5c, 0xb3,
        0xab, 0xbb, 0xb9, 0x86, 'Z', 0xc3, 0x2c, 0xf2, 0xdd, 0xb1, 0xc6, 0x04, 0xc5, 'X', 'E', 0xa9, 0xed, 0xf9, 'V', 'W', '1', 'W',
        0xa2, 0x40, 0x00, 0x00, 0xb2, 0x01, 0x5e, 0xe3, 0x7f, 0xff, 0xff, 0xff, 0xfe, 0xb5, 0xdc, 0xed, 0xfc, 0xc8, 0xaf, 0xe7, 0x5f,
        0xea, 0x24, 0x92, 'T', 'I', 0xa7, 0x2d, 0xb1, 0xa1, 'x', 0xe5, 'M', 0x86, 0x0d, 0xc7, 0x88, 0x16, 0x14, 0x2e, 'J', 'h', 0xe3,
        'F', 'F', 0x16, 'X', 0x06, 0x11, 0x92, 0x17, 0x3a, 0x12, '5', '7', '9', 0xaf, 0x83, 'J', 0xca, 0xda, 0xfb, '5', 0xe4, 0xc8, 0xa0,
        0x1e, 0x09, 'r', 0xd7, 0x12, 'j', 0x96, 0x05, 0xc0, 'H', 'D', 0xea, 0x08, 0xfc, 0x1e, 0x24, 0xc0, 0xb1, 0x5f, 0x5c, 'c', 'G',
        0x82, 't', 0x05, 0xc7, 0xc8, 'p', 0xb3, 0x1c, 0x93, 0x03, 0x28, 0x21, 0x23, 0x90, 0x16, 'j', 0xb2, '4', 't', 0x90, 0xe7,
        0x83, 0xf8, 'O', 0x13, 0xc3, 0x14, 0xfc, 0x2d, 0xa3, 0x2c, 0x19, 'b', 0x90, 0x3a, 'D', 0xe0, 'x', 0x8f, 0xd3, 0xcd, 0xed,
        'E', 0xfa, 'y', 0xc4, 0xc8, 0x60, 0xc2, 0x89, '8', 0xfc, 0xf2, 'f', 'W', 0xb8, 0xc7, 'S', 0xbe, 'Y', 0xb5, 0xe0, 0x2c, 0xb0,
        'R', 'C', 0x8a, 0xfb, 0x87, '4', 0xf2, 'F', 0xdc, '7', 0xd3, 'i', 0xf4, 0x8f, 0x2f, 0x3e, 0xb7, 0xd9, 0xe1, 0x03, 0x60,
        0x00, 'Y', 'M', 0x1e, 0x27, 0x10, 0xb0, 0x8b, 0x9c, 0xa6, 0x0b, 0x5e, 0xb5, 'z', 'r', 0x82, 0x00, 0x00, 0x06, 0x90,
        0x0c, 0xf8, 0x87, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0x0e, 0xad, 'v', 0x0c, 0xe3, 0xff, 0xfb, 0x92, 0x60, 0xb3, 0x80,
        0x04, 0xbe, '7', 0xd5, 'S', 'x', 0xc2, 0xdc, 0x15, 0x40, 0x9b, 0x17, 0x04, 0x40, 0x13, 0x94, 0xcc, 0xeb, 'W', 0xad, 0xe1,
        0xeb, 'p', 'r', 0x18, 'l', 0x1c, 0x10, 0x09, 'n', 'j', 0x97, 'K', 'N', 0xb9, 'r', 'o', 0xc6, 0xd0, 'U', 0x5b, 0xed, '9', 'e', 'm',
        0x88, 0x2c, 0xe3, 0x93, 'H', 'J', 0xcc, 0x20, 'A', 0x98, 0x05, 'M', 'A', 0xc5, 0xc0, 0x27, 0xa2, 0xf3, 0x28, 'p', 0x60, 'j', 0xb6,
        0xa3, 0xd2, 0x89, 0xb0, 0xc0, 0xf1, 'C', 0x2e, 0x9a, 0x7d, 0xf6, 0xba, 'v', 0xae, 0x89, 0x95, 0xf4, 0x89, 'o', 0xe0, 0x89,
        'i', 0x1c, 0x2c, 0xf4, 0xd8, 'Z', 0xca, 0xc8, 0x95, 0x92, 0xf8, 0xa2, 0x95, 0xb7, 'G', 0x0b, 'Q', 0xd6, 0x86, 0x13, 0xc1,
        0x25, 'A', 'P', 0x3c, 0x07, 0x20, 0x7e, 0x13, 0xe1, 0x21, 0x1a, 0xa8, 'R', 'U', 0x0d, 0x0f, 0x28, 'm', 0x07, 0xe6, 0x89,
        0x08, 'I', 0x05, 0x88, 0x0a, 0x24, 0xb0, 0x15, 0xc7, 0x28, 0xb2, 0x3e, 'i', 'l', 0x95, 0x89, 0x3b, 0x13, 'R', 0xd7, 0x7e,
        'W', 'W', 0x7b, 0x15, 0x26, 0xfd, 'z', 0x1d, 0xa4, 0x8c, 0xe1, 0x1a, 0x0b, 0xe8, 0xf1, 'P', 0x13, 0x1d, 0xa6, 0xa3, 0xc6,
        0x85, 0x8c, 'K', 0x9f, 0x81, 0xd4, 'w', 0x85, 0xe8, 0xea, 0xfa, 0xea, 0x8f, 0xfe, '0', 0xe6, 0xbd, 'T', 0xf6, 0x5d, 0xb0,
        0x00, 0x96, 0x40, 0x21, '5', 0xb7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 'N', 0xaf, 0x5b, 0x80, 0xc4, 0x0c, 0x7b, 0xc7, 0x12,
        'Q', 0xe4, 0x9f, 0xce, 0x88, 0xc5, 0xf3, 0xee, 0x8d, 0x96, 'P', 0x1a, 0xe9, 0x27, 0x24, 'i', 0x07, 0xd0, 0xec, 'b', 0x81,
        0x85, 'f', 0x84, 0x3e, 0xc0, 0x07, 'E', 'G', 0x90, 0x00, 0xc4, 0x92, 0xa0, 0xa0, 0xea, 'P', 0xc0, 0xe0, 0xf3, 0x8d, 'X',
        0xb6, 0xe0, 0xe0, 0x12, 0xc8, 0xda, 0xb8, 0x87, 0xab, 0x17, 0x99, 0x24, 0x20, 0xf4, 0x12, 0x81, 0x84, 0xb6, '1', 0x80,
        0x0a, 0xde, 0x98, 0xc4, 0xa3, 0xa0, 0x05, 0x03, 0x1e, 0x00, '4', 'B', 0x2a, 0x97, 0xa3, 0xc9, 0x10, 0x16, 0xa0, 0x8b,
        's', 0x7c, 0x3e, 'V', 0xd3, 0x88, 'I', 0x14, 'F', 0x82, '0', 0xe2, 0x94, '8', 0x14, 0x8c, 0x81, 0x3e, 'y', '9', 0x9f, 0xe6,
        0xcd, 0x82, 0xfc, 0xe2, 'S', 0x89, 0xd0, 0xe7, 0x2b, 0x07, 0x22, 'j', 'T', 0xf2, 0xa6, 0xee, 0xcf, '7', 'u', 0x7f, 'x',
        0xea, 0xe8, 'R', 'E', 0x9a, 0x8a, 0x8d, 'V', 0x90, 'l', 0xdf, 0x15, 0xb5, 'G', 0x8c, 'n', 0x0f, 0xb5, '3', 0x8a, 0x7f,
        'K', 'Z', 0xd7, 0xbc, 0x2f, 0xe6, 0x60, 0xa9, 'r', 0x87, 0x28, 'u', 's', 0x99, '7', 'c', 0xd8, 0xb5, 0x87, 0xcf, 0x0f,
        0xe8, 0x00, 0x01, 'u', 0xa0, 'E', 0xcf, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd3, 0xfc, 0xaf, 0x9d, 0x1e, 0xb9, 0xea, 0xb3,
        0xaf, 't', 0xd4, 0x1d, 'F', 0x90, 'A', 0x16, 'l', 'T', 'A', 'G', 'T', 'i', 't', 'l', 'e', 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        'A', 'r', 't', 'i', 's', 't', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'A', 'l', 'b', 'u', 'm', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'Y', 'e', 'a', 'r', 'C', 'o',
        'm', 'm', 'e', 'n', 't', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, };
    staticassert(countof(firstdata) == 0x82);
    staticassert(countof(seconddata) == 0x3c4);
    if (changeid3)
    {
        firstdata[0x52] = 'x'; /* changes id3v2 comment */
        seconddata[0x368] = 'x'; /* changes id3v1 artist */
    }

    if (changedata)
    {
        seconddata[0x57] = 'r'; /* change mp3 data */
    }

    sv_file file = {};
    check(sv_file_open(&file, path, "wb"));
    fwrite(firstdata, sizeof(byte), countof(firstdata), file.file);
    uint32_t lengthzeros = 2048;
    byte *zeros = sv_calloc(lengthzeros, sizeof(byte));
    fwrite(zeros, sizeof(byte), lengthzeros, file.file);
    fwrite(seconddata, sizeof(byte), countof(seconddata), file.file);
    if (changeid3length)
    {
        fwrite(zeros, sizeof(byte), 4, file.file); /* add 4 zero bytes, does not change mp3 bitstream */
    }

    sv_freenull(zeros);
    sv_file_close(&file);
    check_b(os_getfilesize(path) == (changeid3length ? 3146 : 3142), "wrong filesize writing %s", path);
cleanup:
    return currenterr;
}

check_result test_audio_tags_hashing(const char *dir)
{
    sv_result currenterr = {};
    svdp_archiver archiver = {};
    hash256 hash = {};
    uint32_t crc32 = 0;

    /* write test files */
    bstring pathnormalfile = bformat("%s%snormalfile.txt", dir, pathsep);
    bstring path0byte = bformat("%s%s0byte.txt", dir, pathsep);
    bstring validmp3 = bformat("%s%svalidmp3.mp3", dir, pathsep);
    bstring invalidmp3 = bformat("%s%sinvalid.mp3", dir, pathsep);
    bstring namewithmd5 = bformat("%s%s%smd5=12345678.mp3", dir, pathsep, islinux ? "\n" : "");
    check(os_tryuntil_deletefiles(dir, "*"));
    check(svdp_archiver_open(&archiver, dir, "test", 0, 0, ""));
    check(checkexternaltoolspresent(&archiver, 1 /* use ffmpeg */));
    check(sv_file_writefile(cstr(pathnormalfile), "abcde", "wb"));
    check(sv_file_writefile(cstr(path0byte), "", "wb"));
    check(sv_file_writefile(cstr(invalidmp3), "not-a-valid-mp3", "wb"));

    { /* hash of normal file */
        get_hash_helper(cstr(pathnormalfile), &archiver, false, &hash, &crc32);
        TestTrue(0x4d36bf2cf609ea58ULL == hash.data[0] && 0x19b91b8a95e63aadULL == hash.data[1]);
        TestTrue(0x1f1242b808bf0428ULL == hash.data[2] && 0xf6539e0c067bcadaULL == hash.data[3]);
        TestEqn(0x8587d865, crc32);
    }
    { /* hash of 0byte file. the hash should be non-zero */
        get_hash_helper(cstr(path0byte), &archiver, false, &hash, &crc32);
        TestTrue(0x232706fc6bf50919ULL == hash.data[0] && 0x8b72ee65b4e851c7ULL == hash.data[1]);
        TestTrue(0x88d8e9628fb694aeULL == hash.data[2] && 0x015c99660e766a98ULL == hash.data[3]);
        TestEqn(0x00000000, crc32);
    }
    { /* file is missing */
        os_exclusivefilehandle handle = {};
        check(os_exclusivefilehandle_open(&handle, cstr(pathnormalfile), true, NULL));
        bassignformat(handle.loggingcontext, "%s%sNotExist.mp3", dir, pathsep);
        set_debugbreaks_enabled(false);
        expect_err_with_message(hash_of_file(&handle, true,
            knownfileextension_mp3, cstr(archiver.path_audiometadata_binary),
            &hash, &crc32), "file not found");
        set_debugbreaks_enabled(true);
        os_exclusivefilehandle_close(&handle);
    }
    { /* hash of invalid file handle, get-metadata turned off */
        os_exclusivefilehandle handle = { bstring_open(), NULL, -1 };
        bassign(handle.loggingcontext, pathnormalfile);
        set_debugbreaks_enabled(false);
        expect_err_with_message(hash_of_file(&handle, false,
            knownfileextension_mp3, cstr(archiver.path_audiometadata_binary),
            &hash, &crc32), "bad file handle");
        set_debugbreaks_enabled(true);
        os_exclusivefilehandle_close(&handle);
    }
    { /* hash of invalid file handle, get-metadata turned on */
        os_exclusivefilehandle handle = { bstring_open(), NULL, -1 };
        bassign(handle.loggingcontext, pathnormalfile);
        set_debugbreaks_enabled(false);
        expect_err_with_message(hash_of_file(&handle, true,
            knownfileextension_mp3, cstr(archiver.path_audiometadata_binary),
            &hash, &crc32), "bad file handle");
        set_debugbreaks_enabled(true);
        os_exclusivefilehandle_close(&handle);
    }
    { /* hash of invalid mp3, get-metadata turned off */
        get_hash_helper(cstr(invalidmp3), &archiver, false, &hash, &crc32);
        TestTrue(0x2998bf76385ad9d0 == hash.data[0] && 0xdda72b9b7ffb52ac == hash.data[1]);
        TestTrue(0xeab56cf2bacbb5e2 == hash.data[2] && 0x722db00c51c532f4 == hash.data[3]);
        TestEqn(0x67e2ecb6, crc32);
    }
    { /* hash of invalid mp3, get-metadata turned on */
        set_debugbreaks_enabled(false);
        get_hash_helper(cstr(invalidmp3), &archiver, true, &hash, &crc32);
        set_debugbreaks_enabled(true);
        TestTrue(0x2998bf76385ad9d0 == hash.data[0] && 0xdda72b9b7ffb52ac == hash.data[1]);
        TestTrue(0xeab56cf2bacbb5e2 == hash.data[2] && 0x722db00c51c532f4 == hash.data[3]);
        TestEqn(0x67e2ecb6, crc32);
    }
    { /* hash of valid mp3: id3 unchanged, get-metadata turned off */
        check(writevalidmp3(cstr(validmp3), false, false, false));
        get_hash_helper(cstr(validmp3), &archiver, false, &hash, &crc32);
        TestTrue(0x32892370d570e3ad == hash.data[0] && 0x89b895f43501da14 == hash.data[1]);
        TestTrue(0xdeb97c81dbb62a3b == hash.data[2] && 0x6a796a89e0166c54 == hash.data[3]);
        TestEqn(0xb6a3e71a, crc32);
    }
    { /* hash of valid mp3: id3 changed, get-metadata turned off */
        check(writevalidmp3(cstr(validmp3), true, false, false));
        get_hash_helper(cstr(validmp3), &archiver, false, &hash, &crc32);
        TestTrue(0xaa23935ca198aa6d == hash.data[0] && 0xb86c87c3d3cced09 == hash.data[1]);
        TestTrue(0xa5e99d107ff95dbb == hash.data[2] && 0x3c14348d05e0870b == hash.data[3]);
        TestEqn(0xdcf3b99d, crc32);
    }
    { /* 1 get-metadata on, id3 unchanged, id3 length unchanged, data unchanged */
        check(writevalidmp3(cstr(validmp3), false, false, false));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0xb6a3e71a, crc32);
    }
    { /* 2 get-metadata on, id3 changed, id3 length unchanged, data unchanged */
        check(writevalidmp3(cstr(validmp3), true, false, false));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0xdcf3b99d, crc32);
    }
    { /* 3 get-metadata on, id3 unchanged, id3 length changed, data unchanged */
        check(writevalidmp3(cstr(validmp3), false, true, false));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0x15655751, crc32);
    }
    { /* 4 get-metadata on, id3 changed, id3 length changed, data unchanged */
        check(writevalidmp3(cstr(validmp3), true, true, false));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0x14150f7b, crc32);
    }
    { /* 5 get-metadata on, id3 unchanged, id3 length unchanged, data changed */
        check(writevalidmp3(cstr(validmp3), false, false, true));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0xc5679762, crc32);
    }
    { /* 6 get-metadata on, id3 changed, id3 length unchanged, data changed */
        check(writevalidmp3(cstr(validmp3), true, false, true));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0xaf37c9e5, crc32);
    }
    { /* 7 get-metadata on, id3 unchanged, id3 length changed, data changed */
        check(writevalidmp3(cstr(validmp3), false, true, true));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0x898585be, crc32);
    }
    { /* 8 get-metadata on, id3 changed, id3 length changed, data changed */
        check(writevalidmp3(cstr(validmp3), true, true, true));
        get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
        TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0x88f5dd94, crc32);
    }
    { /* valid mp3, name containing md5 */
        check(writevalidmp3(cstr(namewithmd5), true, true, true));
        get_hash_helper(cstr(namewithmd5), &archiver, true, &hash, &crc32);
        TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
        TestTrue(0x0000000000000000 == hash.data[2] && 0x0000000000000000 == hash.data[3]);
        TestEqn(0x88f5dd94, crc32);
    }
    { /* invalid mp3, name containing md5 */
        check(sv_file_writefile(cstr(namewithmd5), "not-a-valid-mp3", "wb"));
        set_debugbreaks_enabled(false);
        get_hash_helper(cstr(namewithmd5), &archiver, true, &hash, &crc32);
        set_debugbreaks_enabled(true);
        TestTrue(0x2998bf76385ad9d0 == hash.data[0] && 0xdda72b9b7ffb52ac == hash.data[1]);
        TestTrue(0xeab56cf2bacbb5e2 == hash.data[2] && 0x722db00c51c532f4 == hash.data[3]);
        TestEqn(0x67e2ecb6, crc32);
    }

    TestTrue(hash256zeros.data[0] == 0 && hash256zeros.data[1] == 0 &&
        hash256zeros.data[2] == 0 && hash256zeros.data[3] == 0);

cleanup:
    svdp_archiver_close(&archiver);
    bdestroy(pathnormalfile);
    bdestroy(path0byte);
    bdestroy(validmp3);
    bdestroy(invalidmp3);
    bdestroy(namewithmd5);
    return currenterr;
}

check_result test_archiver_tar(unused_ptr(const char))
{
    sv_result currenterr = {};
    bstring s = bstring_open();

    /* we require an absolute path to the tar archive */
    if (islinux)
    {
        check(get_tar_archive_parameter("/path/to/file", s));
        TestEqs("--file=/path/to/file", cstr(s));
        set_debugbreaks_enabled(false);
        expect_err_with_message(get_tar_archive_parameter("relative/path",
            s), "require absolute path");
        set_debugbreaks_enabled(true);
    }
    else
    {
        check(get_tar_archive_parameter("X:\\path\\to\\file", s));
        TestEqs("--file=/X/path/to/file", cstr(s));
        set_debugbreaks_enabled(false);
        expect_err_with_message(get_tar_archive_parameter("relative/path",
            s), "require absolute path");
        expect_err_with_message(get_tar_archive_parameter("\\\\uncshare\\path",
            s), "require absolute path");
        expect_err_with_message(get_tar_archive_parameter("\\fromroot\\path",
            s), "require absolute path");
        set_debugbreaks_enabled(true);
    }

cleanup:
    bdestroy(s);
    return currenterr;
}

check_result test_create_archives(const char *dir)
{
    sv_result currenterr = {};
    svdp_archiver archiver = {};
    bstring restoreto = bformat("%s%srestoreto", dir, pathsep);
    bstring archivepath = bstring_open();
    bstring path = bstring_open();
    bstring contents_got = bstring_open();
    bstring filename = bstring_open();
    bstring large = bstring_open();
    bstr_fill(large, 'a', 250 * 1024);

    /* create input files. the input files are a text file containing a path, except for large. */
    /* we don't yet support files containing the '*' character,
    because 7zip always parses it as a wildcard, both adding and extracting
    tar does support it, because it has a --no-wildcards switch */
    bstring inputfile_normal = bformat("%s%snormal.txt", dir, pathsep);
    bstring inputfile_large = bformat("%s%slarge.txt", dir, pathsep);
    bstring inputfile_empty = bformat("%s%sempty.txt", dir, pathsep);
    bstring inputfile_utf = bformat("%s%sutf_\xF0\x9D\x84\x9E.txt", dir, pathsep);
    bstring inputfile_weirdname1 = bformat("%s%s%s", dir, pathsep, ".txt");
    bstring inputfile_weirdname2 = bformat("%s%s%s", dir, pathsep, islinux ?
        ".\"\n'a'\\n\\a[abc].txt" : ".'a'[abc].txt");

    check(os_tryuntil_deletefiles(dir, "*"));
    check(sv_file_writefile(cstr(inputfile_normal), cstr(inputfile_normal), "wb"));
    check(sv_file_writefile(cstr(inputfile_large), cstr(large), "wb"));
    check(sv_file_writefile(cstr(inputfile_empty), cstr(inputfile_empty), "wb"));
    check(sv_file_writefile(cstr(inputfile_utf), cstr(inputfile_utf), "wb"));
    check(sv_file_writefile(cstr(inputfile_weirdname1), cstr(inputfile_weirdname1), "wb"));
    check(sv_file_writefile(cstr(inputfile_weirdname2), cstr(inputfile_weirdname2), "wb"));
    TestTrue(os_create_dirs(cstr(restoreto)));

    enum {
        test_archiver_use_7z = 0,
        test_archiver_use_7z_encrypt,
        test_archiver_use_tar,
        test_archiver_use_tar_custom_name,
        test_archiver_max,
    };

    for (int ntest = 0; ntest < test_archiver_max; ntest++)
    {
        /* create archiver */
        svdp_archiver_close(&archiver);
        check(svdp_archiver_open(&archiver, dir, "test", 0, 0,
            ntest == test_archiver_use_7z_encrypt ? "encrypted" : ""));
        check(checkexternaltoolspresent(&archiver, 1 /* use ffmpeg */));
        check_b(os_create_dirs(cstr(archiver.path_staging)), "could not create or access %s",
            cstr(archiver.path_staging));
        check(os_tryuntil_deletefiles(cstr(archiver.path_staging), "*"));
        check(os_tryuntil_deletefiles(cstr(restoreto), "*"));

        bstring inputs[] = { inputfile_normal, inputfile_large, inputfile_empty,
            inputfile_utf, inputfile_weirdname1, inputfile_weirdname2 };
        for (int i = 0; i < countof(inputs); i++)
        {
            os_get_filename(cstr(inputs[i]), filename);
            bassignformat(archivepath, "%s%s%s%s",
                cstr(archiver.path_staging), pathsep, cstr(filename),
                ntest < test_archiver_use_tar ? ".7z" : ".tar");

            /* add a file */
            switch (ntest)
            {
            case test_archiver_use_7z: /* fallthrough */
            case test_archiver_use_7z_encrypt:
                check(svdp_7z_add(&archiver, cstr(archivepath), cstr(inputs[i]), i % 2 == 0));
                break;
            case test_archiver_use_tar:
                check(svdp_tar_add(&archiver, cstr(archivepath), cstr(inputs[i])));
                break;
            case test_archiver_use_tar_custom_name:
                bstr_catstatic(filename, ".changed");
                check(svdp_tar_add_with_custom_name(&archiver,
                    cstr(archivepath), cstr(inputs[i]), cstr(filename)));
                break;
            }

            /* extract all files in the archive */
            check(os_tryuntil_deletefiles(cstr(restoreto), "*"));
            check(svdp_7z_extract_overwrite_ok(&archiver,
                cstr(archivepath), cstr(archiver.encryption), "*", cstr(restoreto)));
            check(os_listdirs(cstr(restoreto), archiver.tmp_list, false));
            TestEqn(0, archiver.tmp_list->qty);
            check(os_listfiles(cstr(restoreto), archiver.tmp_list, false));
            TestEqn(1, archiver.tmp_list->qty);
            bassignformat(path, "%s%s%s", cstr(restoreto), pathsep, cstr(filename));
            TestEqs(cstr(path), bstrlist_view(archiver.tmp_list, 0));

            /* verify content */
            check(sv_file_readfile(bstrlist_view(archiver.tmp_list, 0), contents_got));
            TestEqs(s_contains(
                cstr(filename), "large") ? cstr(large) : cstr(inputs[i]), cstr(contents_got));
        }

        if (ntest == test_archiver_use_tar_custom_name)
        {
            /* for these tests, no difference between test_archiver_use_tar
            and test_archiver_use_tar_custom_name  */
            continue;
        }

        /* extraction should overwrite existing files with the same name */
        check(os_tryuntil_deletefiles(cstr(restoreto), "*"));
        bassignformat(path, "%s%snormal.txt", cstr(restoreto), pathsep);
        check(sv_file_writefile(cstr(path), "this-should-be-overwritten", "wb"));
        bassignformat(archivepath, "%s%s%s%s", cstr(archiver.path_staging), pathsep, "normal.txt",
            ntest >= test_archiver_use_tar ? ".tar" : ".7z");
        check(svdp_7z_extract_overwrite_ok(&archiver,
            cstr(archivepath), cstr(archiver.encryption), "*", cstr(restoreto)));
        check(sv_file_readfile(cstr(path), contents_got));
        TestEqs(cstr(inputfile_normal), cstr(contents_got));

        /* attempt to extract non-existant file from existing archive. should be silent. */
        check(os_tryuntil_deletefiles(cstr(restoreto), "*"));
        bassignformat(archivepath, "%s%s%s%s", cstr(archiver.path_staging), pathsep, "normal.txt",
            ntest >= test_archiver_use_tar ? ".tar" : ".7z");
        check(svdp_7z_extract_overwrite_ok(&archiver,
            cstr(archivepath), cstr(archiver.encryption), "not-in-archive", cstr(restoreto)));
        check(os_listfiles(cstr(restoreto), archiver.tmp_list, false));
        TestEqn(0, archiver.tmp_list->qty);

        /* attempt to extract from non-existant archive */
        bassignformat(archivepath, "%s%s%s%s", cstr(archiver.path_staging), pathsep, "does-not-exist",
            ntest >= test_archiver_use_tar ? ".tar" : ".7z");
        set_debugbreaks_enabled(false);
        expect_err_with_message(svdp_7z_extract_overwrite_ok(&archiver,
            cstr(archivepath), cstr(archiver.encryption), "*", cstr(restoreto)), "not found");
        set_debugbreaks_enabled(true);

        /* attempt to add a non-existant file */
        bassignformat(archivepath, "%s%s%s%s", cstr(archiver.path_staging), pathsep, "try-not-found",
            ntest >= test_archiver_use_tar ? ".tar" : ".7z");
        bassignformat(path, "%s%sdoes-not-exist.txt", cstr(restoreto), pathsep);
        set_debugbreaks_enabled(false);
        if (ntest >= test_archiver_use_tar)
        {
            expect_err_with_message(svdp_tar_add(&archiver,
                cstr(archivepath), cstr(path)), "Cannot stat: No such");
        }
        else
        {
            expect_err_with_message(svdp_7z_add(&archiver,
                cstr(archivepath), cstr(path), false),
                islinux ? "Cannot find 1 file" : "The system cannot find the file specified.");
        }
        set_debugbreaks_enabled(true);

        /* attempt to add an inaccessible file */
        bassignformat(archivepath, "%s%s%s%s", cstr(archiver.path_staging), pathsep, "try-add-inaccessible",
            ntest >= test_archiver_use_tar ? ".tar" : ".7z");
        os_exclusivefilehandle handle = {};
        check(os_exclusivefilehandle_open(&handle,
            cstr(inputfile_normal), false /* don't allow read */, NULL));
        TestTrue(os_file_exists(cstr(inputfile_normal)));
        set_debugbreaks_enabled(false);
        if (!islinux && ntest >= test_archiver_use_tar)
        {
            expect_err_with_message(svdp_tar_add(&archiver,
                cstr(archivepath), cstr(inputfile_normal)), "Cannot open: Permission denied");
        }
        else if (!islinux)
        {
            expect_err_with_message(svdp_7z_add(&archiver,
                cstr(archivepath), cstr(inputfile_normal), false), "process cannot access the file");
        }
        set_debugbreaks_enabled(true);
        os_exclusivefilehandle_close(&handle);

        /* attempt to add to an inaccessible archive */
        bassignformat(archivepath, "%s%s%s%s", cstr(archiver.path_staging), pathsep, "normal.txt",
            ntest >= test_archiver_use_tar ? ".tar" : ".7z");
        check(os_exclusivefilehandle_open(&handle,
            cstr(archivepath), true /* allow read but not write */, NULL));
        set_debugbreaks_enabled(false);
        if (!islinux && ntest >= test_archiver_use_tar)
        {
            expect_err_with_message(svdp_tar_add(&archiver,
                cstr(archivepath), cstr(inputfile_large)), "Cannot read:");
        }
        else if (!islinux)
        {
            expect_err_with_message(svdp_7z_add(&archiver,
                cstr(archivepath), cstr(inputfile_large), false), "should not exist yet");
        }
        set_debugbreaks_enabled(true);
        os_exclusivefilehandle_close(&handle);

        if (ntest <= test_archiver_use_7z_encrypt)
        {
            /* we should not be able to re-use a 7z. max of one file per 7z. */
            bassignformat(archivepath, "%s%s%s", cstr(archiver.path_staging), pathsep, "normal.txt.7z");
            TestTrue(os_file_exists(cstr(archivepath)));
            set_debugbreaks_enabled(false);
            expect_err_with_message(svdp_7z_add(&archiver,
                cstr(archivepath), cstr(inputfile_large), false),
                "should not exist yet");
            set_debugbreaks_enabled(true);
        }
    }

cleanup:
    bdestroy(large);
    bdestroy(path);
    bdestroy(restoreto);
    bdestroy(archivepath);
    bdestroy(contents_got);
    bdestroy(inputfile_normal);
    bdestroy(inputfile_large);
    bdestroy(inputfile_empty);
    bdestroy(inputfile_utf);
    bdestroy(inputfile_weirdname1);
    bdestroy(inputfile_weirdname2);
    bdestroy(filename);
    svdp_archiver_close(&archiver);
    return currenterr;
}

void setfileaccessiblity(operations_test_hook *hook, int number, bool val)
{
    TestTrue(number >= 0 && number < countof(hook->filelocks));
    if (val)
    {
        os_exclusivefilehandle_close(&hook->filelocks[number]);
    }
    else
    {
        check_fatal(!hook->filelocks[number].os_handle, "cannot not lock twice");
        check_warn(os_exclusivefilehandle_open(&hook->filelocks[number],
            cstr(hook->filenames[number]), false, NULL), "", exit_on_err);
    }
}

void operations_test_hook_close(operations_test_hook *self)
{
    if (self)
    {
        for (int i = 0; i < countof(self->filenames); i++)
        {
            os_exclusivefilehandle_close(&self->filelocks[i]);
            bdestroy(self->filenames[i]);
        }

        bdestroy(self->tmp_path);
        bdestroy(self->dirfakeuserfiles);
        bdestroy(self->path_untar);
        bdestroy(self->path_group);
        bdestroy(self->path_restoreto);
        bdestroy(self->path_readytoremove);
        bdestroy(self->path_readytoupload);
        bstrlist_close(self->allcontentrows);
        bstrlist_close(self->allfilelistrows);
        set_self_zero();
    }
}

check_result test_hook_get_file_info(void *phook, os_exclusivefilehandle *self,
    unused_ptr(uint64_t), uint64_t *modtime, bstring permissions)
{
    operations_test_hook *hook = (operations_test_hook *)phook;
    if (hook)
    {
        /* our test hook injects fake file metadata*/
        /* test files are named test0 through test9, so we can read the number from the filename */
        const char *filename = cstr(self->loggingcontext);
        TestTrue(strlen(filename) > 10);
        int filenumber = filename[strlen(filename) - 5] - '0';
        TestTrue(filenumber >= 0 && filenumber < countof(hook->fakelastmodtimes));
        *modtime = hook->fakelastmodtimes[filenumber];
        bassignformat(permissions, "%d", 111 * filenumber);
    }
    return OK;
}

check_result test_hook_call_before_processing_queue(
    void *phook, svdb_connection *db, svdp_archiver *archiver)
{
    /* this callback, triggered halfway through compact(), verifies database state is what we expect */
    operations_test_hook *hook = (operations_test_hook *)phook;
    if (hook && archiver)
    {
        archiver->can_add_to_tar_directly = false;
    }

    if (hook && hook->stage_of_test != -1)
    {
        bstrlist_clear(hook->allcontentrows);
        bstrlist_clear(hook->allfilelistrows);
        check_warn(svdb_contentsiter(db, hook->allcontentrows,
            &contents_addallrowstolistofstrings), "", exit_on_err);
        check_warn(svdb_files_in_queue(db, svdb_files_in_queue_get_all,
            hook->allfilelistrows, &files_addallrowstolistofstrings), "", exit_on_err);

        if (hook->stage_of_test == 1)
        {
            TestEqn(0, hook->allcontentrows->qty);
            TestEqn(6, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrlist_view(hook->allfilelistrows, 5)));
        }
        else if (hook->stage_of_test == 2)
        {
            TestEqn(6, hook->allcontentrows->qty);
            TestEqn(6, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 5)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=1, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=6, last_write_time=1, flags=555, most_recent_collection=1, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrlist_view(hook->allfilelistrows, 5)));
        }
        else if (hook->stage_of_test == 3)
        {
            TestEqn(6, hook->allcontentrows->qty);
            TestEqn(6, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 5)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=6, last_write_time=1, flags=555, most_recent_collection=2, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrlist_view(hook->allfilelistrows, 5)));
        }
        else if (hook->stage_of_test == 4)
        {
            TestEqn(9, hook->allcontentrows->qty);
            TestEqn(6, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 5)));
            TestTrue(fnmatch_simple("hash=4e228734f4417f88 f176ff37b912b345 1aa7c6aa85f440aa d6bad0d10f7cd5b2, crc32=9215b3c6, contents_length=19,1??, most_recent_collection=2, original_collection=2, archivenumber=1, id=7", bstrlist_view(hook->allcontentrows, 6)));
            TestTrue(fnmatch_simple("hash=1f670f96631263a0 44d3d56cdd109b5f 793f361d46f9a315 bd7ffe48ae75ebe4, crc32=e8deaef, contents_length=10,1??, most_recent_collection=2, original_collection=2, archivenumber=1, id=8", bstrlist_view(hook->allcontentrows, 7)));
            TestTrue(fnmatch_simple("hash=59547001a163df77 25b6786ba1ab3f71 9780a1194748b574 3ce7a9f21f22f5a2, crc32=dd48b016, contents_length=19,1??, most_recent_collection=2, original_collection=2, archivenumber=1, id=9", bstrlist_view(hook->allcontentrows, 8)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=19, contents_id=7, last_write_time=1, flags=222, most_recent_collection=2, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=2, flags=333, most_recent_collection=2, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=8, last_write_time=2, flags=444, most_recent_collection=2, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=19, contents_id=9, last_write_time=2, flags=555, most_recent_collection=2, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrlist_view(hook->allfilelistrows, 5)));
        }
        else if (hook->stage_of_test == 5)
        {
            /* file 2: file seen previously, removed between adding-to-queue and adding-to-archive */
            TestTrue(os_remove(cstr(hook->filenames[2])));
            hook->fakelastmodtimes[2] = 0;
            /* file 4: file seen previously, inaccessible between adding-to-queue and adding-to-archive */
            setfileaccessiblity(hook, 4, false);
            /* file 5: new file, removed between adding-to-queue and adding-to-archive */
            TestTrue(os_remove(cstr(hook->filenames[5])));
            hook->fakelastmodtimes[5] = 0;
            setfileaccessiblity(hook, 6, false);
            /* file 9: new file, content is changed between adding-to-queue and adding-to-archive */
            check_warn(sv_file_writefile(cstr(hook->filenames[9]),
                "changed-second-with-more", "wb"), "", exit_on_err);

            TestEqn(5, hook->allcontentrows->qty);
            TestEqn(10, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=0, id=1*" pathsep "file0.txt", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file1.txt", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file2.txt", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file3.txt", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file4.txt", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file5.txt", bstrlist_view(hook->allfilelistrows, 5)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=7*" pathsep "file6.txt", bstrlist_view(hook->allfilelistrows, 6)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=8*" pathsep "file7.txt", bstrlist_view(hook->allfilelistrows, 7)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=9*" pathsep "file8.txt", bstrlist_view(hook->allfilelistrows, 8)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=10*" pathsep "file9.txt", bstrlist_view(hook->allfilelistrows, 9)));
        }
        else if (hook->stage_of_test == 6)
        {
            TestEqn(7, hook->allcontentrows->qty);
            TestEqn(6, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,1??, most_recent_collection=2, original_collection=1, archivenumber=1, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("hash=ed9d76f043ec0c97 4c2f7ed570a007e7 95278b72e012766b 3b333ad4d06a8c51, crc32=618f9ed5, contents_length=18,1??, most_recent_collection=2, original_collection=2, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 5)));
            TestTrue(fnmatch_simple("hash=1be52bb37faff0dc 484bb2ad5a0b67b3 670af01d0a133ad3 76ecb5a9eaf24830, crc32=7b206e22, contents_length=24,1??, most_recent_collection=2, original_collection=2, archivenumber=1, id=7", bstrlist_view(hook->allcontentrows, 6)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=2, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file0.txt", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file3.txt", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file4.txt", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=18, contents_id=6, last_write_time=1, flags=777, most_recent_collection=2, e_status=3, id=8*" pathsep "file7.txt", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=18, contents_id=6, last_write_time=1, flags=888, most_recent_collection=2, e_status=3, id=9*" pathsep "file8.txt", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=24, contents_id=7, last_write_time=1, flags=999, most_recent_collection=2, e_status=3, id=10*" pathsep "file9.txt", bstrlist_view(hook->allfilelistrows, 5)));
        }
        else if (hook->stage_of_test == 7)
        {
            TestEqn(0, hook->allcontentrows->qty);
            TestEqn(4, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file0.jpg", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file1.jpg", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file2.jpg", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file3.jpg", bstrlist_view(hook->allfilelistrows, 3)));
        }
        else if (hook->stage_of_test == 8)
        {
            TestEqn(4, hook->allcontentrows->qty);
            TestEqn(4, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=cd270b526ba2e90d 722778efe7b2c29d abd68da25e8b1eb1 bd0c0cd7fa1d6ce7, crc32=646773f3, contents_length=40960,4????, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=6475ff6c168ec2c4 bf46244f6a3386e1 c33373ced8062291 b19877758bf215d0, crc32=d39f5778, contents_length=40960,4????, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=c0ea4a9032d68aeb 68b77b93c9c8e487 7a45f6bab2b0d83d fe4cff3d741bcad6, crc32=802ae62f, contents_length=20480,2????, most_recent_collection=1, original_collection=1, archivenumber=2, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=9ea177d0cd42dbbd 947ed281720090cd da9d6580b6b3d2cf b41b3dfead68ed0, crc32=669eca11, contents_length=256001,25????, most_recent_collection=1, original_collection=1, archivenumber=3, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("contents_length=40960, contents_id=1, last_write_time=2, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file0.jpg", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=40960, contents_id=2, last_write_time=2, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file1.jpg", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=20480, contents_id=3, last_write_time=2, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file2.jpg", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=256001, contents_id=4, last_write_time=2, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file3.jpg", bstrlist_view(hook->allfilelistrows, 3)));
        }
        else if (hook->stage_of_test == 9)
        {
            TestEqn(0, hook->allcontentrows->qty);
            TestEqn(5, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrlist_view(hook->allfilelistrows, 4)));
        }
        else if (hook->stage_of_test == 10)
        {
            TestEqn(2, hook->allcontentrows->qty);
            TestEqn(5, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1,3???, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=1, last_write_time=1, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=444, most_recent_collection=1, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrlist_view(hook->allfilelistrows, 4)));
        }
        else if (hook->stage_of_test == 11)
        {
            TestEqn(2, hook->allcontentrows->qty);
            TestEqn(7, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1,3???, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.mp3", bstrlist_view(hook->allfilelistrows, 5)));
            TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=7*" pathsep "file\xE1\x84\x81_6.mp3", bstrlist_view(hook->allfilelistrows, 6)));
        }
        else if (hook->stage_of_test == 12)
        {
            TestEqn(5, hook->allcontentrows->qty);
            TestEqn(7, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1,1??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1,3???, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=4d098b3072969adc 635dfe534db06698 d2307aea1e18b7b3 922bbf0481ef8e6b, crc32=95bcd25d, contents_length=1,1??, most_recent_collection=2, original_collection=2, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=6e1b0f02bb2220c4 aba9f88dca9fd76f 6d2e86e5f79b4f25 b039cedc3e4fd7af, crc32=b4e1484f, contents_length=1,3???, most_recent_collection=2, original_collection=2, archivenumber=1, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=7d49276b4e05c2bd 39f840313d6eeb4f 0 0, crc32=c5679762, contents_length=1,3???, most_recent_collection=2, original_collection=2, archivenumber=1, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=3, last_write_time=2, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrlist_view(hook->allfilelistrows, 0)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrlist_view(hook->allfilelistrows, 1)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=4, last_write_time=2, flags=222, most_recent_collection=2, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrlist_view(hook->allfilelistrows, 2)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=2, flags=333, most_recent_collection=2, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrlist_view(hook->allfilelistrows, 3)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=5, last_write_time=2, flags=444, most_recent_collection=2, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrlist_view(hook->allfilelistrows, 4)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=555, most_recent_collection=2, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.mp3", bstrlist_view(hook->allfilelistrows, 5)));
            TestTrue(fnmatch_simple("contents_length=1, contents_id=5, last_write_time=1, flags=666, most_recent_collection=2, e_status=3, id=7*" pathsep "file\xE1\x84\x81_6.mp3", bstrlist_view(hook->allfilelistrows, 6)));
        }
        else if (hook->stage_of_test == 30 || hook->stage_of_test == 40 || hook->stage_of_test == 41)
        {
            TestEqn(10, hook->allcontentrows->qty);
            TestEqn(0, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=6d0e4c169cc0c599 8246a2c30669cd32 28b85d3d6074d314 f3a87c2aea57d9c0, crc32=b536af1d, contents_length=30002,30???, most_recent_collection=2, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=c32e8a5950abf496 9bf7b53abbe2fb6a c698b15eb0ce0130 b7b94f962c6e03da, crc32=8b04e435, contents_length=30003,30???, most_recent_collection=2, original_collection=1, archivenumber=2, id=4", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=fb412ff195b17e3f f103f68f57873b05 de3be0dcfb6f1799 7bc921a2fe7f0970, crc32=be8f7e84, contents_length=30004,30???, most_recent_collection=1, original_collection=1, archivenumber=2, id=5", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 5)));
            TestTrue(fnmatch_simple("hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7", bstrlist_view(hook->allcontentrows, 6)));
            TestTrue(fnmatch_simple("hash=22185046db27dcc7 25a0045524fa41ef bc2eaa1e74e47ade 1049dfcc60879063, crc32=da800a06, contents_length=30007,30???, most_recent_collection=2, original_collection=2, archivenumber=1, id=8", bstrlist_view(hook->allcontentrows, 7)));
            TestTrue(fnmatch_simple("hash=deba8dfbf4311d38 9c8b78eafd0cf50b 31f54eeacd34c49e d9773741db16ecd0, crc32=10e9b7c, contents_length=30008,30???, most_recent_collection=3, original_collection=2, archivenumber=2, id=9", bstrlist_view(hook->allcontentrows, 8)));
            TestTrue(fnmatch_simple("hash=2996400e1c920ff2 8a59a0b5f4e83bbe aae8a60f9290c79e 80f77a8e77166f20, crc32=b1058dcf, contents_length=30009,30???, most_recent_collection=2, original_collection=2, archivenumber=2, id=10", bstrlist_view(hook->allcontentrows, 9)));
        }
        else if (hook->stage_of_test == 42)
        {
            TestEqn(5, hook->allcontentrows->qty);
            TestEqn(0, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=deba8dfbf4311d38 9c8b78eafd0cf50b 31f54eeacd34c49e d9773741db16ecd0, crc32=10e9b7c, contents_length=30008,30???, most_recent_collection=3, original_collection=2, archivenumber=2, id=9", bstrlist_view(hook->allcontentrows, 4)));
        }
        else if (hook->stage_of_test == 43)
        {
            TestEqn(6, hook->allcontentrows->qty);
            TestEqn(0, hook->allfilelistrows->qty);
            TestTrue(fnmatch_simple("hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1", bstrlist_view(hook->allcontentrows, 0)));
            TestTrue(fnmatch_simple("hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2", bstrlist_view(hook->allcontentrows, 1)));
            TestTrue(fnmatch_simple("hash=6d0e4c169cc0c599 8246a2c30669cd32 28b85d3d6074d314 f3a87c2aea57d9c0, crc32=b536af1d, contents_length=30002,30???, most_recent_collection=2, original_collection=1, archivenumber=1, id=3", bstrlist_view(hook->allcontentrows, 2)));
            TestTrue(fnmatch_simple("hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6", bstrlist_view(hook->allcontentrows, 3)));
            TestTrue(fnmatch_simple("hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7", bstrlist_view(hook->allcontentrows, 4)));
            TestTrue(fnmatch_simple("hash=22185046db27dcc7 25a0045524fa41ef bc2eaa1e74e47ade 1049dfcc60879063, crc32=da800a06, contents_length=30007,30???, most_recent_collection=2, original_collection=2, archivenumber=1, id=8", bstrlist_view(hook->allcontentrows, 5)));
        }
        else if (hook->stage_of_test == 44 || hook->stage_of_test == 45)
        {
            TestEqn(0, hook->allcontentrows->qty);
            TestEqn(0, hook->allfilelistrows->qty);
        }
    }

    return OK;
}

check_result test_hook_provide_file_list(void *phook, void *context,
    FnRecurseThroughFilesCallback callback)
{
    sv_result currenterr = {};
    operations_test_hook *hook = (operations_test_hook *)phook;
    if (hook)
    {
        /* inject our own list of files to be backed up.
        this way we can control the order and provide a fake last-modified-time. */
        for (uint32_t i = 0; i < countof(hook->filenames); i++)
        {
            if (hook->fakelastmodtimes[i])
            {
                bstring permissions = bformat("%d", 111 * i);
                check(callback(context, hook->filenames[i], hook->fakelastmodtimes[i],
                    os_getfilesize(cstr(hook->filenames[i])), permissions));
                bdestroy(permissions);
            }
        }
    }
cleanup:
    return currenterr;
}

check_result test_operations_backup_check_archivecontents(operations_test_hook *hook,
    const char *archivename, const char *expected)
{
    sv_result currenterr = {};
    bstrlist *archivecontents = bstrlist_open();
    svdp_archiver arch = {};
    check(svdp_archiver_open(&arch, "/", "/", 0, 0, ""));
    check(checkexternaltoolspresent(&arch, 0));
    bstring archivepath = bformat("%s%s%s",
        cstr(hook->path_readytoupload), pathsep, archivename);
    check(svdp_archiver_tar_list_string_testsonly(&arch,
        cstr(archivepath), archivecontents, cstr(hook->path_untar)));

    {	/* a limitation in some versions of 7z is that all utf8 chars are printed as '_'
        (that's the reason why we don't use "7z l" in product code)
        so this test accepts both the correct \xe1\x84\x81 sequence or _ */
        bstring joined = bjoin_static(archivecontents, "|");
        bstr_replaceall(joined, "\xE1\x84\x81", "_");
        TestEqs(expected, cstr(joined));
        bdestroy(joined);
        bdestroy(archivepath);
    }
cleanup:
    bstrlist_close(archivecontents);
    svdp_archiver_close(&arch);
    return currenterr;
}

check_result run_backup_and_reconnect(const svdp_application *app, const svdp_backupgroup *group,
    svdb_connection *db, operations_test_hook *hook, bool disable_debug_breaks)
{
    sv_result currenterr = {};
    bstring dbpath = bstrcpy(db->path);
    if (disable_debug_breaks)
    {
        set_debugbreaks_enabled(false);
    }

    check(svdp_backup(app, group, db, hook));
    set_debugbreaks_enabled(true);
    check(svdb_connection_disconnect(db));
    check(svdb_connection_open(db, cstr(dbpath)));
cleanup:
    bdestroy(dbpath);
    return currenterr;
}

check_result test_operations_backup_reset(const svdp_application *app, const svdp_backupgroup *group,
    svdb_connection *db, operations_test_hook *hook, int numberoffiles, const char *extension,
    bool unicode, bool runbackup)
{
    sv_result currenterr = {};
    bstring tmp = bstring_open();
    printf(".");
    for (int i = 0; i < countof(hook->filenames); i++)
    {
        bdestroy(hook->filenames[i]);
        hook->filenames[i] = bformat("%s%sfile%s%d.%s", cstr(hook->dirfakeuserfiles), pathsep,
            unicode ? "\xE1\x84\x81_" : "", i, extension);
        setfileaccessiblity(hook, i, true);
    }

    /* delete everything in the db and in the working directories */
    check(svdb_clear_database_content(db));
    check(os_tryuntil_deletefiles(cstr(hook->dirfakeuserfiles), "*"));
    check(os_tryuntil_deletefiles(cstr(hook->path_readytoupload), "*"));
    check(os_tryuntil_deletefiles(cstr(hook->path_readytoremove), "*"));
    memset(hook->fakelastmodtimes, 0, sizeof(hook->fakelastmodtimes));
    for (int i = 0; i < numberoffiles; i++)
    {
        bassignformat(tmp, "contents-%d", i);
        check(sv_file_writefile(cstr(hook->filenames[i]), cstr(tmp), "wb"));
        hook->fakelastmodtimes[i] = 1;
    }

    if (runbackup)
    {
        check(run_backup_and_reconnect(app, group, db, hook, false));
    }
cleanup:
    bdestroy(tmp);
    return currenterr;
}

check_result test_operations_backup(const svdp_application *app, svdp_backupgroup *group,
    svdb_connection *db, operations_test_hook *hook)
{
    sv_result currenterr = {};
    sv_file file = {};
    bstring path = bstring_open();
    TestTrue(os_create_dirs(cstr(hook->path_untar)));
    check(os_tryuntil_deletefiles(cstr(hook->path_untar), "*"));

    /* Backup Test 1: add files with utf8 names and see if the archives on disk are as expected */
    hook->stage_of_test = 1;
    check(test_operations_backup_reset(app, group, db, hook, 6, "txt", true, true));
    hook->stage_of_test = 2;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    bassignformat(path, "%s%s00001_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    check(test_operations_backup_check_archivecontents(hook, "00001_00001.tar",
        "00000001.7z|file__0.txt contentlength=10 crc=4de40385 itemtype=3|"
        "00000002.7z|file__1.txt contentlength=10 crc=3ae33313 itemtype=3|"
        "00000003.7z|file__2.txt contentlength=10 crc=a3ea62a9 itemtype=3|"
        "00000004.7z|file__3.txt contentlength=10 crc=d4ed523f itemtype=3|"
        "00000005.7z|file__4.txt contentlength=10 crc=4a89c79c itemtype=3|"
        "00000006.7z|file__5.txt contentlength=10 crc=3d8ef70a itemtype=3|"
        "filenames.txt"));

    /* Backup Test 2: we should correctly determine which files on disk have been changed */
    /* See the design explanation at the top of operations.c */
    /* file 0: same contents, same lmt */
    /* file 1: changed contents, same lmt */
    check(sv_file_writefile(cstr(hook->filenames[1]), "contents-x", "wb"));
    /* file 2: appended contents, same lmt */
    check(sv_file_writefile(cstr(hook->filenames[2]), "contents-x-appended", "wb"));
    /* file 3: same contents, different lmt */
    hook->fakelastmodtimes[3]++;
    /* file 4: changed contents, different lmt */
    check(sv_file_writefile(cstr(hook->filenames[4]), "contents-X", "wb"));
    hook->fakelastmodtimes[4]++;
    /* file 5: appended contents, different lmt */
    check(sv_file_writefile(cstr(hook->filenames[5]), "contents-X-appended", "wb"));
    hook->fakelastmodtimes[5]++;
    hook->stage_of_test = 3;
    check(run_backup_and_reconnect(app, group, db, hook, false));
    hook->stage_of_test = 4;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar",
        "00000007.7z|file__2.txt contentlength=19 crc=9215b3c6 itemtype=3|"
        "00000008.7z|file__4.txt contentlength=10 crc=e8deaef itemtype=3|"
        "00000009.7z|file__5.txt contentlength=19 crc=dd48b016 itemtype=3|"
        "filenames.txt"));

    /* Backup Test 3: adding new files */
    hook->stage_of_test = -1;
    check(test_operations_backup_reset(app, group, db, hook, 5, "txt", false, true));
    /* file 0: accessible, with same contents as the previous file1 */
    check(sv_file_writefile(cstr(hook->filenames[0]), "contents-1", "wb"));
    hook->fakelastmodtimes[0]++;
    /* file 1: file seen previously, removed before directory iteration */
    check_b(os_remove(cstr(hook->filenames[1])), "could not remove file.");
    hook->fakelastmodtimes[1] = 0;
    /* file 2: file seen previously, removed after directory iteration */
    hook->fakelastmodtimes[2]++;
    /* file 3: file seen previously, inaccessible before directory iteration */
    check(sv_file_writefile(cstr(hook->filenames[3]), "contents-added-but-inaccessible", "wb"));
    setfileaccessiblity(hook, 3, false);
    hook->fakelastmodtimes[3]++;
    /* file 4: file seen previously, inaccessible after directory iteration */
    hook->fakelastmodtimes[4]++;
    /* file 5: new file, removed after directory iteration */
    check(sv_file_writefile(cstr(hook->filenames[5]), "newfile-5", "wb"));
    hook->fakelastmodtimes[5]++;
    /* file 6: new file, inaccessible after directory iteration */
    check(sv_file_writefile(cstr(hook->filenames[6]), "newfile-6", "wb"));
    hook->fakelastmodtimes[6]++;
    /* file 7: new file */
    check(sv_file_writefile(cstr(hook->filenames[7]), "a-newly-added-file", "wb"));
    hook->fakelastmodtimes[7]++;
    /* file 8: new file with same contents as file 8 */
    check(sv_file_writefile(cstr(hook->filenames[8]), "a-newly-added-file", "wb"));
    hook->fakelastmodtimes[8]++;
    /* file 9: new file, content is changed between adding-to-queue and adding-to-archive */
    check(sv_file_writefile(cstr(hook->filenames[9]), "changed-first", "wb"));
    hook->fakelastmodtimes[9]++;
    hook->stage_of_test = 5;
    check(run_backup_and_reconnect(app, group, db, hook, true));
    hook->stage_of_test = 6;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    bassignformat(path, "%s%s00002_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    check(test_operations_backup_check_archivecontents(hook, "00001_00001.tar",
        "00000001.7z|file0.txt contentlength=10 crc=4de40385 itemtype=3|"
        "00000002.7z|file1.txt contentlength=10 crc=3ae33313 itemtype=3|"
        "00000003.7z|file2.txt contentlength=10 crc=a3ea62a9 itemtype=3|"
        "00000004.7z|file3.txt contentlength=10 crc=d4ed523f itemtype=3|"
        "00000005.7z|file4.txt contentlength=10 crc=4a89c79c itemtype=3|"
        "filenames.txt"));
    check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar",
        "00000006.7z|file7.txt contentlength=18 crc=618f9ed5 itemtype=3|"
        "00000007.7z|file9.txt contentlength=24 crc=7b206e22 itemtype=3|"
        "filenames.txt"));

    /* Backup Test 4: adding files of different sizes and types */
    /* because the filetype is 'jpg' we'll add these files with no compression. */
    hook->stage_of_test = 7;
    check(test_operations_backup_reset(app, group, db, hook, 4, "jpg", false, false));
    group->approx_archive_size_bytes = 100 * 1024;
    bstring large = bstring_open();
    /* file 0: normal text file */
    bstr_fill(large, 'a', 40 * 1024);
    check(sv_file_writefile(cstr(hook->filenames[0]), cstr(large), "wb"));
    hook->fakelastmodtimes[0]++;
    /* file 1: normal text file */
    bstr_fill(large, 'b', 40 * 1024);
    check(sv_file_writefile(cstr(hook->filenames[1]), cstr(large), "wb"));
    hook->fakelastmodtimes[1]++;
    /* file 2: not that big, but too big to fit in the archive */
    bstr_fill(large, 'c', 20 * 1024);
    check(sv_file_writefile(cstr(hook->filenames[2]), cstr(large), "wb"));
    hook->fakelastmodtimes[2]++;
    /* file 3: a file so big we'll have to create a new archive just for it */
    bstr_fill(large, 'd', 250 * 1024 + 1);
    check(sv_file_writefile(cstr(hook->filenames[3]), cstr(large), "wb"));
    hook->fakelastmodtimes[3]++;
    check(run_backup_and_reconnect(app, group, db, hook, false));
    hook->stage_of_test = 8;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    group->approx_archive_size_bytes = 32 * 1024 * 1024;
    bassignformat(path, "%s%s00001_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    check(test_operations_backup_check_archivecontents(hook, "00001_00001.tar",
        "00000001.7z|file0.jpg contentlength=40960 crc=646773f3 itemtype=2|"
        "00000002.7z|file1.jpg contentlength=40960 crc=d39f5778 itemtype=2|"
        "filenames.txt"));
    check(test_operations_backup_check_archivecontents(hook, "00001_00002.tar",
        "00000003.7z|file2.jpg contentlength=20480 crc=802ae62f itemtype=2|"
        "filenames.txt"));
    check(test_operations_backup_check_archivecontents(hook, "00001_00003.tar",
        "00000004.7z|file3.jpg contentlength=256001 crc=669eca11 itemtype=2|"
        "filenames.txt"));

    /* Backup Test 5: try running backup() when there are no changed files */
    hook->stage_of_test = -1;
    check(run_backup_and_reconnect(app, group, db, hook, false));
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    bassignformat(path, "%s%s00001_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00002_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00001_00002.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00001_00003.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00002_00001.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00002_00002.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));

    /* Backup Test 6: add mp3 files */
    hook->stage_of_test = 9;
    check(test_operations_backup_reset(app, group, db, hook, 5, "mp3", true, false));
    check(sv_file_writefile(cstr(hook->filenames[0]), "contents-0", "wb"));
    check(writevalidmp3(cstr(hook->filenames[1]), false, false, false));
    check(writevalidmp3(cstr(hook->filenames[2]), false, false, false));
    check(writevalidmp3(cstr(hook->filenames[3]), false, false, false));
    check(writevalidmp3(cstr(hook->filenames[4]), false, false, false));
    check(run_backup_and_reconnect(app, group, db, hook, true));
    hook->stage_of_test = 10;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    check(test_operations_backup_check_archivecontents(hook, "00001_00001.tar",
        "00000001.7z|file__0.mp3 contentlength=10 crc=4de40385 itemtype=2|"
        "00000002.7z|file__1.mp3 contentlength=3142 crc=b6a3e71a itemtype=2|"
        "filenames.txt"));

    /* Backup Test 7: test the separate-audio-metadata feature */
    /* file 0: change contents but still invalid */
    check(sv_file_writefile(cstr(hook->filenames[0]), "still-not-a-valid-mp3", "wb"));
    hook->fakelastmodtimes[0]++;
    /* file 1: change the length but not the lmt, we ignore this because we ignore length */
    check(sv_file_open(&file, cstr(hook->filenames[1]), "ab"));
    uint64_t zero = 0;
    fwrite(&zero, sizeof(zero), 1, file.file);
    sv_file_close(&file);
    /* file 2: change lmt and content but not the length, from a valid mp3 to an invalid mp3 */
    uint64_t prevlength = os_getfilesize(cstr(hook->filenames[2]));
    bstr_fill(large, 'a', cast64u32s(prevlength));
    check(sv_file_writefile(cstr(hook->filenames[2]), cstr(large), "wb"));
    hook->fakelastmodtimes[2]++;
    /* file 3: change the metadata only, should not be seen as changed */
    check(writevalidmp3(cstr(hook->filenames[3]), true, false, false));
    hook->fakelastmodtimes[3]++;
    /* file 4: change the data, should be seen as changed */
    check(writevalidmp3(cstr(hook->filenames[4]), false, false, true));
    hook->fakelastmodtimes[4]++;
    /* file 5: a new mp3 file w new length, should be seen as the same as 3 */
    check(writevalidmp3(cstr(hook->filenames[5]), false, true, false));
    hook->fakelastmodtimes[5]++;
    /* file 6: a new mp3 file w new length, should be seen as the same as 4 */
    check(writevalidmp3(cstr(hook->filenames[6]), true, true, true));
    hook->fakelastmodtimes[6]++;
    hook->stage_of_test = 11;
    check(run_backup_and_reconnect(app, group, db, hook, true));
    hook->stage_of_test = 12;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar",
        "00000003.7z|file__0.mp3 contentlength=21 crc=95bcd25d itemtype=2|"
        "00000004.7z|file__2.mp3 contentlength=3142 crc=b4e1484f itemtype=2|"
        "00000005.7z|file__4.mp3 contentlength=3142 crc=c5679762 itemtype=2|"
        "filenames.txt"));
    bdestroy(path);
    bdestroy(large);

cleanup:
    return currenterr;
}

check_result test_hook_call_when_restoring_file(void *phook, const char *originalpath, bstring destpath)
{
    operations_test_hook *hook = (operations_test_hook *)phook;
    if (hook)
    {
        /* instead of writing to that quite-long path, which would be more complicated to delete
        after the test, redirect the destination path someplace simpler (restoreto/a/a/file.txt) */
        const char *pathwithoutroot = originalpath + (islinux ? 1 : 3);
        bstring expectedname = bformat("%s%s%s", cstr(hook->path_restoreto), pathsep, pathwithoutroot);
        TestEqs(cstr(expectedname), cstr(destpath));
        os_get_filename(originalpath, hook->tmp_path);
        bassignformat(destpath, "%s%sa%sa%s%s", cstr(hook->path_restoreto),
            pathsep, pathsep, pathsep, cstr(hook->tmp_path));

        bdestroy(expectedname);
    }
    return OK;
}

check_result test_operations_restore(const svdp_application *app, const svdp_backupgroup *group,
    svdb_connection *db, operations_test_hook *hook)
{
    sv_result currenterr = {};
    bstrlist *files_seen = bstrlist_open();
    bstring contents = bstring_open();
    bstring fullrestoreto = bformat("%s%sa%sa", cstr(hook->path_restoreto), pathsep, pathsep);

    /* run backup and add file0.txt, file1.txt. */
    hook->stage_of_test = -1;
    check(test_operations_backup_reset(app, group, db, hook, 2, "txt", true, true));

    /* modify file1.txt and add file2.txt. */
    check(sv_file_writefile(cstr(hook->filenames[1]), "contents-1-altered", "wb"));
    hook->fakelastmodtimes[1]++;
    check(sv_file_writefile(cstr(hook->filenames[2]), "the-contents-2", "wb"));
    hook->fakelastmodtimes[2]++;
    check(run_backup_and_reconnect(app, group, db, hook, false));

    enum {
        test_operations_restore_scope_to_one_file = 0,
        test_operations_restore_from_many_archives,
        test_operations_restore_include_older_files,
        test_operations_restore_missing_archive,
        test_operations_restore_max,
    };
    for (int i = 0; i < test_operations_restore_max; i++)
    {
        /* clear any existing files */
        check(os_tryuntil_deletefiles(cstr(fullrestoreto), "*"));
        TestTrue(os_tryuntil_remove(cstr(fullrestoreto)));
        bassignformat(hook->tmp_path, "%s%sa", cstr(hook->path_restoreto), pathsep);
        check(os_tryuntil_deletefiles(cstr(hook->tmp_path), "*"));
        TestTrue(os_tryuntil_remove(cstr(hook->tmp_path)));
        check(os_tryuntil_deletefiles(cstr(hook->path_restoreto), "*"));
        TestTrue(os_create_dirs(cstr(hook->path_restoreto)));

        /* run restore */
        svdp_restore_state op = {};
        op.working_dir_archived = bstrcpy(app->path_temp_archived);
        op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
        op.destdir = bfromcstr(cstr(hook->path_restoreto));
        op.scope = bfromcstr(i == test_operations_restore_scope_to_one_file ? "*2.txt" : "*");
        op.destfullpath = bstring_open();
        op.tmp_resultstring = bstring_open();
        op.messages = bstrlist_open();
        op.db = db;
        op.test_context = hook;
        check(svdp_archiver_open(&op.archiver,
            cstr(app->path_app_data), cstr(group->groupname), 0, 0, ""));

        check(checkexternaltoolspresent(&op.archiver, 0));
        TestTrue(os_create_dirs(cstr(op.working_dir_archived)));
        TestTrue(os_create_dirs(cstr(op.working_dir_unarchived)));

        if (i == test_operations_restore_scope_to_one_file)
        {
            check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(1, files_seen->qty);
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_2.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("the-contents-2", cstr(contents));
            TestEqn(0, op.messages->qty);
        }
        else if (i == test_operations_restore_from_many_archives)
        {
            check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(3, files_seen->qty);
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_0.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("contents-0", cstr(contents));
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_1.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("contents-1-altered", cstr(contents));
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_2.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("the-contents-2", cstr(contents));
            TestEqn(0, op.messages->qty);
        }
        else if (i == test_operations_restore_include_older_files)
        {
            /* change the database so that it looks like file0.txt was inaccessible the last time
            we ran backup. we should run smoothly and just restore the latest version we have. */
            svdb_files_row row = {};
            check(svdb_filesbypath(db, hook->filenames[0], &row));
            row.most_recent_collection = 2;
            row.e_status = sv_filerowstatus_queued;
            bassigncstr(contents, "(updated)");
            check(svdb_filesupdate(db, &row, contents));
            check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));

            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(3, files_seen->qty);
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_0.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("contents-0", cstr(contents));
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_1.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("contents-1-altered", cstr(contents));
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_2.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("the-contents-2", cstr(contents));
            TestEqn(0, op.messages->qty);
        }
        else if (i == test_operations_restore_missing_archive)
        {
            /* if an archive is not found, we add the error to a list of messages and continue. */
            bassignformat(hook->tmp_path, "%s%s00002_00001.tar",
                cstr(hook->path_readytoupload), pathsep);
            TestTrue(os_file_exists(cstr(hook->tmp_path)));
            TestTrue(os_tryuntil_remove(cstr(hook->tmp_path)));
            set_debugbreaks_enabled(false);
            check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
            set_debugbreaks_enabled(true);

            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(1, files_seen->qty);
            bassignformat(hook->tmp_path, "%s%sa%sa%sfile\xE1\x84\x81_0.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->tmp_path), contents));
            TestEqs("contents-0", cstr(contents));
            TestEqn(2, op.messages->qty);
            TestTrue(s_contains(bstrlist_view(op.messages, 0), "could not find archive "));
            TestTrue(s_contains(bstrlist_view(op.messages, 1), "could not find archive "));
        }

        svdp_restore_state_close(&op);
    }

cleanup:
    bdestroy(contents);
    bdestroy(fullrestoreto);
    bstrlist_close(files_seen);
    return currenterr;
}

void svdp_compact_archivestats_to_string(const svdp_compact_state *op, bool includefiles, bstring s)
{
    bstrclear(s);
    bstr_catstatic(s, "stats");
    for (uint32_t i = 0; i < op->archive_statistics.arr.length; i++)
    {
        const sv_array *subarray = (const sv_array *)sv_array_atconst(&op->archive_statistics.arr, i);
        for (uint32_t j = 0; j < subarray->length; j++)
        {
            const svdp_archive_stats *o = (const svdp_archive_stats *)sv_array_atconst(subarray, j);
            if (o->needed_entries > 0 || o->old_entries > 0)
            {
                bformata(s, "|%05x_%05x.tar,", o->original_collection, o->archive_number);
                bformata(s, "needed=%llu(%llu),old=%llu(%llu),", castull(o->needed_entries),
                    castull(o->needed_size), castull(o->old_entries), castull(o->old_size));

                if (includefiles)
                {
                    for (uint32_t k = 0; k < o->old_individual_files.length; k++)
                    {
                        bformata(s, "%llu,", sv_array_at64u(&o->old_individual_files, k));
                    }
                }
                else
                {
                    bformata(s, "%d individual files", o->old_individual_files.length);
                }
            }
        }
    }

    bstr_catstatic(s, "archives_to_removefiles|");
    for (uint32_t i = 0; i < op->archives_to_removefiles.length; i++)
    {
        const svdp_archive_stats *o = (const svdp_archive_stats *)sv_array_atconst(
            &op->archives_to_removefiles, i);
        bformata(s, "%05x_%05x.tar|", o->original_collection, o->archive_number);
    }

    bstr_catstatic(s, "archives_to_remove|");
    for (uint32_t i = 0; i < op->archives_to_remove.length; i++)
    {
        const svdp_archive_stats *o = (const svdp_archive_stats *)sv_array_atconst(
            &op->archives_to_remove, i);
        bformata(s, "%05x_%05x.tar|", o->original_collection, o->archive_number);
    }
}

check_result test_operations_compact(const svdp_application *app, svdp_backupgroup *group,
    svdb_connection *db, operations_test_hook *hook)
{
    sv_result currenterr = {};
    bstring contents = bstring_open();
    bstring path = bstring_open();
    check(test_operations_backup_reset(app, group, db, hook, 0, "jpg", false, false));
    group->days_to_keep_prev_versions = 0;
    uint64_t collectionid_to_expire;
    bstring dbpath = bstrcpy(db->path);
    bstrlist *messages = bstrlist_open();
    const time_t october_01_2016 = 1475280000, october_02_2016 = 1475366400,
        october_03_2016 = 1475452800, october_04_2016 = 1475539200, october_20_2016 = 1476921600;

    /* can't run compact if backup has never been run */
    collectionid_to_expire = 9999;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(0, collectionid_to_expire);

    /* can't run compact if backup has only been run once */
    check(svdb_add_collection_row(db, (uint64_t)october_01_2016, (uint64_t)october_01_2016 + 1));
    collectionid_to_expire = 9999;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(0, collectionid_to_expire);

    /* we should not return the latest collection, even if it is old enough to have been expired */
    check(svdb_add_collection_row(db, (uint64_t)october_02_2016, (uint64_t)october_02_2016 + 1));
    collectionid_to_expire = 9999;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(1, collectionid_to_expire);

    /* test finding the cutoff */
    check(svdb_add_collection_row(db, (uint64_t)october_03_2016, (uint64_t)october_03_2016 + 1));
    check(svdb_add_collection_row(db, (uint64_t)october_04_2016, (uint64_t)october_04_2016 + 1));
    group->days_to_keep_prev_versions = 0;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(3, collectionid_to_expire);
    group->days_to_keep_prev_versions = 16;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(3, collectionid_to_expire);
    group->days_to_keep_prev_versions = 17;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(2, collectionid_to_expire);
    group->days_to_keep_prev_versions = 18;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(1, collectionid_to_expire);
    group->days_to_keep_prev_versions = 19;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(0, collectionid_to_expire);
    group->days_to_keep_prev_versions = 100;
    check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
    TestEqn(0, collectionid_to_expire);

    /* let's mimic a real-world compaction scenario. first, add these files with these lifetimes. */
    /* 001_001.tar, file0.jpg (active from 1-4), file1.jpg (active from 1-3), file2.jpg (active from 1-2) */
    /* 001_002.tar, file3.jpg (active from 1-2), file4.jpg (active from 1-1) */
    /* 002_001.tar, file5.jpg (active from 2-4), file6.jpg (active from 2-3), file7.jpg (active from 2-2) */
    /* 002_002.tar, file8.jpg (active from 2-3), file9.jpg (active from 2-2) */
    int filesizes[10] = { 30000, 30001, 30002, 30003, 30004, 30005, 30006, 30007, 30008, 30009 };
    group->approx_archive_size_bytes = 100 * 1024;
    check(test_operations_backup_reset(app, group, db, hook, 0, "jpg", false, false));
    hook->stage_of_test = -1;

    /* files for collection #1 */
    for (int i = 0; i <= 4; i++)
    {
        bstr_fill(contents, 'a', filesizes[i]);
        check(sv_file_writefile(cstr(hook->filenames[i]), cstr(contents), "wb"));
        hook->fakelastmodtimes[i]++;
    }
    check(run_backup_and_reconnect(app, group, db, hook, false));

    /* files for collection #2 */
    for (int i = 5; i <= 9; i++)
    {
        bstr_fill(contents, 'a', filesizes[i]);
        check(sv_file_writefile(cstr(hook->filenames[i]), cstr(contents), "wb"));
        hook->fakelastmodtimes[i]++;
    }
    TestTrue(os_remove(cstr(hook->filenames[4])));
    hook->fakelastmodtimes[4] = 0;
    check(run_backup_and_reconnect(app, group, db, hook, false));

    /* files for collection #3 */
    TestTrue(os_remove(cstr(hook->filenames[2])));
    hook->fakelastmodtimes[2] = 0;
    TestTrue(os_remove(cstr(hook->filenames[3])));
    hook->fakelastmodtimes[3] = 0;
    TestTrue(os_remove(cstr(hook->filenames[7])));
    hook->fakelastmodtimes[7] = 0;
    TestTrue(os_remove(cstr(hook->filenames[9])));
    hook->fakelastmodtimes[9] = 0;
    check(run_backup_and_reconnect(app, group, db, hook, false));

    /* files for collection #4 */
    TestTrue(os_remove(cstr(hook->filenames[1])));
    hook->fakelastmodtimes[1] = 0;
    TestTrue(os_remove(cstr(hook->filenames[6])));
    hook->fakelastmodtimes[6] = 0;
    TestTrue(os_remove(cstr(hook->filenames[8])));
    hook->fakelastmodtimes[8] = 0;
    check(run_backup_and_reconnect(app, group, db, hook, false));

    /* files for collection #5 */
    TestTrue(os_remove(cstr(hook->filenames[0])));
    hook->fakelastmodtimes[0] = 0;
    TestTrue(os_remove(cstr(hook->filenames[5])));
    hook->fakelastmodtimes[5] = 0;
    check(run_backup_and_reconnect(app, group, db, hook, false));

    /* test verify_archive_checksums */
    int mismatches = 0;
    check(svdp_verify_archive_checksums(app, group, db, &mismatches));
    os_clr_console();
    TestTrue(mismatches == 0);

    /* set compressed size to be the same as uncompressed size, for simplified testing.
    otherwise if we changed to a different compression algorithm we'd have to update the test */
    check(svdb_runsql(db,
        str_and_len32s("UPDATE TblContentsList SET CompressedContentLength = ContentLength WHERE 1")));

    /* verify state is what we expect */
    hook->stage_of_test = 30;
    check(test_hook_call_before_processing_queue(hook, db, NULL));
    check(test_operations_backup_check_archivecontents(hook, "00001_00001.tar",
        "00000001.7z|file0.jpg contentlength=30000 crc=6693bf41 itemtype=2|"
        "00000002.7z|file1.jpg contentlength=30001 crc=e90a5cfa itemtype=2|"
        "00000003.7z|file2.jpg contentlength=30002 crc=b536af1d itemtype=2|"
        "filenames.txt"));
    check(test_operations_backup_check_archivecontents(hook, "00001_00002.tar",
        "00000004.7z|file3.jpg contentlength=30003 crc=8b04e435 itemtype=2|"
        "00000005.7z|file4.jpg contentlength=30004 crc=be8f7e84 itemtype=2|"
        "filenames.txt"));
    check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar",
        "00000006.7z|file5.jpg contentlength=30005 crc=2dc7604 itemtype=2|"
        "00000007.7z|file6.jpg contentlength=30006 crc=efd8a62c itemtype=2|"
        "00000008.7z|file7.jpg contentlength=30007 crc=da800a06 itemtype=2|"
        "filenames.txt"));
    check(test_operations_backup_check_archivecontents(hook, "00002_00002.tar",
        "00000009.7z|file8.jpg contentlength=30008 crc=10e9b7c itemtype=2|"
        "0000000a.7z|file9.jpg contentlength=30009 crc=b1058dcf itemtype=2|"
        "filenames.txt"));

    /* make a copy of everything in the database's state in the "saved" directory */
    /* we do this just to speed up the test rather than rebuilding this state from scratch each time */
    bassignformat(path, "%s%s00001_00003.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00002_00003.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00003_00001.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bassignformat(path, "%s%s00004_00001.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bstring databasestate0 = bformat("%s%suserdata%s%s%stest_index.db",
        cstr(app->path_app_data), pathsep, pathsep, cstr(group->groupname), pathsep);

    bstring databasestate1 = bformat("%s%s00001_00001.tar", cstr(hook->path_readytoupload), pathsep);
    bstring databasestate2 = bformat("%s%s00001_00002.tar", cstr(hook->path_readytoupload), pathsep);
    bstring databasestate3 = bformat("%s%s00002_00001.tar", cstr(hook->path_readytoupload), pathsep);
    bstring databasestate4 = bformat("%s%s00002_00002.tar", cstr(hook->path_readytoupload), pathsep);
    bstring databasestate0_saved = bformat("%s%stest_index.db", cstr(hook->path_restoreto), pathsep);
    bstring databasestate1_saved = bformat("%s%s00001_00001.tar", cstr(hook->path_restoreto), pathsep);
    bstring databasestate2_saved = bformat("%s%s00001_00002.tar", cstr(hook->path_restoreto), pathsep);
    bstring databasestate3_saved = bformat("%s%s00002_00001.tar", cstr(hook->path_restoreto), pathsep);
    bstring databasestate4_saved = bformat("%s%s00002_00002.tar", cstr(hook->path_restoreto), pathsep);
    bstring archivestats = bstring_open();
    check(os_tryuntil_deletefiles(cstr(hook->path_restoreto), "*"));
    check(svdb_connection_disconnect(db));
    TestTrue(os_move(cstr(databasestate0), cstr(databasestate0_saved), true));
    TestTrue(os_move(cstr(databasestate1), cstr(databasestate1_saved), true));
    TestTrue(os_move(cstr(databasestate2), cstr(databasestate2_saved), true));
    TestTrue(os_move(cstr(databasestate3), cstr(databasestate3_saved), true));
    TestTrue(os_move(cstr(databasestate4), cstr(databasestate4_saved), true));

    /* now run compact many times, each time with a different 'cutoff'
    dictating which collections have expired */
    for (uint64_t cutoff = 0; cutoff <= 5; cutoff++)
    {
        /* create database state as if we had just added these files */
        check(svdb_connection_disconnect(db));
        check(os_tryuntil_deletefiles(cstr(hook->path_readytoupload), "*"));
        check(os_tryuntil_deletefiles(cstr(hook->path_readytoremove), "*.tar"));
        TestTrue(os_remove(cstr(dbpath)));
        TestTrue(os_copy(cstr(databasestate0_saved), cstr(databasestate0), true));
        TestTrue(os_copy(cstr(databasestate1_saved), cstr(databasestate1), true));
        TestTrue(os_copy(cstr(databasestate2_saved), cstr(databasestate2), true));
        TestTrue(os_copy(cstr(databasestate3_saved), cstr(databasestate3), true));
        TestTrue(os_copy(cstr(databasestate4_saved), cstr(databasestate4), true));
        check(svdb_connection_open(db, cstr(dbpath)));

        const char *expectstatsbefore = NULL, *expectstatsafter = NULL;
        const char *expect_tar_01_01 = "00000001.7z|file0.jpg contentlength=30000 crc=6693bf41 itemtype=2|"
            "00000002.7z|file1.jpg contentlength=30001 crc=e90a5cfa itemtype=2|"
            "00000003.7z|file2.jpg contentlength=30002 crc=b536af1d itemtype=2|"
            "filenames.txt";
        const char *expect_tar_01_02 = "00000004.7z|file3.jpg contentlength=30003 crc=8b04e435 itemtype=2|"
            "00000005.7z|file4.jpg contentlength=30004 crc=be8f7e84 itemtype=2|"
            "filenames.txt";
        const char *expect_tar_02_01 = "00000006.7z|file5.jpg contentlength=30005 crc=2dc7604 itemtype=2|"
            "00000007.7z|file6.jpg contentlength=30006 crc=efd8a62c itemtype=2|"
            "00000008.7z|file7.jpg contentlength=30007 crc=da800a06 itemtype=2|"
            "filenames.txt";
        const char *expect_tar_02_02 = "00000009.7z|file8.jpg contentlength=30008 crc=10e9b7c itemtype=2|"
            "0000000a.7z|file9.jpg contentlength=30009 crc=b1058dcf itemtype=2|"
            "filenames.txt";
        switch (cutoff)
        {
        case 0:
            group->compact_threshold_bytes = 1;
            expectstatsbefore = expectstatsafter = "stats|00001_00001.tar,needed=3(90003),old=0(0),"
                "|00001_00002.tar,needed=2(60007),old=0(0),|00002_00001.tar,needed=3(90018),old=0(0),"
                "|00002_00002.tar,needed=2(60017),old=0(0),archives_to_removefiles|archives_to_remove|";
            break;
        case 1:
            /* set the compact_threshold_bytes to a high value and verify that compact is not run. */
            group->compact_threshold_bytes = 100000;
            expectstatsbefore = expectstatsafter = "stats|00001_00001.tar,needed=3(90003),old=0(0),"
                "|00001_00002.tar,needed=1(30003),old=1(30004),|00002_00001.tar,needed=3(90018),old=0(0),"
                "|00002_00002.tar,needed=2(60017),old=0(0),archives_to_removefiles|archives_to_remove|";
            break;
        case 2:
            /* set the compact_threshold_bytes to a low value and verify that compact is run. */
            group->compact_threshold_bytes = 1;
            expectstatsbefore = "stats|00001_00001.tar,needed=2(60001),old=1(30002),3,|00001_00002.tar,"
                "needed=0(0),old=2(60007),4,5,|00002_00001.tar,needed=2(60011),old=1(30007),8,|"
                "00002_00002.tar,needed=1(30008),old=1(30009),10,archives_to_removefiles|00001_00001.tar"
                "|00002_00001.tar|00002_00002.tar|archives_to_remove|00001_00002.tar|";
            expectstatsafter = "stats|00001_00001.tar,needed=2(60001),old=0(0),|00002_00001.tar,"
                "needed=2(60011),old=0(0),|00002_00002.tar,needed=1(30008),old=0(0),"
                "archives_to_removefiles|archives_to_remove|";
            expect_tar_01_01 = "00000001.7z|file0.jpg contentlength=30000 crc=6693bf41 itemtype=2"
                "|00000002.7z|file1.jpg contentlength=30001 crc=e90a5cfa itemtype=2|filenames.txt";
            expect_tar_01_02 = "not_found";
            expect_tar_02_01 = "00000006.7z|file5.jpg contentlength=30005 crc=2dc7604 itemtype=2"
                "|00000007.7z|file6.jpg contentlength=30006 crc=efd8a62c itemtype=2|filenames.txt";
            expect_tar_02_02 = "00000009.7z|file8.jpg contentlength=30008 crc=10e9b7c itemtype=2"
                "|filenames.txt";
            break;
        case 3:
            /* test what happens if the archives are no longer present on disk.
            for remove-old-files, we skip over, not changing the database or files on disk
            for remove-old-entire-archives, make the change and add a text file to readytoremove dir */
            group->compact_threshold_bytes = 1;
            expectstatsbefore = "stats|00001_00001.tar,needed=1(30000),old=2(60003),2,3,"
                "|00001_00002.tar,needed=0(0),old=2(60007),4,5,|00002_00001.tar,needed=1(30005),"
                "old=2(60013),7,8,|00002_00002.tar,needed=0(0),old=2(60017),9,10,"
                "archives_to_removefiles|00001_00001.tar|00002_00001.tar|archives_to_remove"
                "|00001_00002.tar|00002_00002.tar|";
            expectstatsafter = "stats|00001_00001.tar,needed=1(30000),old=2(60003),2,3,"
                "|00002_00001.tar,needed=1(30005),old=2(60013),7,8,archives_to_removefiles"
                "|00001_00001.tar|00002_00001.tar|archives_to_remove|";
            TestTrue(os_remove(cstr(databasestate1)));
            TestTrue(os_remove(cstr(databasestate2)));
            TestTrue(os_remove(cstr(databasestate3)));
            TestTrue(os_remove(cstr(databasestate4)));
            expect_tar_01_01 = expect_tar_01_02 = expect_tar_02_01 = expect_tar_02_02 = "not_found";
            bassignformat(path, "%s%s00001_00002.tar.txt", cstr(hook->path_readytoremove), pathsep);
            TestTrue(!os_file_exists(cstr(path)));
            bassignformat(path, "%s%s00002_00002.tar.txt", cstr(hook->path_readytoremove), pathsep);
            TestTrue(!os_file_exists(cstr(path)));
            break;
        case 4: /* fallthrough */
        case 5:
            /* everything in the archive has expired, and so we'll be left with no files remaining */
            group->compact_threshold_bytes = 1;
            expectstatsbefore = "stats|00001_00001.tar,needed=0(0),old=3(90003),1,2,3,"
                "|00001_00002.tar,needed=0(0),old=2(60007),4,5,|00002_00001.tar,needed=0(0),"
                "old=3(90018),6,7,8,|00002_00002.tar,needed=0(0),old=2(60017),9,10,"
                "archives_to_removefiles|archives_to_remove|00001_00001.tar|00001_00002.tar"
                "|00002_00001.tar|00002_00002.tar|";
            expectstatsafter = "statsarchives_to_removefiles|archives_to_remove|";
            expect_tar_01_01 = expect_tar_01_02 = expect_tar_02_01 = expect_tar_02_02 = "not_found";
            bassignformat(path, "%s%s00001_00002.tar.txt", cstr(hook->path_readytoremove), pathsep);
            TestTrue(os_file_exists(cstr(path)));
            bassignformat(path, "%s%s00002_00002.tar.txt", cstr(hook->path_readytoremove), pathsep);
            TestTrue(os_file_exists(cstr(path)));
            break;
        }

        /* get archive statistics */
        svdp_compact_state op = {};
        op.archive_statistics = sv_2d_array_open(sizeof32u(svdp_archive_stats));
        op.expiration_cutoff = cutoff;
        op.is_thorough = true;
        op.test_context = hook;
        op.working_dir_archived = bstrcpy(app->path_temp_archived);
        op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
        check(svdb_contentsiter(db, &op, &svdp_compact_getarchivestats));
        svdp_compact_see_what_to_remove(&op, group->compact_threshold_bytes);
        svdp_compact_archivestats_to_string(&op, true, archivestats);
        TestEqs(expectstatsbefore, cstr(archivestats));

        /* run compact */
        bstrlist_clear(messages);
        check(svdp_compact_removeoldarchives(&op, db, cstr(app->path_app_data), cstr(group->groupname),
            cstr(hook->path_readytoupload), messages));
        check(svdp_compact_removeoldfilesfromarchives(&op, app, group, db,
            cstr(hook->path_readytoupload), messages));

        if (cutoff == 2)
        {
            /* test verify_archive_checksums after compaction */
            mismatches = 0;
            check(svdp_verify_archive_checksums(app, group, db, &mismatches));
            os_clr_console();
            TestTrue(mismatches == 0);

            /* corrupt one of the archives by changing a single byte */
            sv_file file = {};
            check(sv_file_open(&file, cstr(databasestate4), "rb+"));
            fseek(file.file, 20000, SEEK_SET);
            fputc('*', file.file);
            sv_file_close(&file);

            /* now the checksum should not match */
            check(svdp_verify_archive_checksums(app, group, db, &mismatches));
            os_clr_console();
            TestTrue(mismatches == 1);
            TestEqn(0, messages->qty);
        }
        else if (cutoff == 3)
        {
            /* when cutoff is 3 we have explicitly removed the .tar files from readytoupload,
            and so we expected to see two messages for the two tar files we didn't see. */
            TestEqn(2, messages->qty);
        }
        else
        {
            TestEqn(0, messages->qty);
        }

        check(test_operations_backup_check_archivecontents(hook, "00001_00001.tar", expect_tar_01_01));
        check(test_operations_backup_check_archivecontents(hook, "00001_00002.tar", expect_tar_01_02));
        check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar", expect_tar_02_01));
        check(test_operations_backup_check_archivecontents(hook, "00002_00002.tar", expect_tar_02_02));
        hook->stage_of_test = 40 + cast64u32s(cutoff);
        check(test_hook_call_before_processing_queue(hook, db, NULL));
        svdp_compact_state_close(&op);

        /* get archive statistics again, afterwards */
        svdp_compact_state op_after = {};
        op_after.archive_statistics = sv_2d_array_open(sizeof32u(svdp_archive_stats));
        op_after.expiration_cutoff = cutoff;
        op_after.is_thorough = true;
        op_after.test_context = hook;
        check(svdb_contentsiter(db, &op_after, &svdp_compact_getarchivestats));
        svdp_compact_see_what_to_remove(&op_after, group->compact_threshold_bytes);
        svdp_compact_archivestats_to_string(&op_after, true, archivestats);
        TestEqs(expectstatsafter, cstr(archivestats));
        svdp_compact_state_close(&op_after);
    }

    bdestroy(dbpath);
    bdestroy(databasestate0);
    bdestroy(databasestate1);
    bdestroy(databasestate2);
    bdestroy(databasestate3);
    bdestroy(databasestate4);
    bdestroy(databasestate0_saved);
    bdestroy(databasestate1_saved);
    bdestroy(databasestate2_saved);
    bdestroy(databasestate3_saved);
    bdestroy(databasestate4_saved);
    bdestroy(archivestats);
    bstrlist_close(messages);
cleanup:
    group->approx_archive_size_bytes = 64 * 1024 * 1024;
    group->compact_threshold_bytes = 32 * 1024 * 1024;
    bdestroy(path);
    bdestroy(contents);
    return currenterr;
}

check_result test_operations(const char *dir)
{
    sv_result currenterr = {};
    svdp_application app = {};
    svdp_backupgroup group = {};
    svdb_connection db = {};
    operations_test_hook hook = {};

    bstring path = bstring_open();
    hook.path_group = bformat("%s%suserdata%stest", dir, pathsep, pathsep);
    hook.path_readytoupload = bformat("%s%suserdata%stest%sreadytoupload", dir, pathsep, pathsep, pathsep);
    hook.path_readytoremove = bformat("%s%suserdata%stest%sreadytoremove", dir, pathsep, pathsep, pathsep);
    hook.path_restoreto = bformat("%s%srestoreto", dir, pathsep);
    hook.path_untar = bformat("%s%suntar", dir, pathsep);
    hook.tmp_path = bstring_open();
    hook.allcontentrows = bstrlist_open();
    hook.allfilelistrows = bstrlist_open();
    uint64_t hashseedwas1 = SvdpHashSeed1, hashseedwas2 = SvdpHashSeed2;
    SvdpHashSeed1 = 0x1999188817771666ULL;
    SvdpHashSeed2 = 0x9988776655443322ULL;

    app.is_low_access = false;
    app.path_app_data = bfromcstr(dir);
    app.path_temp_archived = bformat("%s%stemp%sarchived", cstr(app.path_app_data), pathsep, pathsep);
    app.path_temp_unarchived = bformat("%s%stemp%sunarchived", cstr(app.path_app_data), pathsep, pathsep);
    TestTrue(os_create_dirs(cstr(app.path_temp_archived)));
    TestTrue(os_create_dirs(cstr(app.path_temp_unarchived)));
    app.backup_group_names = bstrlist_open();
    check(os_tryuntil_deletefiles(dir, "*"));
    check(os_tryuntil_deletefiles(cstr(hook.path_group), "*"));
    check(os_tryuntil_deletefiles(cstr(hook.path_readytoupload), "*"));
    check(os_tryuntil_deletefiles(cstr(hook.path_readytoremove), "*"));
    check(svdp_application_creategroup_impl(&app, "test", cstr(hook.path_group)));
    check(svdp_application_findgroupnames(&app));

    /* verify what creating the group should have done */
    TestEqn(1, app.backup_group_names->qty);
    TestEqs("test", bstrlist_view(app.backup_group_names, 0));
    bassignformat(path, "%s%suserdata%stest%stest_index.db", dir, pathsep, pathsep, pathsep);
    TestTrue(os_file_exists(cstr(path)));
    TestTrue(os_dir_exists(cstr(hook.path_readytoupload)));

    /* change group settings. do not need a root directory because of test hook */
    check(loadbackupgroup(&app, &group, &db, "test"));
    group.days_to_keep_prev_versions = 0;
    group.separate_metadata_enabled = 1;
    check(svdp_backupgroup_persist(&db, &group));

    hook.dirfakeuserfiles = bformat("%s%sfakeuserfiles", dir, pathsep);
    TestTrue(os_create_dir(cstr(hook.dirfakeuserfiles)));
    check(test_operations_backup(&app, &group, &db, &hook));
    check(test_operations_restore(&app, &group, &db, &hook));
    check(test_operations_compact(&app, &group, &db, &hook));

cleanup:
    SvdpHashSeed1 = hashseedwas1;
    SvdpHashSeed2 = hashseedwas2;
    bdestroy(path);
    operations_test_hook_close(&hook);
    svdb_connection_close(&db);
    svdp_backupgroup_close(&group);
    svdp_application_close(&app);
    return currenterr;
}

check_result test_find_groups(const char *dir)
{
    sv_result currenterr = {};
    svdp_application fake_app = {};
    bstring userdata = os_make_subdir(dir, "userdata");
    bstring path = bstring_open();
    bstring contents = bstring_open();

    /* incomplete group with no .db files */
    bassignformat(path, "%s%sno_db_files%sreadytoupload",
        cstr(userdata), pathsep, pathsep);
    TestTrue(os_create_dirs(cstr(path)));
    bassignformat(path, "%s%sno_db_files%sreadytoupload%s00001_00001.tar",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));
    bassignformat(path, "%s%sno_db_files%sreadytoupload%s00001_00002.tar",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));

    /* complete group with expected .db file location */
    bassignformat(path, "%s%sexpected%sreadytoupload",
        cstr(userdata), pathsep, pathsep);
    TestTrue(os_create_dirs(cstr(path)));
    bassignformat(path, "%s%sexpected%sexpected_index.db",
        cstr(userdata), pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));
    bassignformat(path, "%s%sexpected%sreadytoupload%s00001_00001.tar",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));
    bassignformat(path, "%s%sexpected%sreadytoupload%s00001_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));

    /* incomplete group that does have .db files, so we'll get the latest */
    bassignformat(path, "%s%sno_main_db%sreadytoupload",
        cstr(userdata), pathsep, pathsep);
    TestTrue(os_create_dirs(cstr(path)));
    bassignformat(path, "%s%sno_main_db%sreadytoupload%s00001_00001.tar",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));
    bassignformat(path, "%s%sno_main_db%sreadytoupload%s00001_00002.tar",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "", "wb"));
    bassignformat(path, "%s%sno_main_db%sreadytoupload%s00001_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "i1", "wb"));
    bassignformat(path, "%s%sno_main_db%sreadytoupload%s00002_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "i2", "wb"));
    bassignformat(path, "%s%sno_main_db%sreadytoupload%s00003_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "i3", "wb"));
    bassignformat(path, "%s%sno_main_db", cstr(userdata), pathsep);
    check(os_tryuntil_deletefiles(cstr(path), "*"));
    bassignformat(path, "%s%sno_main_db%sno_main_db_index.db", cstr(userdata), pathsep, pathsep);
    TestTrue(!os_file_exists(cstr(path)));

    /* incomplete group that does have .db files with hexadecimal names, so we'll get the latest */
    bassignformat(path, "%s%sno_main_db_hex%sreadytoupload",
        cstr(userdata), pathsep, pathsep);
    TestTrue(os_create_dirs(cstr(path)));
    bassignformat(path, "%s%sno_main_db_hex%sreadytoupload%s00001_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "1", "wb"));
    bassignformat(path, "%s%sno_main_db_hex%sreadytoupload%s00002_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "2", "wb"));
    bassignformat(path, "%s%sno_main_db_hex%sreadytoupload%s0000a_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "a", "wb"));
    bassignformat(path, "%s%sno_main_db_hex%sreadytoupload%s0000e_index.db",
        cstr(userdata), pathsep, pathsep, pathsep);
    check(sv_file_writefile(cstr(path), "e", "wb"));
    bassignformat(path, "%s%sno_main_db_hex", cstr(userdata), pathsep);
    check(os_tryuntil_deletefiles(cstr(path), "*"));
    bassignformat(path, "%s%sno_main_db_hex%sno_main_db_hex_index.db",
        cstr(userdata), pathsep, pathsep);
    TestTrue(!os_file_exists(cstr(path)));

    /* run find-groups */
    fake_app.backup_group_names = bstrlist_open();
    fake_app.path_app_data = bfromcstr(dir);
    check(svdp_application_findgroupnames(&fake_app));
    bstrlist_sort(fake_app.backup_group_names);
    TestEqn(3, fake_app.backup_group_names->qty);
    TestEqs("expected", bstrlist_view(fake_app.backup_group_names, 0));
    TestEqs("no_main_db", bstrlist_view(fake_app.backup_group_names, 1));
    TestEqs("no_main_db_hex", bstrlist_view(fake_app.backup_group_names, 2));

    /* we should have brought in the latest db file */
    bassignformat(path, "%s%sno_main_db%sno_main_db_index.db",
        cstr(userdata), pathsep, pathsep);
    check(sv_file_readfile(cstr(path), contents));
    TestEqs("i3", cstr(contents));
    bassignformat(path, "%s%sno_main_db_hex%sno_main_db_hex_index.db",
        cstr(userdata), pathsep, pathsep);
    check(sv_file_readfile(cstr(path), contents));
    TestEqs("e", cstr(contents));
    svdp_application_close(&fake_app);
cleanup:
    bdestroy(userdata);
    bdestroy(path);
    bdestroy(contents);
    return currenterr;
}

void run_sv_tests(bool run_from_ui)
{
    sv_result currenterr = {};
    extern uint32_t sleep_between_tries;
    sleep_between_tries = 1;

    bstring dir = os_get_tmpdir("tmpglacial_backup");
    bool is_not_empty = os_dir_exists(cstr(dir)) && !os_dir_empty(cstr(dir));
    bstring testdir = os_make_subdir(cstr(dir), "tests");
    bstring testdirfileio = os_make_subdir(cstr(testdir), "fileio");
    bstring testdirdb = os_make_subdir(cstr(testdir), "testdb");
    bstring testdirgroup = os_make_subdir(cstr(testdir), "testgroup");
    bstring testdirfindgroup = os_make_subdir(cstr(testdir), "testfindgroup");
    bstring testdircreatearchives = os_make_subdir(cstr(testdir), "createarchives");
    bstring was_restrict_write_access = bstrcpy(restrict_write_access);
    bassigncstr(restrict_write_access, "");
    if (run_from_ui && is_not_empty)
    {
        printf("Please remove the contents of \n%s\nbefore running tests.", cstr(dir));
        alertdialog("");
        goto cleanup;
    }

    puts("running tests...");
    check(run_utils_tests());
    check(test_archiver_file_extension_info());
    check(test_util_audio_tags(cstr(testdirfileio)));
    check(test_audio_tags_hashing(cstr(testdirfileio)));
    check(test_db_basics(cstr(testdirdb)));
    check(test_db_rows(cstr(testdirdb)));
    check(test_create_archives(cstr(testdircreatearchives)));
    check(test_archiver_tar(cstr(testdirgroup)));
    check(test_operations(cstr(testdirgroup)));
    check(test_find_groups(cstr(testdirfindgroup)));

    if (run_from_ui)
    {
        alertdialog("Tests complete. GlacialBackup will now exit.");
        exit(0);
    }

cleanup:
    sleep_between_tries = 250;
    bassign(restrict_write_access, was_restrict_write_access);
    bdestroy(was_restrict_write_access);
    bdestroy(dir);
    bdestroy(testdir);
    bdestroy(testdirfileio);
    bdestroy(testdirdb);
    bdestroy(testdirgroup);
    bdestroy(testdirfindgroup);
    bdestroy(testdircreatearchives);
    check_warn(currenterr, "unit test failed.", exit_on_err);
    puts("tests complete.");
}
