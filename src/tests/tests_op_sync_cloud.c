/*
tests_op_sync_cloud.c

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

SV_BEGIN_TEST_SUITE(tests_sync_cloud_standalone)
{
    const char *jsonsinglequote = "{\n" \
        "    'VaultList': [\n" \
        "        {\n" \
        "            'VaultARN': 'arn:aws:glacier:us-east-1:123456789:vaults/the_vault_name',\n" \
        "            'VaultName': 'the vault name',\n" \
        "            'CreationDate': '2016-08-27T11:12:13.111Z',\n" \
        "            'Duplicate': 'abc',\n" \
        "            'Duplicate': 'def',\n" \
        "            'NumberOfArchives': 0,\n" \
        "            'NoClosing': 'noclose\n" \
        "        }\n" \
        "    ]\n" \
        "}\n";
    TEST_OPEN_EX(bstring, result, bfromcstr(""));
    TEST_OPEN_EX(bstring, json, bfromcstr(jsonsinglequote));
    bstr_replaceall(json, "'", "\"");

    SV_TEST("get from json, failure cases")
    {
        expect_err_with_message(roughselectstrfromjson(cstr(json), "fghfghfg", result), "key not found");
        expect_err_with_message(roughselectstrfromjson(cstr(json), "Duplicate", result), "more than one");
        expect_err_with_message(roughselectstrfromjson(cstr(json), "NoClosing", result), "quote not found");
    }

    SV_TEST("get from json, success cases")
    {
        check(roughselectstrfromjson(cstr(json), "VaultName", result));
        TestEqs("the vault name", cstr(result));
        check(roughselectstrfromjson(cstr(json), "VaultARN", result));
        TestEqs("arn:aws:glacier:us-east-1:123456789:vaults/the_vault_name", cstr(result));
    }

    SV_TEST("localpath_to_cloud_path windows-style")
    {
        TEST_OPEN_EX(bstring, s1, localpath_to_cloud_path("C:\\dir1\\dir2", "C:\\dir1\\dir2\\file.dat"));
        TestEqs("glacial_backup/dir2/file.dat", cstr(s1));
        TEST_OPEN_EX(bstring, s2, localpath_to_cloud_path("C:\\dir1\\dir2", "C:\\dir1\\dir2\\dir3\\file.dat"));
        TestEqs("glacial_backup/dir2/dir3/file.dat", cstr(s2));
        TEST_OPEN_EX(bstring, s3, localpath_to_cloud_path("C:\\dir1\\dir2", "C:\\dir1\\dir2\\dir3\\dir4\\file.dat"));
        TestEqs("glacial_backup/dir2/dir3/dir4/file.dat", cstr(s3));
        TEST_OPEN_EX(bstring, s4, localpath_to_cloud_path("C:\\dir1\\dir2", "C:\\dir1\\file.dat"));
        TestEqs("", cstr(s4));
        TEST_OPEN_EX(bstring, s5, localpath_to_cloud_path("C:\\dir1\\dir2", "file.dat"));
        TestEqs("", cstr(s5));
    }

    SV_TEST("localpath_to_cloud_path unix-style")
    {
        TEST_OPEN_EX(bstring, s1, localpath_to_cloud_path("/dir1/dir2", "/dir1/dir2/file.dat"));
        TestEqs("glacial_backup/dir2/file.dat", cstr(s1));
        TEST_OPEN_EX(bstring, s2, localpath_to_cloud_path("/dir1/dir2", "/dir1/dir2/dir3/file.dat"));
        TestEqs("glacial_backup/dir2/dir3/file.dat", cstr(s2));
        TEST_OPEN_EX(bstring, s3, localpath_to_cloud_path("/dir1/dir2", "/dir1/dir2/dir3/dir4/file.dat"));
        TestEqs("glacial_backup/dir2/dir3/dir4/file.dat", cstr(s3));
        TEST_OPEN_EX(bstring, s4, localpath_to_cloud_path("/dir1/dir2", "/dir1/file.dat"));
        TestEqs("", cstr(s4));
        TEST_OPEN_EX(bstring, s5, localpath_to_cloud_path("/dir1/dir2", "file.dat"));
        TestEqs("", cstr(s5));
    }
    
}
SV_END_TEST_SUITE()

/* let tests access this internal function */
check_result svdb_runsql(svdb_db *self, const char *sql, int len);

check_result test_sync_cloud_db_vaults(
    svdb_db *db) {
    sv_result currenterr = {};
    check(svdb_runsql(db, s_and_len("DELETE FROM TblKnownVaults WHERE 1")));

    /* disallow duplicate names */
    check(svdb_knownvaults_insert(db, "reg1", "nm1", "awsname1", "arn1"));
    expect_err_with_message(svdb_knownvaults_insert(db, "regdiff", "nm1", "awsdiff", "arndiff"), "");

    /* add the rest of the rows */
    check(svdb_knownvaults_insert(db, "reg2", "nm2", "awsname2", "arn2"));
    check(svdb_knownvaults_insert(db, "reg3", "nm3", "awsname3", "arn3"));

    /* query them */
    TEST_OPEN_EX(bstrlist *, regions, bstrlist_open());
    TEST_OPEN_EX(bstrlist *, names, bstrlist_open());
    TEST_OPEN_EX(bstrlist *, awsnames, bstrlist_open());
    TEST_OPEN_EX(bstrlist *, arns, bstrlist_open());
    check(svdb_knownvaults_get(db, regions, names, awsnames, arns));
    TEST_OPEN_EX(bstring, allregions, bjoin_static(regions, "|"));
    TEST_OPEN_EX(bstring, allnames, bjoin_static(names, "|"));
    TEST_OPEN_EX(bstring, allawsnames, bjoin_static(awsnames, "|"));
    TEST_OPEN_EX(bstring, allarns, bjoin_static(arns, "|"));
    TestEqs("reg1|reg2|reg3", cstr(allregions));
    TestEqs("nm1|nm2|nm3", cstr(allnames));
    TestEqs("awsname1|awsname2|awsname3", cstr(allawsnames));
    TestEqs("arn1|arn2|arn3", cstr(allarns));
cleanup:
    return currenterr;
}

