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
    const char *jsonsinglequote =
        "{\n"
        "    'VaultList': [\n"
        "        {\n"
        "            'VaultARN': "
        "'arn:aws:glacier:us-east-1:123456789:vaults/the_vault_name',\n"
        "            'VaultName': 'the vault name',\n"
        "            'CreationDate': '2016-08-27T11:12:13.111Z',\n"
        "            'Duplicate': 'abc',\n"
        "            'Duplicate': 'def',\n"
        "            'NumberOfArchives': 0,\n"
        "            'NoClosing': 'noclose\n"
        "        }\n"
        "    ]\n"
        "}\n";
    TEST_OPEN(bstring, result);
    TEST_OPEN_EX(bstring, json, bfromcstr(jsonsinglequote));
    bstr_replaceall(json, "'", "\"");

    SV_TEST("get from json, failure cases")
    {
        expect_err_with_message(
            roughselectstrfromjson(cstr(json), "NotFound", result),
            "key not found");
        expect_err_with_message(
            roughselectstrfromjson(cstr(json), "Duplicate", result),
            "more than once");
        expect_err_with_message(
            roughselectstrfromjson(cstr(json), "NoClosing", result),
            "quote not found");
    }

    SV_TEST("get from json, success cases")
    {
        check(roughselectstrfromjson(cstr(json), "VaultName", result));
        TestEqs("the vault name", cstr(result));
        check(roughselectstrfromjson(cstr(json), "VaultARN", result));
        TestEqs("arn:aws:glacier:us-east-1:123456789:vaults/the_vault_name",
            cstr(result));
    }

    SV_TEST_WIN("localpath_to_cloud_path windows-style")
    {
        TEST_OPEN_EX(bstring, s1,
            localpath_to_cloud_path(
                "C:\\dir1\\dir2", "C:\\dir1\\dir2\\file.dat", "a"));
        TestEqs("glacial_backup/a/dir2/file.dat", cstr(s1));
        TEST_OPEN_EX(bstring, s2,
            localpath_to_cloud_path(
                "C:\\dir1\\dir2", "C:\\dir1\\dir2\\dir3\\file.dat", "a"));
        TestEqs("glacial_backup/a/dir2/dir3/file.dat", cstr(s2));
        TEST_OPEN_EX(bstring, s3,
            localpath_to_cloud_path(
                "C:\\dir1\\dir2", "C:\\dir1\\dir2\\dir3\\dir4\\file.dat", "a"));
        TestEqs("glacial_backup/a/dir2/dir3/dir4/file.dat", cstr(s3));
        TEST_OPEN_EX(bstring, s4,
            localpath_to_cloud_path(
                "C:\\dir1\\dir2", "C:\\dir1\\dir2file.dat", "a"));
        TestEqs("", cstr(s4));
        TEST_OPEN_EX(bstring, s5,
            localpath_to_cloud_path(
                "C:\\dir1\\dir2", "C:\\dir1\\file.dat", "a"));
        TestEqs("", cstr(s5));
        TEST_OPEN_EX(bstring, s6,
            localpath_to_cloud_path("C:\\dir1\\dir2", "file.dat", "a"));
        TestEqs("", cstr(s6));
    }

    SV_TEST("localpath_to_cloud_path unix-style")
    {
        TEST_OPEN_EX(bstring, s1,
            localpath_to_cloud_path("/dir1/dir2", "/dir1/dir2/file.dat", "a"));
        TestEqs("glacial_backup/a/dir2/file.dat", cstr(s1));
        TEST_OPEN_EX(bstring, s2,
            localpath_to_cloud_path(
                "/dir1/dir2", "/dir1/dir2/dir3/file.dat", "a"));
        TestEqs("glacial_backup/a/dir2/dir3/file.dat", cstr(s2));
        TEST_OPEN_EX(bstring, s3,
            localpath_to_cloud_path(
                "/dir1/dir2", "/dir1/dir2/dir3/dir4/file.dat", "a"));
        TestEqs("glacial_backup/a/dir2/dir3/dir4/file.dat", cstr(s3));
        TEST_OPEN_EX(bstring, s4,
            localpath_to_cloud_path("/dir1/dir2", "/dir1/dir2file.dat", "a"));
        TestEqs("", cstr(s4));
        TEST_OPEN_EX(bstring, s5,
            localpath_to_cloud_path("/dir1/dir2", "/dir1/file.dat", "a"));
        TestEqs("", cstr(s5));
        TEST_OPEN_EX(
            bstring, s6, localpath_to_cloud_path("/dir1/dir2", "file.dat", "a"));
        TestEqs("", cstr(s6));
    }

    SV_TEST("cloud_path_to_description")
    {
        TEST_OPEN(bstring, tmppath);
        check(tmpwritetextfile(tempdir, "tmp.txt", tmppath, "contents"));
        TEST_OPEN_EX(bstring, desc,
            cloud_path_to_description("a/example001.txt", cstr(tmppath)));
        TEST_OPEN_EX(bstring, s, bfromcstr("YTtleGFtcGxlMDAxLnR4dA=="));
        TEST_OPEN_EX(bstring, testdec, bBase64Decode(s));
        TestEqs("a;example001.txt", cstr(testdec));
        const char *expectlike = "<m><v>4</v><p>YTtleGFtcGxlMDAxLnR4dA==</"
                                 "p><lm>20200815T172139Z</lm></m>";
        TestTrue(s_startwith(
            cstr(desc), "<m><v>4</v><p>YTtleGFtcGxlMDAxLnR4dA==</p><lm>2"));
        TestTrue(s_endwith(cstr(desc), "Z</lm></m>"));
        TestEqn(strlen(cstr(desc)), strlen(expectlike));
        printf("\nWe got \n%s\n, which aside from the date+time should look "
               "similar \n%s\n.",
            cstr(desc), expectlike);
    }

    SV_TEST("base64")
    {
        const char *cb64 =
            "ZHVwbGljYXRpL2RhdGFfMV9wb3N0cy9kdXBsaWNhdGktYjcxYmZiMDRlY2Y3MTQzZGM"
            "5NTQ5ZGMxNWQ0YmZmNWI5LmRibG9jay56aXAuYWVz";
        const char *cdec =
            "duplicati/data_1_posts/"
            "duplicati-b71bfb04ecf7143dc9549dc15d4bff5b9.dblock.zip.aes";
        TEST_OPEN_EX(bstring, b64, bfromcstr(cb64));
        TEST_OPEN_EX(bstring, dec, bfromcstr(cdec));
        TEST_OPEN_EX(bstring, testdec, bBase64Decode(b64));
        TEST_OPEN_EX(bstring, testenc, tobase64nospace(cstr(dec)));
        TestEqs(cdec, cstr(testdec));
        TestEqs(cb64, cstr(testenc));
    }

    SV_TEST("base64, has closing =")
    {
        const char *cb64 = "Y19tdXNpYy8wMDAwM18wMDAwMS50YXI=";
        const char *cdec = "c_music/00003_00001.tar";
        TEST_OPEN_EX(bstring, b64, bfromcstr(cb64));
        TEST_OPEN_EX(bstring, dec, bfromcstr(cdec));
        TEST_OPEN_EX(bstring, testdec, bBase64Decode(b64));
        TEST_OPEN_EX(bstring, testenc, tobase64nospace(cstr(dec)));
        TestEqs(cdec, cstr(testdec));
        TestEqs(cb64, cstr(testenc));
    }
}
SV_END_TEST_SUITE()

