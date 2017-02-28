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
#ifndef DBACCESS_H_INCLUDE
#define DBACCESS_H_INCLUDE

#include "util_audio_tags.h"

struct sqlite3;
struct sqlite3_stmt;
typedef struct sqlite3 sqlite_handle;
typedef struct sqlite3_stmt sqlite_qry;

typedef enum svdb_query {
	svdb_nqry_none,
	svdb_nqry_propertyget,
	svdb_nqry_propertyset,
	svdb_nqry_propertycount,
	svdb_nqry_filesbypath,
	svdb_nqry_fileslessthan,
	svdb_nqry_filesupdate,
	svdb_nqry_filesinsert,
	svdb_nqry_filescount,
	svdb_nqry_archiveswritechecksum,
	svdb_nqry_archivesgetchecksums,
	svdb_nqry_collectionget,
	svdb_nqry_collectioninsert,
	svdb_nqry_collectionupdate,
	svdb_nqry_contentsinsert,
	svdb_nqry_contentsupdate,
	svdb_nqry_contentsbyhash,
	svdb_nqry_contentsbyid,
	svdb_nqry_contentsiter,
	svdb_nqry_contentscount,
	svdb_nqry_contents_setlastreferenced,
	svdb_nqry_max,
} svdb_query;

typedef struct svdb_connection {
	bstring path;
	sqlite_handle *db;
	sqlite_qry *qrys[svdb_nqry_max];
	const char *qrystrings[svdb_nqry_max];
} svdb_connection;

typedef enum sv_filerowstatus
{
	sv_filerowstatus_queued,
	sv_filerowstatus_reserved1,
	sv_filerowstatus_reserved2,
	sv_filerowstatus_complete,
} sv_filerowstatus;

typedef struct svdb_files_row {
	uint64_t id;
	uint64_t contents_length;
	uint64_t contents_id;
	uint64_t last_write_time;
	uint64_t most_recent_collection;
	sv_filerowstatus e_status;
} svdb_files_row;

typedef struct svdb_collections_row {
	uint64_t id;
	uint64_t time;
	uint64_t time_finished;
	uint64_t count_total_files;
	uint64_t count_new_contents;
	uint64_t count_new_contents_bytes;
} svdb_collections_row;

typedef struct svdb_contents_row {
	uint64_t id;
	uint64_t contents_length;
	uint64_t compressed_contents_length;
	uint64_t most_recent_collection;
	uint32_t original_collection;
	uint32_t archivenumber;
	hash256 hash;
	uint32_t crc32;
} svdb_contents_row;

/*
We combine status and last-seen-collection id into one int.
Not only saves a column, but makes querying for unprocessed files very simple:
select from FilesList where status < x
*/

inline uint64_t sv_makestatus(uint64_t collectionid, sv_filerowstatus st)
{
	return (collectionid << 2) + st;
}

inline sv_filerowstatus sv_getstatus(uint64_t status)
{
	return (sv_filerowstatus)(status & 0x3);
}

inline uint64_t sv_getcollectionidfromstatus(uint64_t status)
{
	return status >> 2;
}

typedef struct svdb_txn {
	bool active;
} svdb_txn;

typedef struct svdb_qry {
	svdb_query qry_number;
} svdb_qry;

typedef sv_result(*fn_iterate_rows)(void *context, bool *shouldcontinue,
	const svdb_files_row *row, const bstring path, const bstring permissions);

typedef sv_result(*fn_iterate_contents)(void *context, bool *shouldcontinue,
	const svdb_contents_row *svdb_contents_row);

check_result svdb_connection_open(svdb_connection *self, const char *path);
check_result svdb_connection_disconnect(svdb_connection *self);
check_result svdb_clear_database_content(svdb_connection *self);
check_result svdb_preparequery(svdb_connection *self, svdb_query qry_number);
void svdb_connection_close(svdb_connection *self);

check_result svdb_propertygetcount(svdb_connection *self, uint64_t *ret);
check_result svdb_propertygetint(svdb_connection *self, const char *propname,
	int lenpropname, uint32_t *ret);
check_result svdb_propertysetint(svdb_connection *self, const char *propname, 
	int lenpropname, uint32_t val);
check_result svdb_propertygetstr(svdb_connection *self, const char *propname, 
	int lenpropname, bstring s);
check_result svdb_propertysetstr(svdb_connection *self, const char *propname, 
	int lenpropname, const char *val);
check_result svdb_propertygetstrlist(svdb_connection *self, const char *propname, 
	int lenpropname, bstrlist *val);
check_result svdb_propertysetstrlist(svdb_connection *self, const char *propname, 
	int lenpropname, const bstrlist *val);

check_result svdb_filesgetcount(svdb_connection *self, uint64_t *ret);
extern const uint64_t svdb_files_in_queue_get_all;
void svdb_files_row_string(const svdb_files_row *row, const char *path, 
	const char *permissions, bstring s);
check_result svdb_filesbypath(svdb_connection *self, const bstring path, 
	svdb_files_row *out_row);
check_result svdb_filesinsert(svdb_connection *self, const bstring path, 
	uint64_t collectionid, sv_filerowstatus status, uint64_t *outid);
check_result svdb_filesupdate(svdb_connection *self, const svdb_files_row *row, 
	const bstring permissions);
check_result svdb_files_in_queue(svdb_connection *self, uint64_t status, 
	void *context, fn_iterate_rows callback);
check_result svdb_files_bulk_delete(svdb_connection *self, const sv_array *arr, 
	int batchsize);

void svdb_collectiontostring(const svdb_collections_row *row, bool verbose, bool every, bstring s);
check_result svdb_collectioninsert(svdb_connection *self, uint64_t timestarted, uint64_t *rowid);
check_result svdb_collectiongetlast(svdb_connection *self, uint64_t *id);
check_result svdb_collectionsget(svdb_connection *self, sv_array *rows, bool get_all);
check_result svdb_collectionupdate(svdb_connection *self, const svdb_collections_row *row);
check_result svdb_add_collection_row(svdb_connection *db, uint64_t started, uint64_t finished);

void svdb_contents_row_string(const svdb_contents_row *row, bstring s);
check_result svdb_contentsinsert(svdb_connection *self, uint64_t *outid);
check_result svdb_contentsupdate(svdb_connection *self, const svdb_contents_row *row);
check_result svdb_contents_bulk_delete(svdb_connection *self, const sv_array *arr, int batchsize);
check_result svdb_contentsiter(svdb_connection *self, void *context, fn_iterate_contents callback);
check_result svdb_contentsgetcount(svdb_connection *self, uint64_t *ret);
check_result svdb_contents_setlastreferenced(svdb_connection *self,
	uint64_t contentsid, uint64_t collectionid);
check_result svdb_contentsbyhash(svdb_connection *self, const hash256 *hash,
	uint64_t contentslength, svdb_contents_row *row);
check_result svdb_contentsbyid(svdb_connection *self, uint64_t contentsid,
	svdb_contents_row *row);

check_result svdb_archives_get_checksums(svdb_connection *self, 
	bstrlist *filenames, bstrlist *checksums);
check_result svdb_archives_write_checksum(svdb_connection *self, uint64_t archiveid,
	uint64_t timemodified, uint64_t compaction_cutoff, const char *checksum, const char *filepath);

check_result svdb_txn_open(svdb_txn *self, svdb_connection *db);
check_result svdb_txn_commit(svdb_txn *self, svdb_connection *db);
check_result svdb_txn_rollback(svdb_txn *self, svdb_connection *db);
void svdb_txn_close(svdb_txn *self, svdb_connection *db);

#endif
