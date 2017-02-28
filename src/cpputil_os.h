#pragma once

#ifdef _WIN32

inline FILE* fopenwrapper(const char* filename, const char* mode)
{
	FILE* f = 0;
	auto er = fopen_s(&f, filename, mode);
	if (er && f) {
		if (f) fclose(f);
		return 0;
	}

	return er ? 0 : f;
}

inline FILE* fopenwrapper(const oschartype* filename, const char* mode)
{
	std::string stdmode(mode);
	FILE* f = 0;
	auto er = _wfopen_s(&f, filename, std::wstring(stdmode.begin(), stdmode.end()).c_str());
	if (er && f) {
		if (f) fclose(f);
		return 0;
	}

	return er ? 0 : f;
}

Uint64 OS_GetUnixTime()
{
	FILETIME fileTime;
	::GetSystemTimeAsFileTime(&fileTime);
	Uint64 dt = MakeUint64(fileTime.dwHighDateTime, fileTime.dwLowDateTime);
	return dt / 10000000ULL - (11644473600ULL);
}

inline bool OS_CopyFileAndOverwriteExisting(const char* s1, const char* s2)
{
	return !!::CopyFileA(s1, s2, FALSE /* fail if exists */);
}

inline bool OS_CopyFileAndOverwriteExisting(const oschartype* s1, const oschartype* s2)
{
	return !!::CopyFileW(s1, s2, FALSE /* fail if exists */);
}

inline bool OS_RenameFileWithoutOverwriteExisting(const char* s1, const char* s2)
{
	return !!::MoveFileExA(s1, s2, MOVEFILE_WRITE_THROUGH);
}

inline StdString OS_GetDirectoryOfCurrentModule()
{
	char buf[MAX_PATH] = { 0 };
	::GetModuleFileNameA(nullptr, buf, countof(buf));
	int pos = (int)strlen(buf) - 1;
	while (pos > 0)
	{
		if (buf[pos] == L'\\') { buf[pos] = L'\0'; break; }
		pos--;
	}
	return StdString(buf);
}

inline StdString UIApproximateNarrowString(const WCHAR* wz)
{
	std::wstring wzTmp(wz);
	auto str = std::string(wzTmp.begin(), wzTmp.end());
	for (size_t i = 0; i < str.size(); i++)
		if (str[i] == '\0')
			str[i] = '?';
	return str;
}

inline std::wstring UIApproximateWideString(const char* sz)
{
	std::string szTmp(sz);
	return std::wstring(szTmp.begin(), szTmp.end());
}