check_result test_sync_cloud_db_vaults(svdb_db *db)
{
    sv_result currenterr = {};
    check(svdb_runsql(db, s_and_len("DELETE FROM TblKnownVaults WHERE 1"),
        expectchangesunknown));

    /* disallow duplicate names */
    check(svdb_knownvaults_insert(db, "reg1", "nm1", "awsname1", "arn1"));
    expect_err_with_message(
        svdb_knownvaults_insert(db, "regdiff", "nm1", "awsdiff", "arndiff"), "");

    /* add the rest of the rows */
    check(svdb_knownvaults_insert(db, "reg2", "nm2", "awsname2", "arn2"));
    check(svdb_knownvaults_insert(db, "reg3", "nm3", "awsname3", "arn3"));

    /* query them */
    {
        TEST_OPEN_EX(bstrlist *, regions, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, names, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, awsnames, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, arns, bstrlist_open());
        TEST_OPEN_EX(sv_array, ids, sv_array_open_u64());
        check(svdb_knownvaults_get(db, regions, names, awsnames, arns, &ids));
        TEST_OPEN_EX(bstring, allregions, bjoin_static(regions, "|"));
        TEST_OPEN_EX(bstring, allnames, bjoin_static(names, "|"));
        TEST_OPEN_EX(bstring, allawsnames, bjoin_static(awsnames, "|"));
        TEST_OPEN_EX(bstring, allarns, bjoin_static(arns, "|"));
        TestEqs("reg1|reg2|reg3", cstr(allregions));
        TestEqs("nm1|nm2|nm3", cstr(allnames));
        TestEqs("awsname1|awsname2|awsname3", cstr(allawsnames));
        TestEqs("arn1|arn2|arn3", cstr(allarns));
        TestEqn(1, sv_array_at64u(&ids, 0));
        TestEqn(2, sv_array_at64u(&ids, 1));
        TestEqn(3, sv_array_at64u(&ids, 2));
        TestEqn(3, ids.length);
    }

    /* delete one and make sure ids are still correct */
    check(
        svdb_runsql(db, s_and_len("DELETE FROM TblKnownVaults WHERE name='nm2'"),
            expectchangesunknown));
    {
        TEST_OPEN_EX(bstrlist *, regions, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, names, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, awsnames, bstrlist_open());
        TEST_OPEN_EX(bstrlist *, arns, bstrlist_open());
        TEST_OPEN_EX(sv_array, ids, sv_array_open_u64());
        check(svdb_knownvaults_get(db, regions, names, awsnames, arns, &ids));
        TEST_OPEN_EX(bstring, allregions, bjoin_static(regions, "|"));
        TEST_OPEN_EX(bstring, allnames, bjoin_static(names, "|"));
        TEST_OPEN_EX(bstring, allawsnames, bjoin_static(awsnames, "|"));
        TEST_OPEN_EX(bstring, allarns, bjoin_static(arns, "|"));
        TestEqs("reg1|reg3", cstr(allregions));
        TestEqs("nm1|nm3", cstr(allnames));
        TestEqs("awsname1|awsname3", cstr(allawsnames));
        TestEqs("arn1|arn3", cstr(allarns));
        TestEqn(1, sv_array_at64u(&ids, 0));
        TestEqn(3, sv_array_at64u(&ids, 1));
        TestEqn(2, ids.length);
    }
cleanup:
    return currenterr;
}

