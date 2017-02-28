#pragma once

typedef struct sqlite3 dbhandle;
typedef struct sqlite3_stmt dbqry;
typedef Int32 SnpshtID;
typedef long long RowID;
const RowID RowIDInvalid = -1;

#define check_warn_sql(db, code) do { \
	int coderesult = (code); \
	if (coderesult != SQLITE_OK) { \
		check_warn(0, (#code), \
		(db) ? sqlite3_errmsg(db) : \
		sqlite3_errstr(coderesult), coderesult); \
	} } while (0)

#define check_sql(db, code) do { \
	int coderesult = (code); \
	if (coderesult != SQLITE_OK) { \
		check_b(0, (#code), \
		(db) ? sqlite3_errmsg(db) : \
		sqlite3_errstr(coderesult), coderesult); \
	} } while (0)

/* we'll cache database queries for better performance */

typedef enum SvzpEnumQry {
	SvzpQryNone,
	SvzpQryAddDirName,
	SvzpQryAddFileName,
	SvzpQryAddFileInfo,
	SvzpQryGetDirNameId,
	SvzpQryGetFileNameId,
	SvzpQryGetFileInfoId,
	SvzpQrySameFileExists,
	SvzpQrySameHashExists,
	SvzpQrySameHashAndSizeExists,
	SvzpQryAddRow
} SvzpEnumQry;

enum { SvzpQryMAX = 64 };

/* database connection */
typedef struct bcdb {
	dbhandle* db;
	bcstr path;
	dbqry* qrys[SvzpQryMAX];
	const char* sz_qrys[SvzpQryMAX];
} bcdb;

CHECK_RESULT Result bcdb_runsql(bcdb* self, const char* sql)
{
	Result currenterr = {};
	char* errormsg = 0;
	int result = sqlite3_exec(self->db, sql, NULL, 0, &errormsg);
	check_b(result == SQLITE_OK, errormsg, sql, result);
	
cleanup:
	sqlite3_free(errormsg);
	return currenterr;
}

CHECK_RESULT Result bcdb_addschema(bcdb* self, const char* path, bool is_new_db);
CHECK_RESULT Result bcdb_open(bcdb* self, const char* path)
{
	Result currenterr = {};
	set_self_zero();
	self->path = bcstr_open();
	bool is_new_db = !os_file_exists(path);
	bcstr_assign(&self->path, path);

	/* open db in single-threading mode. */
	check_sql(NULL, sqlite3_open_v2(path,
		&self->db, SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));
	check(bcdb_addschema(self, path, is_new_db));

	/* pre-cache database queries, for future use */
	for (int i = 0; i < SvzpQryMAX; i++)
	{
		if (self->sz_qrys[i])
		{
			check_sql(self->db, sqlite3_prepare_v2(self->db, self->sz_qrys[i],
				-1 /*read whole string*/, &self->qrys[i], 0));
			check_b(self->qrys[i], "");
		}
	}

cleanup:
	return currenterr;
}

void bcdb_close(bcdb* self)
{
	if (self)
	{
		/* free all cached queries */
		for (int i=0; i<SvzpQryMAX; i++)
		{
			if (self->qrys[i])
			{
				check_warn_sql(NULL, sqlite3_finalize(self->qrys[i]));
				self->qrys[i] = NULL;
			}
		}
		if (self->db)
		{
			check_warn_sql(NULL, sqlite3_close(self->db));
		}
		bcstr_close(&self->path);
		set_self_zero();
	}
}

typedef struct bcdb_txn {
	bool active;
} bcdb_txn;

CHECK_RESULT Result bcdb_txn_open(bcdb_txn* self, bcdb* db)
{
	Result currenterr = {};
	set_self_zero();
	check(bcdb_runsql(db, "BEGIN TRANSACTION"));
	self->active = true;
cleanup:
	return currenterr;
}

CHECK_RESULT Result bcdb_txn_commit(bcdb_txn* self, bcdb* db)
{
	Result currenterr = {};
	check(bcdb_runsql(db, "COMMIT TRANSACTION"));
	self->active = false;
cleanup:
	return currenterr;
}

CHECK_RESULT Result bcdb_txn_rollback(bcdb_txn* self, bcdb* db)
{
	Result currenterr = {};

	if (self->active)
		check(bcdb_runsql(db, "ROLLBACK TRANSACTION"));

	self->active = false;
cleanup:
	return currenterr;
}

