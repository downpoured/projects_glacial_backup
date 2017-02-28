#pragma once
// high-level operations

#include <unordered_map>
#include <unordered_set>

#if 0
namespace PerfExperiments
{
	template<class T>
	class SimpleBuffer : private NonCopyable
	{
		byte* _ptr = nullptr;
		size_t _maxElements = 0;

	public:
		explicit SimpleBuffer(int count, bool fillzeros)
		{
			AssertThrow(0, count * sizeof(T) > 0);
			_maxElements = count;
			if (fillzeros)
				_ptr = (byte*)calloc(count, sizeof(T));
			else
				_ptr = (byte*)malloc(count * sizeof(T));
		}
		size_t GetMaxNumberOfElements() { return _maxElements; }
		T* Get() { return _ptr; }
		~SimpleBuffer() { free(_ptr); }
	};

	class BufferedFileWriter
	{
		FILE* _file = 0;
		const Uint64 bufsize = 1024 * 1024 * 16;
		size_t _pos = 0;
		byte* _buffer = 0;
		void flush()
		{
			::fwrite(_buffer, 1, _pos, _file);
			_pos = 0;
		}

	public:
		BufferedFileWriter(const wchar_t* filepath, const wchar_t* spec)
		{
			_pos = 0;
			_buffer = new byte[CastUint32(bufsize)];
			_file = 0;
			_wfopen_s(&_file, filepath, spec);
			if (!_file)
			{
				throw std::runtime_error("failed to open BufferedFileWriter file");
			}
		}
		~BufferedFileWriter()
		{
			close();
		}
		void fwrite(const void * ptr, size_t size)
		{
			if (_pos + size < bufsize)
			{
				memcpy(&_buffer[_pos], ptr, size);
				_pos += size;
			}
			else
			{
				// handles both the 1) our buffer full and 2) massive-data-sent-in cases
				flush();
				::fwrite(ptr, size, 1, _file);
			}
		}
		int fseek64(Int64 offset, int origin)
		{
			flush();
			return ::_fseeki64(_file, offset, origin);
		}
		Int64 ftell64()
		{
			flush();
			return ::_ftelli64(_file);
		}
		int fputc(int character)
		{
			_buffer[_pos] = character;
			_pos++;
			if (_pos >= bufsize)
			{
				flush();
			}
			return character;
		}
		void close()
		{
			flush();
			if (_file)
			{
				fclose(_file);
			}

			delete[] _buffer;
			_buffer = 0;
			_file = 0;
		}
	};

	class SimpleFileReader
	{
	public:
		FILE* _file = 0;
		SimpleFileReader(const wchar_t* filepath, const wchar_t* spec)
		{
			_wfopen_s(&_file, filepath, spec);
		}
		~SimpleFileReader()
		{
			close();
		}
		void close()
		{
			if (_file)
			{
				fclose(_file);
			}
		}
	};

	struct SmallStruct { byte data[40]; };
	void GetPathStatistics()
	{
		Uint64 totalPathCharacters = 0, 
			totalFilesizes = 0,
			totalSeen = 0, 
			totalPathCharactersOnlyFilenames = 0;

		const char* szRoot = "/path/to/music";
		auto strlenmusic = strlen(szRoot);
		PerfTimer timeIt;
		std::unordered_set<std::string> directories;
		RecurseFiles(szRoot, [&](
			const char* fullpath, size_t lenFullpath, const wchar_t*, size_t, 
			Uint64 modtime, Uint64 filesize) {
			auto len = strlen(fullpath);
			AssertThrow(0, len == lenFullpath);
			AssertThrow(0, len > strlenmusic);
			totalSeen++;
			totalFilesizes += filesize;
			totalPathCharacters += (len - strlenmusic);
			const char* division = strrchr(fullpath, pathsep[0]);
			AssertThrow(0, division != nullptr);
			directories.emplace(std::string(fullpath, division));
			totalPathCharactersOnlyFilenames += lenFullpath - (division - fullpath);
			return true;
		});

		printf("\ncompleted recursion in %f seconds\n", timeIt.stop());
		printf("totalSeen=%lld totalSize=%lld Gb\n detailSize=%lld\n", totalSeen, totalFilesizes / (1024 * 1024), totalFilesizes);
		printf("total characters=%lld avg characters=%lld\n", totalPathCharacters, totalPathCharacters / totalSeen);
		Uint64 totalPathCharactersOnlyDirs = 0;
		for (auto it = directories.begin(); it != directories.end(); ++it)
			totalPathCharactersOnlyDirs += it->size();
		printf("total DIR characters=%lld total NAME=%lld both=%lld\n", totalPathCharactersOnlyDirs, totalPathCharactersOnlyFilenames, totalPathCharactersOnlyDirs + totalPathCharactersOnlyFilenames);
	}
}
#endif