check_result expect_vaultarchives_row(svdb_db *db, const char *path,
    uint64_t knownvaultid, uint64_t size, uint64_t crc32, uint64_t modtime)
{
    uint64_t outsize = 0, outcrc32 = 0, outmodtime = 0;
    sv_result ret = svdb_vaultarchives_bypath(
        db, path, knownvaultid, &outsize, &outcrc32, &outmodtime);
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

check_result test_sync_cloud_db_vaultarchives(svdb_db *db)
{
    sv_result currenterr = {};
    check(svdb_runsql(db, s_and_len("DELETE FROM TblKnownVaultArchives WHERE 1"),
        expectchangesunknown));

    /* disallow duplicate paths */
    check(svdb_vaultarchives_insert(
        db, "path1", "descpath1", 11, "awsid1", 1002, 1003, 1004));
    expect_err_with_message(
        svdb_vaultarchives_insert(
            db, "path1", "descpath1diff", 11, "awsid1diff", 2002, 2003, 2004),
        "");

    /* insert rows */
    check(svdb_vaultarchives_insert(
        db, "path1", "descpath1", 22, "awsid2", 10002, 10003, 10004));
    check(svdb_vaultarchives_insert(
        db, "path2", "descpath2", 22, "awsid2", 20002, 20003, 20004));
    check(svdb_vaultarchives_insert(
        db, "path3", "descpath3", 22, "awsid3", 30002, 30003, 30004));
    check(svdb_vaultarchives_insert(
        db, "path4", "descpath4", 22, "awsid4", 40002, 40003, 40004));

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
    check(svdb_vaultarchives_insert(
        db, "path1", "descpath1", 22, "awsid2", 90002, 90003, 90004));
    check(expect_vaultarchives_row(db, "path1", 11, 1002, 1003, 1004));
    check(expect_vaultarchives_row(db, "path1", 22, 90002, 90003, 90004));
    check(expect_vaultarchives_row(db, "path1", 33, 0, 0, 0));

    /* delete by path for p2 */
    check(svdb_vaultarchives_delbypath(db, "path2", 22));

    /* query by path for p2 should not exist */
    check(expect_vaultarchives_row(db, "path2", 22, 0, 0, 0));
    check(expect_vaultarchives_row(db, "path3", 22, 30002, 30003, 30004));

    /* insert replacement p2 */
    check(svdb_vaultarchives_insert(
        db, "path2", "descpath2", 22, "awsid2", 990002, 990003, 990004));
    check(expect_vaultarchives_row(db, "path1", 22, 90002, 90003, 90004));
    check(expect_vaultarchives_row(db, "path2", 22, 990002, 990003, 990004));
    check(expect_vaultarchives_row(db, "path3", 22, 30002, 30003, 30004));
cleanup:
    return currenterr;
}

check_result test_sync_cloud_finddirty(svdb_db *db, const char *tmpdir)
{
    sv_result currenterr = {};
    uint32_t intcrc = 0;
    uint32_t v = 123;
    sv_sync_finddirtyfiles finder = {};
    TEST_OPEN2(bstring, tmppath, sql);
    TestTrue(os_create_dirs(tmpdir));
    check(tests_cleardir(tmpdir));
    check(tmpwritetextfile(tmpdir, "f2.txt", tmppath, "abcd"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f2.txt", "desc2", "id2", 4, v));
    check(tmpwritetextfile(tmpdir, "f3.txt", tmppath, "abcde"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f3.txt", "desc3", "id3", 5, v));
    check(tmpwritetextfile(tmpdir, "f4.txt", tmppath, "abcdef"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f4.txt", "desc4", "id4", 6, v));
    check(tmpwritetextfile(tmpdir, "f5.txt", tmppath, "abcdefg"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f5.txt", "desc5", "id5", 7, v));
    check(tmpwritetextfile(tmpdir, "f6.txt", tmppath, "abcdefgh"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f6.txt", "desc6", "id6", 8, v));
    check(tmpwritetextfile(tmpdir, "f7.txt", tmppath, "abcdefghi"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f7.txt", "desc7", "id7", 9, v));
    check(tmpwritetextfile(tmpdir, "f8.txt", tmppath, "abcdefghij"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f8.txt", "desc8", "id7", 10, v));
    check(tmpwritetextfile(tmpdir, "f9.txt", tmppath, "abcdefghijk"));
    check(sv_sync_marksuccessupload(db, cstr(tmppath),
        "glacial_backup/a/untar/f9.txt", "desc9", "id2", 11, v));

    /* test all possible combinations of changing modtime, size, and crc */
    /* f1.txt file is dirty because uploaded to a different vault. */
    check(tmpwritetextfile(tmpdir, "f1.txt", tmppath, "abc"));
    check(sv_sync_marksuccessupload(
        db, cstr(tmppath), "f1.txt", "desc1", "id1", 3, v + 1));
    /* f2.txt same modtime,same size,same crc = clean */
    /* f3.txt same modtime,same size,diff crc = clean (skips due to optimization)
     */
    bsetfmt(tmppath, "%s/f3.txt", tmpdir);
    check(tmpwritetextfile(tmpdir, "f3.txt", tmppath, "abcdX"));
    uint64_t modtime = os_getmodifiedtime(cstr(tmppath));
    bsetfmt(sql,
        "UPDATE TblKnownVaultArchives SET ModifiedTime='%llu' WHERE "
        "CloudPath='glacial_backup/a/untar/f3.txt'",
        modtime);
    check(svdb_runsql(db, cstr(sql), blength(sql), expectchanges));
    /* f4.txt same modtime,diff size,same crc = clean */
    check(tmpwritetextfile(tmpdir, "f4.txt", tmppath, "abcdefZ"));
    check(sv_basic_crc32_wholefile(cstr(tmppath), &intcrc));
    bsetfmt(sql,
        "UPDATE TblKnownVaultArchives SET Crc32='%u' WHERE "
        "CloudPath='glacial_backup/a/untar/f4.txt'",
        intcrc);
    check(svdb_runsql(db, cstr(sql), blength(sql), expectchanges));
    /* f5.txt same modtime,diff size,diff crc = dirty */
    check(tmpwritetextfile(tmpdir, "f5.txt", tmppath, "abcdefgZ"));
    modtime = os_getmodifiedtime(cstr(tmppath));
    bsetfmt(sql,
        "UPDATE TblKnownVaultArchives SET ModifiedTime='%llu' WHERE "
        "CloudPath='glacial_backup/a/untar/f5.txt'",
        modtime);
    check(svdb_runsql(db, cstr(sql), blength(sql), expectchanges));
    /* f6.txt diff modtime,same size,same crc = clean */
    bsetfmt(tmppath, "%s/f6.txt", tmpdir);
    modtime = os_getmodifiedtime(cstr(tmppath));
    TestTrue(os_setmodifiedtime_nearestsecond(cstr(tmppath), modtime + 5));
    /* f7.txt diff modtime,same size,diff crc = dirty */
    os_sleep(2000);
    check(tmpwritetextfile(tmpdir, "f7.txt", tmppath, "abcdefghX"));
    /* f8.txt diff modtime,diff size,same crc = clean */
    check(tmpwritetextfile(tmpdir, "f8.txt", tmppath, "abcdefghijZ"));
    check(sv_basic_crc32_wholefile(cstr(tmppath), &intcrc));
    bsetfmt(sql,
        "UPDATE TblKnownVaultArchives SET Crc32='%u' WHERE "
        "CloudPath='glacial_backup/a/untar/f8.txt'",
        intcrc);
    check(svdb_runsql(db, cstr(sql), blength(sql), expectchanges));
    /* f9.txt diff modtime,diff size,diff crc = dirty */
    check(tmpwritetextfile(tmpdir, "f9.txt", tmppath, "abcdefghijkZ"));

    /* which files do we detect as dirty */
    finder = sv_sync_finddirtyfiles_open(tmpdir, "a");
    finder.db = db;
    finder.knownvaultid = v;
    finder.verbose = false;
    finder.sizes_and_files_clean = bstrlist_open();
    check(sv_sync_finddirtyfiles_find(&finder));

    /* normalize results */
    bstrlist_sort(finder.sizes_and_files);
    bstrlist_sort(finder.sizes_and_files_clean);
    TEST_OPEN_EX(bstring, alldirty, bjoin_static(finder.sizes_and_files, "|"));
    TEST_OPEN_EX(
        bstring, allclean, bjoin_static(finder.sizes_and_files_clean, "|"));
    bstr_replaceall(alldirty, tmpdir, "root");
    bstr_replaceall(alldirty, "\\", "/");
    bstr_replaceall(allclean, tmpdir, "root");
    bstr_replaceall(allclean, "\\", "/");

    /* check results */
    TestEqs("00000003|root/f1.txt|00000008|root/f5.txt|00000009|root/"
            "f7.txt|0000000c|root/f9.txt",
        cstr(alldirty));
    TestEqs("00000004|root/f2.txt|00000005|root/f3.txt|00000007|root/"
            "f4.txt|00000008|root/f6.txt|0000000b|root/f8.txt",
        cstr(allclean));
cleanup:
    return currenterr;
}

check_result test_operations_sync_cloud(svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    const char *tmpdir = cstr(hook->path_untar);
    check(test_sync_cloud_db_vaults(db));
    check(test_sync_cloud_db_vaultarchives(db));
    check(test_sync_cloud_finddirty(db, tmpdir));
cleanup:
    return currenterr;
}
