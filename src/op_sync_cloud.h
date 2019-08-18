/*
op_sync_cloud.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef OP_SYNC_CLOUD_H_INCLUDE
#define OP_SYNC_CLOUD_H_INCLUDE

#include "user_config.h"

check_result roughselectstrfromjson(const char *json, const char *key, bstring result);
bstring localpath_to_cloud_path(const char *rootdir, const char *path, const char *group);
bstring cloud_path_to_description(const char *cloudpath, const char *filename);

check_result sv_sync_marksuccessupload(svdb_db *db, const char *filepath, const char *cloudpath, const char *description, const char *archiveid, uint64_t sz, uint64_t knownvaultid);

typedef struct sv_sync_finddirtyfiles
{
    bstrlist *sizes_and_files;
    bstrlist *sizes_and_files_clean;
    uint64_t totalsizedirty;
    uint64_t totalsizeclean;
    svdb_db *db;
    bstring rootdir;
    bstring groupname;
    uint64_t knownvaultid;
    int countclean;
    bool verbose;
} sv_sync_finddirtyfiles;

sv_sync_finddirtyfiles sv_sync_finddirtyfiles_open(const char *rootdir, const char *groupname);
void sv_sync_finddirtyfiles_close(sv_sync_finddirtyfiles *self);
check_result sv_sync_finddirtyfiles_find(sv_sync_finddirtyfiles *finder);

#endif
