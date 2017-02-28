#pragma once

typedef Result(*FnConfigReaderCallback)(void* context, const char* line, const char* value);
CHECK_RESULT Result read_config_file(const char* filename, void* context, FnConfigReaderCallback fn)
{
	Result currenterr = {};
	bcstr contents = bcstr_open();
	bcstrlist lines = bcstrlist_open();
	bcstr currentline = bcstr_open();

	/* read entire file and split by newline */
	check(bcfile_readfile(filename, &contents, "r"));
	bcstrlist_add_str_split(&lines, contents.s, '\n');

	for (Uint32 i = 0; i < lines.length; i++)
	{
		bcstrlist_at(&lines, i, &currentline);
		bcstr_trimwhitespace(&currentline);
		const char* equals = strchr(currentline.s, '=');
		size_t len = strlen(currentline.s);
		if (currentline.s[0] == ';' || currentline.s[0] == '#')
		{
			/* skip comments */
		}
		else if (currentline.s[0] == '[' && currentline.s[len-1]==']')
		{
			/* send over a section */
			check(fn(context, currentline.s, currentline.s+1));
		}
		else if (equals && !*(equals+1))
		{
			/* ignore if no value provided, e.g. option= */
		}
		else if (equals)
		{
			/* send a normal pair, option=value */
			check(fn(context, currentline.s, equals+1));
		}
		else
		{
			logwritef("unrecognized config. should be form option=value but got %s",
				currentline.s);
		}
	}

cleanup:
	bcstrlist_close(&lines);
	bcstr_close(&currentline);
	bcstr_close(&contents);
	return currenterr;
}

typedef struct SvdpGroupConfig
{
	bcstr groupname;
	bcstr pass;
	bcstr encryption;
	int fSeparatelyTrackTaggingChangesInAudioFiles;
	int targetArchiveSizeInMb;
	bcstrlist arDirs;
	bcstrlist arExcludedDirs;
	bcstrlist arExcludedExtensions;
} SvdpGroupConfig;

SvdpGroupConfig svdpgroup_open()
{
	SvdpGroupConfig ret = {};
	ret.groupname = bcstr_open();
	ret.pass = bcstr_open();
	ret.encryption = bcstr_open();
	ret.fSeparatelyTrackTaggingChangesInAudioFiles = 0;
	ret.targetArchiveSizeInMb = 64;
	ret.arDirs = bcstrlist_open();
	ret.arExcludedDirs = bcstrlist_open();
	ret.arExcludedExtensions = bcstrlist_open();
	return ret;
}

void svdpgroup_close(SvdpGroupConfig* self)
{
	if (self)
	{
		bcstr_close(&self->groupname);
		bcstr_close(&self->pass);
		bcstr_close(&self->encryption);
		bcstrlist_close(&self->arDirs);
		bcstrlist_close(&self->arExcludedDirs);
		bcstrlist_close(&self->arExcludedExtensions);
		set_self_zero();
	}
}

typedef struct SvdpGlobalConfig
{
	bcstr pathFfmpeg;
	bcstr path7zip;
	bcstr locationForLocalIndex;
	bcstr locationForFilesReadyToUpload;
	bcarray groups;
} SvdpGlobalConfig;

SvdpGlobalConfig svdpglobalconfig_open()
{
	SvdpGlobalConfig ret = {};
	ret.pathFfmpeg = bcstr_open();
	ret.path7zip = bcstr_open();
	ret.locationForLocalIndex = bcstr_open();
	ret.locationForFilesReadyToUpload = bcstr_open();
	ret.groups = bcarray_open(sizeof(SvdpGroupConfig), 0);
	return ret;
}

void svdpglobalconfig_close(SvdpGlobalConfig* self)
{
	if (self)
	{
		bcstr_close(&self->pathFfmpeg);
		bcstr_close(&self->path7zip);
		bcstr_close(&self->locationForLocalIndex);
		bcstr_close(&self->locationForFilesReadyToUpload);
		for (Uint32 i=0; i<self->groups.length; i++)
			svdpgroup_close((SvdpGroupConfig*)bcarray_at(&self->groups, i));
		
		bcarray_close(&self->groups);
	}
}

bool config_nothing_set(const SvdpGlobalConfig* config)
{
	if (config->groups.length == 0)
	{
		return true;
	}

	const SvdpGroupConfig* firstgroup = 
		(const SvdpGroupConfig*)bcarray_atconst(&config->groups, 0);

	if (config->groups.length == 1 &&
		firstgroup->arDirs.length == 0 ||
		s_endswith(bcstrlist_viewat(&firstgroup->arDirs, 0), pathsep "a_directory"))
	{
		return true;
	}

	return false;
}

