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

#include "dbaccess.h"
#include "lib_sqlite3.h"

check_result svdb_preparequery(svdb_connection* self, svdb_query qry_number)
{
	sv_result currenterr = {};
	check_fatal(qry_number > svdb_nqry_none && qry_number < svdb_nqry_max, 
		"invalid qry number %d", qry_number);

	if (!self->qrys[qry_number])
	{
		sqlite_qry* prepared = NULL;
		sv_log_writeFmt("svdb_preparequery for query #%d", qry_number);
		check_b(self->qrystrings[qry_number], "no query set for #%d", qry_number);
		check_sql(self->db, sqlite3_prepare_v2(self->db, self->qrystrings[qry_number],
			-1 /*read whole string*/, &prepared, 0));
		check_b(prepared, "prepare returned null for #%s", self->qrystrings[qry_number]);
		self->qrys[qry_number] = prepared;
	}

cleanup:
	return currenterr;
}

svdb_qry svdb_qry_open(svdb_query qry_number, svdb_connection* db)
{
	svdb_qry self = {};
	sv_log_writeFmt("svdb_qry_open #%d", qry_number);
	self.qry_number = qry_number;
	if (!db->qrys[qry_number])
	{
		check_warn(svdb_preparequery(db, self.qry_number), 
			"Failed to load query", exit_on_err);
	}

	return self;
}

check_result svdb_qry_bindint(svdb_qry* self, svdb_connection* db,
	int index, int value)
{
	sv_result currenterr = {};
	check_sql(db->db, sqlite3_bind_int(db->qrys[self->qry_number], index, value));

cleanup:
	return currenterr;
}

check_result svdb_qry_bindint64(svdb_qry* self, svdb_connection* db,
	int index, int64_t value)
{
	sv_result currenterr = {};
	check_sql(db->db, sqlite3_bind_int64(db->qrys[self->qry_number], index, value));

cleanup:
	return currenterr;
}

check_result svdb_qry_bindstr(svdb_qry* self, svdb_connection* db,
	int index, const char* str, int length, bool is_static)
{
	sv_result currenterr = {};
	check_sql(db->db, sqlite3_bind_text(db->qrys[self->qry_number],
		index, str, length, is_static ? SQLITE_STATIC : SQLITE_TRANSIENT));

cleanup:
	return currenterr;
}

void svdb_qry_get_int(svdb_qry* self, svdb_connection* db, int index, uint32_t* out)
{
	/* make the column index 1-based for consistency with binding indices */
	*out = (uint32_t)sqlite3_column_int(db->qrys[self->qry_number], index - 1);
}

void svdb_qry_get_int64(svdb_qry* self, svdb_connection* db, int index, uint64_t* out)
{
	/* make the column index 1-based for consistency with binding indices */
	*out = (uint64_t)sqlite3_column_int64(db->qrys[self->qry_number], index - 1);
}

void svdb_qry_get_str(svdb_qry* self, svdb_connection* db, int index, bstring s)
{
	/* make the column index 1-based for consistency with binding indices */
	/* null and empty-string treated equivalently, both set s to "" */
	int len = sqlite3_column_bytes(db->qrys[self->qry_number], index - 1);
	if (len)
	{
		bassignblk(s, sqlite3_column_text(db->qrys[self->qry_number], index - 1), len);
	}
	else
	{
		bstrClear(s);
	}
}

check_result svdb_qry_run(svdb_qry* self, svdb_connection* db, bool expectChanges, int* prc)
{
	sv_result currenterr = {};
	int rc = sqlite3_step(db->qrys[self->qry_number]);
	if (prc)
	{
		*prc = rc;
	}

	check_sql(db->db, rc);
	check_b(!expectChanges || sqlite3_changes(db->db) > 0,
		"running query %s changed no rows", db->qrystrings[self->qry_number]);

cleanup:
	return currenterr;
}

check_result svdb_qry_disconnect(svdb_qry* self, svdb_connection* db)
{
	sv_result currenterr = {};
	if (self)
	{
		if (self->qry_number)
		{
			check_sql(db->db, sqlite3_reset(db->qrys[self->qry_number]));
		}

		set_self_zero();
	}
cleanup:
	return currenterr;
}

void svdb_qry_close(svdb_qry* self, svdb_connection* db)
{
	check_warn(svdb_qry_disconnect(self, db), 
		"Encountered warning when resetting statement.", continue_on_err);

	set_self_zero();
}

