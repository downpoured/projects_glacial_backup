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
}
SV_END_TEST_SUITE()


check_result test_operations_sync_cloud(
    const sv_app *app, sv_group *grp, svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    const char *temppath = cstr(hook->path_untar);
    TestTrue(os_create_dirs(temppath));
    check(os_tryuntil_deletefiles(temppath, "*"));


    /*
    check(test_backup_add_utf8(app, grp, db, hook));
    check(test_backup_see_content_changes(app, grp, db, hook));
    check(test_backup_see_metadata_changes(app, grp, db, hook));
    check(test_backup_archive_sizing(app, grp, db, hook));
    check(test_backup_no_changed_files(app, grp, db, hook));
    check(test_backup_add_mp3(app, grp, db, hook));
    check(test_backup_ignore_tag_changes(app, grp, db, hook));*/

cleanup:
    return currenterr;
}
