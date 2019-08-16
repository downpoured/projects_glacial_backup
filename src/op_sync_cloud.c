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
    bstrlist *sizes_and_files;
    uint64_t totalsizedirty;
    uint64_t countclean;
    uint64_t totalsizeclean;
    svdb_db *db;
    bstring rootdir;
    uint64_t knownvaultid;
    bool verbose;
} sv_sync_finddirtyfiles;

sv_sync_finddirtyfiles sv_sync_finddirtyfiles_open(const char *rootdir) {
    sv_sync_finddirtyfiles ret = {};
    ret.sizes_and_files = bstrlist_open();
    ret.rootdir = bfromcstr(rootdir);
    return ret;
}

void sv_sync_finddirtyfiles_close(sv_sync_finddirtyfiles *self) {
    if (self) {
        bstrlist_close(self->sizes_and_files);
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
    *isdirty = true;
    
    uint64_t cloud_size = 0, cloud_crc32 = 0, cloud_modtime = 0;
    check(svdb_vaultarchives_bypath(self->db, cloudpath, self->knownvaultid, &cloud_size, &cloud_crc32, &cloud_modtime));
    if (cloud_size == 0 && cloud_crc32 == 0 && cloud_modtime == 0) {
        *isdirty = true;
        if (self->verbose) {
            printf("new: %s\n", cloudpath);
        }
    }
    else if (modtime == cloud_modtime && filesize == cloud_size) {
        /* optimization: if modtime and filesize match, assume they are the same */
        *isdirty = false;
    }
    else {
        /* compute the crc32. */
        uint32_t intcrc = 0;
        check(sv_basic_crc32_wholefile(cstr(filepath), &intcrc));
        if ((uint64_t)intcrc == cloud_crc32) {
            *isdirty = false;
        }
        else {
            *isdirty = true;
            if (self->verbose) {
                printf("changed: %s\n", cloudpath);
            }
        }
    }

cleanup:
    return currenterr;
}

check_result sv_sync_finddirtyfiles_cb(void *context,
    const bstring filepath, uint64_t modtime, uint64_t filesize,
    unused(const bstring)) {
    sv_result currenterr = {};
    bstring sizeandfile = bstring_open();
    sv_sync_finddirtyfiles *self = (sv_sync_finddirtyfiles *)context;
    bstring cloudpath = localpath_to_cloud_path(cstr(self->rootdir), cstr(filepath));
    bool isdirty = true;
    check(sv_sync_finddirtyfiles_isdirty(self, filepath, modtime, filesize, cstr(cloudpath), &isdirty));
    if (isdirty) {
        check_b(filesize < INT_MAX, "file is too large %s", cstr(filepath));
        uint32_t filesize32 = cast64u32u(filesize);
        bsetfmt(sizeandfile, "%08x|%s", filesize32, cstr(filepath));
        bstrlist_appendcstr(self->sizes_and_files, cstr(sizeandfile));
        self->totalsizedirty += filesize;
    }
    else {
        self->countclean += 1;
        self->totalsizeclean += filesize;
    }

  cleanup:
    bdestroy(cloudpath);
    bdestroy(sizeandfile);
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

check_result sv_sync_newvault(svdb_db *db)
{
    sv_result currenterr = {};
    os_clr_console();
    alert("We'll walk you through the processs of creating a vault.\n\n"
    "Press Ctrl-C to exit if you no longer want to create a vault.\n\n"
    "First, create a Amazon Web Services account, if you don't already have one.");
    os_clr_console();
    alert("Then, create a IAM account for the Amazon Web Services account.\n\n"
        "The IAM account will be a 'subaccount' with restricted access, for safety. "
    "If the IAM account is compromised, the potential damage is more limited.\n\n"
        "Give the IAM account permissions at least for Glacier-related actions\n\n"
        "For example, you can use this policy:\n\n"
        "{\n"
        "  \"Version\":\"2012-10-17\",\n"
        "  \"Statement\": [\n"
        "    {\n"
        "      \"Effect\": \"Allow\",\n"
        "      \"Resource\": \"*\",\n"
        "      \"Action\":[\"glacier:*\"] \n"
        "    }\n"
        "  ]\n"
        "}"
    );
    os_clr_console();
    alert("Log out of AWS, then go to the Amazon Web Services console and "
    "log in with your new IAM account.\n\n"
    "Go to https://console.aws.amazon.com/iam/home?#security_credential \n\n"
    "Write down the 'Access Key ID' and 'Secret Access Key' in a trusted place.");
    os_clr_console();
    alert("Go to https://console.aws.amazon.com/glacier \n\n");
    alert("(Optional: change the 'Region' in the top right, such as us-west-1) \n\n");
    alert("Click 'Create Vault' and assign a name.\n\n");
    db;
    return currenterr;
}

check_result sv_sync_rawupload(const char *awscli, const char *vaultname, const char *path, const char *description, bstring outarchiveid) {
    sv_result currenterr = {};
    const char *args[] = { linuxonly(awscli) "glacier",
        "upload-archive",
        "--vault-name",
        vaultname,
        "--account-id",
        "-",
        "--archive-description",
        description,
        "--output",
        "json",
        "--body",
        path, NULL };
    bstring output = bstring_open();
    bstring temp = bstring_open();
    int retcode = -1;
    check(os_run_process(awscli, args, output, temp, false, NULL, NULL, &retcode));
    check(roughselectstrfromjson(cstr(output), "archive-id", outarchiveid));
cleanup:
    return currenterr;
}

check_result sv_sync_main(const sv_app *app, const sv_group *grp, svdb_db *db, const char *, const char *vaultname, const char *, const char *)
{
    sv_result currenterr = {};
    ar_manager ar = {};
    sv_sync_finddirtyfiles finder = {};
    bstring archiveid = bstring_open();

    uint32_t ignoredcollectionid = 1;
    uint32_t ignoredarchivesize = 1;
    check(ar_manager_open(&ar, cstr(app->path_app_data), cstr(grp->grpname), ignoredcollectionid, ignoredarchivesize));
    const char *rootdir = cstr(ar.path_readytoupload);
    finder = sv_sync_finddirtyfiles_open(rootdir);
    finder.db = db;

    /* find dirty files */
    check(sv_sync_finddirtyfiles_find(&finder));
    /* sort, smallest files first */
    bstrlist_sort(finder.sizes_and_files);
    /* begin upload */
    check(sv_sync_rawupload("aws", vaultname, blist_view(finder.sizes_and_files, 0), "", archiveid));

cleanup:
    ar_manager_close(&ar);
    sv_sync_finddirtyfiles_close(&finder);
    return currenterr;
}

check_result sv_sync_cloud(
    const sv_app *app, const sv_group *grp, svdb_db *db)
{
    sv_result currenterr = {};
    bstrlist *regions = bstrlist_open();
    bstrlist *names = bstrlist_open();
    bstrlist *awsnames = bstrlist_open();
    bstrlist *arns = bstrlist_open();
    check(svdb_knownvaults_get(db, regions, names, awsnames, arns));
    
    int chosen = menu_choose(
        "Please choose a Amazon Glacier vault.", names, "Create New", "(Back)", NULL);
    if (chosen == names->qty)
    {
        /* create a new vault */
        check(sv_sync_newvault(db));
        /* recurse */
        check(sv_sync_cloud(app, grp, db));
    }
    else if (chosen < names->qty) {
        check(sv_sync_main(app, grp, db, blist_view(regions, chosen),
            blist_view(names, chosen), blist_view(awsnames, chosen), blist_view(arns, chosen)));
    }
    
cleanup:
    bstrlist_close(regions);
    bstrlist_close(names);
    bstrlist_close(awsnames);
    bstrlist_close(arns);
    return currenterr;
}