const char* schema_cmds[] = {
	/* Schema notes:
		Autoincrement is not needed. sqlite's "Autoincrement"
		prevents the use of Rowids from previously deleted rows,
		not required for us.
		Don't use hash as primary key, for better insert performance.
		In my benchmarks int64[4] performed better than blob[32] */
	"CREATE TABLE TblCollections ("
	"CollectionId INTEGER PRIMARY KEY,"
	"Time INTEGER,"
	"TimeCompleted INTEGER,"
	"CountTotalFiles INTEGER,"
	"CountNewContents INTEGER,"
	"CountNewContentsBytes INTEGER)",
	"CREATE TABLE TblFilesList ("
	"FilesListId INTEGER PRIMARY KEY,"
#ifdef __linux__
	"Path TEXT COLLATE BINARY,"
#else
	"Path TEXT COLLATE NOCASE,"
#endif
	"ContentLength INTEGER,"
	"ContentsId INTEGER,"
	"LastWriteTime INTEGER,"
	"Status INTEGER,"
	"Flags INTEGER)",
	"CREATE TABLE TblContentsList ("
	"ContentsId INTEGER PRIMARY KEY,"
	"ContentsHash1 INTEGER,"
	"ContentsHash2 INTEGER,"
	"ContentsHash3 INTEGER,"
	"ContentsHash4 INTEGER,"
	"ContentLength INTEGER,"
	"Crc32 INTEGER,"
	"ArchiveId INTEGER,"
	"LastCollectionId INTEGER)",
	"CREATE UNIQUE INDEX IxTblFilesListPath "
	"ON TblFilesList(Path)",
	"CREATE INDEX IxTblContentsListHash "
	"ON TblContentsList(ContentsHash1)",
	"CREATE TABLE TblProperties ("
	"PropertyName TEXT PRIMARY KEY,"
	"PropertyVal)",
	"INSERT INTO TblProperties "
	"VALUES ('SchemaVersion', 1)",
	/* placeholder rows that mark id 0 as reserved. */
	"INSERT INTO TblCollections "
	"VALUES (0, 0, 0, 0, 0, 0)",
	"INSERT INTO TblFilesList "
	"VALUES (0, '', 0, 0, 0, 0, 0)",
	"INSERT INTO TblContentsList "
	"VALUES (0, 0, 0, 0, 0, 0, 0, 0, 0)",
};

check_result svdb_runsql(svdb_connection* self, const char* sql, int lensql)
{
	sv_result currenterr = {};
	sv_log_writeLine(sql);
	sqlite3_stmt* stmt = NULL;
	check_sql(self->db, sqlite3_prepare_v2(self->db, sql, lensql, &stmt, NULL));
	check_sql(self->db, sqlite3_step(stmt));
	check_sql(self->db, sqlite3_finalize(stmt));
	stmt = NULL;

cleanup:
	sqlite3_finalize(stmt);
	return currenterr;
}

check_result svdb_addschema(svdb_connection* self)
{
	sv_result currenterr = {};
	svdb_txn txn = {};
	check(svdb_txn_open(&txn, self));
	for (int i = 0; i < countof32s(schema_cmds); i++)
	{
		check(svdb_runsql(self, schema_cmds[i], strlen32s(schema_cmds[i])));
	}

	check(svdb_txn_commit(&txn, self));

cleanup:
	svdb_txn_close(&txn, self);
	return currenterr;
}

