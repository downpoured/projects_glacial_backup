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

check_result svdb_connection_openhandle(svdb_connection* self);
void sphash_compute256(const byte* buf, uint32_t bufLen, hash256* outHash);
check_result svdp_restore_cb(void* context, bool*,
	const svdb_files_row* in_files_row, const bstring path);
bool readhash(const bstring getoutput, hash256* out_hash);
check_result svdp_compact_getexpirationcutoff(svdb_connection* db, const svdp_backupgroup* group, 
	uint64_t* collectionid_to_expire, time_t now);

void test_archiver_file_extension_info()
{
	/* test strings that are too short */
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len(".")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("..")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("...")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len(".mp3")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len(".7z")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("a.7z")));
	/* test strings that are short but usable */
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("aa.7z")));
	TestEqn(knownfileextensionMp3, get_file_extension_info(str_and_len("a.mp3")));
	/* case matters */
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("wrong case.Mp3")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("wrong case.MP3")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("wrong case.jpG")));
	/* strings that shouldn't match */
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("test.7zz")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("test.7z.doc")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("test.7z.")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("test.m4a.")));
	TestEqn(knownfileextensionNone, get_file_extension_info(str_and_len("test.")));
	/* strings that should match */
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("test.7z")));
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("test..7z")));
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("test...7z")));
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("test....7z")));
	TestEqn(knownfileextensionOgg, get_file_extension_info(str_and_len("test.ogg")));
	TestEqn(knownfileextensionOgg, get_file_extension_info(str_and_len("test..ogg")));
	TestEqn(knownfileextensionOgg, get_file_extension_info(str_and_len("test...ogg")));
	TestEqn(knownfileextensionOgg, get_file_extension_info(str_and_len("test....ogg")));
	TestEqn(knownfileextensionFlac, get_file_extension_info(str_and_len("test.flac")));
	TestEqn(knownfileextensionFlac, get_file_extension_info(str_and_len("test..flac")));
	TestEqn(knownfileextensionFlac, get_file_extension_info(str_and_len("test...flac")));
	TestEqn(knownfileextensionFlac, get_file_extension_info(str_and_len("test....flac")));
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("test.doc.7z")));
	TestEqn(knownfileextensionM4a, get_file_extension_info(str_and_len("test.doc.m4a")));
	TestEqn(knownfileextensionOtherBinary, get_file_extension_info(str_and_len("test.doc.webm")));
}

check_result test_util_hashing()
{
	{
		hash256 hash = CAST(hash256) {};
		sphash_compute256((const byte*)"test", strlen32u("test"), &hash);
		TestEqn(0x7b01e8bcec0d8b75, hash.data[0]);
		TestEqn(0x0b6f98a8d5ffac60, hash.data[1]);
		TestEqn(0x8aa8416a1752a8c5, hash.data[2]);
		TestEqn(0x6a5d8430daa59910, hash.data[3]);
	}
	{ /* hash of 0 bytes should not be 0 */
		hash256 hash = CAST(hash256) {};
		sphash_compute256((const byte*)"", 0, &hash);
		TestEqn(0x232706fc6bf50919, hash.data[0]);
		TestEqn(0x8b72ee65b4e851c7, hash.data[1]);
		TestEqn(0x88d8e9628fb694ae, hash.data[2]);
		TestEqn(0x015c99660e766a98, hash.data[3]);
	}
	{ /* test memory alignment */
		byte* buf = os_aligned_malloc(16 * 4096, 4096);

		/* fill the buffer with meaningless values, every element should be accessible */
		int total = 0;
		for (int i = 0; i < 4096; i++)
		{
			buf[i] = (byte)rand();
		}
		for (int i = 0; i < 4096; i++)
		{
			total += buf[i];
		}
		TestTrue(total > 0);
		os_aligned_free(&buf);
	}
	{ /* parse md5 */
		hash256 hash = CAST(hash256) {};
		struct tagbstring input = bsStatic("test\nMD5=1122334455667788a1a2a3a4a5a6a7a8");
		TestTrue(readhash(&input, &hash));
		TestEqn(0x8877665544332211ULL, hash.data[0]);
		TestEqn(0xa8a7a6a5a4a3a2a1ULL, hash.data[1]);
		TestEqn(0, hash.data[2]);
		TestEqn(0, hash.data[3]);
	}
	{ /* parse md5, empty string */
		hash256 hash = CAST(hash256) {};
		struct tagbstring input = bsStatic("");
		TestTrue(!readhash(&input, &hash));
	}
	{ /* parse md5, no hash */
		hash256 hash = CAST(hash256) {};
		struct tagbstring input = bsStatic("\nMD5=11");
		TestTrue(!readhash(&input, &hash));
	}
	{ /* parse md5, skip over partial entries */
		hash256 hash = CAST(hash256) {};
		struct tagbstring input = bsStatic(
			"test\nMD5=11223344\nMD5=11nocharacters\nothertext\nMD5=a1a2a3a4a5a6a7a81122334455667788\ntest");

		TestTrue(readhash(&input, &hash));
		TestEqn(0xa8a7a6a5a4a3a2a1ULL, hash.data[0]);
		TestEqn(0x8877665544332211ULL, hash.data[1]);
		TestEqn(0, hash.data[2]);
		TestEqn(0, hash.data[3]);
	}

	return OK;
}

check_result test_db_basics(const char* dir)
{
	sv_result currenterr = {};
	svdb_connection db = {};
	bstring dbpathreopen = bformat("%s%stestrepoen.db", dir, pathsep);
	bstring dbpathutf8 = bformat("%s%s\xED\x95\x9C test.db", dir, pathsep);
	bstring dbpathtestempty = bformat("%s%stestempty.db", dir, pathsep);
	bstring strGot = bstring_open();
	check(os_tryuntil_deletefiles(dir, "*"));

	/* create new db and open existing db. pathname has utf8 chars */
	check(svdb_connection_open(&db, cstr(dbpathutf8)));
	check(svdb_connection_disconnect(&db));
	check(svdb_connection_open(&db, cstr(dbpathutf8)));
	uint32_t schemaVersion = 0;
	check(svdb_propertygetint(&db, str_and_len32s("SchemaVersion"), &schemaVersion));
	TestEqn(1, schemaVersion);
	check(svdb_connection_disconnect(&db));

	/* create new db and add 2 rows */
	check(svdb_connection_open(&db, cstr(dbpathreopen)));
	check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 123));
	check(svdb_propertysetstr(&db, str_and_len32s("TestStr"), "hello"));
	check(svdb_connection_disconnect(&db));

	/* connect to existing db and read the 2 rows */
	check(svdb_connection_open(&db, cstr(dbpathreopen)));
	uint32_t intGot = 0;
	check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &intGot));
	check(svdb_propertygetstr(&db, str_and_len32s("TestStr"), strGot));
	TestEqn(123, intGot);
	TestEqs("hello", cstr(strGot));
	check(svdb_connection_disconnect(&db));

	{ /* create an empty db file (no schema) and see if we can recover from this incomplete state */
		svdb_connection connTestempty = {};
		connTestempty.path = bstrcpy(dbpathtestempty);
		check(svdb_connection_openhandle(&connTestempty));
		check(svdb_connection_disconnect(&connTestempty));
		set_debugbreaks_enabled(false);
		check(svdb_connection_open(&connTestempty, cstr(dbpathtestempty)));
		set_debugbreaks_enabled(true);
		schemaVersion = 0;
		check(svdb_propertygetint(&connTestempty, str_and_len32s("SchemaVersion"), &schemaVersion));
		TestEqn(1, schemaVersion);
		check(svdb_connection_disconnect(&connTestempty));
		svdb_connection_close(&connTestempty);
	}

	{ /* data is kept if transaction committed */
		check(svdb_connection_open(&db, cstr(dbpathutf8)));
		check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 1));
		svdb_txn txn = {};
		check(svdb_txn_open(&txn, &db));
		check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 2));
		uint32_t nGot = 0;
		check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &nGot));
		TestEqn(2, nGot);
		check(svdb_txn_commit(&txn, &db));
		nGot = 0;
		check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &nGot));
		TestEqn(2, nGot);

		/* data is not kept if transaction rolled back */
		svdb_txn_close(&txn, &db);
		check(svdb_txn_open(&txn, &db));
		check(svdb_propertysetint(&db, str_and_len32s("TestInt"), 3));
		nGot = 0;
		check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &nGot));
		TestEqn(3, nGot);
		check(svdb_txn_rollback(&txn, &db));
		nGot = 0;
		check(svdb_propertygetint(&db, str_and_len32s("TestInt"), &nGot));
		TestEqn(2, nGot);
		check(svdb_connection_disconnect(&db));
	}

cleanup:
	svdb_connection_close(&db);
	bdestroy(dbpathutf8);
	bdestroy(dbpathtestempty);
	bdestroy(strGot);
	bdestroy(dbpathreopen);
	return currenterr;
}

check_result test_tbl_properties(svdb_connection* db)
{
	sv_result currenterr = {};
	bstrList* list = bstrListCreate();
	bstrList* listGot = bstrListCreate();
	bstring strGot = bstring_open();
	svdb_txn txn = {};
	check(svdb_txn_open(&txn, db));

	/* reading from non-existent properties should return default value */
	uint32_t intGot = UINT32_MAX;
	bassigncstr(strGot, "text");
	bstrListAppendCstr(listGot, "text");
	check(svdb_propertygetint(db, str_and_len32s("NonExistInt"), &intGot));
	check(svdb_propertygetstr(db, str_and_len32s("NonExistStr"), strGot));
	check(svdb_propertygetstrlist(db, str_and_len32s("NonExistStrList"), listGot));
	TestEqn(0, intGot);
	TestEqs("", cstr(strGot));
	TestEqList("", listGot);

	/* item values can be replaced */
	check(svdb_propertysetint(db, str_and_len32s("TestInt"), 123));
	check(svdb_propertysetint(db, str_and_len32s("TestInt"), 456));
	check(svdb_propertysetstr(db, str_and_len32s("TestStr"), "hello"));
	check(svdb_propertysetstr(db, str_and_len32s("TestStr"), "otherstring"));
	check(svdb_propertygetint(db, str_and_len32s("TestInt"), &intGot));
	check(svdb_propertygetstr(db, str_and_len32s("TestStr"), strGot));
	TestEqn(456, intGot);
	TestEqs("otherstring", cstr(strGot));

	/* there should be 3 rows now */
	uint64_t countrows = 0;
	check(svdb_propertygetcount(db, &countrows));
	TestEqn(3, countrows);

	/* set and get list with 3 items */
	bstrListClear(list);
	bstrListAppendCstr(list, "item1");
	bstrListAppendCstr(list, "item2");
	bstrListAppendCstr(list, "item3");
	check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
	bstrListClear(listGot);
	check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), listGot));
	TestEqList("item1|item2|item3", listGot);

	/* set and get list with 1 item */
	bstrListClear(list);
	bstrListAppendCstr(list, "item1");
	check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
	bstrListClear(listGot);
	check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), listGot));
	TestEqList("item1", listGot);

	/* set and get list with 0 items */
	bstrListClear(list);
	check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
	bstrListClear(listGot);
	bstrListAppendCstr(listGot, "text");
	check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), listGot));
	TestEqn(0, listGot->qty);

	/* set and get list with item with pipe char */
	bstrListClear(list);
	bstrListAppendCstr(list, "pipechar|isok");
	check(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list));
	bstrListClear(listGot);
	check(svdb_propertygetstrlist(db, str_and_len32s("TestList"), listGot));
	TestEqn(1, listGot->qty);
	TestEqs("pipechar|isok", bstrListViewAt(listGot, 0));

	/* setting invalid string into list should fail */
	bstrListClear(list);
	bstrListAppendCstr(list, "item1");
	bstrListAppendCstr(list, "invalid|||||invalid");
	set_debugbreaks_enabled(false);
	expect_err_with_message(svdb_propertysetstrlist(db, str_and_len32s("TestList"), list), "cannot include");
	set_debugbreaks_enabled(true);

	/* there should only be 4 rows now */
	check(svdb_propertygetcount(db, &countrows));
	TestEqn(4, countrows);
	check(svdb_txn_rollback(&txn, db));
