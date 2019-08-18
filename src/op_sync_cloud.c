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

You create a "vault" which contains "archives" (which for us are files).
An "archive" doesn't have a path, just an automatically assigned opaque id.
You can provide a path in 'archive-description' field, but you don't have to.
You can't quickly see what's in vault - you have to ask for an "inventory" and
    wait for the results to come back,
    updated only once a day.
Contents of the inventory:
    https://docs.aws.amazon.com/amazonglacier/latest/dev/api-job-output-get.html
    an ArchiveList, each entry has
        ArchiveId, ArchiveDescription, CreationDate, Size, SHA256TreeHash

So it's up to the application to keep track of what files are already
uploaded, because an inventory request is slow. Note that because paths
are just descriptions, it's up to the application to prevent having
two archives with the same path.

Possible to talk to aws directly using awssdkcpp, but using awscli
instead. Makes compilation + linux installation much simpler.
*/

#include "op_sync_cloud.h"
#include <time.h>

check_result roughselectstrfromjson(
    const char *json, const char *key, bstring result)
{
    sv_result currenterr = {};
    bstring search = bformat("\"%s\": \"", key);
    bstrlist *partsAfterQuote = bstrlist_open();
    const char *fnd = strstr(json, cstr(search));
    check_b(fnd, "key not found %s in %s", key, json);
    check_b(!strstr(fnd + 1, cstr(search)), "key found more than once, %s in %s",
        key, json);
    bstrlist_splitcstr(partsAfterQuote, fnd + blength(search), '"');
    check_b(
        partsAfterQuote->qty > 1, "closing quote not found %s in %s", key, json);
    bassign(result, partsAfterQuote->entry[0]);
cleanup:
    bdestroy(search);
    bstrlist_close(partsAfterQuote);
    return currenterr;
}

bstring ostime_to_str_or_fallback(uint64_t ostime, const char *fallback)
{
    /* use FastGlacier's format. */
    /* on Windows FAT systems this isn't utc, but that's not too common. */
    uint64_t posixtime64 = os_ostime_to_posixtime(ostime);
    time_t posixtime = (time_t)posixtime64;
    struct tm timestruct = {};
#ifdef _WIN32
    errno_t err = _gmtime64_s(&timestruct, &posixtime);
    bool success = err == 0;
#else
    struct tm *ret = gmtime_r(&posixtime, &timestruct);
    bool success = ret != NULL;
#endif
    if (success)
    {
        char buf[BUFSIZ] = {0};
        strftime(buf, countof(buf) - 1, "%Y%m%dT%H%M%SZ", &timestruct);
        return bfromcstr(buf);
    }
    else
    {
        return bfromcstr(fallback);
    }
}

bstring localpath_to_cloud_path(
    const char *crootdir, const char *cpath, const char *groupname)
{
    /* normalize pathsep */
    bstring rootdir = bfromcstr(crootdir);
    bstring path = bfromcstr(cpath);
    bstr_replaceall(rootdir, "\\", "/");
    bstr_replaceall(path, "\\", "/");
    bstring ret = bstring_open();
    bstring prefix = bstring_open();

    check_fatal(rootdir, "rootdir cannot be null");
    os_get_parent(cstr(rootdir), prefix);
    check_fatal(cstr(rootdir)[blength(rootdir) - 1] != '/',
        "root should not end with dirsep");
    bcatcstr(rootdir, "/");
    if (s_startwith(cstr(path), cstr(rootdir)))
    {
        const char *pathwithoutroot = cstr(path) + blength(prefix) + 1;
        bsetfmt(ret, "glacial_backup/%s/%s", groupname, pathwithoutroot);
    }

    bdestroy(rootdir);
    bdestroy(path);
    bdestroy(prefix);
    return ret;
}