check_result svdb_getcounthelper(svdb_connection* self, enum svdb_query query, bool subtractone, uint64_t* ret)
{
	sv_result currenterr = {};
	int rc = 0;
	svdb_qry qry = svdb_qry_open(query, self);
	check(svdb_qry_run(&qry, self, false, &rc));
	check_b(rc == SQLITE_ROW, "expect row found, got %d", rc);
	svdb_qry_get_int64(&qry, self, 1, ret);
	if (subtractone && *ret > 0)
	{
		/* some tables have one reserved row that we won't include in the count*/
		(*ret)--;
	}

	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_filesgetcount(svdb_connection* self, uint64_t* ret)
{
	self->qrystrings[svdb_nqry_filescount] = "SELECT COUNT(*) FROM TblFilesList";
	return svdb_getcounthelper(self, svdb_nqry_filescount, true, ret);
}

check_result svdb_contentsgetcount(svdb_connection* self, uint64_t* ret)
{
	self->qrystrings[svdb_nqry_contentscount] = "SELECT COUNT(*) FROM TblContentsList";
	return svdb_getcounthelper(self, svdb_nqry_contentscount, true, ret);
}

check_result svdb_propertygetcount(svdb_connection* self, uint64_t* ret)
{
	self->qrystrings[svdb_nqry_propertycount] = "SELECT COUNT(*) FROM TblProperties";
	return svdb_getcounthelper(self, svdb_nqry_propertycount, false, ret);
}

check_result svdb_propertygetint(svdb_connection* self, const char* propname, int lenpropname, uint32_t* ret)
{
	self->qrystrings[svdb_nqry_propertyget] =
		"SELECT PropertyVal FROM TblProperties WHERE PropertyName=?";

	sv_result currenterr = {};
	int rc = 0;
	svdb_qry qry = svdb_qry_open(svdb_nqry_propertyget, self);
	check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
	check(svdb_qry_run(&qry, self, false, &rc));
	*ret = 0; /* return 0 if not found */
	if (rc == SQLITE_ROW)
	{
		svdb_qry_get_int(&qry, self, 1, ret);
	}

	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_propertygetstr(svdb_connection* self, const char* propname, int lenpropname, bstring ret)
{
	sv_result currenterr = {};
	bstrClear(ret); /* return "" if not found */
	int rc = 0;
	svdb_qry qry = svdb_qry_open(svdb_nqry_propertyget, self);
	check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
	check(svdb_qry_run(&qry, self, false, &rc));
	if (rc == SQLITE_ROW)
	{
		svdb_qry_get_str(&qry, self, 1, ret);
	}
	
	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

struct tagbstring propertystrlistDelim = bsStatic("|||||");
check_result svdb_propertygetstrlist(svdb_connection* self, const char* propname, int lenpropname, bstrList* ret)
{
	sv_result currenterr = {};
	bstring all = bstring_open();
	check(svdb_propertygetstr(self, propname, lenpropname, all));
	if (blength(all))
	{ 
		bstrListSplit(ret, all, &propertystrlistDelim);
	}
	else
	{
		bstrListClear(ret);
	}

cleanup:
	bdestroy(all);
	return currenterr;
}

check_result svdb_propertysetstr(svdb_connection* self, const char* propname, int lenpropname, const char* val)
{
	self->qrystrings[svdb_nqry_propertyset] =
		"INSERT OR REPLACE INTO TblProperties (PropertyName, PropertyVal) VALUES (?, ?)";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_propertyset, self);
	check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
	check(svdb_qry_bindstr(&qry, self, 2, val, -1, true));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_propertysetint(svdb_connection* self, const char* propname, int lenpropname, uint32_t val)
{
	self->qrystrings[svdb_nqry_propertyset] =
		"INSERT OR REPLACE INTO TblProperties (PropertyName, PropertyVal) VALUES (?, ?)";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_propertyset, self);
	check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
	check(svdb_qry_bindint(&qry, self, 2, (int32_t)val));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_propertysetstrlist(
	svdb_connection* self, const char* propname, int lenpropname, const bstrList* val)
{
	sv_result currenterr = {};
	bstring all = bjoin(val, &propertystrlistDelim);
	for (int i = 0; i < val->qty; i++)
	{
		check_b(!s_contains(bstrListViewAt(val, i), (const char*)propertystrlistDelim.data),
			"setting to this property cannot include the text '%s'", propertystrlistDelim.data);
	}

	check(svdb_propertysetstr(self, propname, lenpropname, cstr(all)));

cleanup:
	bdestroy(all);
	return currenterr;
}

check_result svdb_filesbypath(svdb_connection* self, const bstring path, svdb_files_row* out_row)
{
	self->qrystrings[svdb_nqry_filesbypath] =
		"SELECT FilesListId, ContentLength, ContentsId, LastWriteTime, Flags, Status "
		"FROM TblFilesList WHERE Path=? LIMIT 1";

	sv_result currenterr = {};
	memset(out_row, 0, sizeof(*out_row));
	int rc = 0;
	svdb_qry qry = svdb_qry_open(svdb_nqry_filesbypath, self);
	check(svdb_qry_bindstr(&qry, self, 1, cstr(path), blength(path), false));
	check(svdb_qry_run(&qry, self, false, &rc));

	if (rc == SQLITE_ROW)
	{
		svdb_qry_get_int64(&qry, self, 1, &out_row->id);
		svdb_qry_get_int64(&qry, self, 2, &out_row->contents_length);
		svdb_qry_get_int64(&qry, self, 3, &out_row->contents_id);
		svdb_qry_get_int64(&qry, self, 4, &out_row->last_write_time);
		svdb_qry_get_int64(&qry, self, 5, &out_row->flags);
		uint64_t status = 0;
		svdb_qry_get_int64(&qry, self, 6, &status);
		out_row->e_status = sv_getstatus(status);
		out_row->most_recent_collection = sv_getcollectionidfromstatus(status);
	}
	
	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_collectionsget(svdb_connection* self, sv_array* rows, bool get_all)
{
	self->qrystrings[svdb_nqry_collectionget] =
		"SELECT CollectionId, Time, TimeCompleted, CountTotalFiles, CountNewContents, "
		"CountNewContentsBytes FROM TblCollections WHERE CollectionId != 0 ORDER BY CollectionId DESC";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_collectionget, self);
	int rc = 0;
	check(svdb_qry_run(&qry, self, false, &rc));
	check_b(rows->elementsize == sizeof32u(svdb_collections_row), "incorrect size");
	while (rc == SQLITE_ROW)
	{
		svdb_collections_row row = { 0 };
		svdb_qry_get_int64(&qry, self, 1, &row.id);
		svdb_qry_get_int64(&qry, self, 2, &row.time);
		svdb_qry_get_int64(&qry, self, 3, &row.time_finished);
		svdb_qry_get_int64(&qry, self, 4, &row.count_total_files);
		svdb_qry_get_int64(&qry, self, 5, &row.count_new_contents);
		svdb_qry_get_int64(&qry, self, 6, &row.count_new_contents_bytes);
		sv_array_append(rows, &row, 1);
		if (!get_all)
		{
			break;
		}

		check(svdb_qry_run(&qry, self, false, &rc));
	}

	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_collectiongetlast(svdb_connection* self, uint64_t* id)
{
	sv_result currenterr = {};
	sv_array rows = sv_array_open(sizeof32u(svdb_collections_row), 0);
	check(svdb_collectionsget(self, &rows, false));
	sv_log_writeLine(rows.length ? "svdb_collectiongetlast row" : "svdb_collectiongetlast no row");
	*id = rows.length ? ((svdb_collections_row*)sv_array_at(&rows, 0))->id : 0;

cleanup:
	sv_array_close(&rows);
	return currenterr;
}

check_result svdb_contentsbyhash(
	svdb_connection* self, const hash256* hash, uint64_t contentslength, svdb_contents_row* row)
{
	self->qrystrings[svdb_nqry_contentsbyhash] =
		"SELECT ContentsId, LastCollectionId, Crc32, ArchiveId FROM TblContentsList "
		"WHERE ContentsHash1=? AND ContentsHash2=? AND ContentsHash3=? AND ContentsHash4=? "
		"AND ContentLength=? LIMIT 1";

	sv_result currenterr = {};
	int rc = 0;
	memset(row, 0, sizeof(*row));
	svdb_qry qry = svdb_qry_open(svdb_nqry_contentsbyhash, self);
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)hash->data[0]));
	check(svdb_qry_bindint64(&qry, self, 2, (int64_t)hash->data[1]));
	check(svdb_qry_bindint64(&qry, self, 3, (int64_t)hash->data[2]));
	check(svdb_qry_bindint64(&qry, self, 4, (int64_t)hash->data[3]));
	check(svdb_qry_bindint64(&qry, self, 5, (int64_t)contentslength));
	check(svdb_qry_run(&qry, self, false, &rc));

	if (rc == SQLITE_ROW)
	{
		svdb_qry_get_int64(&qry, self, 1, &row->id);
		svdb_qry_get_int64(&qry, self, 2, &row->most_recent_collection);
		svdb_qry_get_int(&qry, self, 3, &row->crc32);
		uint64_t archiveid = 0;
		svdb_qry_get_int64(&qry, self, 4, &archiveid);
		row->original_collection = upper32(archiveid);
		row->archivenumber = lower32(archiveid);
		row->hash = *hash;
		row->contents_length = contentslength;
		check_b(row->id != 0, "invalid contentsid");
		check_b(row->archivenumber != 0 && row->original_collection != 0, "invalid value %llu %llu",
			castull(row->archivenumber), castull(row->original_collection));
	}
	
	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_contentsbyid(svdb_connection* self, uint64_t contentsid, svdb_contents_row* row)
{
	self->qrystrings[svdb_nqry_contentsbyid] =
		"SELECT ContentsHash1, ContentsHash2, ContentsHash3, ContentsHash4, ContentLength, "
		"LastCollectionId, Crc32, ArchiveId FROM TblContentsList WHERE ContentsId=? LIMIT 1";

	sv_result currenterr = {};
	int rc = 0;
	svdb_qry qry = svdb_qry_open(svdb_nqry_contentsbyid, self);
	check_b(contentsid, "cannot get row 0");
	memset(row, 0, sizeof(*row));
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)contentsid));
	check(svdb_qry_run(&qry, self, false, &rc));
	if (rc == SQLITE_ROW)
	{
		svdb_qry_get_int64(&qry, self, 1, &row->hash.data[0]);
		svdb_qry_get_int64(&qry, self, 2, &row->hash.data[1]);
		svdb_qry_get_int64(&qry, self, 3, &row->hash.data[2]);
		svdb_qry_get_int64(&qry, self, 4, &row->hash.data[3]);
		svdb_qry_get_int64(&qry, self, 5, &row->contents_length);
		svdb_qry_get_int64(&qry, self, 6, &row->most_recent_collection);
		svdb_qry_get_int(&qry, self, 7, &row->crc32);
		uint64_t archiveid = 0;
		svdb_qry_get_int64(&qry, self, 8, &archiveid);
		row->original_collection = upper32(archiveid);
		row->archivenumber = lower32(archiveid);
		row->id = contentsid;
		check_b(row->archivenumber != 0 && row->original_collection != 0, "invalid value %llu %llu",
			castull(row->archivenumber), castull(row->original_collection));
	}
	
	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_contentsinsert(svdb_connection* self, uint64_t* outid)
{
	self->qrystrings[svdb_nqry_contentsinsert] =
		"INSERT INTO TblContentsList (ContentsHash1, ContentsHash2, ContentsHash3, "
		"ContentsHash4, ContentLength, Crc32, ArchiveId, LastCollectionId) "
		"VALUES (0, 0, 0, 0, 0, 0, 0, 0)";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_contentsinsert, self);
	check(svdb_qry_run(&qry, self, true, NULL));
	*outid = cast64s64u(sqlite3_last_insert_rowid(self->db));
	check_b(*outid != 0, "id should not be 0.");
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_contentsupdate(svdb_connection* self, const svdb_contents_row* row)
{
	self->qrystrings[svdb_nqry_contentsupdate] =
		"UPDATE TblContentsList SET ContentsHash1=?, ContentsHash2=?, ContentsHash3=?, "
		"ContentsHash4=?, ContentLength=?, Crc32=?, ArchiveId=?, LastCollectionId=? "
		"WHERE ContentsId = ?";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_contentsupdate, self);
	check_b(row->id != 0, "rowid should be set.");
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)row->hash.data[0]));
	check(svdb_qry_bindint64(&qry, self, 2, (int64_t)row->hash.data[1]));
	check(svdb_qry_bindint64(&qry, self, 3, (int64_t)row->hash.data[2]));
	check(svdb_qry_bindint64(&qry, self, 4, (int64_t)row->hash.data[3]));
	check(svdb_qry_bindint64(&qry, self, 5, (int64_t)row->contents_length));
	check(svdb_qry_bindint(&qry, self, 6, (int32_t)row->crc32));
	check(svdb_qry_bindint64(&qry, self, 7, (int64_t)make_uint64(cast64u32u(row->original_collection), 
		cast64u32u(row->archivenumber))));

	check(svdb_qry_bindint64(&qry, self, 8, (int64_t)row->most_recent_collection));
	check(svdb_qry_bindint64(&qry, self, 9, (int64_t)row->id));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_bulk_delete_helper(
	svdb_connection* self, const sv_array* arr, const char* qryStart, 
	const char* idcolname, int batchsize)
{
	sv_result currenterr = {};
	bstring query = bfromcstr(qryStart);
	if (!arr || !arr->length)
	{
		goto cleanup;
	}

	batchsize = (batchsize == 0) ? 1000 : batchsize;
	int addedInBatch = 0;
	for (uint32_t i = 0; i < arr->length; i++)
	{
		unsigned long long id = castull(sv_array_at64u(arr, i));
		char buff[PATH_MAX] = { 0 };
		int written = snprintf(buff, countof(buff) - 1, "%s=%llu OR ", idcolname, id);
		check_b(bcatblk(query, buff, written) == BSTR_OK, "concat failed");
		addedInBatch++;
		if (addedInBatch >= batchsize || i >= arr->length - 1)
		{
			bcatblk(query, str_and_len32s("0"));
			check(svdb_runsql(self, cstr(query), blength(query)));
			bassigncstr(query, qryStart);
			addedInBatch = 0;
		}
	}

cleanup:
	bdestroy(query);
	return currenterr;
}

