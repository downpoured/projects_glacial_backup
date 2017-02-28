
enum svdpeoperations
{
	svdpeoperations_none,
	svdpeoperations_backup,
	svdpeoperations_restore,
	svdpeoperations_trim,
	svdpeoperations_filehistory,
	svdpeoperations_totxt,
};

typedef struct svdp_backup {
	const SvdpGlobalConfig* cfg;
	const SvdpGroupConfig* groupcfg;
	bcdb* db;

	svdp_exclusions exclusions;
	svdp_archiver archiver;
	svdp_hashing hashing;
	SnpshtID cursnapshotid;
	SnpshtID prevsnapshotid;
	bool preview;

	/* saved from previous run as a perf optimization */
	int prev_dir_number;
	RowID prev_dir_id;
	bcstr path_copy;
} svdp_backup;

void svdp_backup_close(svdp_backup* self)
{
	if (self)
	{
		svdp_archiver_close(&self->archiver);
		svdp_exclusions_close(&self->exclusions);
		svdp_hashing_close(&self->hashing);
		bcstr_close(&self->path_copy);
		set_self_zero();
	}
}

CHECK_RESULT Result svdp_backup_getfilenameid(svdp_backup* self,
	int dirnumber,
	const char* filepath, Uint32 filepathlen,
	RowID* outdirid, RowID* outnameid)
{
	Result currenterr = {};
	Uint32 dirlen = 0, namelen = 0;
	char *dir = NULL, *name = NULL;
	bcstr_assignszlen(&self->path_copy, filepath, filepathlen);
	os_split_dir_len(bcstr_data_get(&self->path_copy),
		filepathlen, &dir, &dirlen, &name, &namelen);
	
	if (self->prev_dir_number != dirnumber)
	{
		check(svzpdb_getnameid(self->db,
			dir, dirlen,
			SvzpQryGetDirNameId, SvzpQryAddDirName,
			&self->prev_dir_id));

		self->prev_dir_number = dirnumber;
	}

	*outdirid = self->prev_dir_id;
	check(svzpdb_getnameid(self->db,
		name, namelen,
		SvzpQryGetFileNameId, SvzpQryAddFileName,
		outnameid));
	
cleanup:
	return currenterr;
}

CHECK_RESULT Result svdp_backup_one_file(void* context,
	int dirnumber,
	const char* filepath, Uint32 filepathlen,
	const byte* nativepath, Uint32 cbnativepathlen,
	Uint64 modtime, Uint64 filesize)
{
	Result currenterr = {};
	hash192bits contenthash = {};
	RowID dirid = 0, flnameid = 0, fileinfoid = 0;
	svdp_backup* self = (svdp_backup*)context;

	/* check if the user excluded this file. */
	enum EnumFileExtType type = FileExtTypeOther;
	if (svdp_exclusions_isexcluded(&self->exclusions, filepath, filepathlen, &type))
		goto cleanup;

	/* look for a file with the same path, modtime, filesize */
	bool found = false;
	check(svdp_backup_getfilenameid(self, dirnumber, filepath, filepathlen, &dirid, &flnameid));
	check(svzpdb_qrysamefileexists(self->db, self->cursnapshotid, dirid, flnameid, filesize, modtime, self->prevsnapshotid, &found));
	if (found)
		goto cleanup;

	/* for audio files, the filesize doesn't have to match, just the contenthash */
	SvzpEnumQry qry = type == FileExtTypeAudio ? 
		SvzpQrySameHashExists : SvzpQrySameHashAndSizeExists;

	/* look for a file with the same content hash*/
	Int32 archivelocation = 0;
	check(svdp_hashing_gethash(&self->hashing, self->db, nativepath, cbnativepathlen, &contenthash, type, &fileinfoid));
	check(svzpdb_qrysamecontenthash(self->db, qry, filesize, self->prevsnapshotid, &contenthash, &archivelocation));

	/* I guess this is a new file, add it file to archives*/
	if (archivelocation == 0)
	{
		check(svdp_archiver_addfile(&self->archiver, nativepath, cbnativepathlen, type, self->cursnapshotid, filesize, &archivelocation));
	}

	/* Insert new row */
	check_b(dirid > 0);
	check_b(flnameid > 0);
	check_b(fileinfoid > 0);
	check_b(archivelocation > 0);
	check(svzpdb_add(self->db, dirid, flnameid, filesize, modtime, archivelocation, self->cursnapshotid, &contenthash, fileinfoid));

cleanup:
	return currenterr;
}

CHECK_RESULT Result svdp_backup_run_addfiles(svdp_backup* self, int* outsymlinksskipped)
{
	Result currenterr = {};
	bcdb_txn txn = {};
	check(bcdb_txn_open(&txn, self->db));

	/* find the name of the previous run */
	check(svzpdb_getlastsnapshotid(self->db, &self->prevsnapshotid));

	/* get name of our run*/
	Uint64 timestamp = os_unixtime();
	check(svzpdb_addnewsnapshotid(self->db, timestamp, &self->cursnapshotid));

	/* add entries to TblMainTemporary*/
	const int maxdepth = 100;
	int totalsymlinksskipped = 0, symlinksskipped = 0;
	for (Uint32 i=0; i<self->groupcfg->arDirs.length; i++)
	{
		self->prev_dir_id = 0;
		self->prev_dir_number = INT32_MAX;

		const char* root = bcstrlist_viewat(&self->groupcfg->arDirs, i);
		check(os_recurse(self, root, &symlinksskipped, maxdepth, &svdp_backup_one_file));
		totalsymlinksskipped += symlinksskipped;
	}

	*outsymlinksskipped = totalsymlinksskipped;
	check(svdp_archiver_flush(&self->archiver));

	/* copy entries from TblMainTemporary to TblMain */
	//check(svzpdb_move_from_temp_to_main(self->db));
	//check(svzpdb_updatesnapshotid(self->db, self->cursnapshotid, svdp_archiver_count(&self->archiver)));

	if (self->preview)
		check(bcdb_txn_rollback(&txn, self->db));
	else
		check(bcdb_txn_commit(&txn, self->db));

cleanup:
	bcdb_txn_close(&txn, self->db);
	return currenterr;
}