cleanup:
	svdb_txn_close(&txn, db);
	bdestroy(strGot);
	bstrListDestroy(list);
	bstrListDestroy(listGot);
	return currenterr;
}

check_result test_tbl_fileslist_iterate_rows(void* context, unusedboolptr(),
	const svdb_files_row* row, const bstring path)
{
	bstrList* list = (bstrList*)context;
	bstring s = bstring_open();
	svdb_files_row_string(row, s);
	bconcat(s, path);
	bstrListAppendCstr(list, cstr(s));
	bdestroy(s);
	return OK;
}

check_result test_tbl_contentslist_iterate_rows(void* context, unusedboolptr(),
	const svdb_contents_row* row)
{
	bstrList* list = (bstrList*)context;
	bstring s = bstring_open();
	svdb_contents_row_string(row, s);
	bstrListAppendCstr(list, cstr(s));
	bdestroy(s);
	return OK;
}

check_result test_tbl_fileslist(svdb_connection* db)
{
	sv_result currenterr = {};
	bstring path1 = bstring_open();
	bstring path2 = bstring_open();
	bstring path3 = bstring_open();
	bstring srowexpect = bstring_open();
	bstring srowgot = bstring_open();
	svdb_txn txn = {};

	svdb_files_row row1 = { 0,
		5 /*contents_id*/, 5000ULL * 1024 * 1024, /*contents_length*/
		55 /*flags*/, 555 /* last_write_time*/,
		1111 /*most_recent_collection*/, sv_filerowstatus_complete };
	svdb_files_row row2 = { 0,
		6 /*contents_id*/, 6000ULL * 1024 * 1024, /*contents_length*/
		66 /*flags*/, 666 /* last_write_time*/,
		1111 /*most_recent_collection*/, sv_filerowstatus_queued };
	svdb_files_row row3 = { 0,
		7 /*contents_id*/, 7000ULL * 1024 * 1024, /*contents_length*/
		77 /*flags*/, 777 /* last_write_time*/,
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
		check(svdb_filesinsert(db, path1, 1, sv_filerowstatus_queued, &row1.id));
		check(svdb_filesupdate(db, &row1));
		check(svdb_filesinsert(db, path2, 2, sv_filerowstatus_queued, &row2.id));
		check(svdb_filesupdate(db, &row2));
		check(svdb_filesinsert(db, path3, 3, sv_filerowstatus_queued, &row3.id));
		check(svdb_filesupdate(db, &row3));
	}
	{ /* get by path */
		svdb_files_row rowgot = { 0 };
		/* I'd use memcmp to compare, but padding after the struct apparently throws it off */
		memset(&rowgot, 0, sizeof(rowgot));
		check(svdb_filesbypath(db, path1, &rowgot));
		svdb_files_row_string(&row1, srowexpect);
		svdb_files_row_string(&rowgot, srowgot);
		TestEqs(cstr(srowexpect), cstr(srowgot));
		memset(&rowgot, 0, sizeof(rowgot));
		check(svdb_filesbypath(db, path2, &rowgot));
		svdb_files_row_string(&row2, srowexpect);
		svdb_files_row_string(&rowgot, srowgot);
		TestEqs(cstr(srowexpect), cstr(srowgot));
		memset(&rowgot, 0, sizeof(rowgot));
		check(svdb_filesbypath(db, path3, &rowgot));
		svdb_files_row_string(&row3, srowexpect);
		svdb_files_row_string(&rowgot, srowgot);
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
		expect_err_with_message(svdb_filesupdate(db, &rowtest), "changed no rows");
		set_debugbreaks_enabled(true);
	}
	{ /* query status less than */
		bstrList* list = bstrListCreate();
		check(svdb_files_in_queue(db, sv_makestatus(1111, sv_filerowstatus_complete), list, &test_tbl_fileslist_iterate_rows));
		TestEqn(list->qty, 2);
		svdb_files_row_string(&row2, srowexpect);
		bconcat(srowexpect, path2);
		TestEqs(cstr(srowexpect), bstrListViewAt(list, 0));
		svdb_files_row_string(&row3, srowexpect);
		bconcat(srowexpect, path3);
		TestEqs(cstr(srowexpect), bstrListViewAt(list, 1));
		bstrListDestroy(list);
	}
	{ /* query status less than, matches no rows */
		bstrList* list = bstrListCreate();
		check(svdb_files_in_queue(db, sv_makestatus(1, sv_filerowstatus_complete), list, &test_tbl_fileslist_iterate_rows));
		TestEqn(list->qty, 0);
		bstrListDestroy(list);
	}
	{ /* query status less than, matches all rows */
		bstrList* list = bstrListCreate();
		check(svdb_files_in_queue(db, svdb_files_in_queue_get_all, list, &test_tbl_fileslist_iterate_rows));
		TestEqn(list->qty, 4);
		bstrListDestroy(list);
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
		svdb_files_row_string(&row1, srowexpect);
		svdb_files_row_string(&rowgot, srowgot);
		TestEqs(cstr(srowexpect), cstr(srowgot));

		/* row 2 should now be gone */
		memset(&rowgot, 1, sizeof(rowgot));
		check(svdb_filesbypath(db, path2, &rowgot));
		TestEqn(0, rowgot.id);
		sv_array_close(&arr);
	}
	{ /* test row-to-string */
		svdb_files_row_string(&row1, srowexpect);
		TestEqs("contents_length=5, contents_id=5242880000, last_write_time=55, "
			"flags=555, most_recent_collection=1111, e_status=3, id=2", cstr(srowexpect));
	}

	check(svdb_txn_rollback(&txn, db));
cleanup:
	bdestroy(path1);
	bdestroy(path2);
	bdestroy(path3);
	bdestroy(srowexpect);
	bdestroy(srowgot);
	svdb_txn_close(&txn, db);
	return currenterr;
}