check_result svdb_contents_bulk_delete(svdb_connection* self, const sv_array* arr, int batchsize)
{
	return svdb_bulk_delete_helper(self, arr, "DELETE FROM TblContentsList WHERE ", "ContentsId", batchsize);
}

check_result svdb_contentsiter(svdb_connection* self, void* context, fn_iterate_contents callback)
{
	self->qrystrings[svdb_nqry_contentsiter] =
		"SELECT ContentsHash1, ContentsHash2, ContentsHash3, ContentsHash4, "
		"ContentLength, ContentsId, LastCollectionId, Crc32, ArchiveId FROM TblContentsList";

	sv_result currenterr = {};
	int rc = 0;
	svdb_qry qry = svdb_qry_open(svdb_nqry_contentsiter, self);
	check(svdb_qry_run(&qry, self, false, &rc));
	bool shouldcontinue = true;
	while (rc == SQLITE_ROW && shouldcontinue)
	{
		svdb_contents_row row = { 0 };
		svdb_qry_get_int64(&qry, self, 1, &row.hash.data[0]);
		svdb_qry_get_int64(&qry, self, 2, &row.hash.data[1]);
		svdb_qry_get_int64(&qry, self, 3, &row.hash.data[2]);
		svdb_qry_get_int64(&qry, self, 4, &row.hash.data[3]);
		svdb_qry_get_int64(&qry, self, 5, &row.contents_length);
		svdb_qry_get_int64(&qry, self, 6, &row.id);
		svdb_qry_get_int64(&qry, self, 7, &row.most_recent_collection);
		svdb_qry_get_int(&qry, self, 8, &row.crc32);
		uint64_t archiveid = 0;
		svdb_qry_get_int64(&qry, self, 9, &archiveid);
		row.original_collection = upper32(archiveid);
		row.archivenumber = lower32(archiveid);
		if (row.id) /* don't include placeholder row 0. */
		{
			check(callback(context, &shouldcontinue, &row));
		}

		check(svdb_qry_run(&qry, self, false, &rc));
	}

	check(svdb_qry_disconnect(&qry, self));
cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_filesinsert(svdb_connection* self, const bstring path, 
	uint64_t mostrecentcollection, sv_filerowstatus status, uint64_t *outid)
{
	self->qrystrings[svdb_nqry_filesinsert] =
		"INSERT INTO TblFilesList (Path, ContentLength, ContentsId, LastWriteTime, Status, Flags) "
		"VALUES (?, 0, 0, 0, ?, 0)";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_filesinsert, self);
	check(svdb_qry_bindstr(&qry, self, 1, cstr(path), blength(path), false));
	check(svdb_qry_bindint64(&qry, self, 2, (int64_t)sv_makestatus(mostrecentcollection, status)));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));
	if (outid)
	{
		*outid = cast64s64u(sqlite3_last_insert_rowid(self->db));
		check_b(*outid != 0, "id should not be 0.");
	}

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_filesupdate(svdb_connection* self, const svdb_files_row* row)
{
	self->qrystrings[svdb_nqry_filesupdate] =
		"UPDATE TblFilesList SET ContentLength=?, ContentsId=?, LastWriteTime=?, Status=?, Flags=? "
		"WHERE FilesListId=?";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_filesupdate, self);
	check_b(row->id, "cannot set rowid 0");
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)row->contents_length));
	check(svdb_qry_bindint64(&qry, self, 2, (int64_t)row->contents_id));
	check(svdb_qry_bindint64(&qry, self, 3, (int64_t)row->last_write_time));
	check(svdb_qry_bindint64(&qry, self, 4, (int64_t)sv_makestatus(
		row->most_recent_collection, row->e_status)));
	
	check(svdb_qry_bindint64(&qry, self, 5, (int64_t)row->flags));
	check(svdb_qry_bindint64(&qry, self, 6, (int64_t)row->id));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_files_in_queue(svdb_connection* self, uint64_t status, void* context, fn_iterate_rows callback)
{
	self->qrystrings[svdb_nqry_fileslessthan] =
		"SELECT FilesListId, Path, ContentLength, ContentsId, LastWriteTime, Flags, Status "
		"FROM TblFilesList WHERE Status < ?";

	sv_result currenterr = {};
	int rc = 0;
	bstring path = bstring_open();
	svdb_qry qry = svdb_qry_open(svdb_nqry_fileslessthan, self);
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)status));
	check(svdb_qry_run(&qry, self, false, &rc));
	bool shouldcontinue = true;
	while (rc == SQLITE_ROW && shouldcontinue)
	{
		svdb_files_row row = { 0 };
		bstrClear(path);
		svdb_qry_get_int64(&qry, self, 1, &row.id);
		svdb_qry_get_str(&qry, self, 2, path);
		svdb_qry_get_int64(&qry, self, 3, &row.contents_length);
		svdb_qry_get_int64(&qry, self, 4, &row.contents_id);
		svdb_qry_get_int64(&qry, self, 5, &row.last_write_time);
		svdb_qry_get_int64(&qry, self, 6, &row.flags);
		uint64_t statusgot = 0;
		svdb_qry_get_int64(&qry, self, 7, &statusgot);
		row.e_status = sv_getstatus(statusgot);
		row.most_recent_collection = sv_getcollectionidfromstatus(statusgot);
		if (row.id) /* don't include placeholder row 0. */
		{
			check(callback(context, &shouldcontinue, &row, path));
		}

		check(svdb_qry_run(&qry, self, false, &rc));
	}

	check(svdb_qry_disconnect(&qry, self));

