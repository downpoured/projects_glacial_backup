#pragma once
// utilities for archiver.

inline StdString GetGlacialBackupReadyToUploadDirectory()
{
	return StdString(GetDirectoryCacheHome(), pathsep, "downpoured_glacial_backup_ready");
}

inline StdString GetGlacialBackupArchiverDirectory()
{
	return StdString(GetDirectoryConfigHome(), pathsep, "downpoured_glacial_backup");
}

inline StdString GetConfigFileLocation()
{
	return StdString(GetGlacialBackupArchiverDirectory(), pathsep, "config.ini");
}

class BackupConfiguration
{
	StdString _groupname;
	StdString _pass;
	StdString _encryption;
	bool _separatelyTrackTaggingChangesInAudioFiles = false;
	int _considerRemovingDeletedFilesAfterThisManyWeeks = INT_MAX;
	int _targetArchiveSizeInKb = 64 * 1024;
	std::vector<StdString> _arDirs;
	std::vector<StdString> _arExcludedDirs;
	std::vector<StdString> _arExcludedExtensions;

public:
	BackupConfiguration(const char* groupname) : _groupname(groupname) {}
	const char* GetGroupName() const { return _groupname; }
	const char* GetPass() const { return _pass; }
	const char* GetEncryption() const { return _encryption; }
	bool GetSeparatelyTrackTaggingChangesInAudioFiles() const { return _separatelyTrackTaggingChangesInAudioFiles; }
	int GetConsiderRemovingDeletedFilesAfterThisManyWeeks() const { return _considerRemovingDeletedFilesAfterThisManyWeeks; }
	int GetTargetArchiveSizeInKb() const { return _targetArchiveSizeInKb; }
	const std::vector<StdString>* ViewDirs() const { return &_arDirs; }
	const std::vector<StdString>* ViewExcludedDirs() const { return &_arExcludedDirs; }
	const std::vector<StdString>* ViewExcludedExtensions() const { return &_arExcludedExtensions; }

	friend class BackupConfigurationGeneral;
};

// configs are stored in an .ini-like format
class BackupConfigurationGeneral : private NonCopyable
{
	StdString _pathFfmpeg;
	StdString _path7zip;
	StdString _location_for_local_index;
	StdString _location_for_files_ready_to_upload;
	std::vector<BackupConfiguration> _groups;
	int _unknownOptionsSeen = 0;

public:
	BackupConfigurationGeneral()
	{
		_location_for_local_index = StdString(GetGlacialBackupArchiverDirectory(), pathsep, "local_index");
		_location_for_files_ready_to_upload = StdString(GetGlacialBackupReadyToUploadDirectory(), pathsep, "ready_to_upload");
	}
	const char* GetPathFfmpeg() const { return _pathFfmpeg; }
	const char* GetPath7zip() const { return _path7zip; }
	const char* GetLocation_for_local_index() const { return _location_for_local_index; }
	const char* GetLocation_for_files_ready_to_upload() const { return _location_for_files_ready_to_upload; }
	const std::vector<BackupConfiguration>& ViewGroups() const { return _groups; }
	
	static void WriteDemoConfigFile(const char* filename)
	{
		std::ofstream stm(filename, std::ifstream::out);
		stm << newline << "# Welcome to glacial_backup." << newline << newline;
		stm << "location_for_local_index=" << StdString(GetGlacialBackupArchiverDirectory(), pathsep, "local_index") << newline;
		stm << "location_for_files_ready_to_upload=" << StdString(GetGlacialBackupReadyToUploadDirectory(), pathsep, "ready_to_upload") << newline << newline;
		
#if __linux__
		stm << "7zip=/bin/pleaseProvidePathTo/7z" << newline;
		stm << "ffmpeg=/bin/optionallyProvidePathTo/ffmpeg" << newline;
		stm << newline << newline << "[group1]" << newline;
		stm << "dirs=/home/firstDirectoryInGroup|/home/otherDirectoryInGroup" << newline;
#else
		stm << "7zip=C:\\pleaseProvidePathTo\\7z.exe" << newline;
		stm << "ffmpeg=C:\\optionallyProvidePathTo\\ffmpeg.exe" << newline;
		stm << newline << newline << "[group1]" << newline;
		stm << "dirs=C:\\firstDirectoryInGroup|C:\\otherDirectoryInGroup" << newline;
#endif
		stm << "excludedDirs=" << newline;
		stm << "excludedExtensions=.tmp|.pyc" << newline;
		stm << "separatelyTrackTaggingChangesInAudioFiles=0" << newline;
		stm << "considerRemovingDeletedFilesAfterThisManyWeeks=4" << newline;
		stm << "pass=Password1" << newline;
		stm << "encryption=AES-256" << newline;
		stm << "targetArchiveSizeInMb=64" << newline << newline;
	}