CHECK_RESULT Result svdpglobalconfig_writedemo(const char* path, const char* dir1, const char* dir2)
{
	Result currenterr = {};
	bcfile file = {};
	check(bcfile_open(&file, path, "w"));

	fputs("# Welcome to GlacialBackup.\n\n\n"
		"# To start, look for the dirs= line below and"
		"add the directories you want to back up.\n", file.file);

#if __linux__
	fputs("# When ready, save changes and exit.\n\n", file.file);
	fputs("\n\n[group1]\n"
		"dirs=/home/a_directory|/home/another_directory\n"
		"excludedDirs=/home/exclude_this_directory\n"
		"excludedExtensions=tmp|pyc\n"
		"separatelyTrackTaggingChangesInAudioFiles=0\n"
		"pass=(ask)\n"
		"encryption=gpg\n"
		"targetArchiveSizeInMb=64\n", file.file);
#elif _WIN32
	fputs("# Then, save and exit Notepad to proceed.\n\n", file.file);
	
	fputs("7zip=C:\\pleaseProvidePathTo\\7z.exe\n"
		"ffmpeg=C:\\optionallyProvidePathTo\\ffmpeg.exe\n"
		"\n\n[group1]\n"
		"dirs=C:\\a_directory|C:\\another_directory\n"
		"excludedDirs=C:\\exclude_this_directory\n"
		"excludedExtensions=tmp|pyc\n"
		"separatelyTrackTaggingChangesInAudioFiles=0\n"
		"pass=(ask)\n"
		"encryption=AES-256\n"
		"targetArchiveSizeInMb=64\n", file.file);
#endif

	fprintf(file.file, "\n\n\nlocationForLocalIndex=%s\n", dir1);
	fprintf(file.file, "locationForFilesReadyToUpload=%s\n\n", dir2);

cleanup:
	bcfile_close(&file);
	return currenterr;
}