cleanup:
	bdestroy(path);
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_collectioninsert(svdb_connection* self, uint64_t timestarted, uint64_t* rowid)
{
	self->qrystrings[svdb_nqry_collectioninsert] =
		"INSERT INTO TblCollections (Time) "
		"VALUES (?)";

	sv_result currenterr = {};
	*rowid = 0;
	svdb_qry qry = svdb_qry_open(svdb_nqry_collectioninsert, self);
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)timestarted));
	check(svdb_qry_run(&qry, self, true, NULL));
	*rowid = cast64s64u(sqlite3_last_insert_rowid(self->db));
	check_b(*rowid != 0, "collectionid should not be 0.");
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_collectionupdate(svdb_connection* self, const svdb_collections_row* row)
{
	self->qrystrings[svdb_nqry_collectionupdate] =
		"UPDATE TblCollections SET TimeCompleted=?, CountTotalFiles=?, CountNewContents=?, CountNewContentsBytes=? "
		"WHERE CollectionId=?";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_collectionupdate, self);
	check_b(row->id, "cannot set rowid 0");
	check_b(!row->time, "cannot set time started");
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)row->time_finished));
	check(svdb_qry_bindint64(&qry, self, 2, (int64_t)row->count_total_files));
	check(svdb_qry_bindint64(&qry, self, 3, (int64_t)row->count_new_contents));
	check(svdb_qry_bindint64(&qry, self, 4, (int64_t)row->count_new_contents_bytes));
	check(svdb_qry_bindint64(&qry, self, 5, (int64_t)row->id));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_contents_setlastreferenced(svdb_connection* self, uint64_t contentsid, uint64_t collectionid)
{
	self->qrystrings[svdb_nqry_contents_setlastreferenced] =
		"UPDATE TblContentsList SET LastCollectionId=? WHERE ContentsId=?";

	sv_result currenterr = {};
	svdb_qry qry = svdb_qry_open(svdb_nqry_contents_setlastreferenced, self);
	check_b(contentsid != 0, "contentsid should not be 0.");
	check_b(collectionid != 0, "collectionid should not be 0.");
	check(svdb_qry_bindint64(&qry, self, 1, (int64_t)collectionid));
	check(svdb_qry_bindint64(&qry, self, 2, (int64_t)contentsid));
	check(svdb_qry_run(&qry, self, true, NULL));
	check(svdb_qry_disconnect(&qry, self));

cleanup:
	svdb_qry_close(&qry, self);
	return currenterr;
}