check_result test_tbl_collections(svdb_connection* db)
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
		uint64_t rowidGot = UINT64_MAX;
		check(svdb_collectiongetlast(db, &rowidGot));
		TestEqn(0, rowidGot);
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
		uint64_t rowidGot = 0;
		check(svdb_collectiongetlast(db, &rowidGot));
		TestEqn(row3.id, rowidGot);
	}
	{ /* read collections */
		sv_array arr = sv_array_open(sizeof32u(svdb_collections_row), 0);
		check(svdb_collectionsget(db, &arr, true));
		TestEqn(3, arr.length);
		/* remember that the results should be sorted in reverse order */
		svdb_collections_row* rowGot = (svdb_collections_row*)sv_array_at(&arr, 0);
		svdb_collectiontostring(&row3, true, true, srowexpected);
		svdb_collectiontostring(rowGot, true, true, srowgot);
		TestEqs(cstr(srowexpected), cstr(srowgot));
		rowGot = (svdb_collections_row*)sv_array_at(&arr, 1);
		svdb_collectiontostring(&row2, true, true, srowexpected);
		svdb_collectiontostring(rowGot, true, true, srowgot);
		TestEqs(cstr(srowexpected), cstr(srowgot));
		rowGot = (svdb_collections_row*)sv_array_at(&arr, 2);
		svdb_collectiontostring(&row1, true, true, srowexpected);
		svdb_collectiontostring(rowGot, true, true, srowgot);
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

check_result test_tbl_contents(svdb_connection* db)
{
	sv_result currenterr = {};
	bstring srowexpected = bstring_open();
	bstring srowgot = bstring_open();
	svdb_txn txn = {};

	svdb_contents_row rowgot = { 0 };
	svdb_contents_row row1 = { 0,
		1000ULL * 1024 * 1024 /*contents_length*/,
		1 /* most_recent_collection */, 11 /*original_collection*/,
		111 /* archivenumber*/,
			{{ 0x1111111111111111ULL, 0x2222222222222222ULL, 
			0x3333333333333333ULL, 0x4444444444444444ULL }}, /*hash*/
		0x11111111 /* crc32 */ };
	svdb_contents_row row2 = { 0,
		2000ULL * 1024 * 1024 /*contents_length*/,
		2 /* most_recent_collection */, 22 /*original_collection*/,
		222 /* archivenumber*/,
			{{ 0x2222222222222222ULL, 0x3333333333333333ULL,
			0x4444444444444444ULL, 0x5555555555555555ULL }}, /*hash*/
		0x22222222 /* crc32 */ };
	svdb_contents_row row3 = { 0,
		3000ULL * 1024 * 1024 /*contents_length*/,
		3 /* most_recent_collection */, 33 /*original_collection*/,
		333 /* archivenumber*/,
			{{ 0x3333333333333333ULL, 0x4444444444444444ULL,
			0x5555555555555555ULL, 0x6666666666666666ULL }}, /*hash*/
		0x33333333 /* crc32 */ };

	check(svdb_txn_open(&txn, db));

	{ /* test row iteration when empty */
		bstrList* list = bstrListCreate();
		check(svdb_contentsiter(db, list, &test_tbl_contentslist_iterate_rows));
		TestEqn(0, list->qty);
		bstrListDestroy(list);
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
			0x4444444444444444ULL, 0x5555555555555556ULL, /* note final 6 */ }};
		check(svdb_contentsbyhash(db, &hash, 2000ULL * 1024 * 1024, &rowgot));
		TestEqn(0, rowgot.id);
	}
	{ /* wrong length and wrong hash */
		memset(&rowgot, 1, sizeof(rowgot));
		hash256 hash = {{ 0x2222222222222222ULL, 0x3333333333333333ULL,
			0x4444444444444444ULL, 0x5555555555555556ULL, /* note final 6 */ }};
		check(svdb_contentsbyhash(db, &hash, 1234, &rowgot));
		TestEqn(0, rowgot.id);
	}
	{ /* test row iteration */
		bstrList* list = bstrListCreate();
		check(svdb_contentsiter(db, list, &test_tbl_contentslist_iterate_rows));
		TestEqn(3, list->qty);
		svdb_contents_row_string(&row1, srowexpected);
		TestEqs(cstr(srowexpected), bstrListViewAt(list, 0));
		svdb_contents_row_string(&row2, srowexpected);
		TestEqs(cstr(srowexpected), bstrListViewAt(list, 1));
		svdb_contents_row_string(&row3, srowexpected);
		TestEqs(cstr(srowexpected), bstrListViewAt(list, 2));
		bstrListDestroy(list);
	}
	{ /* test row-to-string */
		svdb_contents_row_string(&row1, srowexpected);
		TestEqs("hash=1111111111111111 2222222222222222 3333333333333333 4444444444444444, "
			"crc32=11111111, contents_length=1048576000, most_recent_collection=1, original_collection=11, "
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

check_result test_usergroup_persist(svdb_connection* db)
{
	sv_result currenterr = {};
	svdp_backupgroup group = {};
	svdp_backupgroup groupgot = {};
	group.approx_archive_size_bytes = 111;
	group.compact_threshold_bytes = 222;
	group.days_to_keep_prev_versions = 333;
	group.encryption = bfromcstr("testencryption");
	group.exclusion_patterns = bstrListCreate();
	group.root_directories = bstrListCreate();
	group.separate_metadata_enabled = 444;
	group.groupname = bfromcstr("testgroup");
	bstrListSplitCstr(group.exclusion_patterns, "*.aaa|*.bbb|*.ccc", "|");
	bstrListSplitCstr(group.root_directories, "/path/1|/path/2", "|");
	check(svdp_backupgroup_persist(db, &group));
	
	check(svdp_backupgroup_load(db, &groupgot, "testgroup"));
	TestEqn(groupgot.approx_archive_size_bytes, 111);
	TestEqn(groupgot.compact_threshold_bytes, 222);
	TestEqn(groupgot.days_to_keep_prev_versions, 333);
	TestEqs("testencryption", cstr(groupgot.encryption));
	TestEqList("*.aaa|*.bbb|*.ccc", groupgot.exclusion_patterns);
	TestEqList("/path/1|/path/2", groupgot.root_directories);
	TestEqn(groupgot.separate_metadata_enabled, 444);
	TestEqs("testgroup", cstr(groupgot.groupname));
cleanup:
	svdp_backupgroup_close(&group);
	svdp_backupgroup_close(&groupgot);
	return currenterr;
}

check_result test_db_rows(const char* dir)
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

check_result get_tar_archive_parameter(const char* archive, bstring sout);

check_result test_archiver_tar(const char* dir)
{
	sv_result currenterr = {};
	svdp_archiver archiver = {};
	bstring restoreto = bformat("%s%srestoreto", dir, pathsep);
	bstring path = bstring_open();
	bstring contents = bstring_open();
	bstring current_tar = bstring_open();
	check_b(os_create_dirs(cstr(restoreto)), "os_create_dirs failed");
	check(os_tryuntil_deletefiles(dir, "*"));
	check(os_tryuntil_deletefiles(cstr(restoreto), "*"));
	check(svdp_archiver_open(&archiver, dir, "test", 0, 0, ""));
	check(checkexternaltoolspresent(&archiver, 0 /* use ffmpeg */));

	if (islinux) 
	{
		check(get_tar_archive_parameter("/path/to/file", archiver.strings.paramInput));
		TestEqs("--file=/path/to/file", cstr(archiver.strings.paramInput));
		set_debugbreaks_enabled(false);
		expect_err_with_message(get_tar_archive_parameter("relative/path", archiver.strings.paramInput), "require absolute path");
		set_debugbreaks_enabled(true);
	}
	else
	{
		check(get_tar_archive_parameter("X:\\path\\to\\file", archiver.strings.paramInput));
		TestEqs("--file=/X/path/to/file", cstr(archiver.strings.paramInput));
		set_debugbreaks_enabled(false);
		expect_err_with_message(get_tar_archive_parameter("relative/path", archiver.strings.paramInput), "require absolute path");
		expect_err_with_message(get_tar_archive_parameter("\\\\uncshare\\path", archiver.strings.paramInput), "require absolute path");
		expect_err_with_message(get_tar_archive_parameter("\\fromroot\\path", archiver.strings.paramInput), "require absolute path");
		set_debugbreaks_enabled(true);
	}
	{ /* add a small file */
		bassignformat(current_tar, "%s%stest-small.tar", dir, pathsep);
		TestTrue(!os_file_exists(cstr(current_tar)));
		check(tmpwritetextfile(dir, "small.txt", path, "abcde"));
		check(svdp_tar_add(&archiver, cstr(current_tar), cstr(path)));
		TestTrue(os_getfilesize(cstr(current_tar)) > 0);
	}
	{ /* extract the small file we added */
		bassignformat(current_tar, "%s%stest-small.tar", dir, pathsep);
		bassignformat(path, "%s%ssmall.txt", cstr(restoreto), pathsep);
		TestTrue(!os_file_exists(cstr(path)));
		check(svdp_7z_extract_overwrite_ok(&archiver, cstr(current_tar), NULL, "small.txt", cstr(restoreto)));
		check(sv_file_readfile(cstr(path), contents));
		TestEqs("abcde", cstr(contents));
	}
	{ /* attempt to add a non-existant file */
		bassignformat(current_tar, "%s%stest-add-missing.tar", dir, pathsep);
		bassignformat(path, "%s%sdoes-not-exist.txt", dir, pathsep);
		set_debugbreaks_enabled(false);
		expect_err_with_message(svdp_tar_add(&archiver, cstr(current_tar), cstr(path)), "Cannot stat: No such file");
		set_debugbreaks_enabled(true);
	}
	{ /* attempt to add an inaccessible file */
		bassignformat(current_tar, "%s%stest-add-inaccessible.tar", dir, pathsep);
		bassignformat(path, "%s%slockedfile.txt", dir, pathsep);
		check(sv_file_writefile(cstr(path), "some text", "wb"));
		os_exclusivefilehandle handle = {};
		check(os_exclusivefilehandle_open(&handle, cstr(path), false /* don't allow read */, NULL));
		set_debugbreaks_enabled(false);
		expect_err_with_message(svdp_tar_add(&archiver, cstr(current_tar), cstr(path)), "Cannot open: Permission denied");
		set_debugbreaks_enabled(true);
		os_exclusivefilehandle_close(&handle);
	}
	{ /* attempt to add over the top of a locked tar file should fail */
		bassignformat(current_tar, "%s%stest-locked-7z.tar", dir, pathsep);
		bassignformat(path, "%s%ssmall.txt", dir, pathsep);
		check(sv_file_writefile(cstr(current_tar), "some text", "wb"));
		os_exclusivefilehandle handle = {};
		check(os_exclusivefilehandle_open(&handle, cstr(current_tar), true /* allow read but not write */, NULL));
		set_debugbreaks_enabled(false);
		expect_err_with_message(svdp_tar_add(&archiver, cstr(current_tar), cstr(path)), "Bad file");
		set_debugbreaks_enabled(true);
		os_exclusivefilehandle_close(&handle);
	}
	{ /* attempt to extract from non-existant archive */
		bassignformat(current_tar, "%s.notfound.tar", dir);
		set_debugbreaks_enabled(false);
		expect_err_with_message(svdp_7z_extract_overwrite_ok(&archiver, cstr(current_tar), NULL, 
			"small.txt", cstr(restoreto)), "not found");

		set_debugbreaks_enabled(true);
	}
	{ /* attempt to extract non-existant file from an archive, expect to fail silently  */
		bassignformat(current_tar, "%s%stest-small.tar", dir, pathsep);
		TestTrue(os_file_exists(cstr(current_tar)));
		check(svdp_7z_extract_overwrite_ok(&archiver, cstr(current_tar), NULL, "not-in-archive.txt", cstr(restoreto)));
		bassignformat(path, "%s%snot-in-archive.txt", cstr(restoreto), pathsep);
		TestTrue(!os_file_exists(cstr(path)));
	}

cleanup:
	bdestroy(current_tar);
	bdestroy(restoreto);
	bdestroy(path);
	bdestroy(contents);
	svdp_archiver_close(&archiver);
	return currenterr;
}

check_result test_archiver_7z(const char* dir)
{
	sv_result currenterr = {};
	svdp_archiver archiver = {};

	/* make a mock ready-to-upload */
	bstring path = bstring_open();
	bstring restoreto = bformat("%s%srestoreto", dir, pathsep);
	bstring contents = bstring_open();
	bstring readytoupload = bformat("%s%suserdata%stest%sreadytoupload", dir, pathsep, pathsep, pathsep);
	bstring large = bstring_open();
	bstring current7z = bstring_open();

	bFill(large, 'a', 250 * 1024);
	check_b(os_create_dirs(cstr(readytoupload)), "os_create_dirs failed");
	
	/* run the following tests twice, once without encryption, once with */
	for (int shouldencrypt = 0; shouldencrypt <= 1; shouldencrypt++)
	{
		check(os_tryuntil_deletefiles(cstr(readytoupload), "*"));
		check(os_tryuntil_deletefiles(cstr(restoreto), "*"));
		check(os_tryuntil_deletefiles(dir, "*"));
		check_b(os_create_dirs(cstr(restoreto)), "os_create_dirs failed");
	
		/* create an archiver object */
		check(svdp_archiver_open(&archiver, dir, "test", 0, 0, shouldencrypt ? "encrypted" : ""));
		check(svdp_archiver_beginarchiving(&archiver));
		TestTrue(s_equal(cstr(readytoupload), cstr(archiver.readydir)));
		check(checkexternaltoolspresent(&archiver, 1 /* use ffmpeg */));
	
		{ /* add a small file */
			bassignformat(current7z, "%s%stest-small.7z", cstr(archiver.workingdir), pathsep);
			TestTrue(!os_file_exists(cstr(current7z)));
			check(tmpwritetextfile(dir, "small.txt", path, "abcde"));
			check(svdp_7z_add(&archiver, cstr(current7z), cstr(path), false /*alreadycompressed*/));
			TestTrue(os_getfilesize(cstr(current7z)) > 0);
		}
		{ /* extract the small file we added */
			bassignformat(current7z, "%s%stest-small.7z", cstr(archiver.workingdir), pathsep);
			check(svdp_7z_extract_overwrite_ok(&archiver, cstr(current7z), cstr(archiver.encryption), "small.txt", cstr(restoreto)));
			bassignformat(path, "%s%ssmall.txt", cstr(restoreto), pathsep);
			check(sv_file_readfile(cstr(path), contents));
			TestEqs("abcde", cstr(contents));
		}
		{ /* add a large file, with no compression. */
			bassignformat(current7z, "%s%stest-large.7z", cstr(archiver.workingdir), pathsep);
			check(tmpwritetextfile(dir, "large.txt", path, cstr(large)));
			check(svdp_7z_add(&archiver, cstr(current7z), cstr(path), true /*alreadycompressed*/));
			TestTrue(os_getfilesize(cstr(current7z)) > 240 * 1024);
		}
		{ /* extract the large file we added */
			bassignformat(current7z, "%s%stest-large.7z", cstr(archiver.workingdir), pathsep);
			check(svdp_7z_extract_overwrite_ok(&archiver, cstr(current7z), cstr(archiver.encryption), "large.txt", cstr(restoreto)));
			bassignformat(path, "%s%slarge.txt", cstr(restoreto), pathsep);
			check(sv_file_readfile(cstr(path), contents));
			TestEqs(cstr(large), cstr(contents));
		}
		{ /* extraction should be ok with overwriting an existing file */
			bassignformat(current7z, "%s%stest-small.7z", cstr(archiver.workingdir), pathsep);
			bassignformat(path, "%s%ssmall.txt", cstr(restoreto), pathsep);
			check(sv_file_writefile(cstr(path), "this-should-be-overwritten", "wb"));
			check(svdp_7z_extract_overwrite_ok(&archiver, cstr(current7z), cstr(archiver.encryption), "small.txt", cstr(restoreto)));
			check(sv_file_readfile(cstr(path), contents));
			TestEqs("abcde", cstr(contents));
		}
		{ /* attempt to add a non-existant file */
			bassignformat(current7z, "%s%stest-add-missing.7z", cstr(archiver.workingdir), pathsep);
			bassignformat(path, "%s%sdoes-not-exist.txt", dir, pathsep);
			set_debugbreaks_enabled(false);
			expect_err_with_message(svdp_7z_add(&archiver, cstr(current7z), cstr(path), false /*alreadycompressed*/), 
				"The system cannot find the file specified.");
			set_debugbreaks_enabled(true);
		}
		{ /* we should not be able to re-use a 7z. max of one file per 7z. */
			bassignformat(current7z, "%s%stest-large.7z", cstr(archiver.workingdir), pathsep);
			bassignformat(path, "%s%ssmall.txt", dir, pathsep);
			set_debugbreaks_enabled(false);
			expect_err_with_message(svdp_7z_add(&archiver, cstr(current7z), cstr(path), false /*alreadycompressed*/), 
				"should not exist yet");
			set_debugbreaks_enabled(true);
		}
		{ /* attempt to add an inaccessible file */
			bassignformat(current7z, "%s%stest-add-inaccessible.7z", cstr(archiver.workingdir), pathsep);
			bassignformat(path, "%s%slockedfile.txt", dir, pathsep);
			check(sv_file_writefile(cstr(path), "some text", "wb"));
			os_exclusivefilehandle handle = {};
			check(os_exclusivefilehandle_open(&handle, cstr(path), false /* don't allow read */, NULL));
			set_debugbreaks_enabled(false);
			expect_err_with_message(svdp_7z_add(&archiver, cstr(current7z), cstr(path), false /*alreadycompressed*/), 
				"cannot access the file");
			set_debugbreaks_enabled(true);
			os_exclusivefilehandle_close(&handle);
		}
		{ /* attempt to add over the top of a locked 7z file should fail */
			bassignformat(current7z, "%s%stest-locked-7z.7z", cstr(archiver.workingdir), pathsep);
			bassignformat(path, "%s%ssmall.txt", dir, pathsep);
			check(sv_file_writefile(cstr(current7z), "some text", "wb"));
			os_exclusivefilehandle handle = {};
			check(os_exclusivefilehandle_open(&handle, cstr(current7z), true /* allow read but not write */, NULL));
			set_debugbreaks_enabled(false);
			expect_err_with_message(svdp_7z_add(&archiver, cstr(current7z), cstr(path), false /*alreadycompressed*/), 
				"should not exist yet");
			set_debugbreaks_enabled(true);
			os_exclusivefilehandle_close(&handle);
		}
		{ /* attempt to extract from non-existant archive */
			bassignformat(current7z, "%s.notfound.7z", cstr(archiver.workingdir));
			set_debugbreaks_enabled(false);
			expect_err_with_message(svdp_7z_extract_overwrite_ok(&archiver, cstr(current7z), cstr(archiver.encryption), 
				"small.txt", cstr(restoreto)), "not found");
			set_debugbreaks_enabled(true);
		}
		{ /* attempt to extract non-existant file from an archive, expect to fail silently  */
			bassignformat(current7z, "%s%stest-large.7z", cstr(archiver.workingdir), pathsep);
			TestTrue(os_file_exists(cstr(current7z)));
			check(svdp_7z_extract_overwrite_ok(&archiver, cstr(current7z), cstr(archiver.encryption), "not-in-archive.txt", 
				cstr(restoreto)));
			bassignformat(path, "%s%snot-in-archive.txt", cstr(restoreto), pathsep);
			TestTrue(!os_file_exists(cstr(path)));
		}
		svdp_archiver_close(&archiver);
	}

cleanup:
	bdestroy(large);
	bdestroy(path);
	bdestroy(contents);
	bdestroy(readytoupload);
	bdestroy(restoreto);
	svdp_archiver_close(&archiver);
	return currenterr;
}

void setfileaccessiblity(operations_test_hook* hook, int number, bool val)
{
	TestTrue(number >= 0 && number < countof(hook->filelocks));
	if (val)
	{
		os_exclusivefilehandle_close(&hook->filelocks[number]);
	}
	else
	{
		check_fatal(!hook->filelocks[number].os_handle, "cannot not lock twice");
		check_warn(os_exclusivefilehandle_open(
			&hook->filelocks[number], cstr(hook->filenames[number]), false, NULL), "", exit_on_err);
	}
}

void operations_test_hook_close(operations_test_hook* self)
{
	for (int i = 0; i < countof(self->filenames); i++)
	{
		os_exclusivefilehandle_close(&self->filelocks[i]);
		bdestroy(self->filenames[i]);
	}

	bdestroy(self->tmp);
	bdestroy(self->dirfakeuserfiles);
	bdestroy(self->pathUntar);
	bdestroy(self->pathRestoreTo);
	bdestroy(self->pathReadyDelete);
	bdestroy(self->pathReadyUpload);
	bstrListDestroy(self->allcontentrows);
	bstrListDestroy(self->allfilelistrows);
}

check_result test_hook_get_file_info(
	void* phook, os_exclusivefilehandle* self, uint64_t* unused, uint64_t* modtime, uint64_t* permissions)
{
	operations_test_hook* hook = (operations_test_hook*)phook;
	if (hook)
	{
		const char* filename = cstr(self->loggingcontext);
		TestTrue(strlen(filename) > 10);
		int filenumber = filename[strlen(filename) - 5] - '0';
		TestTrue(filenumber >= 0 && filenumber < countof(hook->fakelastmodtimes));
		*modtime = hook->fakelastmodtimes[filenumber];
		*permissions = cast32s32u(111 * filenumber);
	}
	
	(void)unused;
	return OK;
}

check_result test_hook_call_before_processing_queue(void* phook, svdb_connection* db)
{
	operations_test_hook* hook = (operations_test_hook*)phook;
	if (hook)
	{
		bstrListClear(hook->allcontentrows);
		bstrListClear(hook->allfilelistrows);
		check_warn(svdb_contentsiter(
			db, hook->allcontentrows, &test_tbl_contentslist_iterate_rows), "", exit_on_err);

		check_warn(svdb_files_in_queue(
			db, svdb_files_in_queue_get_all, hook->allfilelistrows, &test_tbl_fileslist_iterate_rows), "", exit_on_err);
		if (hook->stage_of_test == 1)
		{
			TestEqn(0, hook->allcontentrows->qty);
			TestEqn(6, hook->allfilelistrows->qty);
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrListViewAt(hook->allfilelistrows, 5)));
		}
		else if (hook->stage_of_test == 2)
		{
			TestEqn(6, hook->allcontentrows->qty);
			TestEqn(6, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestEqs("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrListViewAt(hook->allcontentrows, 4));
			TestEqs("hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=6", bstrListViewAt(hook->allcontentrows, 5));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=1, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=6, last_write_time=1, flags=555, most_recent_collection=1, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrListViewAt(hook->allfilelistrows, 5)));
		}
		else if (hook->stage_of_test == 3)
		{
			TestEqn(6, hook->allcontentrows->qty);
			TestEqn(6, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestEqs("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrListViewAt(hook->allcontentrows, 4));
			TestEqs("hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=6", bstrListViewAt(hook->allcontentrows, 5));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=6, last_write_time=1, flags=555, most_recent_collection=2, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrListViewAt(hook->allfilelistrows, 5)));
		}
		else if (hook->stage_of_test == 4)
		{
			TestEqn(9, hook->allcontentrows->qty);
			TestEqn(6, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestEqs("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrListViewAt(hook->allcontentrows, 4));
			TestEqs("hash=9f50c1fddbe36b6 63d240fdb8fd9607 af2f66dfcc27af96 e5b9aed399a6767f, crc32=3d8ef70a, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=6", bstrListViewAt(hook->allcontentrows, 5));
			TestEqs("hash=4e228734f4417f88 f176ff37b912b345 1aa7c6aa85f440aa d6bad0d10f7cd5b2, crc32=9215b3c6, contents_length=19, most_recent_collection=2, original_collection=2, archivenumber=1, id=7", bstrListViewAt(hook->allcontentrows, 6));
			TestEqs("hash=1f670f96631263a0 44d3d56cdd109b5f 793f361d46f9a315 bd7ffe48ae75ebe4, crc32=e8deaef, contents_length=10, most_recent_collection=2, original_collection=2, archivenumber=1, id=8", bstrListViewAt(hook->allcontentrows, 7));
			TestEqs("hash=59547001a163df77 25b6786ba1ab3f71 9780a1194748b574 3ce7a9f21f22f5a2, crc32=dd48b016, contents_length=19, most_recent_collection=2, original_collection=2, archivenumber=1, id=9", bstrListViewAt(hook->allcontentrows, 8));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.txt", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.txt", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=19, contents_id=7, last_write_time=1, flags=222, most_recent_collection=2, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.txt", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=2, flags=333, most_recent_collection=2, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.txt", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=8, last_write_time=2, flags=444, most_recent_collection=2, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.txt", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=19, contents_id=9, last_write_time=2, flags=555, most_recent_collection=2, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.txt", bstrListViewAt(hook->allfilelistrows, 5)));
		}
		else if (hook->stage_of_test == 5)
		{
			/* file 2: file seen previously, removed after directory iteration */
			TestTrue(os_remove(cstr(hook->filenames[2])));
			hook->fakelastmodtimes[2] = 0;
			/* file 4: file seen previously, inaccessible after directory iteration */
			setfileaccessiblity(hook, 4, false);
			/* file 5: new file, removed after directory iteration */
			TestTrue(os_remove(cstr(hook->filenames[5])));
			hook->fakelastmodtimes[5] = 0;
			setfileaccessiblity(hook, 6, false);
			/* file 9: new file, content is changed between adding-to-queue and adding-to-archive */
			check_warn(sv_file_writefile(cstr(hook->filenames[9]), "changed-second-with-more", "wb"), "", exit_on_err);
			TestEqn(5, hook->allcontentrows->qty);
			TestEqn(10, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestEqs("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=5", bstrListViewAt(hook->allcontentrows, 4));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=0, id=1*" pathsep "file0.txt", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file1.txt", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=3, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file2.txt", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file3.txt", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file4.txt", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file5.txt", bstrListViewAt(hook->allfilelistrows, 5)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=7*" pathsep "file6.txt", bstrListViewAt(hook->allfilelistrows, 6)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=8*" pathsep "file7.txt", bstrListViewAt(hook->allfilelistrows, 7)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=9*" pathsep "file8.txt", bstrListViewAt(hook->allfilelistrows, 8)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=10*" pathsep "file9.txt", bstrListViewAt(hook->allfilelistrows, 9)));
		}
		else if (hook->stage_of_test == 6)
		{
			TestEqn(7, hook->allcontentrows->qty);
			TestEqn(6, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=c8dfdcd0fbd5415f 4bf8394415f5020f 8ae21fd0d5542d16 b212fddd1f288cb6, crc32=3ae33313, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=8fed55346e6cd6e9 449a4ea5c3f455fb ba4e41e76f83be39 126788bb2ea203a5, crc32=a3ea62a9, contents_length=10, most_recent_collection=1, original_collection=1, archivenumber=1, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=20c77a8888e552d8 4f072594795234d4 bddc24838808c52e 7a16f388f1e80710, crc32=d4ed523f, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestEqs("hash=85b8a35500a71107 4ebb74fe7b27f5e0 ef8e495615dc984d 864e5255399d02e0, crc32=4a89c79c, contents_length=10, most_recent_collection=2, original_collection=1, archivenumber=1, id=5", bstrListViewAt(hook->allcontentrows, 4));
			TestEqs("hash=ed9d76f043ec0c97 4c2f7ed570a007e7 95278b72e012766b 3b333ad4d06a8c51, crc32=618f9ed5, contents_length=18, most_recent_collection=2, original_collection=2, archivenumber=1, id=6", bstrListViewAt(hook->allcontentrows, 5));
			TestEqs("hash=1be52bb37faff0dc 484bb2ad5a0b67b3 670af01d0a133ad3 76ecb5a9eaf24830, crc32=7b206e22, contents_length=24, most_recent_collection=2, original_collection=2, archivenumber=1, id=7", bstrListViewAt(hook->allcontentrows, 6));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=2, last_write_time=2, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file0.txt", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=4, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file3.txt", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=10, contents_id=5, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file4.txt", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=18, contents_id=6, last_write_time=1, flags=777, most_recent_collection=2, e_status=3, id=8*" pathsep "file7.txt", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=18, contents_id=6, last_write_time=1, flags=888, most_recent_collection=2, e_status=3, id=9*" pathsep "file8.txt", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=24, contents_id=7, last_write_time=1, flags=999, most_recent_collection=2, e_status=3, id=10*" pathsep "file9.txt", bstrListViewAt(hook->allfilelistrows, 5)));
		}
		else if (hook->stage_of_test == 7)
		{
			TestEqn(0, hook->allcontentrows->qty);
			TestEqn(4, hook->allfilelistrows->qty);
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file0.jpg", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file1.jpg", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file2.jpg", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file3.jpg", bstrListViewAt(hook->allfilelistrows, 3)));
		}
		else if (hook->stage_of_test == 8)
		{
			TestEqn(4, hook->allcontentrows->qty);
			TestEqn(4, hook->allfilelistrows->qty);
			TestEqs("hash=cd270b526ba2e90d 722778efe7b2c29d abd68da25e8b1eb1 bd0c0cd7fa1d6ce7, crc32=646773f3, contents_length=40960, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=6475ff6c168ec2c4 bf46244f6a3386e1 c33373ced8062291 b19877758bf215d0, crc32=d39f5778, contents_length=40960, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=c0ea4a9032d68aeb 68b77b93c9c8e487 7a45f6bab2b0d83d fe4cff3d741bcad6, crc32=802ae62f, contents_length=20480, most_recent_collection=1, original_collection=1, archivenumber=2, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=9ea177d0cd42dbbd 947ed281720090cd da9d6580b6b3d2cf b41b3dfead68ed0, crc32=669eca11, contents_length=256001, most_recent_collection=1, original_collection=1, archivenumber=3, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestTrue(fnmatch_simple("contents_length=40960, contents_id=1, last_write_time=2, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file0.jpg", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=40960, contents_id=2, last_write_time=2, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file1.jpg", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=20480, contents_id=3, last_write_time=2, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file2.jpg", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=256001, contents_id=4, last_write_time=2, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file3.jpg", bstrListViewAt(hook->allfilelistrows, 3)));
		}
		else if (hook->stage_of_test == 9)
		{
			TestEqn(0, hook->allcontentrows->qty);
			TestEqn(5, hook->allfilelistrows->qty);
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrListViewAt(hook->allfilelistrows, 4)));
		}
		else if (hook->stage_of_test == 10)
		{
			TestEqn(2, hook->allcontentrows->qty);
			TestEqn(5, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1, most_recent_collection=1, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=1, last_write_time=1, flags=0, most_recent_collection=1, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=1, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=222, most_recent_collection=1, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=333, most_recent_collection=1, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=444, most_recent_collection=1, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrListViewAt(hook->allfilelistrows, 4)));
		}
		else if (hook->stage_of_test == 11)
		{
			TestEqn(2, hook->allcontentrows->qty);
			TestEqn(7, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=1, last_write_time=1, flags=0, most_recent_collection=2, e_status=0, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=222, most_recent_collection=2, e_status=0, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=333, most_recent_collection=2, e_status=0, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=444, most_recent_collection=2, e_status=0, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=6*" pathsep "file\xE1\x84\x81_5.mp3", bstrListViewAt(hook->allfilelistrows, 5)));
			TestTrue(fnmatch_simple("contents_length=0, contents_id=0, last_write_time=0, flags=0, most_recent_collection=0, e_status=0, id=7*" pathsep "file\xE1\x84\x81_6.mp3", bstrListViewAt(hook->allfilelistrows, 6)));
		}
		else if (hook->stage_of_test == 12)
		{
			TestEqn(5, hook->allcontentrows->qty);
			TestEqn(7, hook->allfilelistrows->qty);
			TestEqs("hash=fcd1572cd1921b1a 35718c89e8fdc127 d5332a8e5503b4b8 4efdba7c141ea729, crc32=4de40385, contents_length=1, most_recent_collection=1, original_collection=1, archivenumber=1, id=1", bstrListViewAt(hook->allcontentrows, 0));
			TestEqs("hash=611c9c9e3d1e62cc 1ffaa7d05b187124 0 0, crc32=b6a3e71a, contents_length=1, most_recent_collection=2, original_collection=1, archivenumber=1, id=2", bstrListViewAt(hook->allcontentrows, 1));
			TestEqs("hash=4d098b3072969adc 635dfe534db06698 d2307aea1e18b7b3 922bbf0481ef8e6b, crc32=95bcd25d, contents_length=1, most_recent_collection=2, original_collection=2, archivenumber=1, id=3", bstrListViewAt(hook->allcontentrows, 2));
			TestEqs("hash=6e1b0f02bb2220c4 aba9f88dca9fd76f 6d2e86e5f79b4f25 b039cedc3e4fd7af, crc32=b4e1484f, contents_length=1, most_recent_collection=2, original_collection=2, archivenumber=1, id=4", bstrListViewAt(hook->allcontentrows, 3));
			TestEqs("hash=7d49276b4e05c2bd 39f840313d6eeb4f 0 0, crc32=c5679762, contents_length=1, most_recent_collection=2, original_collection=2, archivenumber=1, id=5", bstrListViewAt(hook->allcontentrows, 4));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=3, last_write_time=2, flags=0, most_recent_collection=2, e_status=3, id=1*" pathsep "file\xE1\x84\x81_0.mp3", bstrListViewAt(hook->allfilelistrows, 0)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=111, most_recent_collection=2, e_status=3, id=2*" pathsep "file\xE1\x84\x81_1.mp3", bstrListViewAt(hook->allfilelistrows, 1)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=4, last_write_time=2, flags=222, most_recent_collection=2, e_status=3, id=3*" pathsep "file\xE1\x84\x81_2.mp3", bstrListViewAt(hook->allfilelistrows, 2)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=2, flags=333, most_recent_collection=2, e_status=3, id=4*" pathsep "file\xE1\x84\x81_3.mp3", bstrListViewAt(hook->allfilelistrows, 3)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=5, last_write_time=2, flags=444, most_recent_collection=2, e_status=3, id=5*" pathsep "file\xE1\x84\x81_4.mp3", bstrListViewAt(hook->allfilelistrows, 4)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=2, last_write_time=1, flags=555, most_recent_collection=2, e_status=3, id=6*" pathsep "file\xE1\x84\x81_5.mp3", bstrListViewAt(hook->allfilelistrows, 5)));
			TestTrue(fnmatch_simple("contents_length=1, contents_id=5, last_write_time=1, flags=666, most_recent_collection=2, e_status=3, id=7*" pathsep "file\xE1\x84\x81_6.mp3", bstrListViewAt(hook->allfilelistrows, 6)));
		}
		else if (hook->stage_of_test != -1)
		{
			alertdialog("New test stage. Printing the results for convenience.");
			bstring tmp = bformat("%s%s..%scode.c", cstr(hook->dirfakeuserfiles), pathsep, pathsep);
			FILE* f = fopen(cstr(tmp), "w");
			bstring searchfor = bformat("%s%s", cstr(hook->dirfakeuserfiles), pathsep);
			struct tagbstring replacewith = bsStatic("*\" pathsep \"");
			fprintf(f, "TestEqn(%d, hook->allcontentrows->qty);\n", hook->allcontentrows->qty);
			fprintf(f, "TestEqn(%d, hook->allfilelistrows->qty);\n", hook->allfilelistrows->qty);
			for (int i = 0; i < hook->allcontentrows->qty; i++)
			{
				const char* s = bstrListViewAt(hook->allcontentrows, i);
				fprintf(f, "TestEqs(\"%s\", bstrListViewAt(hook->allcontentrows, %d));\n", s, i);
			}
			for (int i = 0; i < hook->allfilelistrows->qty; i++)
			{
				const char* s = bstrListViewAt(hook->allfilelistrows, i);
				bassignformat(tmp, "TestTrue(fnmatch_simple(\"%s\", bstrListViewAt(hook->allfilelistrows, %d)));\n", s, i);
				bReplaceAll(tmp, searchfor, &replacewith, 0);
				fprintf(f, "%s", cstr(tmp));
			}
			fclose(f);
			bdestroy(tmp);
			bdestroy(searchfor);
		}
	}
	return OK;
}

