/*
whole_db.c

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

check_result test_tbl_properties(svdb_db *db);
check_result test_tbl_fileslist(svdb_db *db);
check_result test_tbl_collections(svdb_db *db);
check_result test_tbl_contents(svdb_db *db);

SV_BEGIN_TEST_SUITE(whole_tests_db)
{
    SV_TEST_() {
        svdb_db db = {};
        bstring path = bformat("%s%srows.db", tempdir, pathsep);
        check(svdb_connect(&db, cstr(path)));
        check(test_tbl_properties(&db));
        check(test_tbl_fileslist(&db));
        check(test_tbl_collections(&db));
        check(test_tbl_contents(&db));
        check(svdb_disconnect(&db));
        svdb_close(&db);
        bdestroy(path);
    }
}
SV_END_TEST_SUITE()

check_result test_tbl_properties(svdb_db *db)
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
    check(svdb_getint(db, s_and_len("NonExistInt"), &int_got));
    check(svdb_getstr(db, s_and_len("NonExistStr"), str_got));
    check(svdb_getlist(db, s_and_len("NonExistStrList"), list_got));
    TestEqn(0, int_got);
    TestEqs("", cstr(str_got));
    TestEqList("", list_got);

    /* item values can be replaced */
    check(svdb_setint(db, s_and_len("TestInt"), 123));
    check(svdb_setint(db, s_and_len("TestInt"), 456));
    check(svdb_setstr(db, s_and_len("TestStr"), "hello"));
    check(svdb_setstr(db, s_and_len("TestStr"), "otherstring"));
    check(svdb_getint(db, s_and_len("TestInt"), &int_got));
    check(svdb_getstr(db, s_and_len("TestStr"), str_got));
    TestEqn(456, int_got);
    TestEqs("otherstring", cstr(str_got));

    /* there should be 3 rows now */
    uint64_t countrows = 0;
    check(svdb_propgetcount(db, &countrows));
    TestEqn(3, countrows);

    /* set and get list with 3 items */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "item1");
    bstrlist_appendcstr(list, "item2");
    bstrlist_appendcstr(list, "item3");
    check(svdb_setlist(db, s_and_len("TestList"), list));
    bstrlist_clear(list_got);
    check(svdb_getlist(db, s_and_len("TestList"), list_got));
    TestEqList("item1|item2|item3", list_got);

    /* set and get list with 1 item */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "item1");
    check(svdb_setlist(db, s_and_len("TestList"), list));
    bstrlist_clear(list_got);
    check(svdb_getlist(db, s_and_len("TestList"), list_got));
    TestEqList("item1", list_got);

    /* set and get list with 0 items */
    bstrlist_clear(list);
    check(svdb_setlist(db, s_and_len("TestList"), list));
    bstrlist_clear(list_got);
    bstrlist_appendcstr(list_got, "text");
    check(svdb_getlist(db, s_and_len("TestList"), list_got));
    TestEqn(0, list_got->qty);

    /* set and get list with item with pipe char */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "pipechar|isok");
    check(svdb_setlist(db, s_and_len("TestList"), list));
    bstrlist_clear(list_got);
    check(svdb_getlist(db, s_and_len("TestList"), list_got));
    TestEqn(1, list_got->qty);
    TestEqs("pipechar|isok", blist_view(list_got, 0));

    /* setting invalid string into list should fail */
    bstrlist_clear(list);
    bstrlist_appendcstr(list, "item1");
    bstrlist_appendcstr(list, "invalid\x1einvalid");
    expect_err_with_message(svdb_setlist(db,
        s_and_len("TestList"), list), "cannot include");

    /* there should only be 4 rows now */
    check(svdb_propgetcount(db, &countrows));
    TestEqn(4, countrows);
    check(svdb_txn_rollback(&txn, db));
cleanup:
    svdb_txn_close(&txn, db);
    bdestroy(str_got);
    bstrlist_close(list);
    bstrlist_close(list_got);
    return currenterr;
}