check_result svdb_files_bulk_delete(svdb_connection* self, const sv_array* arr, int batchsize)
{
	return svdb_bulk_delete_helper(self, arr, "DELETE FROM TblFilesList WHERE ", "FilesListId", batchsize);
}

check_result svdb_confirmschemaversion(svdb_connection* self, const char* path)
{
	uint32_t version = 0;
	sv_result currenterr = {};
	sv_result errGetVersion = svdb_runsql(self, str_and_len32s("SELECT PropertyVal FROM TblProperties WHERE PropertyName='SchemaVersion'"));
	if (errGetVersion.code)
	{
		sv_log_writeLine("could not prepare query... last ditch effort to try adding schema in case this is a blank db");
		sv_result_close(&errGetVersion);
		sv_result errAddSchema = svdb_addschema(self);
		check_warn(errAddSchema, "failed to prepare query, this db is broken/incomplete\n%s", continue_on_err);
		check(errAddSchema);
	}
	
	check(svdb_propertygetint(self, str_and_len32s("SchemaVersion"), &version));
	check_b(version == 1,
		"database %s could not be loaded, it might be from a future version. %d.", path, version);

cleanup:
	return currenterr;
}

check_result svdb_connection_openhandle(svdb_connection* self)
{
	sv_result currenterr = {};
	check_b(!self->db, "db already open?");
	sv_log_writeLines("Opening db handle", cstr(self->path));

	/* open db in single-threaded mode. */
	check_sql(NULL, sqlite3_open_v2(cstr(self->path), &self->db,
		SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));

	/* set sqlite options. */
	/* considered WAL mode but benchmarks did not show an improvement. */
	check(svdb_runsql(self, str_and_len32s("PRAGMA temp_store = memory")));
	check(svdb_runsql(self, str_and_len32s("PRAGMA page_size = 16384")));
	check(svdb_runsql(self, str_and_len32s("PRAGMA cache_size = 1000")));

cleanup:
	return currenterr;
}