void bcdb_txn_close(bcdb_txn* self, bcdb* db)
{
	if (self)
	{
		check_warn(!self->active, "note: exiting transaction before commit");

		Result err = bcdb_txn_rollback(self, db);
		check_warn(err.errortag == 0, err.errormsg);
		Result_close(&err);
		set_self_zero();
	}
}

static const bool bcqry_isselect(const char* sql)
{
	return s_startswith(sql, "SELECT") || s_startswith(sql, "select");
}

/* database query */
typedef struct bcqry {
	dbqry* qry;
	bool from_cache;
	bool is_select;
} bcqry;

CHECK_RESULT Result bcqry_opencached(bcqry* self, bcdb* db, size_t eqry)
{
	Result currenterr = {};
	set_self_zero();
	self->from_cache = true;
	
	check_fatal(eqry >= 0 && eqry < SvzpQryMAX);
	self->qry = db->qrys[eqry];
	self->is_select = bcqry_isselect(db->sz_qrys[eqry]);
	check_b(self->qry != NULL);

cleanup:
	return currenterr;
}

CHECK_RESULT Result bcqry_open(bcqry* self, bcdb* db, const char* sql)
{
	Result currenterr = {};
	set_self_zero();
	self->from_cache = false;

	check_b(self->qry == NULL);
	check_sql(db->db, sqlite3_prepare_v2(db->db, 
		sql, -1 /*read whole string*/, &self->qry, 0));
	check_b(self->qry != NULL);
	self->is_select = bcqry_isselect(sql);

cleanup:
	return currenterr;
}

CHECK_RESULT Result bcqry_bindint64(bcdb* db, 
	bcqry* self, int index, Int64 value)
{
	Result currenterr = {};
	check_sql(db->db, sqlite3_bind_int64(self->qry, index, value));
cleanup:
	return currenterr;
}

CHECK_RESULT Result bcqry_bindint(bcdb* db, 
	bcqry* self, int index, int value)
{
	Result currenterr = {};
	check_sql(db->db, sqlite3_bind_int(self->qry, index, value));
cleanup:
	return currenterr;
}

CHECK_RESULT Result bcqry_bindblobstatic(bcdb* db, 
	bcqry* self, int index, const byte* pb, int cb)
{
	Result currenterr = {};
	check_sql(db->db, sqlite3_bind_blob(self->qry, index, pb, cb, SQLITE_STATIC));
cleanup:
	return currenterr;
}

void bcqry_close(bcqry* self, bcdb* db)
{
	if (self)
	{
		if (self->from_cache && self->qry)
		{
			check_warn_sql(db->db, sqlite3_reset(self->qry));
		}
		else if (!self->from_cache && self->qry)
		{
			check_warn_sql(db->db, sqlite3_finalize(self->qry));
		}
		set_self_zero();
	}
}

/* helper to walk a string chr by chr */
static char bcqry_runf_nextchr(Uint32* i, Uint32 len, const char* sz)
{
	if (*i >= len)
		return '\0';
	else
		return sz[(*i)++];
}

/* we'll treat as either an int or a const char*, if it's a valid pointer */
static const char* bcqry_cached(SvzpEnumQry eqry)
{
	return (const char*)eqry;
}

