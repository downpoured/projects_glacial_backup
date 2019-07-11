/*
util_archiver.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef WRAP_ARCHIVER_H_INCLUDE
#define WRAP_ARCHIVER_H_INCLUDE

#include "util_files.h"
#include "lib_sphash.h"

typedef struct ar_util {
    bstring tar_binary;
    bstring xz_binary;
    bstring audiotag_binary;
    bstrlist *list;
    bstring tmp_arg_tar;
    bstring tmp_inner_name;
    bstring tmp_combined;
    bstring tmp_ascii;
    bstring tmp_results;
    bstring tmp_filename;
    bstring tmp_xz_name;
} ar_util;

void ar_util_close(ar_util *self);
ar_util ar_util_open();
check_result ar_util_add(ar_util *self,
    const char *tarpath,
    const char *inputpath,
    const char *namewithin,
    uint64_t inputsize);
check_result ar_util_verify(ar_util *self,
    const char *tarpath,
    const bstrlist *expectedcontents,
    const sv_array *expectedsizes);
check_result ar_util_extract_overwrite(ar_util *self,
    const char *tarpath,
    const char *namewithin,
    const char *tmpdir,
    bstring extracted_to);
check_result ar_util_delete(ar_util *self,
    const char *archive,
    const char *dir_tmp,
    const char *dir_tmp_unarchived,
    const sv_array *contentids);
check_result ar_util_xz_add(ar_util *self,
    const char *inputpath,
    const char *destpath);
check_result ar_util_xz_verify(ar_util *self,
    const char *archivepath);
check_result ar_util_xz_extract_overwrite(ar_util *self,
    const char *archivepath,
    const char *destination);

typedef struct ar_manager {
    ar_util ar;
    bstring path_working;
    bstring path_staging;
    bstring path_readytoupload;
    uint32_t collectionid;
    int32_t limitperarchive;
    uint64_t target_archive_size;
    bstring currentarchive;
    uint32_t currentarchivenum;
    sv_file namestextfile;
    sv_array current_sizes;
    bstrlist *current_names;
} ar_manager;

void ar_manager_close(ar_manager *self);
check_result ar_manager_begin(ar_manager *self);
check_result ar_manager_finish(ar_manager *self);
check_result ar_manager_advance_to_next(ar_manager *self);
check_result ar_manager_restore(ar_manager *self,
    const char *archive,
    uint64_t contentid,
    const char *working_dir_archived,
    const char *dest);
check_result ar_manager_add(ar_manager *self,
    const char *pathinput,
    bool iscompressed,
    uint64_t contentsid,
    uint32_t *archivenumber,
    uint64_t *compressedsize);
check_result ar_manager_open(ar_manager *self,
    const char *pathapp,
    const char *grpname,
    uint32_t collectionid,
    uint32_t archivesize);

typedef struct hash256 {
    uint64_t data[4];
} hash256;

#endif
