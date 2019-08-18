/*
dbaccess.c

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

svdb_qry svdb_qry_open(svdb_qid qid, svdb_db *db)
{
    svdb_qry self = {};
    sv_log_fmt("qry %d", qid);
    self.qry_number = qid;
    check_fatal(db, "db was null");
    if (!db->qrys[qid])
    {
        check_warn(svdb_preparequery(db, self.qry_number), "Failed to load qid",
            exit_on_err);
    }

    return self;
}

check_result svdb_qry_bind_uint(
    svdb_qry *self, svdb_db *db, int col, uint32_t val)
{
    sv_result currenterr = {};
    check_sql(
        db->db, sqlite3_bind_int(db->qrys[self->qry_number], col, (int32_t)val));

cleanup:
    return currenterr;
}

check_result svdb_qry_bind_uint64(
    svdb_qry *self, svdb_db *db, int col, uint64_t val)
{
    sv_result currenterr = {};
    check_sql(db->db,
        sqlite3_bind_int64(db->qrys[self->qry_number], col, (int64_t)val));

cleanup:
    return currenterr;
}

check_result svdb_qry_bindstr(svdb_qry *self, svdb_db *db, int col,
    const char *str, int length, bool is_static)
{
    sv_result currenterr = {};
    check_sql(db->db,
        sqlite3_bind_text(db->qrys[self->qry_number], col, str, length,
            is_static ? SQLITE_STATIC : SQLITE_TRANSIENT));

cleanup:
    return currenterr;
}

check_result svdb_preparequery(svdb_db *self, svdb_qid qid)
{
    sv_result currenterr = {};
    check_fatal(
        qid > svdb_qid_none && qid < svdb_qid_max, "invalid qry number %d", qid);

    if (!self->qrys[qid])
    {
        sqlite_qry *prepared = NULL;
        sv_log_fmt("svdb_preparequery for qid #%d", qid);
        check_b(self->qrystrings[qid], "no qid set for #%d", qid);
        check_sql(self->db,
            sqlite3_prepare_v2(self->db, self->qrystrings[qid],
                -1 /* read whole string */, &prepared, 0));

        check_b(prepared, "%s", self->qrystrings[qid]);
        self->qrys[qid] = prepared;
    }

cleanup:
    return currenterr;
}

void svdb_qry_get_uint(svdb_qry *self, svdb_db *db, int col, uint32_t *out)
{
    /* make the column 1-based for consistency with binding */
    *out = (uint32_t)sqlite3_column_int(/* allow cast */
        db->qrys[self->qry_number], col - 1);
}

void svdb_qry_get_uint64(svdb_qry *self, svdb_db *db, int col, uint64_t *out)
{
    /* make the column 1-based for consistency with binding */
    *out = (uint64_t)sqlite3_column_int64(/* allow cast */
        db->qrys[self->qry_number], col - 1);
}

void svdb_qry_get_str(svdb_qry *self, svdb_db *db, int col, bstring s)
{
    /* make the column 1-based for consistency with binding */
    /* map both sql-null and "" to "" */
    int len = sqlite3_column_bytes(db->qrys[self->qry_number], col - 1);

    if (len)
    {
        bassignblk(
            s, sqlite3_column_text(db->qrys[self->qry_number], col - 1), len);
    }
    else
    {
        bstrclear(s);
    }
}

check_result svdb_qry_run(
    svdb_qry *self, svdb_db *db, svdb_expectchanges confirm_changes, int *prc)
{
    sv_result currenterr = {};
    int rc = sqlite3_step(db->qrys[self->qry_number]);
    if (prc)
    {
        *prc = rc;
    }

    check_sql(db->db, rc);
    if (confirm_changes != expectchangesunknown)
    {
        /* note that sqlite3_changes is untouched for all but insert, delete,
         * update */
        check_b(
            (confirm_changes == expectchanges) == (sqlite3_changes(db->db) > 0),
            "running qid %s changed/didn't change rows",
            db->qrystrings[self->qry_number]);
    }

cleanup:
    return currenterr;
}

check_result svdb_qry_disconnect(svdb_qry *self, svdb_db *db)
{
    sv_result currenterr = {};
    if (self)
    {
        if (self->qry_number && db)
        {
            check_sql(db->db, sqlite3_reset(db->qrys[self->qry_number]));
        }

        set_self_zero();
    }

cleanup:
    return currenterr;
}

void svdb_qry_close(svdb_qry *self, svdb_db *db)
{
    if (self)
    {
        check_warn(svdb_qry_disconnect(self, db),
            "Encountered warning when closing qry.", continue_on_err);

        set_self_zero();
    }
}