/*
Format string (separated by spaces):
%lld	bind int64
%s%d	bind string and int32 length
%plld	get int64
%ps		get bcstr string
*/
CHECK_RESULT Result bcqry_runf(bcdb* db, bcqry* pqry, bool expect_results, const char* sql, const char* fmt, ...)
{
	Result currenterr = {};
	bcqry qry_temp = {};
	bool need_to_bind = false;
	va_list argp;
	va_start(argp, fmt);

	if (!pqry)
	{
		/* use a temporary query since none was passed in. */
		pqry = &qry_temp;
	}
	if (!pqry->qry && (intptr_t)sql < SvzpQryMAX)
	{
		/* this is a cached query */
		check(bcqry_opencached(pqry, db, (intptr_t)sql));
		need_to_bind = true;
	}
	else if (!pqry->qry)
	{
		/* not a cached query, parse it */
		check(bcqry_open(pqry, db, sql));
		need_to_bind = true;
	}

	/* first, bind inputs to the query */
	Uint32 len = strlen32(fmt);
	Uint32 i = 0, binding = 1;
	while (1)
	{
		bool valid = false;
		char cspace = bcqry_runf_nextchr(&i, len, fmt);
		char c1 = bcqry_runf_nextchr(&i, len, fmt);
		char c2 = bcqry_runf_nextchr(&i, len, fmt);
		if (cspace == '\0' || (cspace == ' ' && c1 == '=' && c2 == '>'))
		{
			break;
		}
		else if (cspace == ' ' && c1 == '%')
		{
			switch (c2)
			{
			case 'l': {
				char c3 = bcqry_runf_nextchr(&i, len, fmt);
				char c4 = bcqry_runf_nextchr(&i, len, fmt);
				if (c3 == 'l' && c4 == 'd')
				{
					valid = true;
					Int64 val64 = va_arg(argp, Int64);
					if (need_to_bind)
						check(bcqry_bindint64(db, pqry, binding++, val64));
				}
				break; }
			case 's': {
				char c3 = bcqry_runf_nextchr(&i, len, fmt);
				char c4 = bcqry_runf_nextchr(&i, len, fmt);
				if (c3 == '%' && c4 == 'd')
				{
					valid = true;
					const byte* bytes = va_arg(argp, const byte*);
					Uint32 len = va_arg(argp, Uint32);
					if (need_to_bind)
						check(bcqry_bindblobstatic(db, pqry, binding++, bytes, len));
				}
				break; }
			default:
				break;
			}
		}

		check_b(valid, "bad format", "", c1, c2);
	}

	/* execute query */
	int rc = sqlite3_step(pqry->qry);
	if (rc == SQLITE_ROW)
	{
		/* retrieve results from query, based on fmt string*/
		Uint32 index = 0;
		while (1)
		{
			bool valid = false;
			char cspace = bcqry_runf_nextchr(&i, len, fmt);
			char c1 = bcqry_runf_nextchr(&i, len, fmt);
			char c2 = bcqry_runf_nextchr(&i, len, fmt);
			char c3 = bcqry_runf_nextchr(&i, len, fmt);
			if (cspace == '\0')
			{
				break;
			}
			else if (cspace == ' ' && c1 == '%' && c2 == 'p')
			{
				/* verify col is not null */
				check_b(sqlite3_column_bytes(pqry->qry, index) > 0, "");
				switch (c3)
				{
				case 'l': {
					char c5 = bcqry_runf_nextchr(&i, len, fmt);
					char c6 = bcqry_runf_nextchr(&i, len, fmt);
					if (c5 == 'l' && c6 == 'd')
					{
						Int64* p64 = va_arg(argp, Int64*);
						*p64 = sqlite3_column_int64(pqry->qry, index++);
						valid = true;
					}
					break; }
				case 's': {
					bcstr* str = va_arg(argp, bcstr*);
					bcstr_data_settozeros(str, sqlite3_column_bytes(pqry->qry, index));
					memcpy(bcstr_data_get(str),
						sqlite3_column_blob(pqry->qry, index),
						sqlite3_column_bytes(pqry->qry, index));
					index++;
					valid = true;
					break; }
				default:
					break;
				}
			}

			check_b(valid, "bad format", "", c1, c2);
		}
	}
	else
	{
		if (rc == SQLITE_DONE && (pqry->is_select && expect_results))
			check_b(0, "no row found");
		else if (rc != SQLITE_DONE)
			check_sql(db->db, rc ? rc : -1);
	}

	if (!pqry->is_select && expect_results)
		check_b(sqlite3_changes(db->db) > 0);

cleanup:
	va_end(argp);
	bcqry_close(&qry_temp, db);
	return currenterr;
}

/* to check format strings, compile with this on and look for Wformat warnings.*/
#ifdef bcqry_runf_debug
Result bcqry_runf_debug_fn(int)
{
	die(0, "bcqry_runf_debug only works for compile-time warnings, not runtime.");
	return ResultOk;
}

#define bcqry_runf(pardb, parpqry, parexpect, parsql, ...) \
	bcqry_runf_debug_fn(printf( __VA_ARGS__))
#endif

/* main table schema. */
/* don't use hash as primary key, for better add performance. */
/* for hash, benchmarks showed that three ints better than blob. */
#define TblMainCols \
	"DirNameId," \
	"FileNameId," \
	"Filesize," \
	"ModifiedTime," \
	"Archive," \
	"SnapshotLastSeen," \
	"ContentHash1," \
	"ContentHash2," \
	"ContentHash3," \
	"FileInfoId"

