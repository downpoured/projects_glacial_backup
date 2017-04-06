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

#ifndef WRAP_ARCHIVER_H_INCLUDE
#define WRAP_ARCHIVER_H_INCLUDE

#include "util_os.h"
#include "lib_sphash.h"

typedef struct svdp_archiver {
    bstring path_intermediate_archives;
    bstring path_staging;
    bstring path_readytoupload;
    bstring path_archiver_binary;
    bstring path_compression_binary;
    bstring path_audiometadata_binary;
    uint32_t collectionid;
    int32_t limitfilesperarchive;
    uint64_t target_archive_size_bytes;
    bstring encryption;
    bstring currentarchive;
    uint32_t currentarchivenumber;
    sv_file friendlyfilenamesfile;
    sv_array currentarchive_sizes;
    bstrlist *currentarchive_contents;
    bstrlist *tmp_list;
    bstring tmp_arg_input;
    bstring tmp_arg_encryption;
    bstring tmp_argscombined;
    bstring tmp_resultstring;
    bstring tmp_parentdir;
    bstring tmp_filename;
    bool can_add_to_tar_directly;
} svdp_archiver;

typedef struct hash256 {
    uint64_t data[4];
} hash256;

check_result svdp_archiver_open(svdp_archiver *self, const char *pathapp, const char *groupname,
    uint32_t currentcollectionid, uint32_t targetsizemb, const char *encryption);
void svdp_archiver_close(svdp_archiver *self);
check_result svdp_archiver_addfile(svdp_archiver *self, const char *pathinput, bool iscompressed,
    uint64_t contentsid, uint32_t *archivenumber, uint64_t *compressedsize);
check_result svdp_archiver_beginarchiving(svdp_archiver *self);
check_result svdp_archiver_finisharchiving(svdp_archiver *self);
check_result svdp_archiver_delete_from_archive(svdp_archiver *self, const char *archive,
    const char *dir_tmp_unarchived, const sv_array *contentids);
check_result svdp_archiver_restore_from_archive(svdp_archiver *self, const char *archive,
    uint64_t contentid, const char *workingdir, const char *subworkingdir, const char *dest);
check_result svdp_archiver_tar_list_string_testsonly(svdp_archiver *self, const char *archive,
    bstrlist *results, const char *subworkingdir);
check_result svdp_archiver_verify_has_one_item(svdp_archiver *self, const char *path7zfile,
    const char *inputfilename, const char *encryption);

check_result svdp_tar_add(svdp_archiver *self, const char *tarname, const char *inputfile);
check_result svdp_tar_add_with_custom_name(svdp_archiver *self, const char *tarname,
    const char *inputfile, const char *namewithinarchive);
check_result svdp_tar_verify_contents(svdp_archiver *self, const char *archive,
    bstrlist *expectedcontents, sv_array *expectedsizes);
check_result svdp_7z_add(svdp_archiver *self, const char *path7zfile, const char *inputfilepath,
    bool is_compressed);
check_result svdp_7z_extract_overwrite_ok(svdp_archiver *self, const char *archive,
    const char *encryption, const char *internalarchivepath, const char *outputdir);
check_result svdp_archiver_7z_list_string_testsonly(svdp_archiver *self, const char *archive,
    bstrlist *results);
check_result svdp_archiver_7z_list_testsonly(svdp_archiver *self, const char *archive,
    bstrlist *paths, sv_array *sizes, sv_array *crcs, sv_array *methods);

extern const hash256 hash256zeros;
inline void hash256tostr(const hash256 *h1, bstring s)
{
    bassignformat(s, "%llx %llx %llx %llx",
        castull(h1->data[0]), castull(h1->data[1]), castull(h1->data[2]), castull(h1->data[3]));
}

inline bool hash256eq(const hash256 *h1, const hash256 *h2)
{
    return h1->data[0] == h2->data[0] &&
        h1->data[1] == h2->data[1] &&
        h1->data[2] == h2->data[2] &&
        h1->data[3] == h2->data[3];
}

#endif