#if 0
namespace ExperimentWorkWithWindowsArchiveBit
{
	struct FileSeen
	{
		FILETIME ftCreationTime;
		FILETIME ftLastWriteTime;
		DWORD    nFileSizeHigh;
		DWORD    nFileSizeLow;
	};

	struct FilesSeenPersistToDiskHeader
	{
		INT32 productVersion;
		wchar_t productName[64];
		wchar_t groupName[128];
	};

	struct FilesSeenPersistToDiskRecord
	{
		Uint64 id;
		FileSeen fileseen;
	};

	class FilesSeenInGroup
	{
	public:
		std::unordered_map<Uint64, FileSeen> map;
		bool dirty = false;
	private:
		void PersistToDiskInternal(const wchar_t* wzFilename, const wchar_t* szGroupname) const
		{
			generalLog.WriteLine(L"PersistToDiskInternal for group ", szGroupname);
			generalLog.WriteLine(L"writing to disk at ", wzFilename);
			BufferedFileWriter writer(wzFilename, L"wb");
			FilesSeenPersistToDiskHeader header = { 0 };
			header.productVersion = 1;
			wcscpy_s(header.productName, L"downpoured_glacial_backup");
			wcscpy_s(header.groupName, szGroupname);
			writer.fwrite(&header, sizeof(header));
			for (std::unordered_map<Uint64, FileSeen>::const_iterator it = map.begin(); it != map.end(); ++it)
			{
				static_assert(sizeof(it->second) == sizeof(FileSeen), "checking struct size");
				FilesSeenPersistToDiskRecord record;
				record.id = it->first;
				record.fileseen = it->second;
				writer.fwrite(&record, sizeof(record));
			}
		}

		void LoadFromDiskInternal(const wchar_t* wzFilename)
		{
			generalLog.WriteLine(L"reading from disk at ", wzFilename);
			SimpleFileReader reader(wzFilename, L"rb");
			FilesSeenPersistToDiskHeader header = { 0 };
			size_t nRead = fread(&header, sizeof(header), 1, reader.file);
			if (nRead != 1)
				throw MvException("db file corrupt? too small");
			if (1 != header.productVersion)
				throw MvException("db file, unsupported product version");
			if (!WStringsEqual(L"downpoured_glacial_backup", header.productName))
				throw MvException("db file corrupt? unexpected product name");

			generalLog.WriteLine(L"reading for group ", header.groupName);
			FilesSeenPersistToDiskRecord buffer[1024 * 4];
			while (true)
			{
				size_t nRead = fread(&buffer[0], sizeof(FilesSeenPersistToDiskRecord), _countof(buffer), reader.file);
				for (size_t i = 0; i < nRead; i++)
				{
					map[buffer[i].id] = buffer[i].fileseen;
				}
				if (nRead != _countof(buffer))
					break;
			}
		}
	public:
		void LoadFromDisk(const wchar_t* wzLogPath, const wchar_t* szGroupname)
		{
			std::wstring wzFilename = wzLogPath;
			wzFilename += L".dat";
			if (!OS_WFileExists(wzFilename.c_str()))
			{
				generalLog.WriteLine(L"did not see database for group, so starting fresh.", szGroupname);
			}
			else
			{
				LoadFromDiskInternal(wzFilename.c_str(), generalLog);
			}
		}

		void PersistToDisk(const wchar_t* wzLogPath, const wchar_t* szGroupname) const
		{
			if (!dirty)
				return;

			std::wstring wzFilename = wzLogPath;
			wzFilename += L".dat";
			std::wstring tmpName(wzFilename);
			tmpName += L".temp";
			PersistToDiskInternal(tmpName.c_str(), szGroupname, generalLog);
			BOOL result = ::CopyFile(tmpName.c_str(), wzFilename.c_str(), FALSE/*bFailIfExists*/);
			if (!result)
				throw MvException("replacing db file failed");
		}
	};

