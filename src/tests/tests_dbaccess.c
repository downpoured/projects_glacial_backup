/*
tests_dbaccess.c

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

check_result svdb_runsql(svdb_db *self, const char *sql, int lensql);
check_result svdb_connection_openhandle(svdb_db *self);

SV_BEGIN_TEST_SUITE(tests_open_db_connection)
{
    SV_TEST("schema version should be set to 1")
    {
        uint32_t version = 0;
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(
            bstring, path, bformat("%s%s\xED\x95\x9C.db", tempdir, pathsep));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_getint(&db, s_and_len("SchemaVersion"), &version));
        check(svdb_disconnect(&db));
        TestEqn(1, version);
    }

    SV_TEST("reject an unsupported schema version")
    {
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(
            bstring, path, bformat("%s%s\xED\x95\x9C.db", tempdir, pathsep));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_setint(&db, s_and_len("SchemaVersion"), 2));
        check(svdb_disconnect(&db));
        expect_err_with_message(svdb_connect(&db, cstr(path)), "future version");
        check(svdb_disconnect(&db));
    }

    SV_TEST("reject missing schema version")
    {
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_runsql(&db, s_and_len("DELETE FROM TblProperties")));
        check(svdb_disconnect(&db));
        expect_err_with_message(svdb_connect(&db, cstr(path)), "future version");
        check(svdb_disconnect(&db));
    }

    SV_TEST("recover from 0 byte db")
    {
        uint32_t version = 0;
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        check(sv_file_writefile(cstr(path), "", "wb"));
        quiet_warnings(true);
        check(svdb_connect(&db, cstr(path)));
        quiet_warnings(false);
        check(svdb_getint(&db, s_and_len("SchemaVersion"), &version));
        check(svdb_disconnect(&db));
        TestEqn(1, version);
    }

    SV_TEST("recover from valid db with no schema")
    {
        uint32_t version = 0;
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        db.path = bstrcpy(path);
        check(svdb_connection_openhandle(&db));
        check(svdb_disconnect(&db));
        quiet_warnings(true);
        check(svdb_connect(&db, cstr(path)));
        quiet_warnings(false);
        check(svdb_getint(&db, s_and_len("SchemaVersion"), &version));
        check(svdb_disconnect(&db));
        TestEqn(1, version);
    }

    SV_TEST("add rows, read from rows")
    {
        uint32_t int_got = 99, int_notset = 99;
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN(bstring, s_got);
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_setstr(&db, s_and_len("SetStr"), "abcde"));
        check(svdb_setint(&db, s_and_len("SetInt"), 123));
        check(svdb_getstr(&db, s_and_len("SetStr"), s_got));
        check(svdb_getint(&db, s_and_len("SetInt"), &int_got));
        check(svdb_getint(&db, s_and_len("OtherInt"), &int_notset));
        check(svdb_disconnect(&db));
        TestEqs("abcde", cstr(s_got));
        TestEqn(123, int_got);
        TestEqn(0, int_notset);
    }

    SV_TEST("add rows, disconnect, reconnect, read from rows")
    {
        uint32_t int_got = 99, int_notset = 99;
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN(bstring, s_got);
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_setstr(&db, s_and_len("SetStr"), "abcde"));
        check(svdb_setint(&db, s_and_len("SetInt"), 123));
        check(svdb_disconnect(&db));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_getstr(&db, s_and_len("SetStr"), s_got));
        check(svdb_getint(&db, s_and_len("SetInt"), &int_got));
        check(svdb_getint(&db, s_and_len("OtherInt"), &int_notset));
        check(svdb_disconnect(&db));
        TestEqs("abcde", cstr(s_got));
        TestEqn(123, int_got);
        TestEqn(0, int_notset);
    }

    SV_TEST("inserted data not kept if transaction is rolled back")
    {
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(svdb_txn, txn, {});
        uint32_t int_got_1 = 99, int_got_2 = 99, int_got_3 = 99;
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_setint(&db, s_and_len("Int1"), 123));
        check(svdb_txn_open(&txn, &db));
        check(svdb_setint(&db, s_and_len("Int2"), 456));
        check(svdb_txn_rollback(&txn, &db));
        check(svdb_txn_open(&txn, &db));
        check(svdb_setint(&db, s_and_len("Int3"), 789));
        check(svdb_txn_commit(&txn, &db));
        check(svdb_getint(&db, s_and_len("Int1"), &int_got_1));
        check(svdb_getint(&db, s_and_len("Int2"), &int_got_2));
        check(svdb_getint(&db, s_and_len("Int3"), &int_got_3));
        check(svdb_disconnect(&db));
        TestEqn(123, int_got_1);
        TestEqn(0, int_got_2);
        TestEqn(789, int_got_3);
    }

    SV_TEST("updated data not kept if transaction is rolled back")
    {
        TEST_OPEN_EX(svdb_db, db, {});
        TEST_OPEN_EX(svdb_txn, txn, {});
        uint32_t int_got = 99;
        TEST_OPEN_EX(bstring, path,
            bformat("%s%s%s.db", tempdir, pathsep, currentcontext));
        check(svdb_connect(&db, cstr(path)));
        check(svdb_setint(&db, s_and_len("SetInt"), 123));
        check(svdb_txn_open(&txn, &db));
        check(svdb_setint(&db, s_and_len("SetInt"), 456));
        check(svdb_txn_commit(&txn, &db));
        check(svdb_txn_open(&txn, &db));
        check(svdb_setint(&db, s_and_len("SetInt"), 789));
        check(svdb_txn_rollback(&txn, &db));
        check(svdb_getint(&db, s_and_len("SetInt"), &int_got));
        check(svdb_disconnect(&db));
        TestEqn(456, int_got);
    }
}
SV_END_TEST_SUITE()