CHECK_RESULT Result svdp_backup_open(svdp_backup* self, const SvdpGlobalConfig* cfg, const SvdpGroupConfig* groupcfg, bcdb* db, bool preview)
{
	Result currenterr = {};
	set_self_zero();
	self->cfg = cfg;
	self->groupcfg = groupcfg;
	self->db = db;
	self->preview = preview;
	self->archiver = CAST(svdp_archiver) {};
	self->hashing = CAST(svdp_hashing) {};
	self->exclusions = CAST(svdp_exclusions) {};
	self->path_copy = bcstr_open();

	check(svdp_archiver_open(&self->archiver, preview, cfg, groupcfg));
	check(svdp_exclusions_open(&self->exclusions, groupcfg));
	check(svdp_hashing_open(&self->hashing, groupcfg));
	check_b(groupcfg->arDirs.length > 0,
		"no directories specified. please change the config file to add dirs=/path/to/dir");

cleanup:
	return currenterr;
}

CHECK_RESULT Result svdp_backup_run(const SvdpGlobalConfig* cfg, const SvdpGroupConfig* groupcfg, bcdb* db, bool preview)
{
	Result currenterr = {};
	svdp_backup engine = {};
	bcstr dbpath = bcstr_openwith(db->path.s);


	int outsymlinksskipped = 0;
	check(svdp_backup_open(&engine, cfg, groupcfg, db, preview));
	check(svdp_backup_run_addfiles(&engine, &outsymlinksskipped));
	if (outsymlinksskipped > 0)
	{
		printf("Note: in this version of glacial_backup, symlinks are skipped.\n"
			"There were %d symlink(s) encountered and skipped.", outsymlinksskipped);
	}

	/* make an archive with the database file, too! */
	bcdb_close(db);
	db = NULL;
	char *dbfile = NULL, *dbdir = NULL;
	os_split_dir(bcstr_data_get(&dbpath), &dbdir, &dbfile);

	// add dbdir to an archive
	check_fatal(false, "not yet implemented");

cleanup:
	bcstr_close(&dbpath);
	svdp_backup_close(&engine);
	return currenterr;
}

CHECK_RESULT Result svdp_runoperationgroup(
	enum svdpeoperations op, 
	bool preview,
	const SvdpGlobalConfig* config, 
	const SvdpGroupConfig* group)
{
	Result currenterr = {};
	bcstr dbpath = bcstr_open();
	bcdb db = {};
	
	/* start database */
	bcstr_append(&dbpath, config->locationForLocalIndex.s, pathsep, group->groupname.s);
	check_b(os_create_dir(dbpath.s));
	bcstr_append(&dbpath, pathsep, "index.db");
	check(bcdb_open(&db, dbpath.s));

	if (op == svdpeoperations_backup)
	{
		check(svdp_backup_run(config, group, &db, preview));
	}
	else
	{
		check_b(0);
	}

cleanup:
	bcstr_close(&dbpath);
	bcdb_close(&db);
	return currenterr;
}

void svdp_configs_in_editor()
{
	Result currenterr = {};
	bcstr configpath = bcstr_open();
	check(svdpglobalconfig_getcreateconfigfile(&configpath));
	os_open_text_editor(configpath.s);

cleanup:
	bcstr_close(&configpath);
	print_error_if_ocurred("", &currenterr);
}

void svdp_viewlocation(bool logs_or_staging)
{
	Result currenterr = {};
	SvdpGlobalConfig config = svdpglobalconfig_open();
	bcstr path = os_get_create_appdatadir();
	if (logs_or_staging)
	{
		bcstr_append(&path, pathsep, "logs");
	}
	else
	{
		check(svdpglobalconfig_get(&config));
		bcstr_assign(&path, config.locationForFilesReadyToUpload.s);
	}

	os_open_dir_ui(path.s);

cleanup:
	svdpglobalconfig_close(&config);
	bcstr_close(&path);
	print_error_if_ocurred("", &currenterr);
}

int svdp_runoperation_groupname(enum svdpeoperations op, bool preview, const char* groupname)
{
	Result currenterr = {};
	const SvdpGroupConfig* group = NULL;
	bool anygroups = false;

	/* parse configs*/
	SvdpGlobalConfig config = svdpglobalconfig_open();
	check(svdpglobalconfig_get(&config));

	/* ask user which group to use */
	if (groupname)
	{
		findgroupbyname(&config, groupname, &group);
	}
	else
	{
		ask_which_group(&config, &anygroups, &group);
	}

	if (!groupname && !anygroups)
	{
		puts("It doesn't look like any backup groups are defined.\n"
			"As an example, you can rename [groupDemonstration] to [group1] in the configuration file,\n"
			"and specify your directories to begin.");
		if (ask_user_yesno("Open config file to make this change?"))
		{
			svdp_configs_in_editor();
		}
	}
	else if (group != NULL)
	{
		check(svdp_runoperationgroup(op, preview, &config, group));
	}

cleanup:
	bc_log_flush(&g_log);
	svdpglobalconfig_close(&config);
	int retcode = (group == NULL) ? -1 : currenterr.errortag;
	print_error_if_ocurred("", &currenterr);
	return retcode;
}

int svdp_runoperation(enum svdpeoperations op, bool preview)
{
	return svdp_runoperation_groupname(op, preview, NULL);
}