const char *schema_cmds[] = {
    "CREATE TABLE TblCollections ("
    "CollectionId INTEGER PRIMARY KEY AUTOINCREMENT,"
    "Time INTEGER,"
    "TimeCompleted INTEGER,"
    "CountTotalFiles INTEGER,"
    "CountNewContents INTEGER,"
    "CountNewContentsBytes INTEGER)",
    "CREATE TABLE TblContentsList ("
    "ContentsId INTEGER PRIMARY KEY AUTOINCREMENT,"
    "ContentsHash1 INTEGER,"
    "ContentsHash2 INTEGER,"
    "ContentsHash3 INTEGER,"
    "ContentsHash4 INTEGER,"
    "ContentLength INTEGER,"
    "CompressedContentLength INTEGER,"
    "Crc32 INTEGER,"
    "ArchiveId INTEGER,"
    "LastCollectionId INTEGER)",
    "CREATE TABLE TblArchives ("
    "RowId INTEGER PRIMARY KEY AUTOINCREMENT,"
    "ArchiveId INTEGER,"
    "ModifiedTime INTEGER,"
    "CompactionRemovedDataBeforeThisCollection INTEGER,"
    "ChecksumString TEXT)",
    "CREATE TABLE TblKnownVaults ("
    "KnownVaultsId INTEGER PRIMARY KEY AUTOINCREMENT,"
    "Name TEXT UNIQUE,"
    "AwsRegion TEXT,"
    "AwsVaultName TEXT,"
    "AwsVaultARN TEXT)",
    "CREATE TABLE TblKnownVaultArchives ("
    "KnownArchiveId INTEGER PRIMARY KEY AUTOINCREMENT,"
    "KnownVaultsId INTEGER,"
    "CloudPath TEXT,"
    "AwsArchiveId TEXT,"
    "AwsDescription TEXT,"
    "AwsCreationDate TEXT,"
    "Size INTEGER,"
    "Crc32 INTEGER,"
    "ModifiedTime INTEGER, "
    "UNIQUE(CloudPath, KnownVaultsId))",
    "CREATE TABLE TblProperties ("
    "PropertyName TEXT PRIMARY KEY,"
    "PropertyVal)",
    "INSERT INTO TblProperties "
    "VALUES ('SchemaVersion', 1)",
    "CREATE TABLE TblFilesList ("
    "FilesListId INTEGER PRIMARY KEY AUTOINCREMENT,"
#ifdef __linux__
    "Path TEXT COLLATE BINARY,"
#else
    "Path TEXT COLLATE NOCASE,"
#endif
    "ContentLength INTEGER,"
    "ContentsId INTEGER,"
    "LastWriteTime INTEGER,"
    "Status INTEGER,"
    "Flags TEXT)",
    "CREATE UNIQUE INDEX IxTblFilesListPath "
    "ON TblFilesList(Path)",
    "CREATE INDEX IxTblContentsListHash "
    "ON TblContentsList(ContentsHash1)",
    "CREATE INDEX IxTblKnownVaultArchivesDescription "
    "ON TblKnownVaultArchives(CloudPath)",

    /* AUTOINCREMENT means that ids are not re-used. We'll enable it
    even though it isn't required by our logic, for better diagnostics.
    We don't use hashes as primary keys as it affects INSERT performance.
    In benchmarks, int64[4] is better than blob[32] for 256 bit data. */
};

check_result svdb_runsql(
    svdb_db *self, const char *sql, int len, svdb_expectchanges confirm_changes)
{
    sv_result currenterr = {};
    sv_log_write(sql);
    sqlite3_stmt *stmt = NULL;
    check_b(self->db, "no db connection?");
    check_sql(self->db, sqlite3_prepare_v2(self->db, sql, len, &stmt, NULL));
    check_sql(self->db, sqlite3_step(stmt));
    check_sql(self->db, sqlite3_finalize(stmt));
    stmt = NULL;
    if (confirm_changes != expectchangesunknown)
    {
        /* note that sqlite3_changes is untouched for all but insert, delete,
         * update */
        check_b((confirm_changes == expectchanges) ==
                (sqlite3_changes(self->db) > 0),
            "running qid %s changed/didn't change rows", sql);
    }

cleanup:
    sqlite3_finalize(stmt);
    return currenterr;
}

check_result svdb_addschema(svdb_db *self)
{
    sv_result currenterr = {};
    svdb_txn txn = {};
    check(svdb_txn_open(&txn, self));
    for (int i = 0; i < countof32s(schema_cmds); i++)
    {
        check(svdb_runsql(self, schema_cmds[i], strlen32s(schema_cmds[i]),
            expectchangesunknown));
    }

    check(svdb_txn_commit(&txn, self));

cleanup:
    svdb_txn_close(&txn, self);
    return currenterr;
}

