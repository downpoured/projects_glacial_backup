#pragma once

inline bool OS_FileExists(const char* filepath)
{
	struct stat st = { 0 };
	return (stat(filepath, &st) == 0 && !S_ISDIR(st.st_mode));
}

inline bool OS_DirectoryExists(const char* filepath)
{
	struct stat st = { 0 };
	return (stat(filepath, &st) == 0 && S_ISDIR(st.st_mode));
}

inline bool OS_CreateDirectory(const char* filepath, int mode = 0777)
{
	auto result = mkdir(filepath, mode);
	if (result != 0 && errno == EEXIST)
		return true;
	else if (result != 0)
		return false;
	else
		return true;
}

inline bool OS_Remove(const char* filepath)
{
	// removes file or empty directory. ok if filepath was previously deleted.
	auto result = remove(filepath);
	struct stat st = { 0 };
	return stat(filepath, &st) != 0;
}

inline Uint64 OS_GetFileSize(const char* filepath)
{
	struct stat st = { 0 };
	if (stat(filepath, &st) == 0)
		return st.st_size;
	else
		return 0;
}

inline StdString OS_ReadTextfile(const char* filepath)
{
	std::ifstream t(filepath, std::ifstream::in);
	std::ostringstream buffer;
	buffer << t.rdbuf();
	AssertThrow(0, !t.fail());
	return buffer.str();
}

inline void OS_WriteTextfile(const char* filepath, const char* text)
{
	std::ofstream t(filepath, std::ifstream::out);
	t << text;
	AssertThrow(0, !t.fail());
}

#if _MSC_VER
#include "util_win32.h"

bool OS_ClearFilesInDirectory(const char* dir)
{
	auto files = OS_ListFilesInDirectory(dir, true /*includeFiles*/, false /*includeDirectories*/, "*");
	for (auto it = files.begin(); it != files.end(); ++it)
	{
		if (!OS_Remove(*it))
			return false;
	}
	return true;
}

#else

Uint64 OS_GetUnixTime()
{
	return (Uint64) time(NULL);
}

inline StdString GetDirectoryConfigHome()
{
	char* home = getenv("HOME");
	AssertThrowMsg(0, home && home[0], "could not find home directory");
	return StdString(home, "/.local/share");
}

inline StdString GetDirectoryCacheHome()
{
	char* home = getenv("HOME");
	AssertThrowMsg(0, home && home[0], "could not find home directory");
	return StdString(home, "/.local/share");
}

inline StdString OS_GetDirectoryOfCurrentModule()
{
	return ".";
}

class OS_PreventMultipleInstances
{
public:
	bool AcquireAndCheckIfOtherInstanceIsRunning(const char*)
	{
		return false;
	}
};

#endif

inline StdString GetNonExistingFilename(const char* filepath, int maxAttempts)
{
	if (!OS_FileExists(filepath))
		return filepath;

	const char* lastDot = strrchr(filepath, '.');
	AssertThrow(0, lastDot != nullptr);
	std::string sBeforeExtension(filepath, lastDot - filepath);
	for (int i = 0; i < maxAttempts; i++)
	{
		char buf[4096] = "";
		sprintf_s(buf, _countof(buf), "%s.%d%s", sBeforeExtension.c_str(), i + 1, lastDot);
		if (!OS_FileExists(buf))
			return StdString(buf);
	}

	Throw(0, "GetNonExistingFilename failed", StrFromInt(maxAttempts));
}