bstring cloud_path_to_description(const char *cloudpath, const char *filename)
{
    /* try to be compatible with
    https://fastglacier.com/fastglacier-metadata-format.aspx */
    uint64_t ostime = os_getmodifiedtime(filename);
    bstring tm = ostime_to_str_or_fallback(ostime, "19010101T010101Z");
    bstring cpath = bfromcstr(cloudpath);
    /* fastglacier creates 'directories' with 1-byte archives.
    for simplicity, I won't use directories, I'll just dump everything in root.
  */
    bstr_replaceall(cpath, "/", ";");
    bstring cpathbase64 = tobase64nospace(cstr(cpath));
    if (!cpathbase64)
    {
        cpathbase64 = bfromcstr("");
    }

    bstring ret = bformat(
        "<m><v>4</v><p>%s</p><lm>%s</lm></m>", cstr(cpathbase64), cstr(tm));
    bdestroy(tm);
    bdestroy(cpath);
    bdestroy(cpathbase64);
    return ret;
}

void setdirforcli(const char *awscli)
{
    if (!islinux)
    {
        /* awscli doesn't work if current dir is on different drive */
        char path[] = "C:\\";
        if (strlen(awscli) > 3 && awscli[1] == ':')
        {
            path[0] = awscli[0];
        }

        int result = chdir(path);
        log_b(result == 0, "chdir returned failure, errno=%d", errno);
    }
}

sv_sync_finddirtyfiles sv_sync_finddirtyfiles_open(
    const char *rootdir, const char *groupname)
{
    sv_sync_finddirtyfiles ret = {};
    ret.sizes_and_files = bstrlist_open();
    ret.rootdir = bfromcstr(rootdir);
    ret.groupname = bfromcstr(groupname);
    ret.verbose = true;
    return ret;
}

void sv_sync_finddirtyfiles_close(sv_sync_finddirtyfiles *self)
{
    if (self)
    {
        bstrlist_close(self->sizes_and_files);
        bdestroy(self->rootdir);
        bdestroy(self->groupname);
        set_self_zero();
    }
}

check_result sv_sync_finddirtyfiles_isdirty(sv_sync_finddirtyfiles *self,
    const bstring filepath, uint64_t modtime, uint64_t filesize,
    const char *cloudpath, bool *isdirty)
{
    sv_result currenterr = {};
    *isdirty = true;

    modtime = os_getmodifiedtime(cstr(filepath));

    uint64_t cloud_size = 0, cloud_crc32 = 0, cloud_modtime = 0;
    check(svdb_vaultarchives_bypath(self->db, cloudpath, self->knownvaultid,
        &cloud_size, &cloud_crc32, &cloud_modtime));
    if (cloud_size == 0 && cloud_crc32 == 0 && cloud_modtime == 0)
    {
        /* case 1: not online yet */
        *isdirty = true;
        if (self->verbose)
        {
            printf("New: %s\n", cloudpath);
        }
    }
    else if (modtime == cloud_modtime && filesize == cloud_size)
    {
        /* case 2: optimization: skip computing crc if modtime and filesize
         * match, assume file is the same */
        *isdirty = false;
    }
    else
    {
        uint32_t intcrc = 0;
        check(sv_basic_crc32_wholefile(cstr(filepath), &intcrc));
        if ((uint64_t)intcrc == cloud_crc32)
        {
            /* case 3: crc matches, it's the same content */
            *isdirty = false;
        }
        else
        {
            /* case 4: crc doesn't match, it's different content */
            *isdirty = true;
            if (self->verbose)
            {
                printf("Changed: %s\n", cloudpath);
            }
        }
    }

cleanup:
    return currenterr;
}