check_result test_hook_provide_file_list(void* phook, void* context, FnRecurseThroughFilesCallback callback)
{
	sv_result currenterr = {};
	operations_test_hook* hook = (operations_test_hook*)phook;
	if (hook)
	{
		/* reasons for this test hook
		1) provide last-modified and flags, without needing to call SetModifiedTime
		2) add the files in a deterministic order, unlike dir iteration */
		for (int i = 0; i < countof(hook->filenames); i++)
		{
			if (hook->fakelastmodtimes[i])
			{
				check(callback(context, hook->filenames[i], hook->fakelastmodtimes[i],
					os_getfilesize(cstr(hook->filenames[i])), cast32s32u(111 * i)));
			}
		}
	}
cleanup:
	return currenterr;
}

check_result test_operations_backup_check_archivecontents(operations_test_hook* hook, const char* archivename,
	const char* expected)
{
	sv_result currenterr = {};
	bstrList* archivecontents = bstrListCreate();
	svdp_archiver arch = {};
	check(svdp_archiver_open(&arch, "/", "/", 0, 0, ""));
	check(checkexternaltoolspresent(&arch, 0));
	bstring archivepath = bformat("%s%s%s", cstr(hook->pathReadyUpload), pathsep, archivename);
	check(svdp_archiver_tar_list_string_testsonly(&arch, cstr(archivepath), archivecontents, cstr(hook->pathUntar)));
	if (s_equal(expected, "---"))
	{
		alertdialog("Printing the results for convenience.");
		bassignformat(hook->tmp, "%s%s..%scode.c", cstr(hook->dirfakeuserfiles), pathsep, pathsep);
		FILE* f = fopen(cstr(hook->tmp), "w");
		for (int i = 0; i < archivecontents->qty / 2; i++)
		{
			fprintf(f, "\"%s|%s|\"\n", bstrListViewAt(archivecontents, i * 2),
				bstrListViewAt(archivecontents, i * 2 + 1));
		}
		if (archivecontents->qty % 2 == 1)
		{
			fprintf(f, "\"%s\"", bstrListViewAt(archivecontents, archivecontents->qty - 1));
		}
		fclose(f);
	}
	else
	{
		int countparts = 0;
		for (int i = 0; i < strlen(expected); i++)
		{
			if (expected[i] == '|')
			{
				countparts++;
			}
		}

		/* a limitation in some versions of 7z is that all utf8 chars are printed as '_'
		(that's one reason why we don't use "7z l" in product code)
		so this test accepts both the correct \xE1\x84\x81 sequence or _ */
		bstring joined = bjoinStatic(archivecontents, "|");
		struct tagbstring find = bsStatic("\xE1\x84\x81");
		struct tagbstring replace = bsStatic("_");
		bReplaceAll(joined, &find, &replace, 0);
		TestEqs(cstr(joined), expected);
		bdestroy(joined);
	}

	bdestroy(archivepath);
cleanup:
	bstrListClear(archivecontents);
	svdp_archiver_close(&arch);
	return currenterr;
}

