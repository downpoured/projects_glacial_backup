/*
whole_operation.c

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

check_result test_operations_files(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook);
check_result test_operations_backup(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook);
check_result test_operations_restore(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook);
check_result test_operations_compact(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook);

SV_BEGIN_TEST_SUITE(whole_tests_operations)
{
    sv_app app = {};
    sv_group grp = {};
    svdb_db db = {};
    sv_test_hook hook = sv_test_hook_open(tempdir);
    uint64_t hashseedwas1 = SvdpHashSeed1, hashseedwas2 = SvdpHashSeed2;
    SvdpHashSeed1 = 0x1999188817771666ULL;
    SvdpHashSeed2 = 0x9988776655443322ULL;
    app.grp_names = bstrlist_open();
    app.low_access = false;
    app.path_app_data = bfromcstr(tempdir);
    app.path_temp_archived = bformat("%s%stemp_archived",
        cstr(app.path_app_data), pathsep);
    app.path_temp_unarchived = bformat("%s%stemp_unarchived",
        cstr(app.path_app_data), pathsep);

    /* create directories */
    check_b(os_create_dirs(cstr(hook.path_group)), "");
    check_b(os_create_dirs(cstr(hook.path_readytoupload)), "");
    check_b(os_create_dirs(cstr(hook.path_readytoremove)), "");
    check_b(os_create_dirs(cstr(hook.path_restoreto)), "");
    check_b(os_create_dirs(cstr(hook.path_userfiles)), "");
    check_b(os_create_dirs(cstr(hook.path_untar)), "");
    check_b(os_create_dirs(cstr(app.path_temp_archived)), "");
    check_b(os_create_dirs(cstr(app.path_temp_unarchived)), "");
    check(sv_app_creategroup_impl(&app, "test", cstr(hook.path_group)));
    check(sv_app_findgroupnames(&app));
    check(load_backup_group(&app, &grp, &db, "test"));
    grp.days_to_keep_prev_versions = 0;
    grp.separate_metadata = 1;
    check(sv_grp_persist(&db, &grp));

    /* run operations */
    SV_TEST_() {
        check(test_operations_files(&app, &grp, &db, &hook));
        check(test_operations_backup(&app, &grp, &db, &hook));
        check(test_operations_restore(&app, &grp, &db, &hook));
        check(test_operations_compact(&app, &grp, &db, &hook));
    }

    /* cleanup */
    svdb_close(&db);
    check(tests_cleardir(cstr(hook.path_readytoupload)));
    check(tests_cleardir(cstr(hook.path_readytoremove)));
    check(tests_cleardir(cstr(hook.path_restoreto)));
    check(tests_cleardir(cstr(hook.path_untar)));
    check(tests_cleardir(cstr(hook.path_userfiles)));
    check(tests_cleardir(cstr(hook.path_group)));
    check(tests_cleardir(cstr(app.path_temp_archived)));
    check(tests_cleardir(cstr(app.path_temp_unarchived)));
    bsetfmt(hook.path_tmp, "%s%suserdata", tempdir, pathsep);
    check(tests_cleardir(cstr(hook.path_tmp)));
    bsetfmt(hook.path_tmp, "%s%stemp", tempdir, pathsep);
    check(tests_cleardir(cstr(hook.path_tmp)));
    sv_test_hook_close(&hook);
    sv_grp_close(&grp);
    sv_app_close(&app);
    SvdpHashSeed1 = hashseedwas1;
    SvdpHashSeed2 = hashseedwas2;
}
SV_END_TEST_SUITE()

check_result files_addallrowstolistofstrings(void *context,
    const sv_file_row *row,
    const bstring path,
    const bstring permissions);
check_result contents_addallrowstolistofstrings(void *context,
    const sv_content_row *row);
check_result sv_restore_cb(void *context,
    const sv_file_row *in_files_row,
    const bstring path,
    const bstring permissions);
check_result sv_compact_getcutoff(svdb_db *db,
    const sv_group *grp,
    uint64_t *collectionid_to_expire,
    time_t now);
check_result svdb_runsql(svdb_db *self,
    const char *sql,
    int lensql);

void setfileaccessiblity(sv_test_hook *hook, int number, bool isaccessible)
{
    TestTrue(number >= 0 && number < countof(hook->filelocks));
    if (isaccessible)
    {
        os_lockedfilehandle_close(&hook->filelocks[number]);
    }
    else
    {
        check_fatal(!hook->filelocks[number].os_handle, "cannot not lock twice");
        check_warn(os_lockedfilehandle_open(&hook->filelocks[number],
            cstr(hook->filenames[number]), false, NULL), "", exit_on_err);
    }
}

check_result hook_get_file_info(void *phook, os_lockedfilehandle *self,
    unused_ptr(uint64_t), uint64_t *modtime, bstring permissions)
{
    sv_test_hook *hook = (sv_test_hook *)phook;
    if (hook)
    {
        /* don't use the real file metadata, use our simulated data.
        test files are named test0 through test9,
        so we can read the number from the filename */
        const char *filename = cstr(self->loggingcontext);
        TestTrue(strlen(filename) > 10);
        int filenumber = filename[strlen(filename) - 5] - '0';
        TestTrue(filenumber >= 0 && filenumber < countof(hook->setlastmodtimes));
        *modtime = hook->setlastmodtimes[filenumber];
        bsetfmt(permissions, "%d", 111 * filenumber);
    }

    return OK;
}

void test_pattern_matches_list(const char *expected,
    bstrlist *listexpected,
    bstrlist *listgot)
{
    if (expected && expected[0])
    {
        bstrlist_splitcstr(listexpected, expected, '|');
        TestEqn(listexpected->qty - 1, listgot->qty);
        for (int i = 0; i < listgot->qty; i++)
        {
            TestTrue(fnmatch_simple(blist_view(listexpected, i),
                blist_view(listgot, i)));
        }
    }
    else
    {
        TestEqn(0, listgot->qty);
    }
}

check_result hook_call_before_process_queue(void *phook, svdb_db *db)
{
    /* this callback, triggered halfway through operation,
    verifies database state is what we expect */
    sv_test_hook *hook = (sv_test_hook *)phook;
    if (hook && hook->expectcontentrows)
    {
        bstrlist_clear(hook->allcontentrows);
        bstrlist_clear(hook->allfilelistrows);
        check_warn(svdb_contentsiter(db, hook->allcontentrows,
            &contents_addallrowstolistofstrings), "", exit_on_err);
        check_warn(svdb_files_iter(db, svdb_all_files,
            hook->allfilelistrows, &files_addallrowstolistofstrings), "", exit_on_err);

        test_pattern_matches_list(hook->expectcontentrows,
            hook->listexpectcontentrows, hook->allcontentrows);
        test_pattern_matches_list(hook->expectfilerows,
            hook->listexpectfilelistrows, hook->allfilelistrows);
    }

    if (hook && hook->messwithfiles)
    {
        /* file 2: file seen previously,
        removed between adding-to-queue and adding-to-archive */
        TestTrue(os_remove(cstr(hook->filenames[2])));
        hook->setlastmodtimes[2] = 0;
        /* file 4: file seen previously,
        inaccessible between adding-to-queue and adding-to-archive */
        setfileaccessiblity(hook, 4, false);
        /* file 5: new file,
        removed between adding-to-queue and adding-to-archive */
        TestTrue(os_remove(cstr(hook->filenames[5])));
        hook->setlastmodtimes[5] = 0;
        setfileaccessiblity(hook, 6, false);
        /* file 9: new file,
        content changed between adding-to-queue and adding-to-archive */
        check_warn(sv_file_writefile(cstr(hook->filenames[9]),
            "changed-second-with-more", "wb"), "", exit_on_err);
    }

    return OK;
}