	bool ParseLineFromConfigFile(const char* szLine, const char* szSubstringAfterEquals)
	{
		if (StrStartsWith(szLine, "ffmpeg="))
		{
			_pathFfmpeg = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "7zip="))
		{
			_path7zip = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "location_for_local_index="))
		{
			_location_for_local_index = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "location_for_files_ready_to_upload="))
		{
			_location_for_files_ready_to_upload = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "dirs="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			_groups[_groups.size() - 1]._arDirs = StrUtilSplitString(szSubstringAfterEquals, '|');
		}
		else if (StrStartsWith(szLine, "excludedDirs="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			_groups[_groups.size() - 1]._arExcludedDirs = StrUtilSplitString(szSubstringAfterEquals, '|');
		}
		else if (StrStartsWith(szLine, "excludedExtensions="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			_groups[_groups.size() - 1]._arExcludedExtensions = StrUtilSplitString(szSubstringAfterEquals, '|');
		}
		else if (StrStartsWith(szLine, "pass="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			_groups[_groups.size() - 1]._pass = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "encryption="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			_groups[_groups.size() - 1]._encryption = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "separatelyTrackTaggingChangesInAudioFiles="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			int val = 0;
			StrUtilSetIntFromString(szSubstringAfterEquals, szLine, val);
			_groups[_groups.size() - 1]._separatelyTrackTaggingChangesInAudioFiles = val != 0;
		}
		else if (StrStartsWith(szLine, "considerRemovingDeletedFilesAfterThisManyWeeks="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			StrUtilSetIntFromString(szSubstringAfterEquals, szLine, _groups[_groups.size() - 1]._considerRemovingDeletedFilesAfterThisManyWeeks);
		}
		else if (StrStartsWith(szLine, "targetArchiveSizeInMb="))
		{
			AssertThrowMsg(0, _groups.size() > 0, "invalid config file, cannot set this outside of a group:", szLine);
			int val = 0;
			StrUtilSetIntFromString(szSubstringAfterEquals, szLine, val);
			_groups[_groups.size() - 1]._targetArchiveSizeInKb = val * 1024;
		}
		else
		{
			return false;
		}

		return true;
	}

	void ParseFromConfigFile(const char* szConfig)
	{
		AssertThrow(0, OS_FileExists(szConfig));
		auto contents = OS_ReadTextfile(szConfig);
		std::vector<StdString> lines = StrUtilSplitString(contents, '\n');

		for (auto it = lines.begin(); it != lines.end(); ++it)
		{
			const char* szSubstringAfterEquals = strstr(it->c_str(), "=");
			if (szSubstringAfterEquals) // move the pointer one past the = character
			{
				szSubstringAfterEquals += 1;
			}

			if (szSubstringAfterEquals == it->c_str() + it->Length())
			{
				// ignore the case where an empty string is provided, e.g. option=
			}
			else if (szSubstringAfterEquals && ParseLineFromConfigFile(it->c_str(), szSubstringAfterEquals))
			{
				// success
			}
			else if (!szSubstringAfterEquals && StrStartsWith(it->c_str(), "[group"))
			{
				auto groupNameAndBracket = std::string(*it).substr(strlen("["));
				AssertThrowMsg(0, groupNameAndBracket.size() > 1, "invalid config file, group name is too short:", it->c_str());

				auto groupName = StdString(groupNameAndBracket.substr(0, groupNameAndBracket.size() - 1));
				auto allNamesUnique = std::all_of(_groups.begin(), _groups.end(), [&](const BackupConfiguration& b) {
					return b._groupname != groupName; });
				AssertThrowMsg(0, allNamesUnique, "duplicate group name", it->c_str());
				_groups.push_back(BackupConfiguration(groupName));
			}
			else if (it->Length() > 0 && it->c_str()[0] != '#' && it->c_str()[0] != ';' && 
				!std::all_of(it->c_str(), it->c_str() + it->Length(), [](char c){ return ::isspace(c); }))
			{
				MvLog("Ignoring unknown config option ", it->c_str());
				_unknownOptionsSeen++;
			}
		}
	}

	friend class BackupConfigurationGeneralTests;
};

// we can save time by not compressing file extensions that indicate already-compressed data
// pack the extension into an integer and look up the integer in a map, faster than dozens of if statements
class LookupFileExtensions
{
public:
	enum class Categories : Int32 {
		UserChoseToDiscard = -1,
		Unknown = 0,
		CompressesVeryPoorly = 1,
		CompressesPoorly = 2,
		CompressesWell = 3,
		CompressesVeryWell = 4,
	};

private:
	typedef Uint64 ExtensionCharactersAsInteger;
	std::unordered_map<ExtensionCharactersAsInteger, Categories> _map;
	static const int c_countCharsInInt = 5; 
	void AddExtensionInternal(const char* sz, Categories classification)
	{
		AssertThrowMsg(0, sz[0] == '.', "extension must be in the form .txt, but got", sz);
		AssertThrowMsg(0, strlen(sz) <= c_countCharsInInt, "extensions must be at most 5 characters, i.e. .jpeg is the longest length", sz);
		ExtensionCharactersAsInteger code = GetIntegerFromCharacters(sz, strlen(sz));
		AssertThrowMsg(0, code != 0, "extension is not valid", sz);
		_map.emplace(code, classification);
	}

	Inline ExtensionCharactersAsInteger GetIntegerFromCharacters(const char* sz, size_t len)
	{
		if (len <= c_countCharsInInt)
			return 0;

		ExtensionCharactersAsInteger out = 0;
		int lenRead = 0;
		for (const char*ptr = sz + len - 1; *ptr != '.'; --ptr)
		{
			out |= (out << 8) | ((ExtensionCharactersAsInteger)*ptr);
			if (++lenRead > c_countCharsInInt)
				return 0;
		}

		return out;
	}

public:
	void AddExtension(const char* sz, Categories classification)
	{
		std::string sUpper(sz);
		for (auto it = sUpper.begin(); it != sUpper.end(); ++it)
			*it = toupper(*it);
		AddExtensionInternal(sUpper.c_str(), classification);
		std::string sLower(sz);
		for (auto it = sUpper.begin(); it != sUpper.end(); ++it)
			*it = tolower(*it);
		AddExtensionInternal(sLower.c_str(), classification);
	}

	Categories Lookup(const char*sz, size_t len)
	{
		auto code = GetIntegerFromCharacters(sz, len);
		auto found = _map.find(code);
		return found == _map.end() ? Categories::Unknown : found->second;
	}

	friend class LookupFileExtensionsTests;
};

inline void ProvideDefaultFileExtensions(LookupFileExtensions& objExt)
{
	objExt.AddExtension(".aif", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".aifc", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".aiff", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".bmp", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".c", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".cc", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".cpp", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".cs", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".css", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".csv", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".cxx", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".wmf", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".emf", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".exe", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".sdf", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".pch", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".pdb", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".ilk", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".ipch", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".doc", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".h", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".htm", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".html", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".hxx", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".i", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".idl", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".inf", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".ini", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".java", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".js", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".mht", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".mid", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".pdn", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".psd", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".ppt", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".py", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".xls", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".reg", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".svg", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".tif", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".ttf", LookupFileExtensions::Categories::CompressesWell);
	objExt.AddExtension(".txt", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".xml", LookupFileExtensions::Categories::CompressesVeryWell);
	objExt.AddExtension(".7z", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".ace", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".arj", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".bz2", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".cab", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".gz", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".jpeg", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".jpg", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".lha", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".lzh", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".mp3", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".rar", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".taz", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".tgz", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".xz", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".z", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".zip", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".png", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".m4a", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".mp4", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".ogg", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".flac", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".webp", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".webm", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".avi", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".mov", LookupFileExtensions::Categories::CompressesVeryPoorly);
	objExt.AddExtension(".flv", LookupFileExtensions::Categories::CompressesVeryPoorly);
}