check_result test_operations_backup_reset(const svdp_application* app, const svdp_backupgroup* group, 
	svdb_connection* db, operations_test_hook* hook, int numberoffiles, const char* extension, bool unicode, bool runbackup)
{
	sv_result currenterr = {};
	bstring tmp = bstring_open();
	for (int i = 0; i < countof(hook->filenames); i++)
	{
		bdestroy(hook->filenames[i]);
		hook->filenames[i] = bformat("%s%sfile%s%d.%s", cstr(hook->dirfakeuserfiles), pathsep, 
			unicode ? "\xE1\x84\x81_" : "", i, extension);
		setfileaccessiblity(hook, i, true);
	}
	
	check(svdb_clear_database_content(db));
	check(os_tryuntil_deletefiles(cstr(hook->dirfakeuserfiles), "*"));
	check(os_tryuntil_deletefiles(cstr(hook->pathReadyUpload), "*"));
	check(os_tryuntil_deletefiles(cstr(hook->pathReadyDelete), "*"));
	check(os_tryuntil_deletefiles(cstr(hook->pathWorking), "*"));
	memset(hook->fakelastmodtimes, 0, sizeof(hook->fakelastmodtimes));
	for (int i = 0; i < numberoffiles; i++)
	{
		bassignformat(tmp, "contents-%d", i);
		check(sv_file_writefile(cstr(hook->filenames[i]), cstr(tmp), "wb"));
		hook->fakelastmodtimes[i] = 1;
	}
	
	if (runbackup)
	{
		bstring dbpath = bfromcstr(cstr(db->path));
		check(svdp_backup(app, group, db, hook));
		check(svdb_connection_disconnect(db));
		check(svdb_connection_open(db, cstr(dbpath)));
	}
cleanup:
	return currenterr;
}