check_result svdb_getcounthelper(svdb_db *self, svdb_qid qid, uint64_t *ret)
{
    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(qid, self);
    int rc = 0;
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    check_b(rc == SQLITE_ROW, "expect row found, got %d", rc);
    svdb_qry_get_uint64(&qry, self, 1, ret);
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_filescount(svdb_db *self, uint64_t *val)
{
    self->qrystrings[svdb_qid_filescount] = "SELECT COUNT(*) FROM TblFilesList";
    return svdb_getcounthelper(self, svdb_qid_filescount, val);
}

check_result svdb_contentscount(svdb_db *self, uint64_t *val)
{
    self->qrystrings[svdb_qid_contentscount] =
        "SELECT COUNT(*) FROM TblContentsList";
    return svdb_getcounthelper(self, svdb_qid_contentscount, val);
}

check_result svdb_propgetcount(svdb_db *self, uint64_t *val)
{
    self->qrystrings[svdb_qid_propcount] = "SELECT COUNT(*) FROM TblProperties";
    return svdb_getcounthelper(self, svdb_qid_propcount, val);
}

check_result svdb_getint(
    svdb_db *self, const char *propname, int lenpropname, uint32_t *val)
{
    self->qrystrings[svdb_qid_propget] =
        "SELECT PropertyVal FROM TblProperties WHERE PropertyName=?";

    sv_result currenterr = {};
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_propget, self);
    check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    *val = 0; /* if row is not found, return 0. */
    if (rc == SQLITE_ROW)
    {
        svdb_qry_get_uint(&qry, self, 1, val);
    }

    check(svdb_qry_disconnect(&qry, self));
cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_getstr(
    svdb_db *self, const char *propname, int lenpropname, bstring val)
{
    sv_result currenterr = {};
    bstrclear(val); /* if row is not found, return "" */
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_propget, self);
    check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    if (rc == SQLITE_ROW)
    {
        svdb_qry_get_str(&qry, self, 1, val);
    }

    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

/* use "record separator" character (ascii 30) as delim */
#define list_delim "\x1e"
check_result svdb_getlist(
    svdb_db *self, const char *propname, int lenpropname, bstrlist *val)
{
    sv_result currenterr = {};
    bstring s = bstring_open();
    check(svdb_getstr(self, propname, lenpropname, s));
    if (blength(s))
    {
        bstrlist_split(val, s, list_delim[0]);
    }
    else
    {
        bstrlist_clear(val);
    }

cleanup:
    bdestroy(s);
    return currenterr;
}

check_result svdb_setstr(
    svdb_db *self, const char *propname, int lenpropname, const char *val)
{
    self->qrystrings[svdb_qid_propset] =
        "INSERT OR REPLACE INTO TblProperties "
        "(PropertyName, PropertyVal) VALUES (?, ?)";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_propset, self);
    check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
    check(svdb_qry_bindstr(&qry, self, 2, val, -1, true));
    check(svdb_qry_run(&qry, self, expectchangesunknown, NULL));
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_setint(
    svdb_db *self, const char *propname, int lenpropname, uint32_t val)
{
    self->qrystrings[svdb_qid_propset] =
        "INSERT OR REPLACE INTO TblProperties "
        "(PropertyName, PropertyVal) VALUES (?, ?)";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_propset, self);
    check(svdb_qry_bindstr(&qry, self, 1, propname, lenpropname, true));
    check(svdb_qry_bind_uint(&qry, self, 2, val));
    check(svdb_qry_run(&qry, self, expectchangesunknown, NULL));
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_setlist(
    svdb_db *self, const char *propname, int lenpropname, const bstrlist *val)
{
    sv_result currenterr = {};
    bstring joined = bjoin_static(val, list_delim);
    for (int i = 0; i < val->qty; i++)
    {
        check_b(!s_contains(blist_view(val, i), list_delim),
            "cannot include this character (ascii 30)");
    }

    check(svdb_setstr(self, propname, lenpropname, cstr(joined)));

cleanup:
    bdestroy(joined);
    return currenterr;
}

check_result svdb_archives_write_checksum(svdb_db *self, uint64_t archiveid,
    uint64_t timemodified, uint64_t compaction_cutoff, const char *checksum,
    const char *filepath)
{
    sv_result currenterr = {};
    self->qrystrings[svdb_qid_archiveswritechecksum] =
        "INSERT INTO TblArchives(ArchiveId, ModifiedTime, "
        "CompactionRemovedDataBeforeThisCollection, ChecksumString) "
        "VALUES (?, ?, ?, ?)";

    /* prepend the filename to the checksum */
    bstring s = bstring_open();
    os_get_filename(filepath, s);
    bstr_catstatic(s, "=");
    bcatcstr(s, checksum);

    svdb_qry qry = svdb_qry_open(svdb_qid_archiveswritechecksum, self);
    check(svdb_qry_bind_uint64(&qry, self, 1, archiveid));
    check(svdb_qry_bind_uint64(&qry, self, 2, timemodified));
    check(svdb_qry_bind_uint64(&qry, self, 3, compaction_cutoff));
    check(svdb_qry_bindstr(&qry, self, 4, cstr(s), blength(s), false));
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    bdestroy(s);
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_archives_get_checksums(
    svdb_db *self, bstrlist *filenames, bstrlist *checksums)
{
    self->qrystrings[svdb_qid_archivesgetchecksums] =
        "SELECT ChecksumString FROM TblArchives ORDER BY ChecksumString";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_archivesgetchecksums, self);
    bstrlist_clear(filenames);
    bstrlist_clear(checksums);
    bstring s = bstring_open();
    bstrlist *temp = bstrlist_open();
    int rc = 0;
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    while (rc == SQLITE_ROW)
    {
        svdb_qry_get_str(&qry, self, 1, s);
        bstrlist_splitcstr(temp, cstr(s), '=');
        check_b(
            temp->qty == 2, "Expected one '=' in string but got %s", cstr(s));

        bstrlist_append(filenames, temp->entry[0]);
        bstrlist_append(checksums, temp->entry[1]);
        check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    }

    check(svdb_qry_disconnect(&qry, self));
cleanup:
    bdestroy(s);
    bstrlist_close(temp);
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_filesbypath(
    svdb_db *self, const bstring path, sv_file_row *out)
{
    self->qrystrings[svdb_qid_filesbypath] =
        "SELECT FilesListId, ContentLength, ContentsId, LastWriteTime, Status "
        "FROM TblFilesList WHERE Path=? LIMIT 1";

    sv_result currenterr = {};
    memset(out, 0, sizeof(*out));
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_filesbypath, self);
    check(svdb_qry_bindstr(&qry, self, 1, cstr(path), blength(path), false));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    if (rc == SQLITE_ROW)
    {
        uint64_t status = 0;
        svdb_qry_get_uint64(&qry, self, 1, &out->id);
        svdb_qry_get_uint64(&qry, self, 2, &out->contents_length);
        svdb_qry_get_uint64(&qry, self, 3, &out->contents_id);
        svdb_qry_get_uint64(&qry, self, 4, &out->last_write_time);
        svdb_qry_get_uint64(&qry, self, 5, &status);
        out->e_status = sv_getstatus(status);
        out->most_recent_collection = sv_collectionidfromstatus(status);
    }

    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_collectionsget(svdb_db *self, sv_array *rows, bool get_all)
{
    self->qrystrings[svdb_qid_collectionget] =
        "SELECT CollectionId, Time, TimeCompleted, CountTotalFiles, "
        "CountNewContents, CountNewContentsBytes FROM TblCollections "
        "ORDER BY CollectionId DESC";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_collectionget, self);
    int rc = 0;
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    check_b(rows->elementsize == sizeof32u(sv_collection_row), "");
    while (rc == SQLITE_ROW)
    {
        sv_collection_row row = {0};
        svdb_qry_get_uint64(&qry, self, 1, &row.id);
        svdb_qry_get_uint64(&qry, self, 2, &row.time);
        svdb_qry_get_uint64(&qry, self, 3, &row.time_finished);
        svdb_qry_get_uint64(&qry, self, 4, &row.count_total_files);
        svdb_qry_get_uint64(&qry, self, 5, &row.count_new_contents);
        svdb_qry_get_uint64(&qry, self, 6, &row.count_new_contents_bytes);
        sv_array_append(rows, &row, 1);
        if (!get_all)
        {
            break;
        }

        check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    }

    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_collectiongetlast(svdb_db *self, uint64_t *id)
{
    sv_result currenterr = {};
    sv_array rows = sv_array_open(sizeof32u(sv_collection_row), 0);
    check(svdb_collectionsget(self, &rows, false));
    sv_log_write(rows.length ? "svdb_collectiongetlast row"
                             : "svdb_collectiongetlast no row");
    *id = rows.length ? ((sv_collection_row *)sv_array_at(&rows, 0))->id : 0;

cleanup:
    sv_array_close(&rows);
    return currenterr;
}

check_result svdb_contentsbyhash(svdb_db *self, const hash256 *hash,
    uint64_t contentslength, sv_content_row *row)
{
    self->qrystrings[svdb_qid_contentsbyhash] =
        "SELECT ContentsId, LastCollectionId, CompressedContentLength, "
        "Crc32, ArchiveId FROM TblContentsList WHERE ContentsHash1=? "
        "AND ContentsHash2=? AND ContentsHash3=? AND ContentsHash4=? "
        "AND ContentLength=? LIMIT 1";

    sv_result currenterr = {};
    int rc = 0;
    memset(row, 0, sizeof(*row));
    svdb_qry qry = svdb_qry_open(svdb_qid_contentsbyhash, self);
    check(svdb_qry_bind_uint64(&qry, self, 1, hash->data[0]));
    check(svdb_qry_bind_uint64(&qry, self, 2, hash->data[1]));
    check(svdb_qry_bind_uint64(&qry, self, 3, hash->data[2]));
    check(svdb_qry_bind_uint64(&qry, self, 4, hash->data[3]));
    check(svdb_qry_bind_uint64(&qry, self, 5, contentslength));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    if (rc == SQLITE_ROW)
    {
        uint64_t archiveid = 0;
        svdb_qry_get_uint64(&qry, self, 1, &row->id);
        svdb_qry_get_uint64(&qry, self, 2, &row->most_recent_collection);
        svdb_qry_get_uint64(&qry, self, 3, &row->compressed_contents_length);
        svdb_qry_get_uint(&qry, self, 4, &row->crc32);
        svdb_qry_get_uint64(&qry, self, 5, &archiveid);
        row->original_collection = upper32(archiveid);
        row->archivenumber = lower32(archiveid);
        row->hash = *hash;
        row->contents_length = contentslength;
        check_b(row->id != 0, "invalid contentsid");
        check_b(row->archivenumber != 0 && row->original_collection != 0,
            "invalid val %llu %llu", castull(row->archivenumber),
            castull(row->original_collection));
    }

    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_contentsbyid(
    svdb_db *self, uint64_t contentsid, sv_content_row *row)
{
    self->qrystrings[svdb_qid_contentsbyid] =
        "SELECT ContentsHash1, ContentsHash2, ContentsHash3, ContentsHash4, "
        "ContentLength, LastCollectionId, CompressedContentLength, Crc32, "
        "ArchiveId FROM TblContentsList WHERE ContentsId=? LIMIT 1";

    sv_result currenterr = {};
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_contentsbyid, self);
    check_b(contentsid, "cannot get row 0");
    memset(row, 0, sizeof(*row));
    check(svdb_qry_bind_uint64(&qry, self, 1, contentsid));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    if (rc == SQLITE_ROW)
    {
        uint64_t archiveid = 0;
        svdb_qry_get_uint64(&qry, self, 1, &row->hash.data[0]);
        svdb_qry_get_uint64(&qry, self, 2, &row->hash.data[1]);
        svdb_qry_get_uint64(&qry, self, 3, &row->hash.data[2]);
        svdb_qry_get_uint64(&qry, self, 4, &row->hash.data[3]);
        svdb_qry_get_uint64(&qry, self, 5, &row->contents_length);
        svdb_qry_get_uint64(&qry, self, 6, &row->most_recent_collection);
        svdb_qry_get_uint64(&qry, self, 7, &row->compressed_contents_length);
        svdb_qry_get_uint(&qry, self, 8, &row->crc32);
        svdb_qry_get_uint64(&qry, self, 9, &archiveid);
        row->original_collection = upper32(archiveid);
        row->archivenumber = lower32(archiveid);
        row->id = contentsid;
        check_b(row->archivenumber != 0 && row->original_collection != 0,
            "invalid val %llu %llu", castull(row->archivenumber),
            castull(row->original_collection));
    }

    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_contentsinsert(svdb_db *self, uint64_t *outid)
{
    self->qrystrings[svdb_qid_contentsinsert] =
        "INSERT INTO TblContentsList (ContentsHash1, ContentsHash2, "
        "ContentsHash3, ContentsHash4, ContentLength, "
        "CompressedContentLength, Crc32, ArchiveId, LastCollectionId) "
        "VALUES (0, 0, 0, 0, 0, 0, 0, 0, 0)";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_contentsinsert, self);
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
    *outid = cast64s64u(sqlite3_last_insert_rowid(self->db));
    check_b(*outid != 0, "id should not be 0.");
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_contentsupdate(svdb_db *db, const sv_content_row *row)
{
    db->qrystrings[svdb_qid_contentsupdate] =
        "UPDATE TblContentsList SET ContentsHash1=?, ContentsHash2=?, "
        "ContentsHash3=?, ContentsHash4=?, ContentLength=?, "
        "CompressedContentLength=?, Crc32=?, ArchiveId=?, LastCollectionId=? "
        "WHERE ContentsId = ?";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_contentsupdate, db);
    check_b(row->id != 0, "rowid should be set.");
    check(svdb_qry_bind_uint64(&qry, db, 1, row->hash.data[0]));
    check(svdb_qry_bind_uint64(&qry, db, 2, row->hash.data[1]));
    check(svdb_qry_bind_uint64(&qry, db, 3, row->hash.data[2]));
    check(svdb_qry_bind_uint64(&qry, db, 4, row->hash.data[3]));
    check(svdb_qry_bind_uint64(&qry, db, 5, row->contents_length));
    check(svdb_qry_bind_uint64(&qry, db, 6, row->compressed_contents_length));
    check(svdb_qry_bind_uint(&qry, db, 7, row->crc32));
    check(svdb_qry_bind_uint64(&qry, db, 8,
        make_u64(cast64u32u(row->original_collection),
            cast64u32u(row->archivenumber))));

    check(svdb_qry_bind_uint64(&qry, db, 9, row->most_recent_collection));
    check(svdb_qry_bind_uint64(&qry, db, 10, row->id));
    check(svdb_qry_run(&qry, db, expectchanges, NULL));
    check(svdb_qry_disconnect(&qry, db));

cleanup:
    svdb_qry_close(&qry, db);
    return currenterr;
}

check_result svdb_bulk_delete_helper(svdb_db *self, const sv_array *arr,
    const char *qry_start, const char *colname, int batchsize)
{
    sv_result currenterr = {};
    const int defaultbatchsize = 200;
    bstring query = bfromcstr(qry_start);
    if (arr && arr->length)
    {
        batchsize = (batchsize == 0) ? defaultbatchsize : batchsize;
        int added_in_batch = 0;
        for (uint32_t i = 0; i < arr->length; i++)
        {
            unsigned long long id = castull(sv_array_at64u(arr, i));
            int n = bformata(query, "%s=%llu OR ", colname, id);
            check_b(n == BSTR_OK, "concat failed %s", cstr(query));
            added_in_batch++;
            if (added_in_batch >= batchsize || i >= arr->length - 1)
            {
                bcatblk(query, s_and_len("0"));
                check(svdb_runsql(
                    self, cstr(query), blength(query), expectchangesunknown));
                bassigncstr(query, qry_start);
                added_in_batch = 0;
            }
        }
    }

cleanup:
    bdestroy(query);
    return currenterr;
}

check_result svdb_contents_bulk_delete(
    svdb_db *self, const sv_array *arr, int batchsize)
{
    return svdb_bulk_delete_helper(self, arr,
        "DELETE FROM TblContentsList WHERE ", "ContentsId", batchsize);
}

check_result svdb_contentsiter(
    svdb_db *self, void *context, fn_iterate_contents callback)
{
    self->qrystrings[svdb_qid_contentsiter] =
        "SELECT ContentsHash1, ContentsHash2, ContentsHash3, ContentsHash4, "
        "ContentLength, ContentsId, LastCollectionId, "
        "CompressedContentLength, Crc32, ArchiveId FROM TblContentsList";

    sv_result currenterr = {};
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_contentsiter, self);
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    while (rc == SQLITE_ROW)
    {
        sv_content_row row = {0};
        uint64_t archiveid = 0;
        svdb_qry_get_uint64(&qry, self, 1, &row.hash.data[0]);
        svdb_qry_get_uint64(&qry, self, 2, &row.hash.data[1]);
        svdb_qry_get_uint64(&qry, self, 3, &row.hash.data[2]);
        svdb_qry_get_uint64(&qry, self, 4, &row.hash.data[3]);
        svdb_qry_get_uint64(&qry, self, 5, &row.contents_length);
        svdb_qry_get_uint64(&qry, self, 6, &row.id);
        svdb_qry_get_uint64(&qry, self, 7, &row.most_recent_collection);
        svdb_qry_get_uint64(&qry, self, 8, &row.compressed_contents_length);
        svdb_qry_get_uint(&qry, self, 9, &row.crc32);
        svdb_qry_get_uint64(&qry, self, 10, &archiveid);
        row.original_collection = upper32(archiveid);
        row.archivenumber = lower32(archiveid);
        if (row.id)
        {
            check(callback(context, &row));
        }

        check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    }

    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_filesinsert(svdb_db *self, const bstring path,
    uint64_t mostrecentcollection, sv_filerowstatus status, uint64_t *outid)
{
    self->qrystrings[svdb_qid_filesinsert] =
        "INSERT INTO TblFilesList (Path, ContentLength, ContentsId, "
        "LastWriteTime, Status, Flags) VALUES (?, 0, 0, 0, ?, 0)";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_filesinsert, self);
    check(svdb_qry_bindstr(&qry, self, 1, cstr(path), blength(path), false));
    check(svdb_qry_bind_uint64(
        &qry, self, 2, sv_makestatus(mostrecentcollection, status)));
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
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

check_result svdb_filesupdate(
    svdb_db *self, const sv_file_row *row, const bstring permissions)
{
    self->qrystrings[svdb_qid_filesupdate] =
        "UPDATE TblFilesList SET ContentLength=?, ContentsId=?, "
        "LastWriteTime=?, Status=?, Flags=? WHERE FilesListId=?";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_filesupdate, self);
    check_b(row->id, "cannot set rowid 0");
    check(svdb_qry_bind_uint64(&qry, self, 1, row->contents_length));
    check(svdb_qry_bind_uint64(&qry, self, 2, row->contents_id));
    check(svdb_qry_bind_uint64(&qry, self, 3, row->last_write_time));
    check(svdb_qry_bind_uint64(&qry, self, 4,
        sv_makestatus(row->most_recent_collection, row->e_status)));

    if (permissions && blength(permissions))
    {
        check(svdb_qry_bindstr(
            &qry, self, 5, cstr(permissions), blength(permissions), false));
    }
    else
    {
        check(svdb_qry_bindstr(&qry, self, 5, "", 0, true));
    }

    check(svdb_qry_bind_uint64(&qry, self, 6, row->id));
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_files_iter(
    svdb_db *self, uint64_t status, void *context, fn_iterate_rows callback)
{
    self->qrystrings[svdb_qid_fileslessthan] =
        "SELECT FilesListId, Path, ContentLength, ContentsId, LastWriteTime, "
        "Flags, Status FROM TblFilesList WHERE Status < ?";

    sv_result currenterr = {};
    int rc = 0;
    bstring path = bstring_open();
    bstring permissions = bstring_open();
    svdb_qry qry = svdb_qry_open(svdb_qid_fileslessthan, self);
    check(svdb_qry_bind_uint64(&qry, self, 1, status));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    while (rc == SQLITE_ROW)
    {
        sv_file_row row = {0};
        bstrclear(path);
        svdb_qry_get_uint64(&qry, self, 1, &row.id);
        svdb_qry_get_str(&qry, self, 2, path);
        svdb_qry_get_uint64(&qry, self, 3, &row.contents_length);
        svdb_qry_get_uint64(&qry, self, 4, &row.contents_id);
        svdb_qry_get_uint64(&qry, self, 5, &row.last_write_time);
        svdb_qry_get_str(&qry, self, 6, permissions);
        uint64_t statusgot = 0;
        svdb_qry_get_uint64(&qry, self, 7, &statusgot);
        row.e_status = sv_getstatus(statusgot);
        row.most_recent_collection = sv_collectionidfromstatus(statusgot);
        if (row.id)
        {
            check(callback(context, &row, path, permissions));
        }

        check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    }

    check(svdb_qry_disconnect(&qry, self));
cleanup:
    bdestroy(path);
    bdestroy(permissions);
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_collectioninsert(
    svdb_db *self, uint64_t timestarted, uint64_t *rowid)
{
    self->qrystrings[svdb_qid_collectioninsert] =
        "INSERT INTO TblCollections (Time) "
        "VALUES (?)";

    sv_result currenterr = {};
    *rowid = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_collectioninsert, self);
    check(svdb_qry_bind_uint64(&qry, self, 1, timestarted));
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
    *rowid = cast64s64u(sqlite3_last_insert_rowid(self->db));
    check_b(*rowid != 0, "collectionid should not be 0.");
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_collectionupdate(svdb_db *self, const sv_collection_row *row)
{
    self->qrystrings[svdb_qid_collectionupdate] =
        "UPDATE TblCollections SET TimeCompleted=?, CountTotalFiles=?, "
        "CountNewContents=?, CountNewContentsBytes=? WHERE CollectionId=?";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_collectionupdate, self);
    check_b(row->id, "cannot set rowid 0");
    check_b(!row->time, "cannot set time started");
    check(svdb_qry_bind_uint64(&qry, self, 1, row->time_finished));
    check(svdb_qry_bind_uint64(&qry, self, 2, row->count_total_files));
    check(svdb_qry_bind_uint64(&qry, self, 3, row->count_new_contents));
    check(svdb_qry_bind_uint64(&qry, self, 4, row->count_new_contents_bytes));
    check(svdb_qry_bind_uint64(&qry, self, 5, row->id));
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_contents_setlastreferenced(
    svdb_db *self, uint64_t contentsid, uint64_t collectionid)
{
    self->qrystrings[svdb_qid_contents_setlastreferenced] =
        "UPDATE TblContentsList SET LastCollectionId=? WHERE ContentsId=?";

    sv_result currenterr = {};
    svdb_qry qry = svdb_qry_open(svdb_qid_contents_setlastreferenced, self);
    check_b(contentsid != 0, "contentsid should not be 0.");
    check_b(collectionid != 0, "collectionid should not be 0.");
    check(svdb_qry_bind_uint64(&qry, self, 1, collectionid));
    check(svdb_qry_bind_uint64(&qry, self, 2, contentsid));
    check(svdb_qry_run(&qry, self, expectchanges, NULL));
    check(svdb_qry_disconnect(&qry, self));

cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_files_delete(svdb_db *db, const sv_array *arr, int batchsize)
{
    return svdb_bulk_delete_helper(
        db, arr, "DELETE FROM TblFilesList WHERE ", "FilesListId", batchsize);
}

check_result svdb_confirmschemaversion(svdb_db *self, const char *path)
{
    uint32_t version = 0;
    sv_result currenterr = {};
    sv_result err_get_version = svdb_runsql(self,
        s_and_len("SELECT PropertyVal FROM TblProperties WHERE "
                  "PropertyName='SchemaVersion'"),
        expectchangesunknown);

    if (err_get_version.code)
    {
        sv_log_write(
            "could not prepare query... let's try adding schema "
            "in case this is an empty database left from a previous crash.");
        sv_result_close(&err_get_version);
        sv_result err_add_schema = svdb_addschema(self);
        check(err_add_schema);
    }

    check(svdb_getint(self, s_and_len("SchemaVersion"), &version));
    check_b(version == 1,
        "database %s could not be loaded, it might be "
        "from a future version. %d.",
        path, version);

cleanup:
    return currenterr;
}

check_result svdb_connection_openhandle(svdb_db *self)
{
    sv_result currenterr = {};
    check_b(!self->db, "db already open?");
    sv_log_writes("Opening db handle", cstr(self->path));

    /* open db in single-threaded mode. */
    check_sql(NULL,
        sqlite3_open_v2(cstr(self->path), &self->db,
            SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL));

    /* set sqlite options. */
    /* benchmarks showed no improvement under WAL mode */
    check(svdb_runsql(
        self, s_and_len("PRAGMA temp_store = memory"), expectchangesunknown));
    check(svdb_runsql(
        self, s_and_len("PRAGMA page_size = 16384"), expectchangesunknown));
    check(svdb_runsql(
        self, s_and_len("PRAGMA cache_size = 1000"), expectchangesunknown));

cleanup:
    return currenterr;
}

check_result svdb_connect(svdb_db *self, const char *path)
{
    sv_result currenterr = {};
    set_self_zero();
    sv_log_writes("svdb_connect", path);
    self->path = bfromcstr(path);
    confirm_writable(path);
    bool is_new_db = !os_file_exists(path);
    check_b(os_isabspath(path), "full path required %s", path);
    check(svdb_connection_openhandle(self));
    if (is_new_db)
    {
        check(svdb_addschema(self));
    }

    check(svdb_confirmschemaversion(self, path));

cleanup:
    return currenterr;
}

check_result svdb_disconnect(svdb_db *self)
{
    sv_result currenterr = {};
    if (self)
    {
        /* free cached queries */
        for (int i = 0; i < svdb_qid_max; i++)
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

void svdb_close(svdb_db *self)
{
    if (self)
    {
        check_warn(svdb_disconnect(self),
            "Encountered warning when closing database.", continue_on_err);

        bdestroy(self->path);
        set_self_zero();
    }
}

/* transactions */
check_result svdb_txn_open(svdb_txn *self, svdb_db *db)
{
    sv_result currenterr = {};
    set_self_zero();
    check(svdb_runsql(db, s_and_len("BEGIN TRANSACTION"), expectchangesunknown));
    self->active = true;

cleanup:
    return currenterr;
}

check_result svdb_txn_commit(svdb_txn *self, svdb_db *db)
{
    sv_result currenterr = {};
    check_b(self->active, "tried to commit an inactive txn");
    check(
        svdb_runsql(db, s_and_len("COMMIT TRANSACTION"), expectchangesunknown));
    self->active = false;

cleanup:
    return currenterr;
}

check_result svdb_txn_rollback(svdb_txn *self, svdb_db *db)
{
    sv_result currenterr = {};
    if (self->active)
    {
        check(svdb_runsql(
            db, s_and_len("ROLLBACK TRANSACTION"), expectchangesunknown));
    }

    self->active = false;
    set_self_zero();

cleanup:
    return currenterr;
}

void svdb_txn_close(svdb_txn *self, svdb_db *db)
{
    if (self)
    {
        if (db && db->db)
        {
            check_warn(svdb_txn_rollback(self, db),
                "Encountered warning when closing transaction.",
                continue_on_err);
        }

        set_self_zero();
    }
}

check_result svdb_clear_database_content(svdb_db *self)
{
    sv_result currenterr = {};
    check(svdb_runsql(
        self, s_and_len("DELETE FROM TblCollections"), expectchangesunknown));
    check(svdb_runsql(
        self, s_and_len("DELETE FROM TblFilesList"), expectchangesunknown));
    check(svdb_runsql(
        self, s_and_len("DELETE FROM TblContentsList"), expectchangesunknown));
    check(svdb_runsql(
        self, s_and_len("DELETE FROM TblArchives"), expectchangesunknown));
    check(svdb_runsql(
        self, s_and_len("DELETE FROM TblKnownVaults"), expectchangesunknown));
    check(svdb_runsql(self, s_and_len("DELETE FROM TblKnownVaultArchives"),
        expectchangesunknown));
    check(svdb_runsql(self,
        s_and_len("DELETE FROM sqlite_sequence WHERE name='TblCollections'"),
        expectchangesunknown));
    check(svdb_runsql(self,
        s_and_len("DELETE FROM sqlite_sequence WHERE name='TblFilesList'"),
        expectchangesunknown));
    check(svdb_runsql(self,
        s_and_len("DELETE FROM sqlite_sequence WHERE name='TblContentsList'"),
        expectchangesunknown));
    check(svdb_runsql(self,
        s_and_len("DELETE FROM sqlite_sequence WHERE name='TblArchives'"),
        expectchangesunknown));
    check(svdb_runsql(self,
        s_and_len("DELETE FROM sqlite_sequence WHERE name='TblKnownVaults'"),
        expectchangesunknown));
    check(svdb_runsql(self,
        s_and_len(
            "DELETE FROM sqlite_sequence WHERE name='TblKnownVaultArchives'"),
        expectchangesunknown));
cleanup:
    return currenterr;
}

check_result svdb_collectioninsert_helper(
    svdb_db *db, uint64_t started, uint64_t finished)
{
    sv_result currenterr = {};
    sv_collection_row row = {};
    row.time_finished = finished;
    check(svdb_collectioninsert(db, started, &row.id));
    check(svdb_collectionupdate(db, &row));

cleanup:
    return currenterr;
}

void svdb_files_row_string(
    const sv_file_row *row, const char *path, const char *permissions, bstring s)
{
    bsetfmt(s,
        "contents_length=%llu, contents_id=%llu, last_write_time=%llu, "
        "flags=%s, most_recent_collection=%llu, e_status=%d, id=%llu%s",
        castull(row->contents_length), castull(row->contents_id),
        castull(row->last_write_time), permissions,
        castull(row->most_recent_collection), row->e_status, castull(row->id),
        path);
}

void svdb_collectiontostring(
    const sv_collection_row *row, bool verbose, bool everyfield, bstring s)
{
    if (everyfield)
    {
        bsetfmt(s,
            "time=%llu, time_finished=%llu, count_total_files=%llu, "
            "count_new_contents=%llu, count_new_contents_bytes=%llu, id=%llu",
            castull(row->time), castull(row->time_finished),
            castull(row->count_total_files), castull(row->count_new_contents),
            castull(row->count_new_contents_bytes), castull(row->id));
    }
    else
    {
        bsetfmt(s, "Backups from ");
        struct tm *ptime = localtime((time_t *)&row->time_finished);
        if (ptime)
        {
            bformata(s, "%04d/%02d/%02d   %02d:%02d\n", ptime->tm_year + 1900,
                ptime->tm_mon + 1, ptime->tm_mday, ptime->tm_hour,
                ptime->tm_min);
        }

        if (verbose)
        {
            bformata(s, "\tTotal files: %llu. Added %llu files (%.3fMb).",
                castull(row->count_total_files),
                castull(row->count_new_contents),
                (double)row->count_new_contents_bytes / (1024.0 * 1024.0));
        }
    }
}

void svdb_contents_row_string(const sv_content_row *row, bstring s)
{
    bstring shash = bstring_open();
    hash256tostr(&row->hash, shash);
    bassigncstr(s, "hash=");
    bconcat(s, shash);
    bdestroy(shash);
    bformata(s,
        ", crc32=%x, contents_length=%llu,%llu, "
        "most_recent_collection=%llu, original_collection=%u, "
        "archivenumber=%u, id=%llu",
        row->crc32, castull(row->contents_length),
        castull(row->compressed_contents_length),
        castull(row->most_recent_collection), row->original_collection,
        row->archivenumber, castull(row->id));
}

check_result svdb_knownvaults_get(svdb_db *self, bstrlist *regions,
    bstrlist *names, bstrlist *awsnames, bstrlist *arns, sv_array *ids)
{
    self->qrystrings[svdb_qid_vault_get] =
        "SELECT KnownVaultsId, AwsRegion, Name, AwsVaultName, AwsVaultARN "
        "FROM TblKnownVaults WHERE 1";

    sv_result currenterr = {};
    int rc = 0;
    bstring tmp = bstring_open();
    svdb_qry qry = svdb_qry_open(svdb_qid_vault_get, self);
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    while (rc == SQLITE_ROW)
    {
        uint64_t id = 0;
        svdb_qry_get_uint64(&qry, self, 1, &id);
        sv_array_add64u(ids, id);
        svdb_qry_get_str(&qry, self, 2, tmp);
        bstrlist_append(regions, tmp);
        svdb_qry_get_str(&qry, self, 3, tmp);
        bstrlist_append(names, tmp);
        svdb_qry_get_str(&qry, self, 4, tmp);
        bstrlist_append(awsnames, tmp);
        svdb_qry_get_str(&qry, self, 5, tmp);
        bstrlist_append(arns, tmp);
        check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    }

    check(svdb_qry_disconnect(&qry, self));
cleanup:
    bdestroy(tmp);
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_knownvaults_insert(svdb_db *self, const char *region,
    const char *name, const char *awsname, const char *arn)
{
    self->qrystrings[svdb_qid_vault_insert] =
        "INSERT INTO TblKnownVaults(Name, AwsRegion, AwsVaultName, AwsVaultARN) "
        "VALUES (?, ?, ?, ?)";

    sv_result currenterr = {};
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_vault_insert, self);
    check(svdb_qry_bindstr(&qry, self, 1, name, strlen32s(name), false));
    check(svdb_qry_bindstr(&qry, self, 2, region, strlen32s(region), false));
    check(svdb_qry_bindstr(&qry, self, 3, awsname, strlen32s(awsname), false));
    check(svdb_qry_bindstr(&qry, self, 4, arn, strlen32s(arn), false));
    check(svdb_qry_run(&qry, self, expectchanges, &rc));
    check(svdb_qry_disconnect(&qry, self));
cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_vaultarchives_bypath(svdb_db *self, const char *path,
    uint64_t knownvaultid, uint64_t *outsize, uint64_t *outcrc32,
    uint64_t *outmodtime)
{
    self->qrystrings[svdb_qid_vaultarchives_bypath] =
        "SELECT Size, Crc32, ModifiedTime FROM TblKnownVaultArchives WHERE "
        "CloudPath=? AND KnownVaultsId=?";

    sv_result currenterr = {};
    *outsize = 0;
    *outcrc32 = 0;
    *outmodtime = 0;
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_vaultarchives_bypath, self);
    check(svdb_qry_bindstr(&qry, self, 1, path, strlen32s(path), false));
    check(svdb_qry_bind_uint64(&qry, self, 2, knownvaultid));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    if (rc == SQLITE_ROW)
    {
        svdb_qry_get_uint64(&qry, self, 1, outsize);
        svdb_qry_get_uint64(&qry, self, 2, outcrc32);
        svdb_qry_get_uint64(&qry, self, 3, outmodtime);
    }

    check(svdb_qry_disconnect(&qry, self));
cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_vaultarchives_delbypath(
    svdb_db *self, const char *path, uint64_t knownvaultid)
{
    self->qrystrings[svdb_qid_vaultarchives_delbypath] =
        "DELETE FROM TblKnownVaultArchives WHERE CloudPath=? AND "
        "KnownVaultsId=?";

    sv_result currenterr = {};
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_vaultarchives_delbypath, self);
    check(svdb_qry_bindstr(&qry, self, 1, path, strlen32s(path), false));
    check(svdb_qry_bind_uint64(&qry, self, 2, knownvaultid));
    check(svdb_qry_run(&qry, self, expectchangesunknown, &rc));
    check(svdb_qry_disconnect(&qry, self));
cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

check_result svdb_vaultarchives_insert(svdb_db *self, const char *path,
    const char *description, uint64_t knownvaultid, const char *awsid,
    uint64_t size, uint64_t crc32, uint64_t modtime)
{
    self->qrystrings[svdb_qid_vaultarchives_insert] =
        "INSERT INTO TblKnownVaultArchives (KnownVaultsId, AwsArchiveId, "
        "CloudPath, AwsDescription, Size, Crc32, ModifiedTime) VALUES (?, ?, ?, "
        "?, ?, ?, ?)";

    sv_result currenterr = {};
    int rc = 0;
    svdb_qry qry = svdb_qry_open(svdb_qid_vaultarchives_insert, self);
    check(svdb_qry_bind_uint64(&qry, self, 1, knownvaultid));
    check(svdb_qry_bindstr(&qry, self, 2, awsid, strlen32s(awsid), false));
    check(svdb_qry_bindstr(&qry, self, 3, path, strlen32s(path), false));
    check(svdb_qry_bindstr(
        &qry, self, 4, description, strlen32s(description), false));
    check(svdb_qry_bind_uint64(&qry, self, 5, size));
    check(svdb_qry_bind_uint64(&qry, self, 6, crc32));
    check(svdb_qry_bind_uint64(&qry, self, 7, modtime));
    check(svdb_qry_run(&qry, self, expectchanges, &rc));
    check(svdb_qry_disconnect(&qry, self));
cleanup:
    svdb_qry_close(&qry, self);
    return currenterr;
}

const uint64_t svdb_all_files = INT64_MAX;
extern inline sv_filerowstatus sv_getstatus(uint64_t status);
extern inline uint64_t sv_collectionidfromstatus(uint64_t status);
extern inline uint64_t sv_makestatus(uint64_t collectionid, sv_filerowstatus st);