const char* svzp_createschema[] = {
	"CREATE TABLE TblProperties ("
	"SchemaVersion INTEGER NOT NULL)",

	/* autoincrement: do not reuse values after rows deleted. */
	"CREATE TABLE TblSnapshots ("
	"SnapshotId INTEGER PRIMARY KEY AUTOINCREMENT,"
	"Created INTEGER NOT NULL,"
	"ArchiveCount INTEGER NOT NULL)",

	"CREATE TABLE TblDirNames ("
	"DirNameId INTEGER PRIMARY KEY AUTOINCREMENT,"
	"DirName BLOB NOT NULL)",

	"CREATE TABLE TblFileNames ("
	"FileNameId INTEGER PRIMARY KEY AUTOINCREMENT,"
	"FileName BLOB NOT NULL)",

	"CREATE TABLE TblFileInfo ("
	"FileInfoId INTEGER PRIMARY KEY AUTOINCREMENT,"
	"FileInfo BLOB NOT NULL)",

	"CREATE TABLE TblMain ("
	"MainId INTEGER PRIMARY KEY,"
	"DirNameId INTEGER NOT NULL,"
	"FileNameId INTEGER NOT NULL,"
	"Filesize INTEGER NOT NULL,"
	"ModifiedTime INTEGER NOT NULL,"
	"Archive INTEGER NOT NULL,"
	"SnapshotLastSeen INTEGER NOT NULL,"
	"ContentHash1 INTEGER NOT NULL,"
	"ContentHash2 INTEGER NOT NULL,"
	"ContentHash3 INTEGER NOT NULL,"
	"FileInfoId INTEGER NOT NULL)",

	"CREATE INDEX IxDirNames on TblDirNames(DirName)",
	"CREATE INDEX IxFileNames on TblFileNames(FileName)",
	"CREATE INDEX IxFileInfo on TblFileInfo(FileInfo)",
	"CREATE INDEX IxMainDirNameId on TblMain(DirNameId)",
	"CREATE INDEX IxMainContentHash on TblMain(ContentHash1)",

	/* let's reserve 0 as an id that will never be used. */
	"INSERT INTO TblSnapshots"
	" VALUES (0,0,0)",
	"INSERT INTO TblDirNames"
	" VALUES (0,x'00010100')",
	"INSERT INTO TblFileNames"
	" VALUES (0,x'00010100')",
	"INSERT INTO TblFileInfo"
	" VALUES (0,x'00010100')",
	"INSERT INTO TblMain"
	" values (0,0,0,0,0,0,0,0,0,0,0)",
	"DELETE FROM TblSnapshots WHERE 1",

	/* schema version, to detect newer or older databases. */
	"INSERT INTO TblProperties (SchemaVersion) VALUES (1)",
};

CHECK_RESULT Result bcdb_getschemaversion(bcdb* self, int* version)
{
	return bcqry_runf(self, NULL, true,
		"SELECT SchemaVersion from TblProperties",
		" => %p4", version);
}