CHECK_RESULT Result svdpglobalconfig_callback(void* vself, const char* line, const char* value)
{
	Result currenterr = {};
	bcstr tmpgroupname = {};
	SvdpGlobalConfig* self = (SvdpGlobalConfig*) vself;
	SvdpGroupConfig* group = self->groups.length ? 
		(SvdpGroupConfig*) bcarray_at(&self->groups, self->groups.length-1) : NULL;

	if (s_startswith(line, "ffmpeg="))
	{
		bcstr_assign(&self->pathFfmpeg, value);
	}
	else if (s_startswith(line, "7zip="))
	{
		bcstr_assign(&self->path7zip, value);
	}
	else if (s_startswith(line, "locationForLocalIndex="))
	{
		bcstr_assign(&self->locationForLocalIndex, value);
	}
	else if (s_startswith(line, "locationForFilesReadyToUpload="))
	{
		bcstr_assign(&self->locationForFilesReadyToUpload, value);
	}
	else if (s_startswith(line, "dirs="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		bcstrlist_add_str_split(&group->arDirs, value, '|');
	}
	else if (s_startswith(line, "excludedDirs="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		bcstrlist_add_str_split(&group->arExcludedDirs, value, '|');
	}
	else if (s_startswith(line, "excludedExtensions="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		bcstrlist_add_str_split(&group->arExcludedExtensions, value, '|');
	}
	else if (s_startswith(line, "pass="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		bcstr_assign(&group->pass, value);
	}
	else if (s_startswith(line, "encryption="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		bcstr_assign(&group->encryption, value);
	}
	else if (s_startswith(line, "separatelyTrackTaggingChangesInAudioFiles="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		check_b(s_intfromstring(value, &group->fSeparatelyTrackTaggingChangesInAudioFiles),
			"this parameter requires a number, but got, ", line);
	}
	else if (s_startswith(line, "targetArchiveSizeInMb="))
	{
		check_b(group, "need to be in a group to set this, ", line);
		check_b(s_intfromstring(value, &group->targetArchiveSizeInMb),
			"this parameter requires a number, but got, ", line);
	}
	else if (s_startswith(line, "[group"))
	{
		tmpgroupname = bcstr_open();
		bcstr_assign(&tmpgroupname, line + strlen("["));
		if (tmpgroupname.s[bcstr_length(&tmpgroupname) - 1] == ']')
			bcstr_truncate(&tmpgroupname, bcstr_length(&tmpgroupname) - 1);

		check_b(bcstr_length(&tmpgroupname) > strlen("group"),
			"group name is too short, ", line);

		/* don't allow duplicate group names*/
		for (Uint32 i = 0; i < self->groups.length; i++)
		{
			check_b(strcasecmp(
				((SvdpGroupConfig*)bcarray_at(&self->groups, i))->groupname.s, 
				tmpgroupname.s)!=0, "duplicate group name, ", tmpgroupname.s);
		}
		
		SvdpGroupConfig group = svdpgroup_open();
		bcstr_assign(&group.groupname, tmpgroupname.s);
		bcarray_add(&self->groups, (byte*)&group, 1);
	}
	else
	{
		logwritef("unrecognized parameter in config file, got %s.", line);
	}

cleanup:
	bcstr_close(&tmpgroupname);
	return currenterr;
}

CHECK_RESULT Result svdpglobalconfig_read(SvdpGlobalConfig* self, const char* filename)
{
	return read_config_file(filename, self, &svdpglobalconfig_callback);
}

CHECK_RESULT Result svdpglobalconfig_getcreateconfigfile(bcstr* path)
{
	Result currenterr = {};
	bcstr_clear(path);
	bcstr appdatadir = os_get_create_appdatadir();
	bcstr dir_default_index = bcstr_open();
	bcstr dir_default_uploads = bcstr_open();

	check_b(bcstr_length(&appdatadir),
		"could not get our data directory %s", appdatadir.s);
	bcstr_append(path, appdatadir.s, pathsep, "settings.ini");

	if (!os_file_exists(path->s))
	{
		bcstr_append(&dir_default_index, appdatadir.s, pathsep, "index");
		bcstr_append(&dir_default_uploads, appdatadir.s, pathsep, "ready_for_upload");
		check(svdpglobalconfig_writedemo(path->s, dir_default_index.s, dir_default_uploads.s));
	}

cleanup:
	bcstr_close(&appdatadir);
	bcstr_close(&dir_default_index);
	bcstr_close(&dir_default_uploads);
	return currenterr;
}

CHECK_RESULT Result svdpglobalconfig_get(SvdpGlobalConfig* out)
{
	Result currenterr = {};
	bcstr configpath = bcstr_open();
	check(svdpglobalconfig_getcreateconfigfile(&configpath));

#define badconfig "Invalid configuration file,"
		"you must go to 'Configure locations...' to address this.";
	currenterr = svdpglobalconfig_read(out, configpath.s);
	if (currenterr.errortag)
		printf("%s, %s", badconfig, currenterr.errormsg ? currenterr.errormsg : "");
	check_b(currenterr.errortag == 0);

	/* ensure directories are absolute paths */
	check_b(os_isabspath(out->locationForFilesReadyToUpload.s),
		badconfig " locationForFilesReadyToUpload must be absolute path but got",
		out->locationForFilesReadyToUpload.s);
	check_b(os_isabspath(out->locationForLocalIndex.s),
		badconfig " locationForLocalIndex must be absolute path but got",
		out->locationForLocalIndex.s);

	/* ensure important directories can be created */
	check_b(os_create_dir(out->locationForFilesReadyToUpload.s),
		badconfig " could not access locationForFilesReadyToUpload at", 
		out->locationForFilesReadyToUpload.s);
	check_b(os_create_dir(out->locationForLocalIndex.s),
		badconfig " could not access locationForLocalIndex at",
		out->locationForLocalIndex.s);
#undef badconfig

cleanup:
	bcstr_close(&configpath);
	return currenterr;
}

void findgroupbyname(SvdpGlobalConfig* config, const char* name, const SvdpGroupConfig** out)
{
	*out = NULL;
	if (name)
	{
		for (Uint32 i = 0; i < config->groups.length; i++)
		{
			const SvdpGroupConfig* grp = (const SvdpGroupConfig*)bcarray_atconst(&config->groups, i);
			if (!s_equal(grp->groupname.s, name))
			{
				*out = grp;
				break;
			}
		}
	}
}

void ask_which_group(SvdpGlobalConfig* config, bool* anygroups, const SvdpGroupConfig** out)
{
	/* are there any actual groups besides the demo one? */
	*out = NULL;
	*anygroups = true;

	if (config->groups.length == 0 || config_nothing_set(config))
	{
		*anygroups = false;
	}
	else if (config->groups.length == 1)
	{
		*out = (SvdpGroupConfig*)bcarray_at(&config->groups, 0);
	}
	else
	{
		puts("0) cancel");
		for (Uint32 i = 0; i<config->groups.length; i++)
		{
			SvdpGroupConfig* group = (SvdpGroupConfig*)bcarray_at(&config->groups, i);
			printf("%d) %s", i + 1, group->groupname.s);
		}

		/* if there's only one choice, skip asking the user. */
		Uint32 choice = ask_user_int("please select a backup group",
			0, config->groups.length);
		if (choice > 0)
		{
			*out = (SvdpGroupConfig*)bcarray_at(&config->groups, choice-1);
		}
	}
}

enum EnumFileExtType {
	FileExtTypeOther = 0,
	FileExtTypeNotCompressable = 1,
	FileExtTypeAudio = 2,
	FileExtTypeExclude = 3
};

static const Uint32 extensionsAudio[] = {
	chars_to_uint32('\0', 'a', 'a', 'c'),
	chars_to_uint32('\0', 'a', 'p', 'e'),
	chars_to_uint32('\0', 'm', 'p', '3'),
	chars_to_uint32('\0', 'm', '4', 'a'),
	chars_to_uint32('\0', 'm', 'p', '4'),
	chars_to_uint32('\0', 'o', 'g', 'g'),
	chars_to_uint32('f', 'l', 'a', 'c')
};

static const Uint32 extensionsNotVeryCompressable[] = {
	chars_to_uint32('\0', '\0', '7', 'z'),
	chars_to_uint32('\0', '\0', 'g', 'z'),
	chars_to_uint32('\0', '\0', 'x', 'z'),
	chars_to_uint32('\0', 'a', 'c', 'e'),
	chars_to_uint32('\0', 'a', 'r', 'j'),
	chars_to_uint32('\0', 'b', 'z', '2'),
	chars_to_uint32('\0', 'c', 'a', 'b'),
	chars_to_uint32('\0', 'j', 'p', 'g'),
	chars_to_uint32('\0', 'l', 'h', 'a'),
	chars_to_uint32('\0', 'l', 'z', 'h'),
	chars_to_uint32('\0', 'r', 'a', 'r'),
	chars_to_uint32('\0', 't', 'a', 'z'),
	chars_to_uint32('\0', 't', 'g', 'z'),
	chars_to_uint32('\0', 'z', 'i', 'p'),
	chars_to_uint32('\0', 'p', 'n', 'g'),
	chars_to_uint32('\0', 'a', 'v', 'i'),
	chars_to_uint32('\0', 'm', 'o', 'v'),
	chars_to_uint32('\0', 'f', 'l', 'v'),
	chars_to_uint32('\0', 'w', 'm', 'a'),
	chars_to_uint32('j', 'p', 'e', 'g'),
	chars_to_uint32('w', 'e', 'b', 'p'),
	chars_to_uint32('w', 'e', 'b', 'm'),
	chars_to_uint32('a', 'l', 'a', 'c')
};

INLINE Uint32 extension_into_uint32(const char* s, Uint32 len)
{
	const char* bufLast = s + len;
	if (len < 7)
		return 0;
	else if (*(bufLast - 3) == '.')
		return chars_to_uint32('\0', '\0', *(bufLast - 2), *(bufLast - 1));
	else if (*(bufLast - 4) == '.')
		return chars_to_uint32('\0', *(bufLast - 3), *(bufLast - 2), *(bufLast - 1));
	else if (*(bufLast - 5) == '.')
		return chars_to_uint32(*(bufLast - 4), *(bufLast - 3), *(bufLast - 2), *(bufLast - 1));
	else
		return 0;
}

CHECK_RESULT Result svdpgetfileextensiontype_add(const bcstrlist* arr, Uint32* customtypes, Uint32 customtypeslen)
{
	Result currenterr = {};
	memset(&customtypes[0], 0, customtypeslen * sizeof(customtypes[0]));
	check_b(arr->length <= customtypeslen,
		"we currently only support this number of extensions.", "", customtypeslen);

	for (Uint32 i=0; i<arr->length; i++)
	{
		const char* ext = bcstrlist_viewat(arr, i);
		check_b(!strchr(ext, '.') && !strchr(ext, '*'),
			"write just the extension, e.g. write 'tmp' and not '*.tmp' , got", ext);

		bcstr mockfilename = bcstr_openwith("aaaaaaaa.", bcstrlist_viewat(arr, i));
		Uint32 extuint32 = extension_into_uint32(mockfilename.s, strlen32(mockfilename.s));
		bcstr_close(&mockfilename);
		check_b(extuint32,
			"extension must be 2, 3, or 4 chars, got", ext);

		customtypes[i] = extuint32;
	}

cleanup:
	return currenterr;
}

enum EnumFileExtType svdpgetfileextensiontype_get(const char* s, Uint32 len, const Uint32* customtypes, Uint32 customtypeslen)
{
	Uint32 extuint32 = extension_into_uint32(s, len);
	if (!extuint32)
	{
		return FileExtTypeOther;
	}

	/* the ones we set are left at zero, which is nice since we know that won't match extuint32*/
	for (Uint32 i=0; i<customtypeslen; i++)
	{
		if (customtypes[i] == extuint32) 
			return FileExtTypeExclude;
	}
	for (size_t i=0; i<countof(extensionsNotVeryCompressable); i++)
	{
		if (extensionsNotVeryCompressable[i] == extuint32) 
			return FileExtTypeNotCompressable;
	}
	for (size_t i=0; i<countof(extensionsAudio); i++)
	{
		if (extensionsAudio[i] == extuint32) 
			return FileExtTypeAudio;
	}

	return FileExtTypeOther;
}

#define c_maxexcludedfileext 4
typedef struct svdp_exclusions
{
	bcstrlist excluded_dirs;
	bool prev_was_excluded_dir;
	Uint32 excluded_ext[c_maxexcludedfileext];
	bool audiometadata_enabled;
} svdp_exclusions;

CHECK_RESULT Result svdp_exclusions_open(svdp_exclusions* self, const SvdpGroupConfig* group)
{
	Result currenterr = {};
	set_self_zero();
	check(svdpgetfileextensiontype_add(&group->arExcludedExtensions, self->excluded_ext, countof(self->excluded_ext)));
	self->audiometadata_enabled = group->fSeparatelyTrackTaggingChangesInAudioFiles != 0;

	/* add an ending slash to excluded dirs, so that they don't match the wrong files */
	for (Uint32 i=0; i<group->arExcludedDirs.length; i++)
	{
		bcstrlist_pushback(&self->excluded_dirs,
			bcstrlist_viewat(&group->arExcludedDirs, i));
		bcstr_addchar(bcstrlist_viewstr(&self->excluded_dirs, i), pathsep[0]);
	}

cleanup:
	return currenterr;
}

bool svdp_exclusions_isexcluded(svdp_exclusions* self, const char* filepath, Uint32 filepathlen, enum EnumFileExtType* outtype)
{
	/* is this in an excluded directory? */
	bool isexcluded = false;
	for (Uint32 i=0; i<self->excluded_dirs.length; i++)
	{
		/* does it begin with the string? */
		bcstr* excluded = bcstrlist_viewstr(&self->excluded_dirs, i);
		if (s_startswithlen(filepath, filepathlen,
			excluded->s, bcstr_length(excluded)))
		{
			isexcluded = true;
			break;
		}
	}

	if (isexcluded && isexcluded != self->prev_was_excluded_dir)
	{
		logwritef("skipping file %s as it is in an excluded directory",
			filepath);
	}

	/* is this an excluded extension? */
	*outtype = svdpgetfileextensiontype_get(filepath, filepathlen,
			self->excluded_ext, countof(self->excluded_ext));
		
	if (*outtype == FileExtTypeAudio && !self->audiometadata_enabled)
		*outtype = FileExtTypeNotCompressable;

	self->prev_was_excluded_dir = isexcluded;
	return isexcluded || *outtype == FileExtTypeExclude;
}

void svdp_exclusions_close(svdp_exclusions* self)
{
	if (self)
	{
		bcstrlist_close(&self->excluded_dirs);
		set_self_zero();
	}
}

CHECK_RESULT Result svdp_check_other_instances()
{
	Result currenterr = {};
	bcstr path = os_get_create_appdatadir();
	check_b(bcstr_length(&path),
		"could not get our data directory, ", path.s);
	bcstr_append(&path, pathsep, "running.pid");

	int retcode = 0;
	check_b(
		!os_are_other_instances_running(path.s, &retcode),
		"looks like another instance is running because file is locked ",
		path.s, retcode);

cleanup:
	bcstr_close(&path);
	return currenterr;
}

CHECK_RESULT Result svdp_startgloballogging(bc_log* log)
{
	Result currenterr = {};
	bcstr path = os_get_create_appdatadir();
	check_b(bcstr_length(&path),
		"could not get our data directory ", path.s);
	bcstr_append(&path, pathsep, "logs");
	check_b(os_create_dir(path.s),
		"could create log directory ", path.s);

	if (!bcstr_length(&log->filename_template))
	{
		bcstr_append(&path, "log");
		check(bc_log_open(log, path.s));
	}

cleanup:
	bcstr_close(&path);
	return currenterr;
}