inline std::vector<StdString> OS_ListFilesInDirectory(
	const char* dir, bool includeFiles, bool includeDirectories = false, const char* pattern = "*", bool bSort = false)
{
	std::vector<StdString> ret;
	WIN32_FIND_DATAA findFileData = {};

	StdString searchSpec(dir, pathsep, pattern);
	HANDLE hFind(::FindFirstFileA(searchSpec, &findFileData));
	RunWhenLeavingScope cleanupFind([&] { ::FindClose(hFind); });
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do {
			if (includeDirectories && (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				!StrEqual(".", findFileData.cFileName) && !StrEqual("..", findFileData.cFileName))
				ret.push_back(StdString(dir, pathsep, findFileData.cFileName));
			else if (includeFiles && !(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				ret.push_back(StdString(dir, pathsep, findFileData.cFileName));

		} while (FindNextFileA(hFind, &findFileData) != 0);
	}

	if (bSort)
	{
		std::sort(ret.begin(), ret.end());
	}

	return ret;
}

struct StackBasedWideString
{
	static const int cBufferSize = MAX_PATH+1;
	oschartype _buffer[cBufferSize];
	size_t _pos;
};

void StackBasedWideString_MakeImpl(StackBasedWideString* obj)
{
	obj->_buffer[0] = '\0';
	obj->_pos = 0;
}

#define StackBasedWideString_Make(objname) \
	StackBasedWideString objname##stack; \
	StackBasedWideString* objname = & ( objname##stack ); \
	StackBasedWideString_MakeImpl(objname)

void StackBasedWideString_Free(StackBasedWideString* obj) {}

bool StackBasedWideString_AppendStrKnownLength(StackBasedWideString* obj, const oschartype* str, size_t len)
{
	AssertThrowMsg(0, false, "not yet implemented");
	return true;
}

void StackBasedWideString_AppendChar(StackBasedWideString* obj, oschartype ch)
{
	AssertThrowMsg(0, false, "not yet implemented");
}

WCHAR *wstpncpy_withnul(WCHAR *dst, const WCHAR *src, size_t nOrig)
{
	WCHAR *q = dst;
	const WCHAR *p = src;
	WCHAR ch;
	size_t n = nOrig;

	while (n--) {
		*q = ch = *p++;
		if (!ch)
			break;
		q++;
	}
	dst[nOrig - 1] = L'\0';
	return q;
}

// the callback can return false to stop iteration
typedef std::function<bool(const oschartype* /*wzFullpath*/,
	size_t /*lenwzFullpath*/, Uint64 /*modtime*/, Uint64 /*filesize*/)> FnRecurseFilesCallback;

void RecurseFilesInternal(const oschartype* wzPath, FnRecurseFilesCallback callback,
	const std::unordered_set<std::wstring>& disallowedDirectories)
{
	// create a string of the form "path\\*"
	StackBasedWideString_Make(strBuffer);
	WCHAR wzBuffer[MAX_PATH + 1] = L"";
	WCHAR* last = wstpncpy_withnul(wzBuffer, wzPath, countof(wzBuffer));
	AssertThrowMsg(0, last - &wzBuffer[0] < countof(wzBuffer), "path too long");
	*(last) = '\\';
	*(last+1) = '*';
	*(last+2) = '\0';
	
	WIN32_FIND_DATAW findFileData = { 0 };
	HANDLE hFind(FindFirstFileW(wzBuffer, &findFileData));
	RunWhenLeavingScope cleanupFind([&] { ::FindClose(hFind); });
	auto disallowedDirectoriesSize = disallowedDirectories.size();
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (StrEqual(L".", findFileData.cFileName) || StrEqual(L"..", findFileData.cFileName))
				continue;
	
			// get the full path. memcpy is >20% faster than sprintf here. wstpncpy_withnul saves calls to strlen.
			WCHAR wzBufferFullpath[MAX_PATH + 1] = L"";
			memcpy(&wzBufferFullpath[0], &wzBuffer[0], sizeof(WCHAR)*((last-wzBuffer )+ 2));
			WCHAR* last2 = wstpncpy_withnul(&wzBufferFullpath[(last - wzBuffer) + 1], 
				findFileData.cFileName, countof(wzBufferFullpath) - ((last - wzBuffer) + 1));
			AssertThrowMsg(last2 - &wzBufferFullpath[0] < countof(wzBufferFullpath), "path too long");
			size_t cchFullPath = last2 - &wzBufferFullpath[0];
	
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
					Throw(0, "Directories that contains symlinks are not supported", UIApproximateNarrowString(wzBufferFullpath));
	
				if (disallowedDirectoriesSize == 0 || disallowedDirectories.find(wzBufferFullpath) == disallowedDirectories.end())
					RecurseFilesInternal(wzBufferFullpath, callback, disallowedDirectories);
			}
			else if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) // skip all symlinks!
			{
				auto modtime = MakeUint64(findFileData.ftLastWriteTime.dwHighDateTime, findFileData.ftLastWriteTime.dwLowDateTime);
				auto filesize = MakeUint64(findFileData.nFileSizeHigh, findFileData.nFileSizeLow);
				if (!callback(wzBufferFullpath, cchFullPath, modtime, filesize))
					break;
			}
		} while (FindNextFileW(hFind, &findFileData) != 0);
	}

	StackBasedWideString_Free(strBuffer);
}

inline void RecurseFiles(const char* dir, FnRecurseFilesCallback callback,
	const std::vector<StdString>& disallowedDirectories = std::vector<StdString>())
{
	AssertThrowMsg(0, !StrStartsWith(dir, "\\\\"), "we make MAX_PATH assumptions.");
	std::unordered_set<std::wstring> setDisallowedDirectories;
	for (auto it = disallowedDirectories.begin(); it != disallowedDirectories.end(); ++it)
	{
		std::string tmp(*it);
		setDisallowedDirectories.emplace(std::wstring(tmp.begin(), tmp.end()));
	}
	std::string tmpDir(dir);
	RecurseFilesInternal(std::wstring(tmpDir.begin(), tmpDir.end()).c_str(), callback, setDisallowedDirectories);
}

inline StdString GetDirectoryConfigHome()
{
	char buffer[MAX_PATH + 1] = "";
	HRESULT hr = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, buffer);
	if (!SUCCEEDED(hr))
	{
		Throw(0, "SHGetFolderPathA failed with code ", StrFromInt(hr));
	}

	return buffer;
}

inline StdString GetDirectoryCacheHome()
{
	return GetDirectoryConfigHome();
}