check_result files_addallrowstolistofstrings(void *context,
    const sv_file_row *row, const bstring path, const bstring permissions)
{
    bstrlist *list = (bstrlist *)context;
    bstring s = bstring_open();
    svdb_files_row_string(row, cstr(path), cstr(permissions), s);
    bstrlist_append(list, s);
    bdestroy(s);
    return OK;
}

check_result contents_addallrowstolistofstrings(void *context,
    const sv_content_row *row)
{
    bstrlist *list = (bstrlist *)context;
    bstring s = bstring_open();
    svdb_contents_row_string(row, s);
    bstrlist_append(list, s);
    bdestroy(s);
    return OK;
}

check_result test_tbl_fileslist(svdb_db *db)
{
    sv_result currenterr = {};
    bstring path1 = bstring_open();
    bstring path2 = bstring_open();
    bstring path3 = bstring_open();
    bstring permissions = bstring_open();
    bstring srowexpect = bstring_open();
    bstring srowgot = bstring_open();
    svdb_txn txn = {};

    sv_file_row row1 = { 0,
        5 /*contents_id*/, 5000ULL * 1024 * 1024, /*contents_length*/
        555 /* last_write_time*/,
        1111 /*most_recent_collection*/, sv_filerowstatus_complete };
    sv_file_row row2 = { 0,
        6 /*contents_id*/, 6000ULL * 1024 * 1024, /*contents_length*/
        666 /* last_write_time*/,
        1111 /*most_recent_collection*/, sv_filerowstatus_queued };
    sv_file_row row3 = { 0,
        7 /*contents_id*/, 7000ULL * 1024 * 1024, /*contents_length*/
        777 /* last_write_time*/,
        1000 /*most_recent_collection*/, sv_filerowstatus_queued };

    check(svdb_txn_open(&txn, db));
    uint64_t rowid0;

    { /* unique index should prevent duplicate paths. */
        bassigncstr(path1, "/test/path/1");
        bassigncstr(path2, islinux ? "/test/path/1" : "/TEST/PATH/1");
        check(svdb_filesinsert(db, path1, 8888,
            sv_filerowstatus_queued, &rowid0));
        expect_err_with_message(svdb_filesinsert(db, path2, 9999,
            sv_filerowstatus_queued, NULL), "UNIQUE constraint failed");
        uint64_t filescount = 0;
        check(svdb_filescount(db, &filescount));
        TestEqn(1, filescount);
    }
    { /* add 3 rows to the database */
        TEST_OPEN_EX(bstring, f1, bfromcstr("/test/addrows/1"));
        TEST_OPEN_EX(bstring, f2, bfromcstr("/test/addrows/2"));
        TEST_OPEN_EX(bstring, f3, bfromcstr("/test/addrows/3"));
        bassigncstr(permissions, "pm1");
        check(svdb_filesinsert(db, f1, 1, sv_filerowstatus_queued, &row1.id));
        check(svdb_filesupdate(db, &row1, permissions));
        bassigncstr(permissions, "pm2");
        check(svdb_filesinsert(db, f2, 2, sv_filerowstatus_queued, &row2.id));
        check(svdb_filesupdate(db, &row2, permissions));
        bassigncstr(permissions, "pm3");
        check(svdb_filesinsert(db, f3, 3, sv_filerowstatus_queued, &row3.id));
        check(svdb_filesupdate(db, &row3, permissions));
    }
    { /* get by path */
        sv_file_row rowgot = { 0 };
        /* I'd use memcmp to compare, but it does not work because of
            padding after the struct. */
        bassigncstr(path1, "/test/addrows/1");
        bassigncstr(path2, "/test/addrows/2");
        bassigncstr(path3, "/test/addrows/3");
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
        sv_file_row rowgot = { 0 };
        memset(&rowgot, 1, sizeof(rowgot));
        bassigncstr(srowexpect, "/test/addrows/");
        check(svdb_filesbypath(db, srowexpect, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* try to update nonexistant row */
        sv_file_row rowtest = { 1234 /* bogus rowid */, 5, 5, 5 };
        expect_err_with_message(svdb_filesupdate(db, &rowtest, permissions),
            "changed no rows");
    }
    { /* query status less than */
        bstrlist *list = bstrlist_open();
        check(svdb_files_iter(db, sv_makestatus(1111,
            sv_filerowstatus_complete),
            list, &files_addallrowstolistofstrings));
        TestEqn(list->qty, 2);
        svdb_files_row_string(&row2, cstr(path2), "pm2", srowexpect);
        TestEqs(cstr(srowexpect), blist_view(list, 0));
        svdb_files_row_string(&row3, cstr(path3), "pm3", srowexpect);
        TestEqs(cstr(srowexpect), blist_view(list, 1));
        bstrlist_close(list);
    }
    { /* query status less than, matches no rows */
        bstrlist *list = bstrlist_open();
        check(svdb_files_iter(db, sv_makestatus(1, sv_filerowstatus_complete),
            list, &files_addallrowstolistofstrings));
        TestEqn(list->qty, 0);
        bstrlist_close(list);
    }
    { /* query status less than, matches all rows */
        bstrlist *list = bstrlist_open();
        check(svdb_files_iter(db, svdb_all_files,
            list, &files_addallrowstolistofstrings));
        TestEqn(list->qty, 4);
        bstrlist_close(list);
    }
    { /* ok to batch delete 0 rows */
        uint64_t filescount = 0;
        check(svdb_filescount(db, &filescount));
        TestEqn(4, filescount);
        sv_array arr = sv_array_open_u64();
        check(svdb_files_delete(db, &arr, 0));
        check(svdb_filescount(db, &filescount));
        TestEqn(4, filescount);
        sv_array_close(&arr);
    }
    { /* batch delete */
        uint64_t filescount = 0;
        check(svdb_filescount(db, &filescount));
        TestEqn(4, filescount);
        sv_array arr = sv_array_open_u64();
        sv_array_add64u(&arr, rowid0);
        sv_array_add64u(&arr, row2.id);
        sv_array_add64u(&arr, row3.id);

        /* deleting 3 rows with batchsize 2 covers both
            the full-batch and partial-batch cases */
        check(svdb_files_delete(db, &arr, 2));

        /* row 1 should still exist */
        check(svdb_filescount(db, &filescount));
        TestEqn(1, filescount);
        sv_file_row rowgot = { 0 };
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
        TestEqs("contents_length=5, contents_id=5242880000, "
            "last_write_time=555, flags=flags, most_recent_collection=1111, "
            "e_status=3, id=2path", cstr(srowexpect));
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

check_result test_tbl_collections(svdb_db *db)
{
    sv_result currenterr = {};
    bstring srowexpected = bstring_open();
    bstring srowgot = bstring_open();
    svdb_txn txn = {};

    sv_collection_row row1 = { 0, 0 /*time*/, 11 /*time_finished*/,
        111 /* count_total_files */, 1111 /*count_new_contents*/,
        1000ULL * 1024 * 1024 /* count_new_contents_bytes*/ };
    sv_collection_row row2 = { 0, 0 /*time*/, 22 /*time_finished*/,
        222 /* count_total_files */, 2222 /*count_new_contents*/,
        2000ULL * 1024 * 1024 /* count_new_contents_bytes*/ };
    sv_collection_row row3 = { 0, 0 /*time*/, 33 /*time_finished*/,
        333 /* count_total_files */, 3333 /*count_new_contents*/,
        3000ULL * 1024 * 1024 /* count_new_contents_bytes*/ };

    check(svdb_txn_open(&txn, db));

    { /* get latest on empty db */
        uint64_t rowid_got = UINT64_MAX;
        check(svdb_collectiongetlast(db, &rowid_got));
        TestEqn(0, rowid_got);
    }
    { /* read collections on empty db */
        sv_array arr = sv_array_open(sizeof32u(sv_collection_row), 0);
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
        sv_collection_row rowtest = { row1.id, 9999 /* change timestarted */ };
        quiet_warnings(true);
        sv_result res = svdb_collectionupdate(db, &rowtest);
        TestTrue(res.code != 0);
        TestTrue(s_contains(cstr(res.msg), "cannot set time started"));
        quiet_warnings(false);
    }
    { /* try to update non-existant row */
        sv_collection_row rowtest = { 1234 /* bogus rowid */, 0, 0 };
        quiet_warnings(true);
        sv_result res = svdb_collectionupdate(db, &rowtest);
        TestTrue(res.code != 0);
        TestTrue(s_contains(cstr(res.msg), "changed no rows"));
        sv_result_close(&res);
        quiet_warnings(false);
    }
    { /* get latest id */
        uint64_t rowid_got = 0;
        check(svdb_collectiongetlast(db, &rowid_got));
        TestEqn(row3.id, rowid_got);
    }
    { /* read collections */
        sv_array arr = sv_array_open(sizeof32u(sv_collection_row), 0);
        check(svdb_collectionsget(db, &arr, true));
        TestEqn(3, arr.length);
        /* remember that the results should be sorted in reverse order */
        sv_collection_row *row_got = (sv_collection_row *)sv_array_at(&arr, 0);
        svdb_collectiontostring(&row3, true, true, srowexpected);
        svdb_collectiontostring(row_got, true, true, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        row_got = (sv_collection_row *)sv_array_at(&arr, 1);
        svdb_collectiontostring(&row2, true, true, srowexpected);
        svdb_collectiontostring(row_got, true, true, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        row_got = (sv_collection_row *)sv_array_at(&arr, 2);
        svdb_collectiontostring(&row1, true, true, srowexpected);
        svdb_collectiontostring(row_got, true, true, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
        sv_array_close(&arr);
    }
    { /* test row-to-string */
        svdb_collectiontostring(&row1, true, true, srowexpected);
        TestEqs("time=5, time_finished=11, count_total_files=111, "
            "count_new_contents=1111, count_new_contents_bytes="
            "1048576000, id=1", cstr(srowexpected));
    }

    check(svdb_txn_rollback(&txn, db));
cleanup:
    bdestroy(srowexpected);
    bdestroy(srowgot);
    svdb_txn_close(&txn, db);
    return currenterr;
}

check_result test_tbl_contents(svdb_db *db)
{
    sv_result currenterr = {};
    bstring srowexpected = bstring_open();
    bstring srowgot = bstring_open();
    svdb_txn txn = {};

    sv_content_row rowgot = { 0 };
    sv_content_row row1 = { 0,
        1000ULL * 1024 * 1024 /*contents_length*/,
        1001ULL * 1024 * 1024 /* compressed_contents_length */,
        1 /* most_recent_collection */, 11 /* original_collection */,
        111 /* archivenumber */,
        { { 0x1111111111111111ULL, 0x2222222222222222ULL,
        0x3333333333333333ULL, 0x4444444444444444ULL } }, /* hash */
        0x11111111 /* crc32 */ };
    sv_content_row row2 = { 0,
        2000ULL * 1024 * 1024 /*contents_length*/,
        2002ULL * 1024 * 1024 /* compressed_contents_length */,
        2 /* most_recent_collection */, 22 /*original_collection*/,
        222 /* archivenumber*/,
        { { 0x2222222222222222ULL, 0x3333333333333333ULL,
        0x4444444444444444ULL, 0x5555555555555555ULL, } }, /*hash*/
        0x22222222 /* crc32 */ };
    sv_content_row row3 = { 0,
        3000ULL * 1024 * 1024 /*contents_length*/,
        3003ULL * 1024 * 1024 /* compressed_contents_length */,
        3 /* most_recent_collection */, 33 /*original_collection*/,
        333 /* archivenumber*/,
        { { 0x3333333333333333ULL, 0x4444444444444444ULL,
        0x5555555555555555ULL, 0x6666666666666666ULL } }, /*hash*/
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
        hash256 hash = { { 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555555ULL } };
        check(svdb_contentsbyhash(db, &hash, 2000ULL * 1024 * 1024, &rowgot));
        svdb_contents_row_string(&row2, srowexpected);
        svdb_contents_row_string(&rowgot, srowgot);
        TestEqs(cstr(srowexpected), cstr(srowgot));
    }
    { /* right hash but wrong length */
        memset(&rowgot, 1, sizeof(rowgot));
        hash256 hash = { { 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555555ULL } };
        check(svdb_contentsbyhash(db, &hash, 1234, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* right length but wrong hash */
        memset(&rowgot, 1, sizeof(rowgot));
        hash256 hash = { { 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555556ULL /* note final 6 */ } };
        check(svdb_contentsbyhash(db, &hash, 2000ULL * 1024 * 1024, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* wrong length and wrong hash */
        memset(&rowgot, 1, sizeof(rowgot));
        hash256 hash = { { 0x2222222222222222ULL, 0x3333333333333333ULL,
            0x4444444444444444ULL, 0x5555555555555556ULL /* note final 6 */ } };
        check(svdb_contentsbyhash(db, &hash, 1234, &rowgot));
        TestEqn(0, rowgot.id);
    }
    { /* test row iteration */
        bstrlist *list = bstrlist_open();
        check(svdb_contentsiter(db, list, &contents_addallrowstolistofstrings));
        TestEqn(3, list->qty);
        svdb_contents_row_string(&row1, srowexpected);
        TestEqs(cstr(srowexpected), blist_view(list, 0));
        svdb_contents_row_string(&row2, srowexpected);
        TestEqs(cstr(srowexpected), blist_view(list, 1));
        svdb_contents_row_string(&row3, srowexpected);
        TestEqs(cstr(srowexpected), blist_view(list, 2));
        bstrlist_close(list);
    }
    { /* test row-to-string */
        svdb_contents_row_string(&row1, srowexpected);
        TestEqs("hash=1111111111111111 2222222222222222 3333333333333333 "
            "4444444444444444, crc32=11111111, contents_length=1048576000,"
            "1049624576, most_recent_collection=1, original_collection=11, "
            "archivenumber=111, id=1", cstr(srowexpected));
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
        quiet_warnings(true);
        sv_result res = svdb_contents_setlastreferenced(
            db, 1234 /*bogus rowid*/, 99);
        TestTrue(res.code != 0);
        TestTrue(s_contains(cstr(res.msg), "changed no rows"));
        sv_result_close(&res);
        quiet_warnings(false);
    }
    { /* test batch delete of nothing */
        uint64_t count = 0;
        check(svdb_contentscount(db, &count));
        TestEqn(3, count);
        sv_array arr = sv_array_open_u64();
        check(svdb_contents_bulk_delete(db, &arr, 0));
        check(svdb_contentscount(db, &count));
        TestEqn(3, count);
        sv_array_close(&arr);
    }
    { /* test batch delete of nonexistant ids */
        uint64_t count = 0;
        check(svdb_contentscount(db, &count));
        TestEqn(3, count);
        sv_array arr = sv_array_open_u64();
        sv_array_add64u(&arr, 12345 /* bogus rowid */);
        sv_array_add64u(&arr, 123456 /* bogus rowid */);
        check(svdb_contents_bulk_delete(db, &arr, 0));
        check(svdb_contentscount(db, &count));
        TestEqn(3, count);
        sv_array_close(&arr);
    }
    { /* test batch delete */
        uint64_t count = 0;
        check(svdb_contentscount(db, &count));
        TestEqn(3, count);
        sv_array arr = sv_array_open_u64();
        sv_array_add64u(&arr, row1.id);
        sv_array_add64u(&arr, row3.id);
        check(svdb_contents_bulk_delete(db, &arr, 2));

        /* row 2 should still exist */
        check(svdb_contentscount(db, &count));
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