CHECK_RESULT Result bcdb_addschema(bcdb* self, const char* path, bool is_new_db)
{
	Result currenterr = {};
	bcdb_txn txn = {};

	/* add cached queries */
	self->sz_qrys[SvzpQryAddDirName] = 
		"INSERT INTO TblDirNames (?, ?)";
	self->sz_qrys[SvzpQryAddFileName] = 
		"INSERT INTO TblFileNames (?, ?)";
	self->sz_qrys[SvzpQryAddFileInfo] = 
		"INSERT INTO TblFileInfo (?, ?)";
	self->sz_qrys[SvzpQryGetDirNameId] = 
		"SELECT DirNameId FROM TblDirNames where DirName = ?";
	self->sz_qrys[SvzpQryGetFileNameId] =
		"SELECT FileNameId FROM TblFileNames where FileName = ?";
	self->sz_qrys[SvzpQryGetDirNameId] =
		"SELECT FileInfoId FROM TblFileInfo where FileInfo = ?";
	self->sz_qrys[SvzpQrySameFileExists] =
		"UPDATE TblMain set SnapshotLastSeen=? where DirNameId=? and FileNameId=? "
		"and Filesize=? and ModifiedTime=? and LastSeen=?";
	self->sz_qrys[SvzpQrySameHashAndSizeExists] =
		"SELECT Archive from TblMain where "
		"Filesize=? and "
		"SnapshotLastSeen=? and "
		"ContentHash1=? and "
		"ContentHash2=? and "
		"ContentHash3=?) LIMIT 1";
	self->sz_qrys[SvzpQrySameHashExists] =
		"SELECT Archive from TblMain where "
		"Filesize*0=?*0 and "
		"SnapshotLastSeen=? and "
		"ContentHash1=? and "
		"ContentHash2=? and "
		"ContentHash3=?) LIMIT 1";
	self->sz_qrys[SvzpQryAddRow] =
		"INSERT INTO TblMain"
		" values (null, ?,?,?,?,?,?,?,?,?,?,?)";

	/* set sqlite opitions. */
	/* using WAL mode is an option, benchmarks did not show improvement. */
	check(bcdb_runsql(self, "PRAGMA temp_store = memory"));
	check(bcdb_runsql(self, "PRAGMA page_size = 16384"));
	check(bcdb_runsql(self, "PRAGMA cache_size = 1000"));
	
	if (is_new_db)
	{
		check(bcdb_txn_open(&txn, self));
		for (Uint32 i=0; i<countof32(svzp_createschema); i++)
			check(bcdb_runsql(self, svzp_createschema[i]));
	
		check(bcdb_txn_commit(&txn, self));
	}
	else
	{
		int version = 0;
		check(bcdb_getschemaversion(self, &version));
		check_b(version == 1, path, "database corrupt or incomplete. "
			"please exit and rename the file to continue. saw version:", version);
	}

cleanup:
	bcdb_txn_close(&txn, self);
	return currenterr;
}


CHECK_RESULT Result svzpdb_getnameid(bcdb* db,
	const char* s, Uint32 len, SvzpEnumQry get, SvzpEnumQry add, RowID* out)
{
	Result currenterr = {};
	*out = -1;
	check(bcqry_runf(db, NULL, false, bcqry_cached(get),
		" %s%d => %p8", s, len, out));

	if (*out == -1)
	{
		check(bcqry_runf(db, NULL, true, bcqry_cached(add),
			" %s%d", s, len));
		*out = sqlite3_last_insert_rowid(db->db);
	}

cleanup:
	return currenterr;
}

/* if the same file exists at the same location, simply update last seen. */
CHECK_RESULT Result svzpdb_qrysamefileexists(bcdb* db, 
	SnpshtID cursnapshotid, 
	RowID dirid, RowID flnameid,
	Uint64 filesize, Uint64 modtime,
	SnpshtID prevsnapshotid, bool* wasfound)
{
	*wasfound = false;

	if (prevsnapshotid <= 0)
		return ResultOk;

	/* benchmarks show that smaller integers use less disk space */
	filesize &= UINT32_MAX;
	modtime &= UINT32_MAX;
	Result ret = bcqry_runf(db, NULL, false,
		bcqry_cached(SvzpQrySameFileExists),
		" %lld %lld %lld %lld %lld %lld",
		(Uint64)cursnapshotid,
		dirid, flnameid,
		filesize, modtime,
		(Uint64)prevsnapshotid);

	*wasfound = sqlite3_changes(db->db) > 0;
	return ret;
}

/* look for an entry with the same content hash */
CHECK_RESULT Result svzpdb_qrysamecontenthash(bcdb* db,
	SvzpEnumQry equery, Uint64 filesize,
	SnpshtID prevsnapshotid, const hash192bits* contenthash,
	Int32* outarchive)
{
	Int64 outarchive64 = 0;
	*outarchive = 0;

	if (prevsnapshotid <= 0)
		return ResultOk;

	/* benchmarks show that smaller integers use less disk space */
	filesize &= UINT32_MAX;
	Result ret = bcqry_runf(db, NULL, false, bcqry_cached(equery),
		" %lld %lld %lld %lld %lld => %plld",
		filesize,
		(Uint64)prevsnapshotid,
		contenthash->n[0], contenthash->n[1], contenthash->n[2],
		&outarchive64);
	
	*outarchive = outarchive64 & UINT32_MAX;
	return ret;
}

CHECK_RESULT Result svzpdb_add(bcdb* db,
	RowID dirid, RowID flnameid,
	Uint64 filesize, Uint64 modtime,
	Int32 archive, SnpshtID cursnapshotid, 
	const hash192bits* contenthash, RowID flinfoid)
{
	/* benchmarks show that smaller integers use less disk space */
	filesize &= UINT32_MAX;
	modtime &= UINT32_MAX;
	return bcqry_runf(db, NULL, true, bcqry_cached(SvzpQryAddRow),
		" %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
		dirid, flnameid,
		filesize, modtime,
		archive, (Uint64)cursnapshotid,
		contenthash->n[0], contenthash->n[1], contenthash->n[2],
		flinfoid);
}