check_result svdb_connection_open(svdb_connection* self, const char* path)
{
	sv_result currenterr = {};
	set_self_zero();
	sv_log_writeLines("svdb_connection_open", path);
	self->path = bfromcstr(path);

	bool is_new_db = !os_file_exists(path);
	check_b(os_isabspath(path), "database expected full path but got %s", path);
	check(svdb_connection_openhandle(self));
	if (is_new_db)
	{
		check(svdb_addschema(self));
	}

	check(svdb_confirmschemaversion(self, path));

cleanup:
	return currenterr;
}

check_result svdb_connection_disconnect(svdb_connection* self)
{
	sv_result currenterr = {};
	if (self)
	{
		/* free all cached queries */
		for (int i = 0; i < svdb_nqry_max; i++)
		{
			if (self->qrys[i])
			{
				check_sql(self->db, sqlite3_finalize(self->qrys[i]));
				self->qrys[i] = NULL;
			}
		}

		if (self->db)
		{
			check_sql(NULL, sqlite3_close(self->db));
		}

		bdestroy(self->path);
		set_self_zero();
	}
cleanup:
	return currenterr;
}

void svdb_connection_close(svdb_connection* self)
{
	check_warn(svdb_connection_disconnect(self), 
		"Encountered warning when closing database.", continue_on_err);

	set_self_zero();
}