check_result sv_sync_finddirtyfiles_cb(void *context, const bstring filepath,
    uint64_t modtime, uint64_t filesize, unused(const bstring))
{
    sv_result currenterr = {};
    bstring sizeandfile = bstring_open();
    sv_sync_finddirtyfiles *self = (sv_sync_finddirtyfiles *)context;
    bstring cloudpath = localpath_to_cloud_path(
        cstr(self->rootdir), cstr(filepath), cstr(self->groupname));
    check_b(blength(cloudpath), "could not make cloudpath %s, %s",
        cstr(self->rootdir), cstr(filepath));
    check_b(filesize < INT_MAX, "file is too large %s", cstr(filepath));
    uint32_t filesize32 = cast64u32u(filesize);
    bsetfmt(sizeandfile, "%08x|%s", filesize32, cstr(filepath));
    bool isdirty = true;
    check(sv_sync_finddirtyfiles_isdirty(
        self, filepath, modtime, filesize, cstr(cloudpath), &isdirty));
    if (isdirty)
    {
        bstrlist_appendcstr(self->sizes_and_files, cstr(sizeandfile));
        self->totalsizedirty += filesize;
    }
    else
    {
        if (self->sizes_and_files_clean)
        {
            bstrlist_appendcstr(self->sizes_and_files_clean, cstr(sizeandfile));
        }

        self->countclean += 1;
        self->totalsizeclean += filesize;
    }

cleanup:
    bdestroy(cloudpath);
    bdestroy(sizeandfile);
    return currenterr;
}

check_result sv_sync_finddirtyfiles_find(sv_sync_finddirtyfiles *finder)
{
    sv_result currenterr = {};
    os_recurse_params params = {};
    params.context = finder;
    params.root = cstr(finder->rootdir);
    params.callback = sv_sync_finddirtyfiles_cb;
    params.max_recursion_depth = 1024;
    params.messages = bstrlist_open();

    /* wrap it all in a read txn */
    svdb_txn txn = {};
    check(svdb_txn_open(&txn, finder->db));
    check(os_recurse(&params));
    check(svdb_txn_commit(&txn, finder->db));
cleanup:
    svdb_txn_close(&txn, finder->db);
    return currenterr;
}

bool sv_sync_askcreds(const char *region)
{
    bstring keyid = bstring_open();
    bstring key = bstring_open();
    bool ret = false;
    os_clr_console();
    ask_user_str("Please type in the 'aws access key id':", true, keyid);
    if (!blength(keyid))
    {
        goto cleanup;
    }

    os_clr_console();
    ask_user_str("Please type in the 'aws secret access key':", true, key);
    if (!blength(key))
    {
        goto cleanup;
    }

    os_set_env("AWS_DEFAULT_REGION", region);
    os_set_env("AWS_ACCESS_KEY_ID", cstr(keyid));
    os_set_env("AWS_SECRET_ACCESS_KEY", cstr(key));
    ret = true;
cleanup:
    bdestroy(keyid);
    bdestroy(key);
    return ret;
}

check_result sv_sync_findawscli(bstring out)
{
    sv_result currenterr = {};
    check(os_binarypath(islinux ? "aws" : "aws.exe", out));
    if (os_file_exists(cstr(out)))
    {
        goto cleanup;
    }

    bassigncstr(out, "C:\\Program Files\\Amazon\\AWSCLIV2\\aws.exe");
    if (os_file_exists(cstr(out)))
    {
        goto cleanup;
    }

    bassigncstr(out, "C:\\Program Files (x86)\\Amazon\\AWSCLIV2\\aws.exe");
    if (os_file_exists(cstr(out)))
    {
        goto cleanup;
    }

    bstrclear(out);
cleanup:
    return currenterr;
}

check_result sv_sync_findcheckawscli(bstring awscli)
{
    sv_result currenterr = {};
    bstring output = bstring_open();
    bstring temp = bstring_open();
    check(sv_sync_findawscli(awscli));
    if (!blength(awscli))
    {
        os_clr_console();
        printf("We could not find the aws cli.\n\n");
        if (islinux)
        {
            alert("Please install it with a command like 'sudo apt-get install "
                  "awscli'.\n\n");
        }
        else
        {
            alert("Please install it (version 2 or later) from Amazon's website "
                  "at https://aws.amazon.com/cli/.\n\n");
        }
    }
    else
    {
        const char *args[] = {linuxonly(cstr(awscli)) "--version", NULL};
        int retcode = -1;
        setdirforcli(cstr(awscli));
        check(os_run_process(
            cstr(awscli), args, output, temp, false, NULL, NULL, &retcode));
        bltrimws(output);
        if (!s_startwith(cstr(output), "aws-cli/"))
        {
            /* allow cli version 1 for now,
            it's what's in apt-get install and it seems to work fine. */
            bstrclear(awscli);
            printf("We expected 'aws --version' to print a version like "
                   "'aws-cli/2.0.34' but got '%s'. Old version or broken "
                   "install?\n\n",
                cstr(output));
            if (islinux)
            {
                alert("Please install awscli with a command like 'sudo apt-get "
                      "install awscli'.\n\n");
            }
            else
            {
                alert("Please install awscli (version 2 or later) from Amazon's "
                      "website at https://aws.amazon.com/cli/.\n\n");
            }
        }
    }
cleanup:
    bdestroy(output);
    bdestroy(temp);
    return currenterr;
}