check_result hook_call_when_restoring_file(void *phook,
    const char *originalpath,
    bstring destpath)
{
    sv_test_hook *hook = (sv_test_hook *)phook;
    if (hook)
    {
        /* instead of writing to that quite-long path, which would be more
        complicated to delete after the test, redirect the destination path
        someplace simpler (restoreto/a/a/file.txt) */
        TestTrue(os_isabspath(originalpath));
        const char *pathwithoutroot = originalpath + (islinux ? 1 : 3);
        bstring expectedname = bformat("%s%s%s", cstr(hook->path_restoreto),
            pathsep, pathwithoutroot);
        TestEqs(cstr(expectedname), cstr(destpath));
        os_get_filename(originalpath, hook->path_tmp);
        bsetfmt(destpath, "%s%sa%sa%s%s", cstr(hook->path_restoreto),
            pathsep, pathsep, pathsep, cstr(hook->path_tmp));

        bdestroy(expectedname);
    }
    return OK;
}

check_result tests_op_check_tar_contents(sv_test_hook *hook,
    const char *archivename,
    const char *expected,
    bool check_order)
{
    sv_result currenterr = {};
    ar_util ar = ar_util_open();
    bstring archivepath = bformat("%s%s%s",
        cstr(hook->path_readytoupload), pathsep, archivename);

    if (expected)
    {
        check(checkbinarypaths(&ar, false, NULL));
        check(tests_check_tar_contents(&ar, cstr(archivepath), expected,
            check_order));
    }

cleanup:
    ar_util_close(&ar);
    bdestroy(archivepath);
    return currenterr;
}

check_result hook_provide_file_list(void *phook, void *context)
{
    sv_result currenterr = {};
    sv_test_hook *hook = (sv_test_hook *)phook;
    if (hook)
    {
        /* inject our own list of files to be backed up.
        this way we can control the order and provide a fake last-modified-time. */
        for (uint32_t i = 0; i < countof(hook->filenames); i++)
        {
            if (hook->setlastmodtimes[i])
            {
                bstring permissions = bformat("%d", 111 * i);
                check(sv_backup_addtoqueue_cb(context, hook->filenames[i],
                    hook->setlastmodtimes[i],
                    os_getfilesize(cstr(hook->filenames[i])), permissions));
                bdestroy(permissions);
            }
        }
    }
cleanup:
    return currenterr;
}

check_result run_backup_and_reconnect(const sv_app *app, const sv_group *grp,
    svdb_db *db, sv_test_hook *hook, bool disable_debug_breaks)
{
    sv_result currenterr = {};
    bstring dbpath = bstrcpy(db->path);
    if (disable_debug_breaks)
    {
        quiet_warnings(true);
    }

    check(sv_backup(app, grp, db, hook));
    quiet_warnings(false);
    check(svdb_disconnect(db));
    check(svdb_connect(db, cstr(dbpath)));

cleanup:
    bdestroy(dbpath);
    return currenterr;
}

check_result test_operations_backup_reset(const sv_app *app, const sv_group *grp,
    svdb_db *db, sv_test_hook *hook, int numberoffiles, const char *extension,
    bool unicode, bool runbackup)
{
    sv_result currenterr = {};
    bstring tmp = bstring_open();
    printf(".");
    for (int i = 0; i < countof(hook->filenames); i++)
    {
        bdestroy(hook->filenames[i]);
        hook->filenames[i] = bformat("%s%sfile%s%d.%s",
            cstr(hook->path_userfiles), pathsep,
            unicode ? "\xE1\x84\x81_" : "",
            i, extension);
        setfileaccessiblity(hook, i, true);
    }

    /* delete everything in the db and in the working directories */
    check(svdb_clear_database_content(db));
    check(os_tryuntil_deletefiles(cstr(hook->path_userfiles), "*"));
    check(os_tryuntil_deletefiles(cstr(hook->path_readytoupload), "*"));
    check(os_tryuntil_deletefiles(cstr(hook->path_readytoremove), "*"));
    memset(hook->setlastmodtimes, 0, sizeof(hook->setlastmodtimes));
    for (int i = 0; i < numberoffiles; i++)
    {
        bsetfmt(tmp, "contents-%d", i);
        check(sv_file_writefile(cstr(hook->filenames[i]), cstr(tmp), "wb"));
        hook->setlastmodtimes[i] = 1;
    }

    if (runbackup)
    {
        check(run_backup_and_reconnect(app, grp, db, hook, false));
    }

cleanup:
    bdestroy(tmp);
    return currenterr;
}

