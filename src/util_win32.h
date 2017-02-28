#pragma once

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
	// do not add MOVEFILE_REPLACE_EXISTING
	return !!::MoveFileExA(s1, s2, MOVEFILE_WRITE_THROUGH);
}

inline Uint64 OS_GetFileSizeWin(const char* szFilename)
{
	BOOL success;
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;

	success = ::GetFileAttributesExA(szFilename, GetFileExInfoStandard, (void*)&fileInfo);
	if (!success)
		return 0;

	return MakeUint64(fileInfo.nFileSizeHigh, fileInfo.nFileSizeLow);
}

inline StdString OS_GetDirectoryOfCurrentModule()
{
	char buf[MAX_PATH] = { 0 };
	::GetModuleFileNameA(nullptr, buf, _countof(buf));
	int pos = (int)strlen(buf) - 1;
	while (pos > 0)
	{
		if (buf[pos] == L'\\') { buf[pos] = L'\0'; break; }
		pos--;
	}
	return StdString(buf);
}

class OS_PreventMultipleInstances : private NonCopyable
{
	HANDLE _handle = INVALID_HANDLE_VALUE;
public:
	bool AcquireAndCheckIfOtherInstanceIsRunning(const char* szName)
	{
		// note to check GetLastError() even if a valid handle is returned
		_handle = CreateMutexA(NULL, FALSE, szName);
		return (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED);
	}

	~OS_PreventMultipleInstances()
	{
		if (_handle != INVALID_HANDLE_VALUE)
		{
			ReleaseMutex(_handle);
			CloseHandle(_handle);
		}
	}
};

class FindHandleCloser : private NonCopyable
{
	HANDLE _handle = INVALID_HANDLE_VALUE;
public:
	explicit FindHandleCloser(HANDLE handle) : _handle(handle) {}
	operator HANDLE() { return _handle; }
	~FindHandleCloser()
	{
		if (_handle != INVALID_HANDLE_VALUE)
			::FindClose(_handle);
	}
};

inline StdString UIApproximateNarrowString(const WCHAR* widestring)
{
	std::wstring tmp(widestring);
	auto str = std::string(tmp.begin(), tmp.end());
	for (size_t i = 0; i < str.size(); i++)
		if (str[i] == '\0')
			str[i] = '?';
	return str;
}

inline std::vector<StdString> OS_ListFilesInDirectory(const char* dir, bool includeFiles, bool includeDirectories = false, const char* pattern = "*", bool bSort = false)
{
	std::vector<StdString> ret;
	WIN32_FIND_DATAA findFileData = {};

	StdString searchSpec(dir, pathsep, pattern);
	FindHandleCloser hFind(::FindFirstFileA(searchSpec, &findFileData));
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
		std::sort(ret.begin(), ret.end());

	return ret;
}

inline int WindowsUtf16ToUtf8(LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte)
{
	int nBytesWritten = ::WideCharToMultiByte(CP_UTF8, 0, &lpWideCharStr[0], cchWideChar, &lpMultiByteStr[0], cbMultiByte - 1, NULL, NULL);
	AssertThrow(0, nBytesWritten < cbMultiByte);
	lpMultiByteStr[nBytesWritten] = '\0';
	return nBytesWritten;
}