class PerfTimer
{
	LARGE_INTEGER m_start;
public:
	PerfTimer()
	{
		m_start.QuadPart = 0;
		::QueryPerformanceCounter(&m_start);
	}
	double stop()
	{
		LARGE_INTEGER nstop;
		::QueryPerformanceCounter(&nstop);
		UINT64 ndiff = nstop.QuadPart - m_start.QuadPart;
		LARGE_INTEGER freq;
		::QueryPerformanceFrequency(&freq);
		return (ndiff) / ((double)freq.QuadPart);
	}
};


inline bool OS_Remove(const oschartype* s)
{
	return DeleteFileW(s) || GetLastError()==ERROR_FILE_NOT_FOUND;
}

inline void PrintMemoryUsageInfo()
{
	PROCESS_MEMORY_COUNTERS_EX obj = {};
	obj.cb = sizeof(obj);
	AssertThrowMsg(0, ::GetProcessMemoryInfo(
		::GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&obj, obj.cb), "could not get memoryinfo");

	printf("\tPageFaultCount: %d (%fMb)\n", obj.PageFaultCount, obj.PageFaultCount / (1024.0*1024.0));
	printf("\tPeakWorkingSetSize: %d (%fMb)\n", obj.PeakWorkingSetSize, obj.PeakWorkingSetSize / (1024.0*1024.0));
	printf("\tWorkingSetSize: %d (%fMb)\n", obj.WorkingSetSize, obj.WorkingSetSize / (1024.0*1024.0));
	printf("\tPagefileUsage: %d (%fMb)\n", obj.PagefileUsage, obj.PagefileUsage / (1024.0*1024.0));
	printf("\tPeakPagefileUsage: %d (%fMb)\n", obj.PeakPagefileUsage, obj.PeakPagefileUsage / (1024.0*1024.0));
	printf("\tPrivateUsage: %d (%fMb)\n", obj.PrivateUsage, obj.PrivateUsage / (1024.0*1024.0));
}

void MvStringAppendOsString(const oschartype* wz, MvString* str)
{
	while (*wz != 0)
	{
		str->AddChar((int)*wz > (int)MAXCHAR ? '?' : (char)*wz);
		wz++;
	}
}

#endif

inline StdString GetLatestExistingFilename(const char* sz, int maxAttempts)
{
	auto lastDot = strrchr(sz, '.');
	AssertThrow(0, lastDot != nullptr);
	std::string sBeforeExtension(sz, lastDot);
	for (int i = 0; i < maxAttempts; i++)
	{
		char buf[4096] = "";
		sprintf_s(buf, "%s.%d%s", sBeforeExtension.c_str(), i + 1, lastDot);
		if (!OS_FileExists(buf))
		{
			char bufPrev[4096] = "";
			sprintf_s(bufPrev, "%s.%d%s", sBeforeExtension.c_str(), i, lastDot);
			return i == 0 ? StdString(buf) : StdString(bufPrev);
		}
	}

	Throw(0, "GetLatestExistingFilename failed", StrFromInt(maxAttempts));
}

inline StdString GetLatestNonExistingFilename(const char* sz, int maxAttempts)
{
	auto lastDot = strrchr(sz, '.');
	AssertThrow(0, lastDot != nullptr);
	std::string sBeforeExtension(sz, lastDot);
	for (int i = 0; i < maxAttempts; i++)
	{
		char buf[4096] = "";
		sprintf_s(buf, "%s.%d%s", sBeforeExtension.c_str(), i + 1, lastDot);
		if (!OS_FileExists(buf))
		{
			return StdString(buf);
		}
	}

	Throw(0, "GetNonExistingFilename failed", StrFromInt(maxAttempts));
}

bool OS_ClearFilesInDirectory(const char* dir)
{
	auto files = OS_ListFilesInDirectory(dir, true /*includeFiles*/, false /*includeDirectories*/, "*");
	for (auto it = files.begin(); it != files.end(); ++it)
	{
		if (!OS_Remove(*it))
		{
			return false;
		}
	}

	return true;
}

class OS_PreventMultipleInstances : private NonCopyable
{
	CStyleFile _f;
	StdString getLocation()
	{
		auto dirHome = GetDirectoryCacheHome();
		if (OS_DirectoryExists(dirHome) && OS_CreateDirectory(StdString(dirHome, pathsep, "projects_glacial_backup")))
		{
			return StdString(dirHome, pathsep, "projects_glacial_backup", pathsep, "mv_prevent_multiple_instances.tmp");
		}

		Throw(0, "could not get directory");
	}

public:
	bool AcquireAndCheckIfOtherInstanceIsRunning(const char* szName)
	{
		StdString location(getLocation());
		_f.Attach(fopenwrapper(location.c_str(), "wb"));
		return _f ? false : true;
	}
};