	struct RunArchiveContext
	{
		FilesSeenInGroup* _filesSeen;
		const wchar_t* _wzLog;
		const std::vector<std::wstring>* _dirs;
		bool _disregardTaggingChangesInAudioFiles;
		RunArchiveContext(FilesSeenInGroup& filesSeen, const wchar_t* wzLog, const std::vector<std::wstring>& dirs, bool disregardTaggingChangesInAudioFiles)
		{
			_filesSeen = &filesSeen;
			_wzLog = wzLog;
			_dirs = &dirs;
			_disregardTaggingChangesInAudioFiles = disregardTaggingChangesInAudioFiles;
		}
	};

	void SetArchiveBit(const wchar_t* file, DWORD flags, bool fset)
	{
		if (!!(flags & FILE_ATTRIBUTE_ARCHIVE) != fset)
		{
			if (fset)
				flags |= FILE_ATTRIBUTE_ARCHIVE;
			else
				flags &= ~((DWORD)FILE_ATTRIBUTE_ARCHIVE);
			BOOL ret = ::SetFileAttributes(file, flags);
			if (!ret)
				throw MvException("could not SetArchiveBit for file");
		}
	}

	__forceinline bool CmpFiletime(const FILETIME& f1, const FILETIME& f2)
	{
		return memcmp(&f1, &f2, sizeof(f1)) == 0;
	}

	bool RunArchiveCallback(void* voidContext, const wchar_t* parentdir, const wchar_t* filename)
	{
		RunArchiveContext* context = (RunArchiveContext*)voidContext;

		wchar_t wzBufferFullpath[MAX_PATH + 1] = { 0 };
		swprintf_s(wzBufferFullpath, _countof(wzBufferFullpath), L"%s\\%s", parentdir, filename);
		HANDLE hFile = CreateFile(wzBufferFullpath,
			0 /* open for metadata only*/,
			FILE_SHARE_READ,
			NULL,                  // default security
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);

		BY_HANDLE_FILE_INFORMATION info = { 0 };
		BOOL ret = GetFileInformationByHandle(hFile, &info);
		CloseHandle(hFile);

		if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			// skipping link files since we don't need to handle them
			SetArchiveBit(wzBufferFullpath, info.dwFileAttributes, false);
			return true;
		}

		Uint64 fileid = (((Uint64)info.nFileIndexHigh) << 32) + (Uint64)info.nFileIndexLow;
		auto found = context->_filesSeen->map.find(fileid);
		if (found == context->_filesSeen->map.end())
		{
			// not found in the map... we'll have to add it to the map
			SetArchiveBit(wzBufferFullpath, info.dwFileAttributes, true);
			FileSeen record;
			record.ftCreationTime = info.ftCreationTime;
			record.ftLastWriteTime = info.ftLastWriteTime;
			record.nFileSizeHigh = info.nFileSizeHigh;
			record.nFileSizeLow = info.nFileSizeLow;
			context->_filesSeen->dirty = true;
			context->_filesSeen->map[fileid] = record;
		}
		else
		{
			bool creationTimesMatch = CmpFiletime(found->second.ftCreationTime, info.ftCreationTime);
			bool doWeNeedToBackup = (context->) ? creationTimesMatch :
				creationTimesMatch &&
				CmpFiletime(found->second.ftLastWriteTime, info.ftLastWriteTime) &&
				found->second.nFileSizeLow == info.nFileSizeLow &&
				found->second.nFileSizeLow == info.nFileSizeHigh;

			if (doWeNeedToBackup)
			{
				AssertThrowMsg(0, false, "not yet implemented");

			}
		}
		return true;
	}

	void RunArchive(RunArchiveContext& context)
	{
		for (std::vector<std::wstring>::const_iterator it = context._dirs->begin(); it != context._dirs->end(); ++it)
		{
			if (!OS_WDirectoryExists(it->c_str()))
				throw MvException("specified directory not found");

			WalkThroughFiles_Impl(it->c_str(), &context, &RunArchiveCallback);
		}
	}
}
#endif
