#pragma once

inline StdString GetGlacialBackupReadyToUploadDirectory()
{
	return StdString(GetDirectoryCacheHome(), pathsep, "projects_glacial_backup");
}

inline StdString GetGlacialBackupArchiverDirectory()
{
	return StdString(GetDirectoryConfigHome(), pathsep, "projects_glacial_backup");
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
	int _considerRemovingDeletedFilesAfterThisManyWeeks = MAXINT;
	int _targetArchiveSizeInKb = 64*1024;
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
	StdString _locationForLocalIndex;
	StdString _locationForFilesReadyToUpload;
	std::vector<BackupConfiguration> _groups;
	bool _strictModifiedTimeComparison = true;
	int _testing_countUnrecognizedParameters = 0;

public:
	BackupConfigurationGeneral()
	{
		_locationForLocalIndex = StdString(
			GetGlacialBackupArchiverDirectory(), pathsep, "local_index");
		_locationForFilesReadyToUpload = StdString(
			GetGlacialBackupReadyToUploadDirectory(), pathsep, "ready_to_upload");
	}
	const char* GetPathFfmpeg() const { return _pathFfmpeg; }
	const char* GetPath7zip() const { return _path7zip; }
	const char* GetLocationForLocalIndex() const { return _locationForLocalIndex; }
	const char* GetLocationForFilesReadyToUpload() const { return _locationForFilesReadyToUpload; }
	const std::vector<BackupConfiguration>& ViewGroups() const { return _groups; }
	
	static void WriteDemoConfigFile(const char* filename)
	{
		std::ofstream stm(filename, std::ifstream::out | std::ifstream::binary);
		stm << newline << "# Welcome to projects_glacial_backup." << newline << newline;
		stm << "locationForLocalIndex=" << 
			StdString(GetGlacialBackupArchiverDirectory(), pathsep, "local_index") << newline;
		stm << "locationForFilesReadyToUpload=" << 
			StdString(GetGlacialBackupReadyToUploadDirectory(), pathsep, "ready_to_upload") << newline << newline;
		stm << "strictModifiedTimeComparison=1" << newline;
		
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
		stm << "excludedExtensions=tmp|pyc" << newline;
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
		else if (StrStartsWith(szLine, "locationForLocalIndex="))
		{
			_locationForLocalIndex = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "locationForFilesReadyToUpload="))
		{
			_locationForFilesReadyToUpload = szSubstringAfterEquals;
		}
		else if (StrStartsWith(szLine, "strictModifiedTimeComparison="))
		{
			int val = 0;
			StrUtilSetIntFromString(szSubstringAfterEquals, szLine, val);
			_strictModifiedTimeComparison = val != 0;
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
			StrUtilSetIntFromString(
				szSubstringAfterEquals, szLine, 
				_groups[_groups.size() - 1]._considerRemovingDeletedFilesAfterThisManyWeeks);
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
		auto contents = OS_ReadFile(szConfig, false /*binary*/);
		std::vector<StdString> lines = StrUtilSplitString(contents, '\n');

		for (auto it = lines.begin(); it != lines.end(); ++it)
		{
			const char* szSubstringAfterEquals = strstr(it->c_str(), "=");
			if (szSubstringAfterEquals) // move the pointer one past the = character
				szSubstringAfterEquals += 1;

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
				AssertThrowMsg(
					0, groupNameAndBracket.size() > 1, "invalid config file, group name is too short:", it->c_str());

				auto groupName = StdString(groupNameAndBracket.substr(0, groupNameAndBracket.size() - 1));
				AssertThrowMsg(0, std::all_of(_groups.begin(), _groups.end(), [&](const BackupConfiguration& b)
				{
					return b._groupname != groupName;  
				}), "duplicate group name", it->c_str());
				_groups.push_back(BackupConfiguration(groupName));
			}
			else if (it->Length() > 0 && it->c_str()[0] != '#' && it->c_str()[0] != ';' && 
				!std::all_of(it->c_str(), it->c_str() + it->Length(), [](char c){return ::isspace(c); }))
			{
				MvLog("Ignoring unknown config option ", it->c_str());
				_testing_countUnrecognizedParameters++;
			}
		}
	}

	friend class BackupConfigurationGeneralTests;
};


class ClassifyFileExtension
{
	static const int cNumberSupportedUserExtensions = 5;
	Uint32 packedUserExts[cNumberSupportedUserExtensions];
public:
	enum class ClassifyFileExtensionType : Uint32 { 
		Other = 0,
		NotVeryCompressable = 1,
		ContainsAudioMetadata = 2,
		UserSpecified = 3
	};