check_result sv_sync_newvault(svdb_db *db, const char *awscli)
{
    sv_result currenterr = {};
    os_clr_console();
    bstring region = bstring_open();
    bstring output = bstring_open();
    bstring temp = bstring_open();
    bstring vaultname = bstring_open();
    bstring awsvaultname = bstring_open();
    bstring awsvaultarn = bstring_open();
    int retcode = -1;

    do
    {
        os_clr_console();
        printf("You're now ready to connect to the vault.\n\n"
               "Files we store here will be visible by tools like FastGlacier. "
               "Or, you can use awscli to initiate a download, we write the "
               "file's name in base64 in the archive's 'description'.\n\n");

        ask_user_str("Please type in the name of the vault, or 'q' to cancel:",
            true, vaultname);
        if (!blength(vaultname))
        {
            goto cleanup;
        }

        printf("We'll now see if we can connect to the vault.\n\n");
        ask_user_str(
            "Please type in the region (e.g. 'us-west-1'):", true, region);
        if (!blength(region))
        {
            goto cleanup;
        }

        if (!sv_sync_askcreds(cstr(region)))
        {
            goto cleanup;
        }

        const char *args[] = {linuxonly(awscli) "glacier", "describe-vault",
            "--vault-name", cstr(vaultname), "--account-id", "-", "--output",
            "json", NULL};

        setdirforcli(awscli);
        sv_result ret = os_run_process(
            awscli, args, output, temp, false, NULL, NULL, &retcode);
        if (ret.code || retcode != 0)
        {
            printf("awscli describe-vault failed with: %s\nexit code=%d",
                cstr(output), retcode);
            retcode = -1;
            if (ask_user("Try again? y/n"))
            {
                continue;
            }
        }

    } while (false);

    if (retcode == 0)
    {
        check(roughselectstrfromjson(cstr(output), "VaultName", awsvaultname));
        check(roughselectstrfromjson(cstr(output), "VaultARN", awsvaultarn));
        check(svdb_knownvaults_insert(db, cstr(region), cstr(vaultname),
            cstr(awsvaultname), cstr(awsvaultarn)));
        printf("Successfully connected to %s\n", cstr(awsvaultarn));
        alert("");
    }
cleanup:
    bdestroy(region);
    bdestroy(output);
    bdestroy(temp);
    bdestroy(vaultname);
    bdestroy(awsvaultname);
    bdestroy(awsvaultarn);
    return currenterr;
}

