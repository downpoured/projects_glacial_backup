/*
op_sync_cloud.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

/*
Amazon Glacier works differently than a traditional file server.
After reading the docs a lot, here's my current understanding:

You can create a "vault" which contains "archives".
An "archive" doesn't have a path, just a long automatically assigned opaque id.
You can provide a path in 'archive-description' field, but you don't have to.
You can't quickly see what's in vault - you have to ask for an "inventory" and
    wait for the results to come back,
    by default updated only once a day.
Contents of the inventory:
    https://docs.aws.amazon.com/amazonglacier/latest/dev/api-job-output-get.html
    an ArchiveList, each entry has
        ArchiveId, ArchiveDescription, CreationDate, Size, SHA256TreeHash

So it's up to the application to keep track of what files are already
uploaded, because an inventory request is slow. Note that because paths
are just descriptions, it's up to the application to prevent having
two archives with the same path.

At first, I built this in a separate executable using awssdkcpp,
but decided to call awscli instead. Code is slightly simpler,
and linux installation is much simpler.

*/



#include "op_sync_cloud.h"

check_result roughselectstrfromjson(const char *json, const char *key, bstring result) {
    sv_result currenterr = {};
    bstring bjson = bfromcstr(json);
    bstring search = bformat("\"%s\": \"");
    bstrlist *partsAfterQuote = NULL;
    bstrlist *parts = bsplits(bjson, search);
    check_b(parts->qty != 0, "key not found %s in %s", key, json);
    check_b(parts->qty == 1, "key found more than once, %s in %s", key, json);
    partsAfterQuote = bsplit(parts->entry[0], '"');
    check_b(partsAfterQuote->qty != 0, "closing quote not found %s in %s", key, json);
    bassign(result, partsAfterQuote->entry[0]);
cleanup:
    bdestroy(bjson);
    bdestroy(search);
    bstrlist_close(partsAfterQuote);
    bstrlist_close(parts);
    return currenterr;
}

typedef struct sv_sync_finddirtyfiles
{
    bstrlist *files;
    bstrlist *cloudpaths;
    uint64_t totalsize;
    svdb_db *db;
    bstring rootdir;
    uint64_t knownvaultid;
    bool verbose;
} sv_sync_finddirtyfiles;

sv_sync_finddirtyfiles sv_sync_finddirtyfiles_open(const char *rootdir) {
    sv_sync_finddirtyfiles ret = {};
    ret.files = bstrlist_open();
    ret.cloudpaths = bstrlist_open();
    ret.rootdir = bfromcstr(rootdir);
    return ret;
}

void sv_sync_finddirtyfiles_close(sv_sync_finddirtyfiles *self) {
    if (self) {
        bstrlist_close(self->files);
        bstrlist_close(self->cloudpaths);
        bdestroy(self->rootdir);
        set_self_zero();
    }
}

bstring localpath_to_cloud_path(const char *rootdir, const char *path) {
    /* let's keep the leaf name of the rootdir */
    bstring ret = bstring_open();
    bstring prefix = bstring_open();
    os_get_parent(rootdir, prefix);
    check_fatal(!strchr("/\\", rootdir[strlen(rootdir) - 1]), "root should not end with dirsep");
    if (s_startwith(path, cstr(prefix))) {
        const char *pathwithoutroot = path + blength(prefix);
        bsetfmt(ret, "glacial_backup/%s", pathwithoutroot);
        bstr_replaceall(ret, "\\", "/");
    }

    bdestroy(prefix);
    return ret;
}

check_result sv_sync_finddirtyfiles_isdirty(sv_sync_finddirtyfiles *self,
    const bstring filepath, uint64_t modtime, uint64_t filesize, const char *cloudpath, bool *isdirty) {
    sv_result currenterr = {};
    uint64_t cloud_size = 0, cloud_crc32 = 0, cloud_modtime = 0;
    check(svdb_vaultarchives_bypath(self->db, cloudpath, self->knownvaultid, &cloud_size, &cloud_crc32, &cloud_modtime));
    filepath;
    modtime;
    filesize;
    *isdirty = true;
cleanup:
    return currenterr;
}

check_result sv_sync_finddirtyfiles_cb(void *context,
    const bstring filepath, uint64_t modtime, uint64_t filesize,
    unused(const bstring)) {
    sv_result currenterr = {};
    sv_sync_finddirtyfiles *self = (sv_sync_finddirtyfiles *)context;
    bstring cloudpath = localpath_to_cloud_path(cstr(self->rootdir), cstr(filepath));
    bool isdirty = true;
    check(sv_sync_finddirtyfiles_isdirty(self, filepath, modtime, filesize, cstr(cloudpath), &isdirty));
    if (isdirty) {
        bstrlist_appendcstr(self->cloudpaths, cstr(cloudpath));
        bstrlist_appendcstr(self->files, cstr(filepath));
        self->totalsize += filesize;
    }

  cleanup:
    bdestroy(cloudpath);
    return currenterr;
}


check_result sv_sync_finddirtyfiles_find(sv_sync_finddirtyfiles *finder) {
    sv_result currenterr = {};
    os_recurse_params params = {};
    params.context = finder;
    params.root = cstr(finder->rootdir);
    params.callback = sv_sync_finddirtyfiles_cb;
    params.max_recursion_depth = 1024;
    params.messages = bstrlist_open();
    check(os_recurse(&params));
cleanup:
    return currenterr;
}

check_result sv_sync_cloud(
    const sv_app *app, const sv_group *grp, svdb_db *db)
{
    app;
    grp;
    db;
    return OK;
}