	ClassifyFileExtension(const std::vector<StdString>& userExts)
	{
		memset(&packedUserExts[0], 0, sizeof(packedUserExts));
		AssertThrowMsg(0, userExts.size() <= cNumberSupportedUserExtensions, 
			"we currently only support ", 
			StrFromInt(cNumberSupportedUserExtensions), " user-defined extensions to exclude.");

		for (int i = 0; i < userExts.size(); i++)
		{
			AssertThrowMsg(0, !strchr(userExts[i].c_str(), '.') && !strchr(userExts[i].c_str(), '*'), 
				"specify just the extension, instead of \".tmp\" write just \"tmp\"");

			if (userExts[i].Length() == 2)
			{
				packedUserExts[i] = PackCharactersIntoUint32(
					'\0', '\0', userExts[i].c_str()[0], userExts[i].c_str()[1]);
			}
			else if (userExts[i].Length() == 3)
			{
				packedUserExts[i] = PackCharactersIntoUint32(
					'\0', userExts[i].c_str()[0], userExts[i].c_str()[1], userExts[i].c_str()[2]);
			}
			else if (userExts[i].Length() == 4)
			{
				packedUserExts[i] = PackCharactersIntoUint32(
					userExts[i].c_str()[0], userExts[i].c_str()[1], userExts[i].c_str()[2], userExts[i].c_str()[3]);
			}
			else
			{
				Throw(0, "we only support extensions of 2, 3, or 4 chars");
			}
		}
	}

	Inline ClassifyFileExtensionType GetExt(const char* sz, size_t slen)
	{
		const char* bufLast = sz + slen;
		
		Uint32 packed = 0;
		if (slen < 7)
		{
			return ClassifyFileExtensionType::Other;
		}
		else if (*(bufLast - 3) == '.')
		{

			packed = PackCharactersIntoUint32('\0', '\0', *(bufLast - 2), *(bufLast - 1));

			if (IsUserExtension(packed)) return ClassifyFileExtensionType::UserSpecified;
			if (packed == PackCharactersIntoUint32('\0', '\0', '7', 'z')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', '\0', 'g', 'z')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', '\0', 'x', 'z')) return ClassifyFileExtensionType::NotVeryCompressable;
		}
		else if (*(bufLast - 4) == '.')
		{
			packed = PackCharactersIntoUint32('\0', *(bufLast - 3), *(bufLast - 2), *(bufLast - 1));

			if (IsUserExtension(packed)) return ClassifyFileExtensionType::UserSpecified;
			if (packed == PackCharactersIntoUint32('\0', 'a', 'a', 'c')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('\0', 'a', 'c', 'e')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'a', 'p', 'e')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('\0', 'a', 'r', 'j')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'b', 'z', '2')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'c', 'a', 'b')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'j', 'p', 'g')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'l', 'h', 'a')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'l', 'z', 'h')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'm', 'p', '3')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('\0', 'r', 'a', 'r')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 't', 'a', 'z')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 't', 'g', 'z')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'z', 'i', 'p')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'p', 'n', 'g')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'm', '4', 'a')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('\0', 'm', 'p', '4')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('\0', 'o', 'g', 'g')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('\0', 'a', 'v', 'i')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'm', 'o', 'v')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'f', 'l', 'v')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('\0', 'w', 'm', 'a')) return ClassifyFileExtensionType::NotVeryCompressable;
		}
		else if (*(bufLast - 5) == '.')
		{
			packed = PackCharactersIntoUint32(*(bufLast - 4), *(bufLast - 3), *(bufLast - 2), *(bufLast - 1));

			if (IsUserExtension(packed)) return ClassifyFileExtensionType::UserSpecified;
			if (packed == PackCharactersIntoUint32('j', 'p', 'e', 'g')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('f', 'l', 'a', 'c')) return ClassifyFileExtensionType::ContainsAudioMetadata;
			if (packed == PackCharactersIntoUint32('w', 'e', 'b', 'p')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('w', 'e', 'b', 'm')) return ClassifyFileExtensionType::NotVeryCompressable;
			if (packed == PackCharactersIntoUint32('a', 'l', 'a', 'c')) return ClassifyFileExtensionType::NotVeryCompressable;
		}

		return ClassifyFileExtensionType::Other;
	}

	Inline bool IsUserExtension(Uint32 packed)
	{
		return (packed == packedUserExts[0] || packed == packedUserExts[1] || packed == packedUserExts[2] ||
			packed == packedUserExts[3] || packed == packedUserExts[4]);
	}

	friend class ClassifyFileExtensionTests;
};

struct MvArchiveLocation;
struct RemovedArchives
{
	void Init() {}
	bool isRemovedArchive(const MvArchiveLocation* location) { return false;  }
};