CHECK_RESULT Result svzpdb_getlastsnapshotid(bcdb* db, SnpshtID* outsnapshotid)
{
	*outsnapshotid = -1;
	return bcqry_runf(db, NULL, false,
		"SELECT MAX(SnapshotId) from TblSnapshots",
		" => %plld", outsnapshotid);
}

CHECK_RESULT Result svzpdb_addnewsnapshotid(bcdb* db, Uint64 timecreated, SnpshtID* outsnapshotid)
{
	Result ret = bcqry_runf(db, NULL, true,
		"INSERT INTO TblSnapshots (Created, ArchiveCount) values (?, 0)",
		"%d", timecreated);

	*outsnapshotid = safecast32s(sqlite3_last_insert_rowid(db->db));
	return ret;
}

/* using sqlite_dump from sqliteshell.c is possible, 
but since we store strings as blobs, not very readable. */
CHECK_RESULT Result bcdb_dumptodisk(bcdb* db, const char* path)
{
	Result currenterr = {};
	bcfile file = {};
	bcqry qry = {};
	bcstr str = bcstr_open();
	check(bcfile_open(&file, path, "w"));
	RowID rowid = 0;
	long long n1=0, n2=0, n3=0, n4=0, n5=0,
		n6=0, n7=0, n8=0, n9=0, n10=0;

	/* dump TblProperties */
	fputs("SELECT FROM TblProperties\n", file.file);
	fputs("SchemaVersion\n", file.file);
	do
	{
		rowid = RowIDInvalid;
		check(bcqry_runf(db, &qry, false,
			"SELECT SchemaVersion from TblProperties",
			" => %plld", &rowid));

		if (rowid != RowIDInvalid)
		{
			fprintf(file.file, "%lld\n", rowid);
		}
	} while (rowid != RowIDInvalid);
	bcqry_close(&qry, db);

	/* dump TblSnapshots */
	fputs("SELECT FROM TblSnapshots\n", file.file);
	fputs("SnapshotId Created ArchiveCount\n", file.file);
	do
	{
		rowid = RowIDInvalid;
		check(bcqry_runf(db, &qry, false,
			"SELECT SnapshotId, Created, ArchiveCount from TblSnapshots",
			" => %plld %plld %plld", &rowid, &n1, &n2));

		if (rowid != RowIDInvalid)
		{
			fprintf(file.file, "%lld %lld %lld\n",
				rowid, n1, n2);
		}
	} while (rowid != RowIDInvalid);
	bcqry_close(&qry, db);

	/* dump TblMain */
	fputs("SELECT FROM TblMain\n", file.file);
	fputs("MainId " TblMainCols "\n", file.file);
	do
	{
		rowid = RowIDInvalid;
		check(bcqry_runf(db, &qry, false,
			"SELECT MainId, " TblMainCols " from TblMain",
			" => %plld %plld %plld %plld %plld %plld %plld %plld %plld %plld %plld",
			&rowid, &n1, &n2, &n3, &n4, &n5, &n6, &n7, &n8, &n9, &n10));
	
		if (rowid == RowIDInvalid)
		{
			fprintf(file.file,
				"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
				rowid, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10);
		}
	} while (rowid != RowIDInvalid);
	bcqry_close(&qry, db);

	/* these 3 tables all have the same structure */
	const char* tables[] = {"SELECT FROM TblDirNames", 
		"SELECT FROM TblFileNames", "SELECT FROM TblFileInfo"};
	for (Uint32 i = 0; i < countof32(tables); i++)
	{
		fputs(tables[i], file.file);
		fputs("\n", file.file);
		do
		{
			rowid = RowIDInvalid;
			check(bcqry_runf(db, &qry, false, tables[i],
				" => %plld %ps", &rowid, &str));

			if (rowid != RowIDInvalid)
			{
				fprintf(file.file, "%lld \t%s\n", rowid, str.s);
			}
		} while (rowid != RowIDInvalid);
		bcqry_close(&qry, db);
	}

cleanup:
	bcstr_close(&str);
	bcfile_close(&file);
	bcqry_close(&qry, db);
	return currenterr;
}