void sv_compact_archivestats_to_string(const sv_compact_state *op,
    bool includefiles,
    bstring s)
{
    bstrclear(s);
    bstr_catstatic(s, "stats");
    for (uint32_t i = 0; i < op->archive_stats.arr.length; i++)
    {
        const sv_array *child = (const sv_array *)sv_array_atconst(
            &op->archive_stats.arr, i);
        for (uint32_t j = 0; j < child->length; j++)
        {
            const sv_archive_stats *o = (const sv_archive_stats *)
                sv_array_atconst(child, j);
            if (o->count_new > 0 || o->count_old > 0)
            {
                bformata(s, "|%05x_%05x.tar,", o->original_collection,
                    o->archive_number);
                bformata(s, "needed=%llu(%llu),old=%llu(%llu),", castull(o->count_new),
                    castull(o->size_new), castull(o->count_old), castull(o->size_old));

                if (includefiles)
                {
                    for (uint32_t k = 0; k < o->old_individual_files.length; k++)
                    {
                        bformata(s, "%llu,", sv_array_at64u(
                            &o->old_individual_files, k));
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
    for (uint32_t i = 0; i < op->archives_to_strip.length; i++)
    {
        const sv_archive_stats *o = (const sv_archive_stats *)sv_array_atconst(
            &op->archives_to_strip, i);
        bformata(s, "%05x_%05x.tar|", o->original_collection, o->archive_number);
    }

    bstr_catstatic(s, "archives_to_remove|");
    for (uint32_t i = 0; i < op->archives_to_remove.length; i++)
    {
        const sv_archive_stats *o = (const sv_archive_stats *)sv_array_atconst(
            &op->archives_to_remove, i);
        bformata(s, "%05x_%05x.tar|", o->original_collection, o->archive_number);
    }
}

check_result test_operations_compact(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TEST_OPEN3(bstring, archivestats, contents, path);
    TEST_OPEN_EX(bstring, dbpath, bstrcpy(db->path));
    TEST_OPEN_EX(bstrlist *, messages, bstrlist_open());
    uint64_t collectionid_to_expire = 0;
    uint32_t countrun = 0;
    const char *currentcontext = "";
    const time_t october_01_2016 = 1475280000, october_02_2016 = 1475366400,
        october_03_2016 = 1475452800, october_04_2016 = 1475539200,
        october_20_2016 = 1476921600;

    check(test_operations_backup_reset(app, grp, db, hook, 0, "jpg", false, false));
    grp->days_to_keep_prev_versions = 0;

    SV_TEST("can't run compact if backup has never been run") {
        collectionid_to_expire = 9999;
        check(sv_compact_getcutoff(
            db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(0, collectionid_to_expire);
    }

    SV_TEST("can't run compact if backup has never been run") {
        collectionid_to_expire = 9999;
        check(svdb_collectioninsert_helper(
            db, (uint64_t)october_01_2016, (uint64_t)october_01_2016 + 1));
        check(sv_compact_getcutoff(
            db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(0, collectionid_to_expire);
    }

    SV_TEST("don't return the latest collection, even if old enough to expire") {
        collectionid_to_expire = 9999;
        check(svdb_collectioninsert_helper(
            db, (uint64_t)october_02_2016, (uint64_t)october_02_2016 + 1));
        check(sv_compact_getcutoff(
            db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(1, collectionid_to_expire);
    }

    SV_TEST_() {
        check(svdb_collectioninsert_helper(db, (uint64_t)october_03_2016,
            (uint64_t)october_03_2016 + 1));
        check(svdb_collectioninsert_helper(db, (uint64_t)october_04_2016,
            (uint64_t)october_04_2016 + 1));
        grp->days_to_keep_prev_versions = 0;
        check(sv_compact_getcutoff(db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(3, collectionid_to_expire);
    }

    SV_TEST_() {
        grp->days_to_keep_prev_versions = 16;
        check(sv_compact_getcutoff(db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(3, collectionid_to_expire);
    }

    SV_TEST_() {
        grp->days_to_keep_prev_versions = 17;
        check(sv_compact_getcutoff(db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(2, collectionid_to_expire);
    }

    SV_TEST_() {
        grp->days_to_keep_prev_versions = 18;
        check(sv_compact_getcutoff(db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(1, collectionid_to_expire);
    }

    SV_TEST_() {
        grp->days_to_keep_prev_versions = 19;
        check(sv_compact_getcutoff(db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(0, collectionid_to_expire);
    }

    SV_TEST_() {
        grp->days_to_keep_prev_versions = 100;
        check(sv_compact_getcutoff(db, grp, &collectionid_to_expire, october_20_2016));
        TestEqn(0, collectionid_to_expire);
    }

    /* real-world compaction scenario where files are deleted over time */
    int32_t filesizes[10] = { 30000, 30001, 30002, 30003, 30004, 30005, 30006,
        30007, 30008, 30009 };
    uint32_t filepresence[10][5] = {
        /* 001_001.tar */
        { 1, 1, 1, 1, 0 }, /* file0.jpg is present days 1-4 */
        { 1, 1, 1, 0, 0 }, /* file1.jpg is present days 1-3 */
        { 1, 1, 0, 0, 0 }, /* file2.jpg is present days 1-2 */
                           /* 001_002.tar */
        { 1, 1, 0, 0, 0 }, /* file3.jpg is present days 1-2 */
        { 1, 0, 0, 0, 0 }, /* file4.jpg is present days 1-1 */
                           /* 002_001.tar */
        { 0, 1, 1, 1, 0 }, /* file5.jpg is present days 2-4 */
        { 0, 1, 1, 0, 0 }, /* file6.jpg is present days 2-3 */
        { 0, 1, 0, 0, 0 }, /* file7.jpg is present days 2-2 */
                           /* 002_002.tar */
        { 0, 1, 1, 0, 0 }, /* file8.jpg is present days 2-3 */
        { 0, 1, 0, 0, 0 }, /* file9.jpg is present days 2-2 */
    };

    grp->approx_archive_size_bytes = 100 * 1024;
    check(test_operations_backup_reset(app, grp, db, hook, 0, "jpg", false, false));
    hook->expectcontentrows = hook->expectfilerows = NULL;
    for (int day = 0; day < 5; day++)
    {
        for (int file = 0; file < 10; file++)
        {
            hook->setlastmodtimes[file] = filepresence[file][day];
            if (filepresence[file][day] && !os_file_exists(cstr(hook->filenames[file])))
            {
                bstr_fill(contents, 'a', filesizes[file]);
                check(sv_file_writefile(cstr(hook->filenames[file]),
                    cstr(contents), "wb"));
            }
            else if (!filepresence[file][day])
            {
                TestTrue(os_remove(cstr(hook->filenames[file])));
            }
        }

        check(run_backup_and_reconnect(app, grp, db, hook, false));
    }

    /* set compressed size to be the same as uncompressed size, for simplified
    testing. otherwise if we changed to a different compression algorithm
    we'd have to rewrite numbers in the test */
    int mismatches = 0;
    check(sv_verify_archives(app, grp, db, &mismatches));
    check(svdb_runsql(db, s_and_len("UPDATE TblContentsList SET "
        "CompressedContentLength = ContentLength WHERE 1")));
    TestEqn(0, mismatches);
    os_clr_console();

    /* verify state is what we expect */
    hook->expectfilerows = "";
    hook->expectcontentrows = "hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1|"
        "hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2|"
        "hash=6d0e4c169cc0c599 8246a2c30669cd32 28b85d3d6074d314 f3a87c2aea57d9c0, crc32=b536af1d, contents_length=30002,30???, most_recent_collection=2, original_collection=1, archivenumber=1, id=3|"
        "hash=c32e8a5950abf496 9bf7b53abbe2fb6a c698b15eb0ce0130 b7b94f962c6e03da, crc32=8b04e435, contents_length=30003,30???, most_recent_collection=2, original_collection=1, archivenumber=2, id=4|"
        "hash=fb412ff195b17e3f f103f68f57873b05 de3be0dcfb6f1799 7bc921a2fe7f0970, crc32=be8f7e84, contents_length=30004,30???, most_recent_collection=1, original_collection=1, archivenumber=2, id=5|"
        "hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6|"
        "hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7|"
        "hash=22185046db27dcc7 25a0045524fa41ef bc2eaa1e74e47ade 1049dfcc60879063, crc32=da800a06, contents_length=30007,30???, most_recent_collection=2, original_collection=2, archivenumber=1, id=8|"
        "hash=deba8dfbf4311d38 9c8b78eafd0cf50b 31f54eeacd34c49e d9773741db16ecd0, crc32=10e9b7c, contents_length=30008,30???, most_recent_collection=3, original_collection=2, archivenumber=2, id=9|"
        "hash=2996400e1c920ff2 8a59a0b5f4e83bbe aae8a60f9290c79e 80f77a8e77166f20, crc32=b1058dcf, contents_length=30009,30???, most_recent_collection=2, original_collection=2, archivenumber=2, id=10|";
    check(hook_call_before_process_queue(hook, db));
    check(tests_op_check_tar_contents(hook, "00001_00001.tar",
        "00000001.file|00000002.file|00000003.file|filenames.txt^"
        "30000|30001|30002|11111111", true));
    check(tests_op_check_tar_contents(hook, "00001_00002.tar",
        "00000004.file|00000005.file|filenames.txt^"
        "30003|30004|178", true));
    check(tests_op_check_tar_contents(hook, "00002_00001.tar",
        "00000006.file|00000007.file|00000008.file|filenames.txt^"
        "30005|30006|30007|267", true));
    check(tests_op_check_tar_contents(hook, "00002_00002.tar",
        "00000009.file|0000000a.file|filenames.txt^"
        "30008|30009|178", true));

    /* make a copy of the database files and archives */
    /* don't need to rebuild the state from scratch each time */
    bsetfmt(path, "%s%s00001_00003.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00002_00003.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00003_00001.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00004_00001.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    TEST_OPEN_EX(bstring, databasestate0,
        bformat("%s%suserdata%s%s%stest_index.db", cstr(app->path_app_data),
        pathsep, pathsep, cstr(grp->grpname), pathsep));
    TEST_OPEN_EX(bstring, databasestate1, bformat("%s%s00001_00001.tar",
        cstr(hook->path_readytoupload), pathsep));
    TEST_OPEN_EX(bstring, databasestate2, bformat("%s%s00001_00002.tar",
        cstr(hook->path_readytoupload), pathsep));
    TEST_OPEN_EX(bstring, databasestate3, bformat("%s%s00002_00001.tar",
        cstr(hook->path_readytoupload), pathsep));
    TEST_OPEN_EX(bstring, databasestate4, bformat("%s%s00002_00002.tar",
        cstr(hook->path_readytoupload), pathsep));
    TEST_OPEN_EX(bstring, databasestate0_saved, bformat("%s%stest_index.db",
        cstr(hook->path_restoreto), pathsep));
    TEST_OPEN_EX(bstring, databasestate1_saved, bformat("%s%s00001_00001.tar",
        cstr(hook->path_restoreto), pathsep));
    TEST_OPEN_EX(bstring, databasestate2_saved, bformat("%s%s00001_00002.tar",
        cstr(hook->path_restoreto), pathsep));
    TEST_OPEN_EX(bstring, databasestate3_saved, bformat("%s%s00002_00001.tar",
        cstr(hook->path_restoreto), pathsep));
    TEST_OPEN_EX(bstring, databasestate4_saved, bformat("%s%s00002_00002.tar",
        cstr(hook->path_restoreto), pathsep));
    bsetfmt(hook->path_tmp, "%s%sa", cstr(hook->path_restoreto), pathsep);
    check(tests_cleardir(cstr(hook->path_tmp)));
    check(tests_cleardir(cstr(hook->path_restoreto)));
    check(svdb_disconnect(db));
    TestTrue(os_move(cstr(databasestate0), cstr(databasestate0_saved), true));
    TestTrue(os_move(cstr(databasestate1), cstr(databasestate1_saved), true));
    TestTrue(os_move(cstr(databasestate2), cstr(databasestate2_saved), true));
    TestTrue(os_move(cstr(databasestate3), cstr(databasestate3_saved), true));
    TestTrue(os_move(cstr(databasestate4), cstr(databasestate4_saved), true));

    /* now run compact many times, each time with a different cutoff.
    the cutoff says that all collections prior to this collection id have expired */
    for (uint64_t cutoff = 0; cutoff <= 5; cutoff++)
    {
        /* rebuild database state */
        check(svdb_disconnect(db));
        check(os_tryuntil_deletefiles(cstr(hook->path_readytoupload), "*"));
        check(os_tryuntil_deletefiles(cstr(hook->path_readytoremove), "*.tar"));
        TestTrue(os_remove(cstr(dbpath)));
        TestTrue(os_copy(cstr(databasestate0_saved), cstr(databasestate0), true));
        TestTrue(os_copy(cstr(databasestate1_saved), cstr(databasestate1), true));
        TestTrue(os_copy(cstr(databasestate2_saved), cstr(databasestate2), true));
        TestTrue(os_copy(cstr(databasestate3_saved), cstr(databasestate3), true));
        TestTrue(os_copy(cstr(databasestate4_saved), cstr(databasestate4), true));
        check(svdb_connect(db, cstr(dbpath)));

        const char *expectstatsbefore = NULL, *expectstatsafter = NULL;
        const char *expect_tar_01_01 = "00000001.file|00000002.file|"
            "00000003.file|filenames.txt^30000|30001|30002|267";
        const char *expect_tar_01_02 = "00000004.file|00000005.file|"
            "filenames.txt^30003|30004|178";
        const char *expect_tar_02_01 = "00000006.file|00000007.file|"
            "00000008.file|filenames.txt^30005|30006|30007|267";
        const char *expect_tar_02_02 = "00000009.file|0000000a.file|"
            "filenames.txt^30008|30009|178";
        switch (cutoff)
        {
        case 0:
            grp->compact_threshold_bytes = 1;
            expectstatsbefore = expectstatsafter = "stats|00001_00001.tar,needed=3(90003),old=0(0),|00001_00002.tar,needed=2(60007),old=0(0),|00002_00001.tar,needed=3(90018),old=0(0),|00002_00002.tar,needed=2(60017),old=0(0),archives_to_removefiles|archives_to_remove|";
            break;
        case 1:
            /* set the compact_threshold_bytes to a high value and verify
                that compact is not run. */
            grp->compact_threshold_bytes = 100000;
            expectstatsbefore = expectstatsafter = "stats|00001_00001.tar,needed=3(90003),old=0(0),|00001_00002.tar,needed=1(30003),old=1(30004),|00002_00001.tar,needed=3(90018),old=0(0),|00002_00002.tar,needed=2(60017),old=0(0),archives_to_removefiles|archives_to_remove|";
            break;
        case 2:
            /* set the compact_threshold_bytes to a low value and verify
                that compact is run. */
            grp->compact_threshold_bytes = 1;
            expectstatsbefore = "stats|00001_00001.tar,needed=2(60001),old=1(30002),3,|00001_00002.tar,needed=0(0),old=2(60007),4,5,|00002_00001.tar,needed=2(60011),old=1(30007),8,|00002_00002.tar,needed=1(30008),old=1(30009),10,archives_to_removefiles|00001_00001.tar|00002_00001.tar|00002_00002.tar|archives_to_remove|00001_00002.tar|";
            expectstatsafter = "stats|00001_00001.tar,needed=2(60001),old=0(0),|00002_00001.tar,needed=2(60011),old=0(0),|00002_00002.tar,needed=1(30008),old=0(0),archives_to_removefiles|archives_to_remove|";
            expect_tar_01_01 = "00000001.file|00000002.file|"
                "filenames.txt^30000|30001|267";
            expect_tar_01_02 = NULL;
            expect_tar_02_01 = "00000006.file|00000007.file|"
                "filenames.txt^30005|30006|267";
            expect_tar_02_02 = "00000009.file|filenames.txt^30008|178";
            hook->expectcontentrows = "hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1|"
                "hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2|"
                "hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6|"
                "hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7|"
                "hash=deba8dfbf4311d38 9c8b78eafd0cf50b 31f54eeacd34c49e d9773741db16ecd0, crc32=10e9b7c, contents_length=30008,30???, most_recent_collection=3, original_collection=2, archivenumber=2, id=9|";
            hook->expectfilerows = "";
            break;
        case 3:
            /* test what happens if the archives are no longer present on disk.
            for remove-old-files, we skip over,
                not changing the database or files on disk
            for remove-old-entire-archives,
                make the change and add a text file to readytoremove dir */
            grp->compact_threshold_bytes = 1;
            expectstatsbefore = "stats|00001_00001.tar,needed=1(30000),old=2(60003),2,3,|00001_00002.tar,needed=0(0),old=2(60007),4,5,|00002_00001.tar,needed=1(30005),old=2(60013),7,8,|00002_00002.tar,needed=0(0),old=2(60017),9,10,archives_to_removefiles|00001_00001.tar|00002_00001.tar|archives_to_remove|00001_00002.tar|00002_00002.tar|";
            expectstatsafter = "stats|00001_00001.tar,needed=1(30000),old=2(60003),2,3,|00002_00001.tar,needed=1(30005),old=2(60013),7,8,archives_to_removefiles|00001_00001.tar|00002_00001.tar|archives_to_remove|";
            TestTrue(os_remove(cstr(databasestate1)));
            TestTrue(os_remove(cstr(databasestate2)));
            TestTrue(os_remove(cstr(databasestate3)));
            TestTrue(os_remove(cstr(databasestate4)));
            expect_tar_01_01 = expect_tar_01_02 = NULL;
            expect_tar_02_01 = expect_tar_02_02 = NULL;
            bsetfmt(path, "%s%s00001_00002.tar.txt",
                cstr(hook->path_readytoremove), pathsep);
            TestTrue(!os_file_exists(cstr(path)));
            bsetfmt(path, "%s%s00002_00002.tar.txt",
                cstr(hook->path_readytoremove), pathsep);
            TestTrue(!os_file_exists(cstr(path)));
            hook->expectcontentrows = "hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1|"
                "hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2|"
                "hash=6d0e4c169cc0c599 8246a2c30669cd32 28b85d3d6074d314 f3a87c2aea57d9c0, crc32=b536af1d, contents_length=30002,30???, most_recent_collection=2, original_collection=1, archivenumber=1, id=3|"
                "hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6|"
                "hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7|"
                "hash=22185046db27dcc7 25a0045524fa41ef bc2eaa1e74e47ade 1049dfcc60879063, crc32=da800a06, contents_length=30007,30???, most_recent_collection=2, original_collection=2, archivenumber=1, id=8|";
            hook->expectfilerows = "";
            break;
        case 4: /* fallthrough */
        case 5:
            /* everything in the archive has expired, and so we'll be left
                with no files remaining */
            grp->compact_threshold_bytes = 1;
            expectstatsbefore = "stats|00001_00001.tar,needed=0(0),old=3(90003),1,2,3,|00001_00002.tar,needed=0(0),old=2(60007),4,5,|00002_00001.tar,needed=0(0),old=3(90018),6,7,8,|00002_00002.tar,needed=0(0),old=2(60017),9,10,archives_to_removefiles|archives_to_remove|00001_00001.tar|00001_00002.tar|00002_00001.tar|00002_00002.tar|";
            expectstatsafter = "statsarchives_to_removefiles|archives_to_remove|";
            expect_tar_01_01 = expect_tar_01_02 = NULL;
            expect_tar_02_01 = expect_tar_02_02 = NULL;
            bsetfmt(path, "%s%s00001_00002.tar.txt",
                cstr(hook->path_readytoremove), pathsep);
            TestTrue(os_file_exists(cstr(path)));
            bsetfmt(path, "%s%s00002_00002.tar.txt",
                cstr(hook->path_readytoremove), pathsep);
            TestTrue(os_file_exists(cstr(path)));
            hook->expectcontentrows = "";
            hook->expectfilerows = "";
            break;
        }

        /* get archive statistics */
        sv_compact_state op = {};
        op.archive_stats = sv_2darray_open(sizeof32u(sv_archive_stats));
        op.expiration_cutoff = cutoff;
        op.is_thorough = true;
        op.test_context = hook;
        op.working_dir_archived = bstrcpy(app->path_temp_archived);
        op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
        check(svdb_contentsiter(db, &op, &sv_compact_getarchivestats));
        sv_compact_see_what_to_remove(&op, grp->compact_threshold_bytes);
        sv_compact_archivestats_to_string(&op, true, archivestats);
        TestEqs(expectstatsbefore, cstr(archivestats));

        /* run compact */
        bstrlist_clear(messages);
        check(sv_remove_entire_archives(&op, db, cstr(app->path_app_data),
            cstr(grp->grpname), messages));
        check(sv_strip_archive_removing_old_files(&op, app, grp, db,
            messages));

        if (cutoff == 0 || cutoff == 1)
        {
            hook->expectcontentrows = "hash=1ae24f7db75d0ad6 8e024ae5f8544008 2efda40e01a7dfb6 3b88c11491f0a5c9, crc32=6693bf41, contents_length=30000,30???, most_recent_collection=4, original_collection=1, archivenumber=1, id=1|"
                "hash=873058c3b7f4ace5 d7bd5b2de07398bd d47151c9e92f8a9e d7d7484c857d9c9f, crc32=e90a5cfa, contents_length=30001,30???, most_recent_collection=3, original_collection=1, archivenumber=1, id=2|"
                "hash=6d0e4c169cc0c599 8246a2c30669cd32 28b85d3d6074d314 f3a87c2aea57d9c0, crc32=b536af1d, contents_length=30002,30???, most_recent_collection=2, original_collection=1, archivenumber=1, id=3|"
                "hash=c32e8a5950abf496 9bf7b53abbe2fb6a c698b15eb0ce0130 b7b94f962c6e03da, crc32=8b04e435, contents_length=30003,30???, most_recent_collection=2, original_collection=1, archivenumber=2, id=4|"
                "hash=fb412ff195b17e3f f103f68f57873b05 de3be0dcfb6f1799 7bc921a2fe7f0970, crc32=be8f7e84, contents_length=30004,30???, most_recent_collection=1, original_collection=1, archivenumber=2, id=5|"
                "hash=4895c6d611040647 1fd156e3a560c5b8 4072d62a9069d5a5 f53dacfca283c635, crc32=2dc7604, contents_length=30005,30???, most_recent_collection=4, original_collection=2, archivenumber=1, id=6|"
                "hash=e590549050eda339 24314d6669c98924 de3395418bc315e8 f034beaaff19d248, crc32=efd8a62c, contents_length=30006,30???, most_recent_collection=3, original_collection=2, archivenumber=1, id=7|"
                "hash=22185046db27dcc7 25a0045524fa41ef bc2eaa1e74e47ade 1049dfcc60879063, crc32=da800a06, contents_length=30007,30???, most_recent_collection=2, original_collection=2, archivenumber=1, id=8|"
                "hash=deba8dfbf4311d38 9c8b78eafd0cf50b 31f54eeacd34c49e d9773741db16ecd0, crc32=10e9b7c, contents_length=30008,30???, most_recent_collection=3, original_collection=2, archivenumber=2, id=9|"
                "hash=2996400e1c920ff2 8a59a0b5f4e83bbe aae8a60f9290c79e 80f77a8e77166f20, crc32=b1058dcf, contents_length=30009,30???, most_recent_collection=2, original_collection=2, archivenumber=2, id=10|";
            hook->expectfilerows = "";
        }
        if (cutoff == 2)
        {
            /* test verify_archive_checksums after compaction */
            mismatches = 0;
            check(sv_verify_archives(app, grp, db, &mismatches));
            os_clr_console();
            TestTrue(mismatches == 0);

            /* corrupt one of the archives by changing a single byte */
            sv_file file = {};
            check(sv_file_open(&file, cstr(databasestate4), "rb+"));
            fseek(file.file, 20000, SEEK_SET);
            fputc('*', file.file);
            sv_file_close(&file);

            /* now the checksum should not match */
            check(sv_verify_archives(app, grp, db, &mismatches));
            os_clr_console();
            TestTrue(mismatches == 1);
            TestEqn(0, messages->qty);
        }
        else if (cutoff == 3)
        {
            /* when cutoff is 3 we have explicitly removed the .tar files from
                    readytoupload, and so we expected to see two messages for
                    the two tar files we didn't see. */
            TestEqn(2, messages->qty);
        }
        else
        {
            TestEqn(0, messages->qty);
        }

        check(tests_op_check_tar_contents(hook, "00001_00001.tar",
            expect_tar_01_01, false));
        check(tests_op_check_tar_contents(hook, "00001_00002.tar",
            expect_tar_01_02, false));
        check(tests_op_check_tar_contents(hook, "00002_00001.tar",
            expect_tar_02_01, false));
        check(tests_op_check_tar_contents(hook, "00002_00002.tar",
            expect_tar_02_02, false));
        check(hook_call_before_process_queue(hook, db));
        sv_compact_state_close(&op);

        /* get archive statistics again, afterwards */
        sv_compact_state op_after = {};
        op_after.archive_stats = sv_2darray_open(sizeof32u(sv_archive_stats));
        op_after.expiration_cutoff = cutoff;
        op_after.is_thorough = true;
        op_after.test_context = hook;
        check(svdb_contentsiter(db, &op_after, &sv_compact_getarchivestats));
        sv_compact_see_what_to_remove(&op_after, grp->compact_threshold_bytes);
        sv_compact_archivestats_to_string(&op_after, true, archivestats);
        TestEqs(expectstatsafter, cstr(archivestats));
        sv_compact_state_close(&op_after);
    }

cleanup:
    if (currenterr.code && currentcontext && currentcontext[0])
    {
        printf("context: %s\n", currentcontext);
    }

    grp->approx_archive_size_bytes = 64 * 1024 * 1024;
    grp->compact_threshold_bytes = 32 * 1024 * 1024;
    return currenterr;
}

check_result test_operations_files(const sv_app *app, unused_ptr(sv_group),
    unused_ptr(svdb_db), sv_test_hook *hook)
{
    /* verify what creating the group should have done */
    TestEqn(1, app->grp_names->qty);
    TestEqs("test", blist_view(app->grp_names, 0));
    bsetfmt(hook->path_tmp, "%s%stest_index.db", cstr(hook->path_group), pathsep);
    TestTrue(os_file_exists(cstr(hook->path_tmp)));
    TestTrue(os_dir_exists(cstr(hook->path_readytoupload)));
    return OK;
}

check_result test_backup_add_utf8(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TEST_OPEN(bstring, path);
    hook->expectcontentrows = "";
    hook->expectfilerows = "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file\xE1\x84\x81_1.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.txt|";
    check(test_operations_backup_reset(app, grp, db, hook, 6, "txt", true, true));
    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=2|"
        "hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3|"
        "hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=4|"
        "hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5|"
        "hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=6|";
    hook->expectfilerows = "contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt|"
        "contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt|"
        "contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.txt|"
        "contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.txt|"
        "contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=1, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.txt|"
        "contents_length=10, contents_id=6, last_write_time=1, flags=555, most_recent_collection=1, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.txt|";
    check(hook_call_before_process_queue(hook, db));
    bsetfmt(path, "%s%s00001_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    check(tests_op_check_tar_contents(hook, "00001_00001.tar",
        "00000001.xz|00000002.xz|00000003.xz|00000004.xz|00000005.xz|00000006.xz|"
        "filenames.txt^64|64|64|64|64|64|558", true));

cleanup:
    return currenterr;
}

check_result test_backup_see_content_changes(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    /* we should correctly determine which files on disk have been changed */
    /* See the design explanation at the top of operations.c */
    /* file 0: same contents, same lmt */
    /* file 1: changed contents, same lmt */
    check(sv_file_writefile(cstr(hook->filenames[1]), "contents-x", "wb"));
    /* file 2: appended contents, same lmt */
    check(sv_file_writefile(cstr(hook->filenames[2]), "contents-x-appended", "wb"));
    /* file 3: same contents, different lmt */
    hook->setlastmodtimes[3]++;
    /* file 4: changed contents, different lmt */
    check(sv_file_writefile(cstr(hook->filenames[4]), "contents-X", "wb"));
    hook->setlastmodtimes[4]++;
    /* file 5: appended contents, different lmt */
    check(sv_file_writefile(cstr(hook->filenames[5]), "contents-X-appended", "wb"));
    hook->setlastmodtimes[5]++;
    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=1|"
        "hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=2|"
        "hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3|"
        "hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=4|"
        "hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5|"
        "hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=6|";
    hook->expectfilerows = "contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt|"
        "contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt|"
        "contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.txt|"
        "contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.txt|"
        "contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.txt|"
        "contents_length=10, contents_id=6, last_write_time=1, flags=555, most_recent_collection=2, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.txt|";
    check(run_backup_and_reconnect(app, grp, db, hook, false));
    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=1|"
        "hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=2|"
        "hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3|"
        "hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=4|"
        "hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5|"
        "hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=6|"
        "hash=4e228734f4417f88 f176ff37b912b345 1aa7c6aa85f440aa d6bad0d10f7cd5b2, crc32=9215b3c6, contents_length=19,??, most_recent_collection=2, original_collection=2, archivenumber=1, id=7|"
        "hash=1f670f96631263a0 44d3d56cdd109b5f 793f361d46f9a315 bd7ffe48ae75ebe4, crc32=e8deaef, contents_length=10,??, most_recent_collection=2, original_collection=2, archivenumber=1, id=8|"
        "hash=59547001a163df77 25b6786ba1ab3f71 9780a1194748b574 3ce7a9f21f22f5a2, crc32=dd48b016, contents_length=19,??, most_recent_collection=2, original_collection=2, archivenumber=1, id=9|";
    hook->expectfilerows = "contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt|"
        "contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt|"
        "contents_length=19, contents_id=7, last_write_time=1, flags=222, most_recent_collection=2, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.txt|"
        "contents_length=10, contents_id=4, last_write_time=2, flags=333, most_recent_collection=2, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.txt|"
        "contents_length=10, contents_id=8, last_write_time=2, flags=444, most_recent_collection=2, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.txt|"
        "contents_length=19, contents_id=9, last_write_time=2, flags=555, most_recent_collection=2, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.txt|";
    check(hook_call_before_process_queue(hook, db));
    check(tests_op_check_tar_contents(hook, "00002_00001.tar",
        "00000007.xz|00000008.xz|00000009.xz|filenames.txt^72|64|72|279", true));

cleanup:
    return currenterr;
}

check_result test_backup_see_metadata_changes(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TEST_OPEN(bstring, path);
    /* changing metadata and adding new files */
    hook->expectcontentrows = hook->expectfilerows = NULL;
    check(test_operations_backup_reset(app, grp, db, hook, 5, "txt", false, true));
    /* file 0: accessible, with same contents as the previous file1 */
    check(sv_file_writefile(cstr(hook->filenames[0]), "contents-1", "wb"));
    hook->setlastmodtimes[0]++;
    /* file 1: file seen previously, removed before directory iteration */
    check_b(os_remove(cstr(hook->filenames[1])), "could not remove file.");
    hook->setlastmodtimes[1] = 0;
    /* file 2: file seen previously, removed after directory iteration */
    hook->setlastmodtimes[2]++;
    /* file 3: file seen previously, inaccessible before directory iteration */
    check(sv_file_writefile(cstr(hook->filenames[3]),
        "contents-added-but-inaccessible", "wb"));
    setfileaccessiblity(hook, 3, false);
    hook->setlastmodtimes[3]++;
    /* file 4: file seen previously, inaccessible after directory iteration */
    hook->setlastmodtimes[4]++;
    /* file 5: new file, removed after directory iteration */
    check(sv_file_writefile(cstr(hook->filenames[5]), "newfile-5", "wb"));
    hook->setlastmodtimes[5]++;
    /* file 6: new file, inaccessible after directory iteration */
    check(sv_file_writefile(cstr(hook->filenames[6]), "newfile-6", "wb"));
    hook->setlastmodtimes[6]++;
    /* file 7: new file */
    check(sv_file_writefile(cstr(hook->filenames[7]), "a-newly-added-file", "wb"));
    hook->setlastmodtimes[7]++;
    /* file 8: new file with same contents as file 8 */
    check(sv_file_writefile(cstr(hook->filenames[8]), "a-newly-added-file", "wb"));
    hook->setlastmodtimes[8]++;
    /* file 9: new file, content is changed between adding-to-queue
        and adding-to-archive */
    check(sv_file_writefile(cstr(hook->filenames[9]), "changed-first", "wb"));
    hook->setlastmodtimes[9]++;

    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=2|"
        "hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3|"
        "hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=4|"
        "hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=5|";
    hook->expectfilerows = "contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=0, id=1*" pathsep "file0.txt|"
        "contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file1.txt|"
        "contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file2.txt|"
        "contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file3.txt|"
        "contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file4.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file5.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=7*" pathsep "file6.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=8*" pathsep "file7.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=9*" pathsep "file8.txt|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=10*" pathsep "file9.txt|";
    hook->messwithfiles = true;
    check(run_backup_and_reconnect(app, grp, db, hook, true));
    hook->messwithfiles = false;

    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=2|"
        "hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=3|"
        "hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=4|"
        "hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10,??, most_recent_collection=2, original_collection=1, archivenumber=1, id=5|"
        "hash=ed9d76f043ec0c97 4c2f7ed570a007e7 95278b72e012766b 3b333ad4d06a8c51, crc32=618f9ed5, contents_length=18,??, most_recent_collection=2, original_collection=2, archivenumber=1, id=6|"
        "hash=1be52bb37faff0dc 484bb2ad5a0b67b3 670af01d0a133ad3 76ecb5a9eaf24830, crc32=7b206e22, contents_length=24,??, most_recent_collection=2, original_collection=2, archivenumber=1, id=7|";
    hook->expectfilerows = "contents_length=10, contents_id=2, last_write_time=2, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file0.txt|"
        "contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file3.txt|"
        "contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file4.txt|"
        "contents_length=18, contents_id=6, last_write_time=1, flags=777, most_recent_collection=2, e_status=3, id=8*" pathsep "file7.txt|"
        "contents_length=18, contents_id=6, last_write_time=1, flags=888, most_recent_collection=2, e_status=3, id=9*" pathsep "file8.txt|"
        "contents_length=24, contents_id=7, last_write_time=1, flags=999, most_recent_collection=2, e_status=3, id=10*" pathsep "file9.txt|";
    check(hook_call_before_process_queue(hook, db));
    bsetfmt(path, "%s%s00002_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    check(tests_op_check_tar_contents(hook, "00001_00001.tar",
        "00000001.xz|00000002.xz|00000003.xz|00000004.xz|00000005.xz|"
        "filenames.txt^64|64|64|64|64|445", true));
    check(tests_op_check_tar_contents(hook, "00002_00001.tar",
        "00000006.xz|00000007.xz|filenames.txt^72|76|178", true));

cleanup:
    return currenterr;
}

check_result test_backup_archive_sizing(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TEST_OPEN2(bstring, large, path);
    /* Adding files of different sizes and types */
    /* because the filetype is 'jpg' we'll add these files with no compression. */
    hook->expectcontentrows = "";
    hook->expectfilerows = "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file0.jpg|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file1.jpg|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file2.jpg|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file3.jpg|";
    check(test_operations_backup_reset(app, grp, db, hook, 4, "jpg", false, false));
    grp->approx_archive_size_bytes = 100 * 1024;
    /* file 0: normal text file */
    bstr_fill(large, 'a', 40 * 1024);
    check(sv_file_writefile(cstr(hook->filenames[0]), cstr(large), "wb"));
    hook->setlastmodtimes[0]++;
    /* file 1: normal text file */
    bstr_fill(large, 'b', 40 * 1024);
    check(sv_file_writefile(cstr(hook->filenames[1]), cstr(large), "wb"));
    hook->setlastmodtimes[1]++;
    /* file 2: not that big, but too big to fit in the archive */
    bstr_fill(large, 'c', 20 * 1024);
    check(sv_file_writefile(cstr(hook->filenames[2]), cstr(large), "wb"));
    hook->setlastmodtimes[2]++;
    /* file 3: a file so big we'll have to create a new archive just for it */
    bstr_fill(large, 'd', 250 * 1024 + 1);
    check(sv_file_writefile(cstr(hook->filenames[3]), cstr(large), "wb"));
    hook->setlastmodtimes[3]++;
    check(run_backup_and_reconnect(app, grp, db, hook, false));

    hook->expectcontentrows = "hash=cd270b526ba2e90d 722778efe7b2c29d abd68da25e8b1eb1 bd0c0cd7fa1d6ce7, crc32=646773f3, contents_length=40960,4????, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=6475ff6c168ec2c4 bf46244f6a3386e1 c33373ced8062291 b19877758bf215d0, crc32=d39f5778, contents_length=40960,4????, most_recent_collection=1, original_collection=1, archivenumber=1, id=2|"
        "hash=c0ea4a9032d68aeb 68b77b93c9c8e487 7a45f6bab2b0d83d fe4cff3d741bcad6, crc32=802ae62f, contents_length=20480,2????, most_recent_collection=1, original_collection=1, archivenumber=2, id=3|"
        "hash=9ea177d0cd42dbbd 947ed281720090cd da9d6580b6b3d2cf b41b3dfead68ed0, crc32=669eca11, contents_length=256001,25????, most_recent_collection=1, original_collection=1, archivenumber=3, id=4|";
    hook->expectfilerows = "contents_length=40960, contents_id=1, last_write_time=2, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file0.jpg|"
        "contents_length=40960, contents_id=2, last_write_time=2, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file1.jpg|"
        "contents_length=20480, contents_id=3, last_write_time=2, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file2.jpg|"
        "contents_length=256001, contents_id=4, last_write_time=2, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file3.jpg|";
    check(hook_call_before_process_queue(hook, db));
    grp->approx_archive_size_bytes = 32 * 1024 * 1024;
    bsetfmt(path, "%s%s00001_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    check(tests_op_check_tar_contents(hook, "00001_00001.tar",
        "00000001.file|00000002.file|filenames.txt^40960|40960|178", true));
    check(tests_op_check_tar_contents(hook, "00001_00002.tar",
        "00000003.file|filenames.txt^20480|89", true));
    check(tests_op_check_tar_contents(hook, "00001_00003.tar",
        "00000004.file|filenames.txt^256001|89", true));

cleanup:
    bdestroy(large);
    return currenterr;
}

check_result test_backup_no_changed_files(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TEST_OPEN(bstring, path);
    /* try running backup() when there are no changed files */
    hook->expectcontentrows = hook->expectfilerows = NULL;
    check(run_backup_and_reconnect(app, grp, db, hook, false));
    check(hook_call_before_process_queue(hook, db));
    bsetfmt(path, "%s%s00001_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00002_index.db", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00001_00002.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00001_00003.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00002_00001.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
    bsetfmt(path, "%s%s00002_00002.tar", cstr(hook->path_readytoupload), pathsep);
    TestTrue(!os_file_exists(cstr(path)));
cleanup:
    return currenterr;
}

check_result test_backup_add_mp3(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    hook->expectfilerows = "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.mp3|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file\xE1\x84\x81_1.mp3|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.mp3|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.mp3|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.mp3|";
    check(test_operations_backup_reset(app, grp, db, hook, 5, "mp3", true, false));
    check(sv_file_writefile(cstr(hook->filenames[0]), "contents-0", "wb"));
    check(writevalidmp3(cstr(hook->filenames[1]), false, false, false));
    check(writevalidmp3(cstr(hook->filenames[2]), false, false, false));
    check(writevalidmp3(cstr(hook->filenames[3]), false, false, false));
    check(writevalidmp3(cstr(hook->filenames[4]), false, false, false));
    check(run_backup_and_reconnect(app, grp, db, hook, true));

    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1,3???, most_recent_collection=1, original_collection=1, archivenumber=1, id=2|";
    hook->expectfilerows = "contents_length=1, contents_id=1, last_write_time=1, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=444, most_recent_collection=1, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.mp3|";
    check(hook_call_before_process_queue(hook, db));
    check(tests_op_check_tar_contents(hook, "00001_00001.tar",
        "00000001.file|00000002.file|filenames.txt^10|3142|186", true));

cleanup:
    return currenterr;
}

check_result test_backup_ignore_tag_changes(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    sv_file file = {};
    TEST_OPEN(bstring, large);
    /* test the separate-audio-metadata feature */
    /* file 0: change contents but still invalid */
    check(sv_file_writefile(cstr(hook->filenames[0]),
        "still-not-a-valid-mp3", "wb"));
    hook->setlastmodtimes[0]++;
    /* file 1: change the length but not the lmt,
        we ignore this because we ignore length */
    check(sv_file_open(&file, cstr(hook->filenames[1]), "ab"));
    uint64_t zero = 0;
    fwrite(&zero, sizeof(zero), 1, file.file);
    sv_file_close(&file);
    /* file 2: change lmt and content but not the length,
        from a valid mp3 to an invalid mp3 */
    uint64_t prevlength = os_getfilesize(cstr(hook->filenames[2]));
    bstr_fill(large, 'a', cast64u32s(prevlength));
    check(sv_file_writefile(cstr(hook->filenames[2]), cstr(large), "wb"));
    hook->setlastmodtimes[2]++;
    /* file 3: change the metadata only, should not be seen as changed */
    check(writevalidmp3(cstr(hook->filenames[3]), true, false, false));
    hook->setlastmodtimes[3]++;
    /* file 4: change the data, should be seen as changed */
    check(writevalidmp3(cstr(hook->filenames[4]), false, false, true));
    hook->setlastmodtimes[4]++;
    /* file 5: a new mp3 file w new length, should be seen as the same as 3 */
    check(writevalidmp3(cstr(hook->filenames[5]), false, true, false));
    hook->setlastmodtimes[5]++;
    /* file 6: a new mp3 file w new length, should be seen as the same as 4 */
    check(writevalidmp3(cstr(hook->filenames[6]), true, true, true));
    hook->setlastmodtimes[6]++;

    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1,3???, most_recent_collection=2, original_collection=1, archivenumber=1, id=2|";
    hook->expectfilerows = "contents_length=1, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.mp3|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.mp3|"
        "contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=7*" pathsep "file\xE1\x84\x81_6.mp3|";
    check(run_backup_and_reconnect(app, grp, db, hook, true));

    hook->expectcontentrows = "hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1,??, most_recent_collection=1, original_collection=1, archivenumber=1, id=1|"
        "hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1,3???, most_recent_collection=2, original_collection=1, archivenumber=1, id=2|"
        "hash=4d098b3072969adc 635dfe534db06698 d2307aea1e18b7b3 922bbf0481ef8e6b, crc32=95bcd25d, contents_length=1,??, most_recent_collection=2, original_collection=2, archivenumber=1, id=3|"
        "hash=6e1b0f02bb2220c4 aba9f88dca9fd76f 6d2e86e5f79b4f25 b039cedc3e4fd7af, crc32=b4e1484f, contents_length=1,3???, most_recent_collection=2, original_collection=2, archivenumber=1, id=4|"
        "hash=7d49276b4e05c2bd 39f840313d6eeb4f 0 0, crc32=c5679762, contents_length=1,3???, most_recent_collection=2, original_collection=2, archivenumber=1, id=5|";
    hook->expectfilerows = "contents_length=1, contents_id=3, last_write_time=2, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3|"
        "contents_length=1, contents_id=4, last_write_time=2, flags=222, most_recent_collection=2, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.mp3|"
        "contents_length=1, contents_id=2, last_write_time=2, flags=333, most_recent_collection=2, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.mp3|"
        "contents_length=1, contents_id=5, last_write_time=2, flags=444, most_recent_collection=2, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.mp3|"
        "contents_length=1, contents_id=2, last_write_time=1, flags=555, most_recent_collection=2, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.mp3|"
        "contents_length=1, contents_id=5, last_write_time=1, flags=666, most_recent_collection=2, e_status=3, id=7*" pathsep "file\xE1\x84\x81_6.mp3|";
    check(hook_call_before_process_queue(hook, db));
    check(tests_op_check_tar_contents(hook, "00002_00001.tar",
        "00000003.file|00000004.file|00000005.file|filenames.txt"
        "^21|3142|3142|279", true));

cleanup:
    bdestroy(large);
    return currenterr;
}

check_result test_operations_restore(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TEST_OPEN_EX(bstrlist *, files_seen, bstrlist_open());
    TEST_OPEN_EX(bstring, fullrestoreto, bformat("%s%sa%sa",
        cstr(hook->path_restoreto), pathsep, pathsep));
    TEST_OPEN(bstring, contents);

    /* run backup and add file0.txt, file1.txt. */
    hook->expectcontentrows = hook->expectfilerows = NULL;
    check(test_operations_backup_reset(
        app, grp, db, hook, 2, "txt", true, true));

    /* modify file1.txt and add file2.txt. */
    check(sv_file_writefile(cstr(hook->filenames[1]),
        "contents-1-altered", "wb"));
    hook->setlastmodtimes[1]++;
    check(sv_file_writefile(cstr(hook->filenames[2]),
        "the-contents-2", "wb"));
    hook->setlastmodtimes[2]++;
    check(run_backup_and_reconnect(app, grp, db, hook, false));

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
        bsetfmt(hook->path_tmp, "%s%sa", cstr(hook->path_restoreto), pathsep);
        check(os_tryuntil_deletefiles(cstr(hook->path_tmp), "*"));
        TestTrue(os_tryuntil_remove(cstr(hook->path_tmp)));
        check(os_tryuntil_deletefiles(cstr(hook->path_restoreto), "*"));
        TestTrue(os_create_dirs(cstr(hook->path_restoreto)));

        /* run restore */
        sv_restore_state op = {};
        op.working_dir_archived = bstrcpy(app->path_temp_archived);
        op.working_dir_unarchived = bstrcpy(app->path_temp_unarchived);
        op.destdir = bfromcstr(cstr(hook->path_restoreto));
        op.scope = bfromcstr(i == test_operations_restore_scope_to_one_file ?
            "*2.txt" : "*");
        op.destfullpath = bstring_open();
        op.tmp_result = bstring_open();
        op.messages = bstrlist_open();
        op.db = db;
        op.test_context = hook;
        check(ar_manager_open(&op.archiver, cstr(app->path_app_data),
            cstr(grp->grpname), 0, 0));
        check(checkbinarypaths(&op.archiver.ar, false, NULL));
        TestTrue(os_create_dirs(cstr(op.working_dir_archived)));
        TestTrue(os_create_dirs(cstr(op.working_dir_unarchived)));

        if (i == test_operations_restore_scope_to_one_file)
        {
            check(svdb_files_iter(op.db, svdb_all_files, &op, &sv_restore_cb));
            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(1, files_seen->qty);
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_2.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("the-contents-2", cstr(contents));
            TestEqn(0, op.messages->qty);
        }
        else if (i == test_operations_restore_from_many_archives)
        {
            check(svdb_files_iter(op.db, svdb_all_files, &op, &sv_restore_cb));
            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(3, files_seen->qty);
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_0.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("contents-0", cstr(contents));
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_1.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("contents-1-altered", cstr(contents));
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_2.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("the-contents-2", cstr(contents));
            TestEqn(0, op.messages->qty);
        }
        else if (i == test_operations_restore_include_older_files)
        {
            /* change the database so that it looks like file0.txt was
                inaccessible the last time we ran backup. we should run
                smoothly and just restore the latest version we have. */
            sv_file_row row = {};
            check(svdb_filesbypath(db, hook->filenames[0], &row));
            row.most_recent_collection = 2;
            row.e_status = sv_filerowstatus_queued;
            bassigncstr(contents, "(updated)");
            check(svdb_filesupdate(db, &row, contents));
            check(svdb_files_iter(op.db, svdb_all_files, &op, &sv_restore_cb));

            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(3, files_seen->qty);
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_0.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("contents-0", cstr(contents));
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_1.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("contents-1-altered", cstr(contents));
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_2.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("the-contents-2", cstr(contents));
            TestEqn(0, op.messages->qty);
        }
        else if (i == test_operations_restore_missing_archive)
        {
            /* if an archive is not found,
                we add the error to a list of messages and continue. */
            bsetfmt(hook->path_tmp, "%s%s00002_00001.tar",
                cstr(hook->path_readytoupload), pathsep);
            TestTrue(os_file_exists(cstr(hook->path_tmp)));
            TestTrue(os_tryuntil_remove(cstr(hook->path_tmp)));
            quiet_warnings(true);
            check(svdb_files_iter(op.db, svdb_all_files, &op, &sv_restore_cb));
            quiet_warnings(false);

            check(os_listfiles(cstr(fullrestoreto), files_seen, false));
            TestEqn(1, files_seen->qty);
            bsetfmt(hook->path_tmp, "%s%sa%sa%sfile\xE1\x84\x81_0.txt",
                cstr(hook->path_restoreto), pathsep, pathsep, pathsep);
            check(sv_file_readfile(cstr(hook->path_tmp), contents));
            TestEqs("contents-0", cstr(contents));
            TestEqn(2, op.messages->qty);
            TestTrue(s_contains(blist_view(op.messages, 0),
                "couldn't find archive "));
            TestTrue(s_contains(blist_view(op.messages, 1),
                "couldn't find archive "));
        }

        sv_restore_state_close(&op);
    }

cleanup:
    return currenterr;
}

check_result test_operations_backup(const sv_app *app, sv_group *grp,
    svdb_db *db, sv_test_hook *hook)
{
    sv_result currenterr = {};
    TestTrue(os_create_dirs(cstr(hook->path_untar)));
    check(os_tryuntil_deletefiles(cstr(hook->path_untar), "*"));
    check(test_backup_add_utf8(app, grp, db, hook));
    check(test_backup_see_content_changes(app, grp, db, hook));
    check(test_backup_see_metadata_changes(app, grp, db, hook));
    check(test_backup_archive_sizing(app, grp, db, hook));
    check(test_backup_no_changed_files(app, grp, db, hook));
    check(test_backup_add_mp3(app, grp, db, hook));
    check(test_backup_ignore_tag_changes(app, grp, db, hook));

cleanup:
    return currenterr;
}