check_result test_operations_backup(const svdp_application* app, svdp_backupgroup* group, svdb_connection* db, 
	operations_test_hook* hook)
{
	sv_result currenterr = {};
	sv_file file = {};
	bstring path = bstring_open();
	bstring dbpath = bfromcstr(cstr(db->path));
	TestTrue(os_create_dirs(cstr(hook->pathUntar)));
	check(os_tryuntil_deletefiles(cstr(hook->pathUntar), "*"));

	/* Backup Test 1: add files with utf8 names and see if the archives on disk are as expected */
	hook->stage_of_test = 1;
	check(test_operations_backup_reset(app, group, db, hook, 6, "txt", true, true));
	hook->stage_of_test = 2;
	check(test_hook_call_before_processing_queue(hook, db));
	bassignformat(path, "%s%s00001_index.db", cstr(hook->pathReadyUpload), pathsep);
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
	/* See the comment at the top of operations.c for an explanation why we cover these cases. */
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
	check(svdp_backup(app, group, db, hook));
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));
	hook->stage_of_test = 4;
	check(test_hook_call_before_processing_queue(hook, db));
	check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar",
		"00000007.7z|file__2.txt contentlength=19 crc=9215b3c6 itemtype=3|"
		"00000008.7z|file__4.txt contentlength=10 crc=e8deaef itemtype=3|"
		"00000009.7z|file__5.txt contentlength=19 crc=dd48b016 itemtype=3|"
		"filenames.txt"));

	/* Backup Test 3: adding new files/missing files */
	/* See the comment at the top of operations.c for an explanation why we cover these cases. */
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
	set_debugbreaks_enabled(false);
	check(svdp_backup(app, group, db, hook));
	set_debugbreaks_enabled(true);
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));
	hook->stage_of_test = 6;
	check(test_hook_call_before_processing_queue(hook, db));
	bassignformat(path, "%s%s00002_index.db", cstr(hook->pathReadyUpload), pathsep);
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
	/* because the filetype is 'jpg' we are using compression level 0 (copy) */
	hook->stage_of_test = 7;
	check(test_operations_backup_reset(app, group, db, hook, 4, "jpg", false, false));
	group->approx_archive_size_bytes = 100 * 1024;
	bstring large = bstring_open();
	/* file 0: normal text file */
	bFill(large, 'a', 40 * 1024);
	check(sv_file_writefile(cstr(hook->filenames[0]), cstr(large), "wb"));
	hook->fakelastmodtimes[0]++;
	/* file 1: normal text file */
	bFill(large, 'b', 40 * 1024);
	check(sv_file_writefile(cstr(hook->filenames[1]), cstr(large), "wb"));
	hook->fakelastmodtimes[1]++;
	/* file 2: not that big, but too big to fit in the archive */
	bFill(large, 'c', 20 * 1024);
	check(sv_file_writefile(cstr(hook->filenames[2]), cstr(large), "wb"));
	hook->fakelastmodtimes[2]++;
	/* file 3: a file so big we'll have to create a new archive just for it */
	bFill(large, 'd', 250 * 1024 + 1);
	check(sv_file_writefile(cstr(hook->filenames[3]), cstr(large), "wb"));
	hook->fakelastmodtimes[3]++;
	check(svdp_backup(app, group, db, hook));
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));
	hook->stage_of_test = 8;
	check(test_hook_call_before_processing_queue(hook, db));
	group->approx_archive_size_bytes = 32 * 1024 * 1024;
	bassignformat(path, "%s%s00001_index.db", cstr(hook->pathReadyUpload), pathsep);
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
	
	/* Backup Test 5: it's ok if there are no files that need backup. */
	hook->stage_of_test = -1;
	check(svdp_backup(app, group, db, hook));
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));
	check(test_hook_call_before_processing_queue(hook, db));
	bassignformat(path, "%s%s00001_index.db", cstr(hook->pathReadyUpload), pathsep);
	TestTrue(os_file_exists(cstr(path)));
	bassignformat(path, "%s%s00002_index.db", cstr(hook->pathReadyUpload), pathsep);
	TestTrue(os_file_exists(cstr(path)));
	bassignformat(path, "%s%s00001_00002.tar", cstr(hook->pathReadyUpload), pathsep);
	TestTrue(os_file_exists(cstr(path)));
	bassignformat(path, "%s%s00001_00003.tar", cstr(hook->pathReadyUpload), pathsep);
	TestTrue(os_file_exists(cstr(path)));
	bassignformat(path, "%s%s00002_00001.tar", cstr(hook->pathReadyUpload), pathsep);
	TestTrue(!os_file_exists(cstr(path)));
	bassignformat(path, "%s%s00002_00002.tar", cstr(hook->pathReadyUpload), pathsep);
	TestTrue(!os_file_exists(cstr(path)));

	/* Backup Test 6: add mp3 files (we'll use ffmpeg to hash) */
	hook->stage_of_test = 9;
	check(test_operations_backup_reset(app, group, db, hook, 5, "mp3", true, false));
	check(sv_file_writefile(cstr(hook->filenames[0]), "contents-0", "wb"));
	check(writevalidmp3(cstr(hook->filenames[1]), false, false, false));
	check(writevalidmp3(cstr(hook->filenames[2]), false, false, false));
	check(writevalidmp3(cstr(hook->filenames[3]), false, false, false));
	check(writevalidmp3(cstr(hook->filenames[4]), false, false, false));
	set_debugbreaks_enabled(false);
	check(svdp_backup(app, group, db, hook));
	set_debugbreaks_enabled(true);
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));
	hook->stage_of_test = 10;
	check(test_hook_call_before_processing_queue(hook, db));
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
	bFill(large, 'a', cast64u32s(prevlength));
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
	set_debugbreaks_enabled(false);
	check(svdp_backup(app, group, db, hook));
	set_debugbreaks_enabled(true);
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));
	hook->stage_of_test = 12;
	check(test_hook_call_before_processing_queue(hook, db));
	check(test_operations_backup_check_archivecontents(hook, "00002_00001.tar",
		"00000003.7z|file__0.mp3 contentlength=21 crc=95bcd25d itemtype=2|"
		"00000004.7z|file__2.mp3 contentlength=3142 crc=b4e1484f itemtype=2|"
		"00000005.7z|file__4.mp3 contentlength=3142 crc=c5679762 itemtype=2|"
		"filenames.txt"));
	bdestroy(large);