check_result sv_sync_newvault_intro(svdb_db *db)
{
    sv_result currenterr = {};
    bstring awscli = bstring_open();
    check(sv_sync_findcheckawscli(awscli));
    if (!blength(awscli))
    {
        goto cleanup;
    }

    os_clr_console();
    alert("We'll walk you through the processs of creating a vault.\n\n"
          "Press Ctrl-C to exit if you no longer want to create a vault.\n\n"
          "First, create a Amazon Web Services account, if you don't already "
          "have one.");
    os_clr_console();
    alert("Then, create a IAM account for the Amazon Web Services account.\n\n"
          "The IAM account will be a 'subaccount' with restricted access, for "
          "safety. (If the IAM account is compromised, the potential damage is "
          "more "
          "limited).\n\n"
          "Give the IAM account permissions at least for Glacier-related "
          "actions\n\n"
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
          "}");
    os_clr_console();
    alert(
        "Log out of AWS, then go to the Amazon Web Services console and "
        "log in with your new IAM account.\n\n"
        "Go to https://console.aws.amazon.com/iam/home?#security_credential \n\n"
        "Write down the 'Access Key ID' and 'Secret Access Key' in a trusted "
        "place.");
    os_clr_console();
    alert("Next,\n\nGo to https://console.aws.amazon.com/glacier \n\n"
          "Notice the 'Region' in the top right (e.g. us-west-1) and write it "
          "down, or optionally change it.\n\n"
          "1) Click 'Create Vault'.\n\n"
          "2) Enter a vault name.\n\n"
          "3) Click Next through each page (you don't need to set up "
          "notifications) and click Create Vault.\n\n");
    check(sv_sync_newvault(db, cstr(awscli)));

cleanup:
    bdestroy(awscli);
    return currenterr;
}

check_result sv_sync_rawupload(const char *awscli, const char *vaultname,
    const char *path, const char *description, bstring outarchiveid)
{
    sv_result currenterr = {};
    const char *args[] = {linuxonly(awscli) "glacier", "upload-archive",
        "--vault-name", vaultname, "--account-id", "-", "--archive-description",
        description, "--output", "json", "--body", path, NULL};
    log_b(false,
        "starting a typical upload, vaultname=%s description=%s path=%s",
        vaultname, description, path);
    bstring output = bstring_open();
    bstring temp = bstring_open();
    int retcode = -1;
    setdirforcli(awscli);
    check(
        os_run_process(awscli, args, output, temp, false, NULL, NULL, &retcode));
    check_b(retcode == 0, "failed with retcode=%d, %s\n", retcode, cstr(output));
    check(roughselectstrfromjson(cstr(output), "archiveId", outarchiveid));
    check_b(blength(outarchiveid), "archiveId shouldn't be empty");
cleanup:
    return currenterr;
}

check_result sv_sync_marksuccessupload(svdb_db *db, const char *filepath,
    const char *cloudpath, const char *description, const char *archiveid,
    uint64_t sz, uint64_t knownvaultid)
{
    sv_result currenterr = {};
    /* compute the crc32. */
    uint32_t intcrc = 0;
    check(sv_basic_crc32_wholefile(filepath, &intcrc));
    uint64_t modtime = os_getmodifiedtime(filepath);

    /* overwrite any existing files with that path. */
    check(svdb_vaultarchives_delbypath(db, cloudpath, knownvaultid));
    check(svdb_vaultarchives_insert(db, cloudpath, description, knownvaultid,
        archiveid, sz, intcrc, modtime));
cleanup:
    return currenterr;
}

check_result sv_sync_uploadone(svdb_db *db, const char *awscli, int i, int total,
    const char *root, const char *grpname, const char *sizeandfile,
    const char *vault, uint64_t knownvaultid)
{
    sv_result currenterr = {};
    bstring cloudpath = NULL;
    bstring description = NULL;
    bstring archiveid = bstring_open();
    const char *filepath = strchr(sizeandfile, '|');
    check_b(filepath, "did not contain '|', %s", sizeandfile);
    filepath += 1;
    if (os_file_exists(filepath))
    {
        cloudpath = localpath_to_cloud_path(root, filepath, grpname);
        check_b(blength(cloudpath), "could not make cloudpath %s, %s", root,
            filepath);
        description = cloud_path_to_description(cstr(cloudpath), filepath);
        uint64_t sz = os_getfilesize(filepath);
        bool succeeded = false;
        do
        {
            printf("Uploading file %d/%d, size=%f Mb\n...This may take some "
                   "time...\n",
                i, total, (double)sz / (1024.0 * 1024.0));
            sv_result err = sv_sync_rawupload(
                awscli, vault, filepath, cstr(description), archiveid);
            if (err.code)
            {
                printf("Message: %s", blength(err.msg) ? cstr(err.msg) : "");
                if (ask_user("Try again? y/n"))
                {
                    continue;
                }
            }
            else
            {
                succeeded = true;
            }
        } while (false);

        if (succeeded)
        {
            check(sv_sync_marksuccessupload(db, filepath, cstr(cloudpath),
                cstr(description), cstr(archiveid), sz, knownvaultid));
        }
    }
    else
    {
        printf("Note: file not found %s\n", filepath);
        alert("");
    }

cleanup:
    bdestroy(cloudpath);
    bdestroy(description);
    bdestroy(archiveid);
    return currenterr;
}