check_result expect_vaultarchives_row(svdb_db *db, const char *path, uint64_t knownvaultid, uint64_t size, uint64_t crc32, uint64_t modtime) {
    uint64_t outsize = 0, outcrc32 = 0, outmodtime = 0;
    check_result ret = svdb_vaultarchives_bypath(db, path, knownvaultid, &outsize, &outcrc32, &outmodtime);
    if (ret.code)
    {
        return ret;
    }
    else
    {
        TestEqn(size, outsize);
        TestEqn(crc32, outcrc32);
        TestEqn(modtime, outmodtime);
        return OK;
    }
}

check_result test_sync_cloud_db_vaultarchives(
    svdb_db *db) {
    sv_result currenterr = {};
    check(svdb_runsql(db, s_and_len("DELETE FROM TblKnownVaultArchives WHERE 1")));

    /* disallow duplicate paths */
    check(svdb_vaultarchives_insert(db, "path1", 11, "awsid1", 1002, 1003, 1004));
    expect_err_with_message(svdb_vaultarchives_insert(db, "path1", 11, "awsid1diff", 2002, 2003, 2004), "");

    /* insert rows */
    check(svdb_vaultarchives_insert(db, "path1", 22, "awsid2", 10002, 10003, 10004));
    check(svdb_vaultarchives_insert(db, "path2", 22, "awsid2", 20002, 20003, 20004));
    check(svdb_vaultarchives_insert(db, "path3", 22, "awsid3", 30002, 30003, 30004));
    check(svdb_vaultarchives_insert(db, "path4", 22, "awsid4", 40002, 40003, 40004));
    
    /* query by path needs the right vaultid */
    check(expect_vaultarchives_row(db, "path1", 22, 10002, 10003, 10004));
    check(expect_vaultarchives_row(db, "path1", 11, 1002, 1003, 1004));
    check(expect_vaultarchives_row(db, "path1", 33, 0, 0, 0));

    /* query by paths */
    check(expect_vaultarchives_row(db, "path3", 22, 30002, 30003, 30004));
    check(expect_vaultarchives_row(db, "path2", 22, 20002, 20003, 20004));
    check(expect_vaultarchives_row(db, "path0", 22, 0, 0, 0));

    /* query by path for not-exist vault */
    check(expect_vaultarchives_row(db, "path0", 33, 0, 0, 0));

    /* ok to delete what doesn't exist */
    check(svdb_vaultarchives_delbypath(db, "path2--", 11));
    check(svdb_vaultarchives_delbypath(db, "path2", 33));
    check(svdb_vaultarchives_delbypath(db, "path0", 33));
    check(expect_vaultarchives_row(db, "path2", 22, 20002, 20003, 20004));

    /* delete by path for p1 */
    check(svdb_vaultarchives_delbypath(db, "path1", 22));

    /* query by path for p1 should not exist */
    check(expect_vaultarchives_row(db, "path1", 11, 1002, 1003, 1004));
    check(expect_vaultarchives_row(db, "path1", 22, 0, 0, 0));
    check(expect_vaultarchives_row(db, "path1", 33, 0, 0, 0));

    /* insert replacement p1 */
    check(svdb_vaultarchives_insert(db, "path1", 22, "awsid2", 90002, 90003, 90004));
    check(expect_vaultarchives_row(db, "path1", 11, 1002, 1003, 1004));
    check(expect_vaultarchives_row(db, "path1", 22, 90002, 90003, 90004));
    check(expect_vaultarchives_row(db, "path1", 33, 0, 0, 0));

    /* delete by path for p2 */
    check(svdb_vaultarchives_delbypath(db, "path2", 22));

    /* query by path for p2 should not exist */
    check(expect_vaultarchives_row(db, "path2", 22, 0, 0, 0));
    check(expect_vaultarchives_row(db, "path3", 22, 30002, 30003, 30004));

    /* insert replacement p2 */
    check(svdb_vaultarchives_insert(db, "path2", 22, "awsid2", 990002, 990003, 990004));
    check(expect_vaultarchives_row(db, "path1", 22, 90002, 90003, 90004));
    check(expect_vaultarchives_row(db, "path2", 22, 990002, 990003, 990004));
    check(expect_vaultarchives_row(db, "path3", 22, 30002, 30003, 30004));
cleanup:
    return currenterr;
}

check_result test_operations_sync_cloud(
    const sv_app *app, sv_group *grp, svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    const char *temppath = cstr(hook->path_untar);
    TestTrue(os_create_dirs(temppath));
    check(os_tryuntil_deletefiles(temppath, "*"));
    check(test_sync_cloud_db_vaults(db));
    check(test_sync_cloud_db_vaultarchives(db));
    app;
    grp;

cleanup:
    return currenterr;
}