/* transactions */
check_result svdb_txn_open(svdb_txn* self, svdb_connection* db)
{
	sv_result currenterr = {};
	set_self_zero();
	check(svdb_runsql(db, str_and_len32s("BEGIN TRANSACTION")));
	self->active = true;
cleanup:
	return currenterr;
}

check_result svdb_txn_commit(svdb_txn* self, svdb_connection* db)
{
	sv_result currenterr = {};
	check_b(self->active, "tried to commit an inactive txn");
	check(svdb_runsql(db, str_and_len32s("COMMIT TRANSACTION")));
	self->active = false;
cleanup:
	return currenterr;
}

check_result svdb_txn_rollback(svdb_txn* self, svdb_connection* db)
{
	sv_result currenterr = {};
	if (self->active)
	{
		check(svdb_runsql(db, str_and_len32s("ROLLBACK TRANSACTION")));
	}

	self->active = false;
	set_self_zero();
cleanup:
	return currenterr;
}

void svdb_txn_close(svdb_txn* self, svdb_connection* db)
{
	if (self && db)
	{
		check_warn(svdb_txn_rollback(self, db), 
			"Encountered warning when closing transaction.", continue_on_err);
	}

	set_self_zero();
}

check_result svdb_clear_database_content(svdb_connection* self)
{
	sv_result currenterr = {};
	check(svdb_runsql(self, str_and_len32s("DELETE FROM TblCollections WHERE CollectionId != 0")));
	check(svdb_runsql(self, str_and_len32s("DELETE FROM TblFilesList WHERE FilesListId != 0")));
	check(svdb_runsql(self, str_and_len32s("DELETE FROM TblContentsList WHERE ContentsId != 0")));
cleanup:
	return currenterr;
}

void svdb_files_row_string(const svdb_files_row* row, bstring s)
{
	bassignformat(s, "contents_length=%llu, contents_id=%llu, last_write_time=%llu, "
		"flags=%llu, most_recent_collection=%llu, e_status=%d, id=%llu",
		castull(row->contents_length), castull(row->contents_id), castull(row->last_write_time), 
		castull(row->flags), castull(row->most_recent_collection), 
		row->e_status, castull(row->id));
}

void svdb_collectiontostring(const svdb_collections_row* row, bool verbose, bool everyfield, bstring s)
{
	if (everyfield)
	{
		bassignformat(s, "time=%llu, time_finished=%llu, count_total_files=%llu, "
			"count_new_contents=%llu, count_new_contents_bytes=%llu, id=%llu",
			castull(row->time), castull(row->time_finished), castull(row->count_total_files),
			castull(row->count_new_contents), castull(row->count_new_contents_bytes), castull(row->id));
	}
	else
	{
		bassignformat(s, "Collection :%llu ", castull(row->id));
		struct tm* ptime = localtime((time_t*)row->time_finished);
		if (ptime)
		{
			bformata(s, "%04d/%02d/%02d   %02d:%02d%02d\n", 
				ptime->tm_year + 1900, ptime->tm_mon + 1, ptime->tm_mday,
				ptime->tm_hour, ptime->tm_min, ptime->tm_sec);
		}
		if (verbose)
		{
			double new_bytes = (double)row->count_new_contents_bytes / (1024.0 * 1024.0);
			bformata(s, "\tTotal files: %llu. Added %.3fMb data across %llu changed files.",
				castull(row->count_total_files),
				new_bytes,
				castull(row->count_new_contents));
		}
	}
}

void svdb_contents_row_string(const svdb_contents_row* row, bstring s)
{
	bstring shash = bstring_open();
	hash256tostr(&row->hash, shash);
	bassigncstr(s, "hash=");
	bconcat(s, shash);
	bdestroy(shash);
	bformata(s, ", crc32=%x, contents_length=%llu, most_recent_collection=%llu, "
		"original_collection=%u, archivenumber=%u, id=%llu",
		row->crc32, castull(row->contents_length), castull(row->most_recent_collection), 
		row->original_collection, row->archivenumber, castull(row->id));
}