cleanup:
	bdestroy(dbpath);
	return currenterr;
}

check_result test_hook_call_when_restoring_file(void* phook, const char* originalpath, bstring destpath)
{
	operations_test_hook* hook = (operations_test_hook*)phook;
	if (hook)
	{
		const char* pathwithoutroot = originalpath + (islinux ? 1 : 3);
		bstring expectedname = bformat("%s%s%s", cstr(hook->pathRestoreTo), pathsep, pathwithoutroot);
		TestEqs(cstr(expectedname), cstr(destpath));

		/* instead of writing to that quite-long path, which would be a bit more complicated to delete after the test,
		point the dest path someplace simpler (restoreto/a/a/file.txt) */
		os_get_filename(originalpath, hook->tmp);
		bassignformat(destpath, "%s%sa%sa%s%s", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep, cstr(hook->tmp));
	}
	return OK;
}

check_result test_operations_restore(const svdp_application* app, const svdp_backupgroup* group,
	svdb_connection* db, operations_test_hook* hook)
{
	sv_result currenterr = {};
	bstrList* filesSeen = bstrListCreate();
	bstring contents = bstring_open();
	bstring fullrestoreto = bformat("%s%sa%sa", cstr(hook->pathRestoreTo), pathsep, pathsep);

	/* run backup and add file0.txt, file1.txt. */
	bstring dbpath = bstrcpy(db->path);
	hook->stage_of_test = -1;
	check(test_operations_backup_reset(app, group, db, hook, 2, "txt", true, true));

	/* modify file1.txt and add file2.txt. */
	check(sv_file_writefile(cstr(hook->filenames[1]), "contents-1-altered", "wb"));
	hook->fakelastmodtimes[1]++;
	check(sv_file_writefile(cstr(hook->filenames[2]), "the-contents-2", "wb"));
	hook->fakelastmodtimes[2]++;
	check(svdp_backup(app, group, db, hook));
	check(svdb_connection_disconnect(db));
	check(svdb_connection_open(db, cstr(dbpath)));

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
		os_tryuntil_remove(cstr(fullrestoreto));
		bassignformat(hook->tmp, "%s%sa", cstr(hook->pathRestoreTo), pathsep);
		check(os_tryuntil_deletefiles(cstr(hook->tmp), "*"));
		os_tryuntil_remove(cstr(hook->tmp));
		check(os_tryuntil_deletefiles(cstr(hook->pathRestoreTo), "*"));
		os_create_dirs(cstr(hook->pathRestoreTo));

		/* run restore */
		svdp_restore_state op = {};
		op.workingdir = bformat("%s%stemp", cstr(hook->groupdir), pathsep);
		op.subworkingdir = bformat("%s%stemp%sout", cstr(hook->groupdir), pathsep, pathsep);
		op.destdir = bfromcstr(cstr(hook->pathRestoreTo));
		op.scope = bfromcstr(i == test_operations_restore_scope_to_one_file ? "*2.txt" : "*");
		op.destfullpath = bstring_open();
		op.errmsg = bstring_open();
		op.messages = bstrListCreate();
		op.db = db;
		op.test_context = hook;
		check(svdp_archiver_open(&op.archiver, cstr(app->path_app_data), cstr(group->groupname), 0, 0, ""));
		check(checkexternaltoolspresent(&op.archiver, 0));
		TestTrue(os_create_dirs(cstr(op.subworkingdir)));

		if (i == test_operations_restore_scope_to_one_file)
		{
			check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
			check(os_listfiles(cstr(fullrestoreto), filesSeen, false));
			TestEqn(1, filesSeen->qty);
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_2.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("the-contents-2", cstr(contents));
			TestEqn(0, op.messages->qty);
		}
		else if (i == test_operations_restore_from_many_archives)
		{
			check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
			check(os_listfiles(cstr(fullrestoreto), filesSeen, false));
			TestEqn(3, filesSeen->qty);
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_0.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("contents-0", cstr(contents));
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_1.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("contents-1-altered", cstr(contents));
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_2.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("the-contents-2", cstr(contents));
			TestEqn(0, op.messages->qty);
		}
		else if (i == test_operations_restore_include_older_files)
		{
			/* change the database so that it looks like file0.txt was inaccessible the last time we ran backup.
			we should run smoothly and just restore the latest version we have. */
			svdb_files_row row = {};
			check(svdb_filesbypath(db, hook->filenames[0], &row));
			row.most_recent_collection = 2;
			row.e_status = sv_filerowstatus_queued;
			check(svdb_filesupdate(db, &row));
			check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
			
			check(os_listfiles(cstr(fullrestoreto), filesSeen, false));
			TestEqn(3, filesSeen->qty);
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_0.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("contents-0", cstr(contents));
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_1.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("contents-1-altered", cstr(contents));
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_2.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("the-contents-2", cstr(contents));
			TestEqn(0, op.messages->qty);
		}
		else if (i == test_operations_restore_missing_archive)
		{
			/* if an archive is not found, we add the error to a list of messages and continue. */
			bassignformat(hook->tmp, "%s%s00002_00001.tar", cstr(hook->pathReadyUpload), pathsep);
			TestTrue(os_file_exists(cstr(hook->tmp)));
			TestTrue(os_tryuntil_remove(cstr(hook->tmp)));
			set_debugbreaks_enabled(false);
			check(svdb_files_in_queue(op.db, svdb_files_in_queue_get_all, &op, &svdp_restore_cb));
			set_debugbreaks_enabled(true);
			
			check(os_listfiles(cstr(fullrestoreto), filesSeen, false));
			TestEqn(1, filesSeen->qty);
			bassignformat(hook->tmp, "%s%sa%sa%sfile\xE1\x84\x81_0.txt", cstr(hook->pathRestoreTo), pathsep, pathsep, pathsep);
			check(sv_file_readfile(cstr(hook->tmp), contents));
			TestEqs("contents-0", cstr(contents));
			TestEqn(2, op.messages->qty);
			TestTrue(s_contains(bstrListViewAt(op.messages, 0), "could not find archive "));
			TestTrue(s_contains(bstrListViewAt(op.messages, 1), "could not find archive "));
		}
	}
	
cleanup:
	bdestroy(dbpath);
	bdestroy(contents);
	bstrListDestroy(filesSeen);
	return currenterr;
}

check_result addcollection(svdb_connection* db, uint64_t started, uint64_t finished)
{
	uint64_t rowid = 0;
	check_warn(svdb_collectioninsert(db, started, &rowid), "", exit_on_err);
	svdb_collections_row row = {
		rowid, 0,
		finished, 0
	};
	return svdb_collectionupdate(db, &row);
}

check_result test_operations_compact(const svdp_application* app, svdp_backupgroup* group, 
	svdb_connection* db, operations_test_hook* hook)
{
	sv_result currenterr = {};
	check(test_operations_backup_reset(app, group, db, hook, 0, "txt", false, false));
	group->days_to_keep_prev_versions = 0;
	uint64_t collectionid_to_expire;
	const time_t october_01_2016 = 1475280000, october_02_2016 = 1475366400, october_03_2016 = 1475452800,
		october_04_2016 = 1475539200, october_20_2016 = 1476921600;

	/* can't run compact if backup has never been run */
	collectionid_to_expire = 9999;
	check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
	TestEqn(0, collectionid_to_expire);

	/* can't run compact if backup has only been run once */
	check(addcollection(db, (uint64_t)october_01_2016, (uint64_t)(october_01_2016 + 1)));
	collectionid_to_expire = 9999;
	check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
	TestEqn(0, collectionid_to_expire);

	/* we should not return the latest collection, even if it is old enough to have been expired */
	check(addcollection(db, (uint64_t)october_02_2016, (uint64_t)(october_02_2016 + 1)));
	collectionid_to_expire = 9999;
	check(svdp_compact_getexpirationcutoff(db, group, &collectionid_to_expire, october_20_2016));
	TestEqn(1, collectionid_to_expire);

	/* test finding the cutoff */
	check(addcollection(db, (uint64_t)october_03_2016, (uint64_t)(october_03_2016 + 1)));
	check(addcollection(db, (uint64_t)october_04_2016, (uint64_t)(october_04_2016 + 1)));
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

cleanup:
	return currenterr;
}