check_result sv_sync_main(const sv_app *app, const sv_group *grp, svdb_db *db,
    const char *region, const char *vault, uint64_t knownvaultid)
{
    sv_result currenterr = {};
    ar_manager ar = {};
    sv_sync_finddirtyfiles finder = {};
    bstring awscli = bstring_open();
    check(sv_sync_findcheckawscli(awscli));
    if (!blength(awscli))
    {
        goto cleanup;
    }

    /* enter credentials */
    if (!sv_sync_askcreds(region))
    {
        goto cleanup;
    }

    /* get the ready-to-upload dir */
    uint32_t ignoredcollectionid = 1;
    uint32_t ignoredarchivesize = 1;
    check(ar_manager_open(&ar, cstr(app->path_app_data), cstr(grp->grpname),
        ignoredcollectionid, ignoredarchivesize));
    check_b(ar.path_readytoupload, "path_readytoupload cannot be null");
    const char *rootdir = cstr(ar.path_readytoupload);

    /* find dirty files */
    finder = sv_sync_finddirtyfiles_open(rootdir, cstr(grp->grpname));
    finder.db = db;
    finder.knownvaultid = knownvaultid;
    check(sv_sync_finddirtyfiles_find(&finder));

    /* sort the list
    1) want smaller files first since more likely for upload to succeed
    2) add determinism
    3) for test verification */
    bstrlist_sort(finder.sizes_and_files);
    printf("\n\n%d file(s) are already on the cloud (%f Mb)\n\n",
        finder.countclean, (double)finder.totalsizeclean / (1024.0 * 1024.0));
    printf("%d file(s) need to be uploaded (%f Mb)\n\n",
        finder.sizes_and_files->qty,
        (double)finder.totalsizedirty / (1024.0 * 1024.0));
    if (ask_user("Upload the files now?"))
    {
        for (int i = 0; i < finder.sizes_and_files->qty; i++)
        {
            check(sv_sync_uploadone(db, cstr(awscli), i,
                finder.sizes_and_files->qty, cstr(finder.rootdir),
                cstr(finder.groupname), blist_view(finder.sizes_and_files, i),
                vault, knownvaultid));
        }
    }

    alert("Sync complete.");

cleanup:
    ar_manager_close(&ar);
    sv_sync_finddirtyfiles_close(&finder);
    bdestroy(awscli);
    return currenterr;
}

check_result sv_sync_cloud(const sv_app *app, const sv_group *grp, svdb_db *db)
{
    sv_result currenterr = {};
    bstrlist *regions = bstrlist_open();
    bstrlist *names = bstrlist_open();
    bstrlist *awsnames = bstrlist_open();
    bstrlist *arns = bstrlist_open();
    sv_array ids = sv_array_open_u64();
    check(svdb_knownvaults_get(db, regions, names, awsnames, arns, &ids));

    int chosen = menu_choose("Please choose a Amazon Glacier vault.", names,
        NULL, "Create...", "(Back)");
    if (chosen == names->qty)
    {
        /* create a new vault */
        check(sv_sync_newvault_intro(db));
        /* recurse */
        check(sv_sync_cloud(app, grp, db));
    }
    else if (chosen < names->qty)
    {
        check(sv_sync_main(app, grp, db, blist_view(regions, chosen),
            blist_view(awsnames, chosen),
            sv_array_at64u(&ids, cast32s32u(chosen))));
    }

cleanup:
    bstrlist_close(regions);
    bstrlist_close(names);
    bstrlist_close(awsnames);
    bstrlist_close(arns);
    return currenterr;
}
