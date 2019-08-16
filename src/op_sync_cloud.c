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

check_result sv_sync_cloud(
    const sv_app *app, const sv_group *grp, svdb_db *db)
{
    app;
    grp;
    db;
    return OK;
}