check_result test_operations(const char* dir)
{
	sv_result currenterr = {};
	svdp_application app = {};
	svdp_backupgroup group = {};
	svdb_connection db = {};
	operations_test_hook hook = {};

	bstring path = bstring_open();
	hook.groupdir = bformat("%s%suserdata%stest", dir, pathsep, pathsep);
	hook.pathReadyUpload = bformat("%s%suserdata%stest%sreadytoupload", dir, pathsep, pathsep, pathsep);
	hook.pathReadyDelete = bformat("%s%suserdata%stest%sreadytodelete", dir, pathsep, pathsep, pathsep);
	hook.pathWorking = bformat("%s%suserdata%stest%stemp", dir, pathsep, pathsep, pathsep);
	hook.pathRestoreTo = bformat("%s%srestoreto", dir, pathsep);
	hook.pathUntar = bformat("%s%suntar", dir, pathsep);
	hook.tmp = bstring_open();
	hook.allcontentrows = bstrListCreate();
	hook.allfilelistrows = bstrListCreate();

	app.is_low_access = false;
	app.path_app_data = bfromcstr(dir);
	app.backup_group_names = bstrListCreate();
	check(os_tryuntil_deletefiles(dir, "*"));
	check(os_tryuntil_deletefiles(cstr(hook.groupdir), "*"));
	check(os_tryuntil_deletefiles(cstr(hook.pathReadyUpload), "*"));
	check(os_tryuntil_deletefiles(cstr(hook.pathReadyDelete), "*"));
	check(os_tryuntil_deletefiles(cstr(hook.pathWorking), "*"));
	check(svdp_application_creategroup_impl(&app, "test", cstr(hook.groupdir)));
	check(svdp_application_findgroupnames(&app));

	/* verify what creating the group should have done */
	TestEqn(1, app.backup_group_names->qty);
	TestEqs("test", bstrListViewAt(app.backup_group_names, 0));
	bassignformat(path, "%s%suserdata%stest%stest_index.db", dir, pathsep, pathsep, pathsep);
	TestTrue(os_file_exists(cstr(path)));
	TestTrue(os_dir_exists(cstr(hook.pathReadyUpload)));
	
	/* change group settings. do not need a root directory because of test hook */
	app.current_group_name_index = 0;
	check(loadbackupgroup(&app, &group, &db));
	group.days_to_keep_prev_versions = 0;
	group.separate_metadata_enabled = 1;
	check(svdp_backupgroup_persist(&db, &group));

	hook.dirfakeuserfiles = bformat("%s%sfakeuserfiles", dir, pathsep);
	TestTrue(os_create_dir(cstr(hook.dirfakeuserfiles)));
	app.current_group_name_index = 0;
	check(test_operations_backup(&app, &group, &db, &hook));
	check(test_operations_restore(&app, &group, &db, &hook));
	check(test_operations_compact(&app, &group, &db, &hook));

cleanup:
	operations_test_hook_close(&hook);
	svdb_connection_close(&db);
	svdp_backupgroup_close(&group);
	svdp_application_close(&app);
	return currenterr;
}

void get_hash_helper(const char* path, svdp_archiver* archiver, bool sepmetadata, hash256* hash, uint32_t* crc)
{
	*hash = CAST(hash256) {};
	*crc = UINT_MAX;
	os_exclusivefilehandle handle = {};
	check_warn(os_exclusivefilehandle_open(&handle, path, true, NULL),"", exit_on_err);
	check_warn(hash_of_file(&handle, sepmetadata ? 1 : 0, sepmetadata ? knownfileextensionMp3 : knownfileextensionOtherBinary,
		cstr(archiver->path_audiometadata_binary), hash, crc), "", exit_on_err);
	os_exclusivefilehandle_close(&handle);
}

check_result writevalidmp3(const char* path, bool changeid3, bool changeid3length, bool changedata)
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
	byte* zeros = sv_calloc(lengthzeros, sizeof(byte));
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

check_result test_audio_tags_hashing(const char* dir)
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
	check(os_tryuntil_deletefiles(dir, "*"));
	check(svdp_archiver_open(&archiver, dir, "test", 0, 0, ""));
	check(checkexternaltoolspresent(&archiver, 1 /* use ffmpeg */));
	check(checkffmpegworksasexpected(&archiver, 1 /* use ffmpeg */, dir));
	check(sv_file_writefile(cstr(pathnormalfile), "abcde", "wb"));
	check(sv_file_writefile(cstr(path0byte), "", "wb"));
	check(sv_file_writefile(cstr(invalidmp3), "not-a-valid-mp3", "wb"));

	{ /* hash of normal file */
		get_hash_helper(cstr(pathnormalfile), &archiver, false, &hash, &crc32);
		TestTrue(0x4d36bf2cf609ea58ULL == hash.data[0] && 0x19b91b8a95e63aadULL == hash.data[1]);
		TestTrue(0x1f1242b808bf0428ULL == hash.data[2] && 0xf6539e0c067bcadaULL == hash.data[3]);
		TestEqn(0x8587d865, crc32);
	}
	{ /* hash of 0byte file */
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
		expect_err_with_message(hash_of_file(&handle, true, knownfileextensionMp3, 
			cstr(archiver.path_audiometadata_binary), &hash, &crc32), "file not found");
		set_debugbreaks_enabled(true);
		os_exclusivefilehandle_close(&handle);
	}
	{ /* hash of invalid file handle, get-metadata turned off */
		os_exclusivefilehandle handle = { bstring_open(), NULL, -1 };
		bassign(handle.loggingcontext, pathnormalfile);
		set_debugbreaks_enabled(false);
		expect_err_with_message(hash_of_file(&handle, false, knownfileextensionMp3,
			cstr(archiver.path_audiometadata_binary), &hash, &crc32), "bad file handle");
		set_debugbreaks_enabled(true);
		os_exclusivefilehandle_close(&handle);
	}
	{ /* hash of invalid file handle, get-metadata turned on */
		os_exclusivefilehandle handle = { bstring_open(), NULL, -1 };
		bassign(handle.loggingcontext, pathnormalfile);
		set_debugbreaks_enabled(false);
		expect_err_with_message(hash_of_file(&handle, true, knownfileextensionMp3,
			cstr(archiver.path_audiometadata_binary), &hash, &crc32), "bad file handle");
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
		TestTrue(0xcb35704d356c0850 == hash.data[0] && 0xbf569578bb33293f == hash.data[1]);
		TestTrue(0xe532a2823c4e4565 == hash.data[2] && 0x75a2eaee6a61240d == hash.data[3]);
		TestEqn(0xb6a3e71a, crc32);
	}
	{ /* hash of valid mp3: id3 changed, get-metadata turned off */
		check(writevalidmp3(cstr(validmp3), true, false, false));
		get_hash_helper(cstr(validmp3), &archiver, false, &hash, &crc32);
		TestTrue(0x95c30239b5ffa5e7 == hash.data[0] && 0x6c9d0db61b3ca5e1 == hash.data[1]);
		TestTrue(0xb706ca2ec55eaa19 == hash.data[2] && 0xc3d4d9e680c21034 == hash.data[3]);
		TestEqn(0xdcf3b99d, crc32);
	}
	{ /* 1 get-metadata on, id3 unchanged, id3 length unchanged, data unchanged */
		check(writevalidmp3(cstr(validmp3), false, false, false));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0xb6a3e71a, crc32);
	}
	{ /* 2 get-metadata on, id3 changed, id3 length unchanged, data unchanged */
		check(writevalidmp3(cstr(validmp3), true, false, false));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0xdcf3b99d, crc32);
	}
	{ /* 3 get-metadata on, id3 unchanged, id3 length changed, data unchanged */
		check(writevalidmp3(cstr(validmp3), false, true, false));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0x15655751, crc32);
	}
	{ /* 4 get-metadata on, id3 changed, id3 length changed, data unchanged */
		check(writevalidmp3(cstr(validmp3), true, true, false));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x611c9c9e3d1e62cc == hash.data[0] && 0x1ffaa7d05b187124 == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0x14150f7b, crc32);
	}
	{ /* 5 get-metadata on, id3 unchanged, id3 length unchanged, data changed */
		check(writevalidmp3(cstr(validmp3), false, false, true));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x7d49276b4e05c2bd == hash.data[0]&& 0x39f840313d6eeb4f == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0xc5679762, crc32);
	}
	{ /* 6 get-metadata on, id3 changed, id3 length unchanged, data changed */
		check(writevalidmp3(cstr(validmp3), true, false, true));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x7d49276b4e05c2bd == hash.data[0]&& 0x39f840313d6eeb4f == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0xaf37c9e5, crc32);
	}
	{ /* 7 get-metadata on, id3 unchanged, id3 length changed, data changed */
		check(writevalidmp3(cstr(validmp3), false, true, true));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0x898585be, crc32);
	}
	{ /* 8 get-metadata on, id3 changed, id3 length changed, data changed */
		check(writevalidmp3(cstr(validmp3), true, true, true));
		get_hash_helper(cstr(validmp3), &archiver, true, &hash, &crc32);
		TestTrue(0x7d49276b4e05c2bd == hash.data[0] && 0x39f840313d6eeb4f == hash.data[1]);
		TestTrue(0x0 == hash.data[2] && 0x0 == hash.data[3]);
		TestEqn(0x88f5dd94, crc32);
	}
cleanup:
	svdp_archiver_close(&archiver);
	bdestroy(pathnormalfile);
	bdestroy(path0byte);
	bdestroy(validmp3);
	bdestroy(invalidmp3);
	return currenterr;
}

void run_sv_tests(void)
{
	puts("running tests...");
	sv_result currenterr = {};

	bstring dir = os_get_tmpdir("tmpglacial_backup");
	bstring testdir = os_make_subdir(cstr(dir), "tests");
	bstring testdirfileio = os_make_subdir(cstr(testdir), "fileio");
	bstring testdirdb = os_make_subdir(cstr(testdir), "testdb");
	bstring testdir7z = os_make_subdir(cstr(testdir), "test7z");
	extern uint32_t sleep_between_tries;
	sleep_between_tries = 1;
	
	test_archiver_file_extension_info();
	check(run_utils_tests());
	check(test_util_hashing());
	check(test_audio_tags_hashing(cstr(testdirfileio)));
	check(test_db_basics(cstr(testdirdb)));
	check(test_db_rows(cstr(testdirdb)));
	check(test_archiver_7z(cstr(testdir7z)));
	check(test_archiver_tar(cstr(testdir7z)));
	check(test_operations(cstr(testdir7z)));

cleanup:
	sleep_between_tries = 250;
	bdestroy(dir);
	bdestroy(testdir);
	bdestroy(testdirfileio);
	bdestroy(testdirdb);
	bdestroy(testdir7z);
	puts("tests complete.");
	if (currenterr.code)
	{
		printf("unit test failed, error message is %s", currenterr.msg ? cstr(currenterr.msg) : "null");
		alertdialog("unit test failed");
		sv_result_close(&currenterr);
	}
}

