/*
dbaccess.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef DBACCESS_H_INCLUDE
#define DBACCESS_H_INCLUDE

#include "util_audio_tags.h"

struct sqlite3;
struct sqlite3_stmt;
typedef struct sqlite3 sqlite_handle;
typedef struct sqlite3_stmt sqlite_qry;

typedef enum svdb_qid {
    svdb_qid_none,
    svdb_qid_propget,
    svdb_qid_propset,
    svdb_qid_propcount,
    svdb_qid_filesbypath,
    svdb_qid_fileslessthan,
    svdb_qid_filesupdate,
    svdb_qid_filesinsert,
    svdb_qid_filescount,
    svdb_qid_archiveswritechecksum,
    svdb_qid_archivesgetchecksums,
    svdb_qid_collectionget,
    svdb_qid_collectioninsert,
    svdb_qid_collectionupdate,
    svdb_qid_contentsinsert,
    svdb_qid_contentsupdate,
    svdb_qid_contentsbyhash,
    svdb_qid_contentsbyid,
    svdb_qid_contentsiter,
    svdb_qid_contentscount,
    svdb_qid_contents_setlastreferenced,
    svdb_qid_max,
} svdb_qid;

typedef struct svdb_db {
    bstring path;
    sqlite_handle *db;
    sqlite_qry *qrys[svdb_qid_max];
    const char *qrystrings[svdb_qid_max];
} svdb_db;

typedef enum sv_filerowstatus
{
    sv_filerowstatus_queued,
    sv_filerowstatus_reserved1,
    sv_filerowstatus_reserved2,
    sv_filerowstatus_complete,
} sv_filerowstatus;

typedef struct sv_file_row {
    uint64_t id;
    uint64_t contents_length;
    uint64_t contents_id;
    uint64_t last_write_time;
    uint64_t most_recent_collection;
    sv_filerowstatus e_status;
} sv_file_row;

typedef struct sv_collection_row {
    uint64_t id;
    uint64_t time;
    uint64_t time_finished;
    uint64_t count_total_files;
    uint64_t count_new_contents;
    uint64_t count_new_contents_bytes;
} sv_collection_row;

typedef struct sv_content_row {
    uint64_t id;
    uint64_t contents_length;
    uint64_t compressed_contents_length;
    uint64_t most_recent_collection;
    uint32_t original_collection;
    uint32_t archivenumber;
    hash256 hash;
    uint32_t crc32;
} sv_content_row;

/* Combine status and last-seen-collection id into one int.
Makes querying for unprocessed files very simple:
select from FilesList where status < x */

inline uint64_t sv_makestatus(uint64_t collectionid, sv_filerowstatus st)
{
    return (collectionid << 2) + st;
}

inline sv_filerowstatus sv_getstatus(uint64_t status)
{
    return (sv_filerowstatus)(status & 0x3);
}

inline uint64_t sv_collectionidfromstatus(uint64_t status)
{
    return status >> 2;
}

typedef struct svdb_txn {
    bool active;
} svdb_txn;

typedef struct svdb_qry {
    svdb_qid qry_number;
} svdb_qry;

typedef sv_result(*fn_iterate_rows)(void *context,
    const sv_file_row *row,
    const bstring path,
    const bstring permissions);

typedef sv_result(*fn_iterate_contents)(void *context,
    const sv_content_row *sv_content_row);

check_result svdb_connect(svdb_db *self, const char *path);
check_result svdb_disconnect(svdb_db *self);
check_result svdb_clear_database_content(svdb_db *self);
check_result svdb_preparequery(svdb_db *self, svdb_qid qry_number);
void svdb_close(svdb_db *self);

check_result svdb_propgetcount(svdb_db *self, uint64_t *val);
check_result svdb_getint(svdb_db *self, const char *propname,
    int lenpropname, uint32_t *val);
check_result svdb_setint(svdb_db *self, const char *propname,
    int lenpropname, uint32_t val);
check_result svdb_getstr(svdb_db *self, const char *propname,
    int lenpropname, bstring s);
check_result svdb_setstr(svdb_db *self, const char *propname,
    int lenpropname, const char *val);
check_result svdb_getlist(svdb_db *self, const char *propname,
    int lenpropname, bstrlist *val);
check_result svdb_setlist(svdb_db *self, const char *propname,
    int lenpropname, const bstrlist *val);

check_result svdb_filescount(svdb_db *self, uint64_t *val);
extern const uint64_t svdb_all_files;
void svdb_files_row_string(const sv_file_row *row, const char *path,
    const char *permissions, bstring s);
check_result svdb_filesbypath(svdb_db *self, const bstring path,
    sv_file_row *out_row);
check_result svdb_filesinsert(svdb_db *self, const bstring path,
    uint64_t collectionid, sv_filerowstatus status, uint64_t *outid);
check_result svdb_filesupdate(svdb_db *self, const sv_file_row *row,
    const bstring permissions);
check_result svdb_files_iter(svdb_db *self, uint64_t status,
    void *context, fn_iterate_rows callback);
check_result svdb_files_delete(svdb_db *self, const sv_array *arr,
    int batchsize);

void svdb_collectiontostring(const sv_collection_row *row,
    bool verbose, bool every, bstring s);
check_result svdb_collectioninsert(svdb_db *self,
    uint64_t timestarted, uint64_t *rowid);
check_result svdb_collectiongetlast(svdb_db *self,
    uint64_t *id);
check_result svdb_collectionsget(svdb_db *self,
    sv_array *rows, bool get_all);
check_result svdb_collectionupdate(svdb_db *self,
    const sv_collection_row *row);
check_result svdb_collectioninsert_helper(svdb_db *db,
    uint64_t started, uint64_t finished);

void svdb_contents_row_string(const sv_content_row *row, bstring s);
check_result svdb_contentsinsert(svdb_db *self,
    uint64_t *outid);
check_result svdb_contentsupdate(svdb_db *self,
    const sv_content_row *row);
check_result svdb_contents_bulk_delete(svdb_db *self,
    const sv_array *arr, int batchsize);
check_result svdb_contentsiter(svdb_db *self,
    void *context, fn_iterate_contents callback);
check_result svdb_contentscount(svdb_db *self,
    uint64_t *val);
check_result svdb_contents_setlastreferenced(svdb_db *self,
    uint64_t contentsid, uint64_t collectionid);
check_result svdb_contentsbyid(svdb_db *self,
    uint64_t contentsid, sv_content_row *row);
check_result svdb_contentsbyhash(svdb_db *self,
    const hash256 *hash, uint64_t contentslength, sv_content_row *row);

check_result svdb_archives_get_checksums(svdb_db *self,
    bstrlist *filenames, bstrlist *checksums);
check_result svdb_archives_write_checksum(svdb_db *self,
    uint64_t archiveid, uint64_t timemodified, uint64_t compaction_cutoff,
    const char *checksum, const char *filepath);

check_result svdb_txn_open(svdb_txn *self, svdb_db *db);
check_result svdb_txn_commit(svdb_txn *self, svdb_db *db);
check_result svdb_txn_rollback(svdb_txn *self, svdb_db *db);
void svdb_txn_close(svdb_txn *self, svdb_db *db);

#endif