// the callback can return false to stop iteration
typedef std::function<bool(const char* /*szFullpath*/, size_t /*lenFullpath*/, const WCHAR* /*wzFullpath*/, size_t /*lenwzFullpath*/, Uint64 /*modtime*/, Uint64 /*filesize*/)> FnRecurseFilesCallback;
void RecurseFilesInternal(const WCHAR* wzPath, FnRecurseFilesCallback callback, const std::unordered_set<std::wstring>& disallowedDirectories)
{
	WIN32_FIND_DATA findFileData = { 0 };
	WCHAR widebuffer[MAX_PATH + 1] = { 0 };

	// swprintf_s will catch the case if the path is too long for the buffer.
	swprintf_s(widebuffer, _countof(widebuffer), L"%s\\*", wzPath);
	FindHandleCloser hFind(::FindFirstFile(widebuffer, &findFileData));
	auto disallowedDirectoriesSize = disallowedDirectories.size();
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (StrEqual(L".", findFileData.cFileName) || StrEqual(L"..", findFileData.cFileName))
				continue;

			// get the full path
			WCHAR wzBufferFullpath[MAX_PATH + 1] = { 0 };
			int cchFullPath = swprintf_s(wzBufferFullpath, _countof(wzBufferFullpath), L"%s\\%s", wzPath, findFileData.cFileName);
			DebugOnly(AssertThrow(0, wcslen(wzBufferFullpath) == cchFullPath));

			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
					Throw(0, "Directories that contains symlinks are not supported", UIApproximateNarrowString(wzBufferFullpath));

				if (disallowedDirectoriesSize == 0 || disallowedDirectories.find(wzBufferFullpath) == disallowedDirectories.end())
					RecurseFilesInternal(wzBufferFullpath, callback, disallowedDirectories);
			}
			else if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) // skip all symlinks!
			{
				// get utf-8 of the full path
				char wzBufferFullpathUtf8[4096] = "";
				AssertThrowMsg(0, cchFullPath * 4 < _countof(wzBufferFullpathUtf8), "ensure space, a wchar16 will become at most 4 bytes");
				int nBytesWritten = WindowsUtf16ToUtf8(&wzBufferFullpath[0], cchFullPath, &wzBufferFullpathUtf8[0], sizeof(wzBufferFullpathUtf8));
				AssertThrowMsg(0, nBytesWritten > 0, "WindowsUtf16ToUtf8 failed.", StrFromInt(GetLastError()));

				auto modtime = MakeUint64(findFileData.ftLastWriteTime.dwHighDateTime, findFileData.ftLastWriteTime.dwLowDateTime);
				auto filesize = MakeUint64(findFileData.nFileSizeHigh, findFileData.nFileSizeLow);
				if (!callback(wzBufferFullpathUtf8, nBytesWritten, wzBufferFullpath, cchFullPath, modtime, filesize))
					break;
			}
		} while (FindNextFile(hFind, &findFileData) != 0);
	}
}

inline void RecurseFiles(const char* dir, FnRecurseFilesCallback callback, const std::vector<StdString>& disallowedDirectories = std::vector<StdString>())
{
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
		Throw(0, "SHGetFolderPathA failed with code ", StrFromInt(hr));

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

inline void PrintMemoryUsageInfo()
{
	PROCESS_MEMORY_COUNTERS_EX obj = {};
	obj.cb = sizeof(obj);
	AssertThrowMsg(0, ::GetProcessMemoryInfo(::GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&obj, obj.cb), "could not get memoryinfo");
	printf("\tPageFaultCount: %d (%fMb)\n", CastUint32(obj.PageFaultCount), obj.PageFaultCount / (1024.0*1024.0));
	printf("\tPeakWorkingSetSize: %d (%fMb)\n", CastUint32(obj.PeakWorkingSetSize), obj.PeakWorkingSetSize / (1024.0*1024.0));
	printf("\tWorkingSetSize: %d (%fMb)\n", CastUint32(obj.WorkingSetSize), obj.WorkingSetSize / (1024.0*1024.0));
	printf("\tPagefileUsage: %d (%fMb)\n", CastUint32(obj.PagefileUsage), obj.PagefileUsage / (1024.0*1024.0));
	printf("\tPeakPagefileUsage: %d (%fMb)\n", CastUint32(obj.PeakPagefileUsage), obj.PeakPagefileUsage / (1024.0*1024.0));
	printf("\tPrivateUsage: %d (%fMb)\n", CastUint32(obj.PrivateUsage), obj.PrivateUsage / (1024.0*1024.0));
}

inline int RunProcessAndGetStdOutStdErr(const char* szFilename, const char* szArgs, StdString& stdOut, StdString& stdErr, bool getStdOut = true, bool getStdErr = true)
{
	SECURITY_ATTRIBUTES saAttr = {};
	// Set the bInheritHandle flag so pipe handles are inherited. 
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
	AssertThrowMsg(0, false, "not yet implemented");
	return 0;
}

