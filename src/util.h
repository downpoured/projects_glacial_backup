#pragma once

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <vector>
#include <functional>
#include <algorithm> 
#include <string.h>
#include <stddef.h>
#include <memory.h>
#include <unordered_map>
#include <unordered_set>

#include <sys/stat.h>

#if __linux__
#include <signal.h>
#else
#include <windows.h>
#include <Shlobj.h>
#include <psapi.h>
#endif

#ifndef _MSC_VER

#include <wchar.h>
#include <stdint.h>
#include <unistd.h>
#define Inline inline
typedef  int64_t  Int64;
typedef  uint64_t  Uint64;
typedef  uint32_t  Uint32;
typedef  uint16_t  Uint16;
typedef  uint8_t   Uint8;
typedef  uint8_t   byte;
typedef int32_t Int32;
typedef char oschartype;
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#define __FUNCTION__ __func__
#define NORETURN __attribute__ ((__noreturn__))
/* todo: replace all sprintf calls with bstrlib */ 
#define sprintf_s(buf, size, ...) snprintf(buf, size, __VA_ARGS__)
#define fprintf_s(stream, fmt, ...) fprintf(stream, fmt, __VA_ARGS__)
#define _wfopen_s(fptr, filename, mode) *(fptr) = (fwopen(filename, mode))
inline int os_access(const char * _Filename, int _AccessMode) { return access(_Filename, _AccessMode); }
inline FILE* fopenwrapper(const oschartype* sz, const oschartype* mode) { return fopen(sz, mode); }
inline void alertdialog(const char* sz) { fprintf(stderr, "%s\nPress enter to continue...\n", sz); std::cin.get(); }
#define BreakIfAttached() raise(SIGTRAP)
#else

#include <direct.h>
#include <io.h>
#define Inline __forceinline
typedef __int64 Int64;
typedef unsigned __int64 Uint64;
typedef unsigned __int32 Uint32;
typedef unsigned __int16 Uint16;
typedef unsigned __int8  Uint8;
typedef unsigned __int8  byte;
typedef INT32 Int32;
typedef wchar_t oschartype;
inline bool S_ISDIR(int mode) { return (mode & S_IFDIR) != 0; }
inline int __cdecl os_access(const char * _Filename, int _AccessMode) { return _access(_Filename, _AccessMode); }
inline int mkdir(const char * _Filename, int /*_AccessMode*/) { return _mkdir(_Filename); }
FILE* fopenwrapper(const char* sz, const char* mode);
FILE* fopenwrapper(const oschartype* sz, const char* mode);
inline void alertdialog(const char* sz) { ::MessageBoxA(0, sz, "glacial backup", 0); }
#define BreakIfAttached() do { if (IsDebuggerPresent()) ::DebugBreak(); } while (false)
#define NORETURN __declspec(noreturn)

#endif

Uint64 OS_GetUnixTime();

class NonCopyable
{
protected:
	NonCopyable() {}
	~NonCopyable() {}
private:
	NonCopyable(const NonCopyable &);
	NonCopyable & operator = (const NonCopyable &);
};

class RunWhenLeavingScope : private NonCopyable
{
	std::function<void(void)> _fn;
public:
	RunWhenLeavingScope(std::function<void(void)> fn) : _fn(fn) {}
	~RunWhenLeavingScope() { _fn(); }
};

class StdString
{
	std::string _str;
public:
	StdString() {}
	StdString(const StdString& other)
	{
		_str = (const char*)other;
	}
	StdString& operator=(const StdString& other)
	{
		_str = (const char*)other;
		return *this;
	}
	StdString(StdString&& other)
	{
		_str = std::move(other._str);
	}
	StdString& operator= (StdString&& other)
	{
		std::swap(_str, other._str);
		return *this;
	}

	StdString(std::string&& s)
	{
		_str = std::move(s);
	}
	StdString(const std::string& s)
	{
		_str = s;
	}
	StdString(const char* s1, const char* s2 = "", const char* s3 = "", const char* s4 = "", const char* s5 = "",
		const char* s6 = "", const char* s7 = "", const char* s8 = "")
	{
		AppendMany(s1, s2, s3, s4, s5, s6, s7, s8);
	}

	void AppendMany(const char* s1 = "", const char* s2 = "", const char* s3 = "", const char* s4 = "", const char* s5 = "",
		const char* s6 = "", const char* s7 = "", const char* s8 = "")
	{
		Append(s1);
		Append(s2);
		Append(s3);
		Append(s4);
		Append(s5);
		Append(s6);
		Append(s7);
		Append(s8);
	}

	void Set(const char* s)
	{
		_str = s;
	}

	void Append(const char* s)
	{
		_str += s;
	}

	size_t Length() const
	{
		return _str.size();
	}

	operator const char*() const
	{
		return _str.c_str();
	}

	const char* c_str() const
	{
		return _str.c_str();
	}

	// without providing these, the operator const char* is invoked and gives bad results.
	bool operator==(const StdString& right) const { return this->_str == right._str; }
	bool operator!=(const StdString& right) const { return this->_str != right._str; }
	bool operator<(const StdString& right) const { return this->_str < right._str; }
	bool operator>(const StdString& right) const { return this->_str > right._str; }
	bool operator<=(const StdString& right) const { return this->_str <= right._str; }
	bool operator>=(const StdString& right) const { return this->_str >= right._str; }
	
	// let's remove these, too.
	operator bool() = delete;
	bool const operator!() = delete;
	char operator[] (int nIndex) = delete;
	
#ifdef _MSC_VER
	friend StdString operator+(StdString a, const StdString& b) = delete;
	friend StdString operator-(StdString a, const StdString& b) = delete;
#endif
};

#ifdef __linux__
#define pathsep "/"
#define newline "\n"
#else
#define pathsep "\\"
#define newline "\n"
#endif

std::ostream& operator<<(std::ostream& os, const StdString& s)
{
	return os << (const char*)s;
}

std::ostream& operator<<(std::ostream& os, const std::vector<StdString>& arr)
{
	os << "vector length=" << arr.size() << " [";
	for (auto it = arr.begin(); it != arr.end(); ++it)
		os << "\"" << *it << "\" (next item is:) " << newline;
	return os << "]";
}

inline StdString StrFromInt(int i)
{
	std::ostringstream ss;
	ss << i;
	return ss.str();
}

inline bool StrEqual(const char* s1, const char* s2)
{
	return strcmp(s1, s2) == 0;
}

inline bool StrEqual(const wchar_t* s1, const wchar_t* s2)
{
	return wcscmp(s1, s2) == 0;
}

inline bool StrStartsWith(const char* s1, const char* s2)
{
	return strncmp((s1), (s2), strlen(s2)) == 0;
}

inline bool StrStartsWith(const wchar_t* s1, const wchar_t* s2)
{
	return wcsncmp((s1), (s2), wcslen(s2)) == 0;
}

inline bool StrEndsWith(const char* s1, const char*s2)
{
	size_t len1 = strlen(s1), len2 = strlen(s2);
	if (len2>len1) return false;
	return strcmp(s1 + len1 - len2, s2) == 0;
}

inline bool StrEndsWith(const wchar_t* s1, const wchar_t*s2)
{
	size_t len1 = wcslen(s1), len2 = wcslen(s2);
	if (len2>len1) return false;
	return wcscmp(s1 + len1 - len2, s2) == 0;
}

inline bool StrContains(const char* s1, const char*s2)
{
	return strstr(s1, s2) != 0;
}

class MvException : public std::runtime_error
{
	int _lineno;
	int _exceptionId;
	StdString _filename;
	StdString _message;
	StdString _fnName;
	StdString _fullmessage;
	static int _suppressDialogs;

public:
	MvException() : std::runtime_error("") {}
	explicit MvException(const char* szMessage) :
		std::runtime_error(""), _message(szMessage) {}
	explicit MvException(int exceptionId, int lineno, const char* fnname, const char* file, const char* wzMessage) :
		std::runtime_error(""),
		_exceptionId(exceptionId),
		_lineno(lineno),
		_fnName(fnname ? fnname : ""),
		_filename(file ? file : ""),
		_message(wzMessage ? wzMessage : "") {
	
		if (_message.Length())
			_fullmessage.AppendMany("Could not continue, ", _message, newline);
		else
			_fullmessage.AppendMany("Could not continue.", newline);
		if (_exceptionId)
			_fullmessage.AppendMany("Number ", StrFromInt(_exceptionId), newline);
		if (_filename.Length())
			_fullmessage.AppendMany(newline, "Details: file ", _filename, " function ", _fnName, " line ", StrFromInt(_lineno), newline);
	}

	static bool AreDialogsSuppressed() { return _suppressDialogs!=0; }
	const char* ShortMessage() { return _message; }
	virtual const char* what() const throw () { return _fullmessage; }
	virtual ~MvException() throw () {}

	class SuppressDialogs
	{
	public:
		SuppressDialogs() { ++_suppressDialogs; }
		~SuppressDialogs() { --_suppressDialogs; }
	};
};

NORETURN void ThrowImpl(int exceptionnumber, int lineno, const char* fnname, const char* file, const char* msg1 = "", const char* msg2 = "", const char* msg3 = "", const char* msg4 = "", const char* msg5 = "");

#if defined(_DEBUG)
#define DebugOnly(code) code
#define DebugDialog(exceptionnumber, ...) \
	if (!MvException::AreDialogsSuppressed()) { \
		StdString sMsg(__VA_ARGS__); \
		alertdialog(sMsg); \
		BreakIfAttached(); } 
#else
#define DebugOnly(code)
#define DebugDialog(...)
#endif

#define Throw(exceptionnumber, ...) do { \
	DebugDialog(exceptionnumber, __VA_ARGS__);  \
	ThrowImpl((exceptionnumber), __LINE__, __FUNCTION__, __FILE__, __VA_ARGS__); \
	} while(false)

#define AssertThrowMsg(exceptionnumber, condition, ...) \
	do { if (!(condition)) { Throw(exceptionnumber, #condition, __VA_ARGS__); } } while(false)

#define AssertThrow(exceptionnumber, condition) \
	do { if (!(condition)) { Throw(exceptionnumber, #condition, ""); } } while(false)

#define FillStructWithZeros() memset(this, 0, sizeof(*this))
StdString GetNonExistingFilename(const char* sz, int maxAttempts=1000);

inline Uint64 MakeUint64(Uint32 hi, Uint32 lo);
Uint32 CastUint32(Uint64 n);

// Use the 3rd-party "easylogging" library?
// Potentially useful features:
// succinct syntax: LOG(INFO) << "Testing";
// gcc crash handler (ELPP_DISABLE_DEFAULT_CRASH_HANDLING)
// ELPP_DISABLE_LOGGING_FLAGS_FROM_ARG
// ELPP_DISABLE_CUSTOM_FORMAT_SPECIFIERS
// ELPP_DISABLE_LOG_FILE_FROM_ARG
// Cons:
// Needs winsock on windows
// My own logging code in a few dozen lines is faster (although it doesn't format datetime as nicely)

// For 5 million log entry benchmark,
// MvLog ostringstream and flush every 16, 24.1s
// MvLog sprintf and flush every 16, 23.4
// MvLog fprintf and directly flush every entry, 27s
// easylogging.h and flushing every 16, 32s
// spdlog.h, synchronous, 26s
class MvSimpleLogging
{
	char _buffer[4096];
	size_t _locationInBuffer = 0;
	FILE* _file = 0;
	Uint32 _counter = 0;
	StdString _filename;
	static const int cTargetFileSize = 8 * 1024 * 1024 /*size in mb*/;
	static const int cHowOftenToFlush = 64;
	static const int cHowOftenToCheckFilesize = 64;

public:
	bool Start(const char* filename)
	{
		if (StrEqual(_filename, filename) && _file != nullptr)
			return true;

		FILE* newfile = fopenwrapper(filename, "ab");
		if (!newfile)
			return false;

		Flush();
		if (_file)
			fclose(_file);
		
		_file = newfile;
		_filename = filename;
		_locationInBuffer = 0;
		_buffer[0] = '\0';
		return true;
	}
	void WriteLine(const char* sz1, size_t len1, const char* sz2="", const char* sz3="")
	{
		_counter++;
		size_t len2 = strlen(sz2);
		size_t len3 = strlen(sz3);
		Uint64 nApproxSeconds = OS_GetUnixTime() - 16436*24*60*60 /*days since 2015*/;
		Uint32 secsRounded = nApproxSeconds % 60;
		Uint32 minRounded = (nApproxSeconds / 60) % 60;
		Uint64 hrRounded = (nApproxSeconds / (60 * 60));
		Uint64 hrNumber = (hrRounded % 24);
		Uint64 roughDays = (hrRounded) / 24;

		Uint64 totalLengthIncrease = (Uint64)len1 + len2 + len3 + 9 /*whitespace */ +
			20 /*max roughDays*/ + 2 /*hrNumber*/ + 2 /*minRounded*/ + 2 /*secsRounded*/;
		if (totalLengthIncrease >= _countof(_buffer))
			return WriteLine("(too much text to log)", strlen("(too much text to log)"), "", "");
		else if (_counter % cHowOftenToFlush == 0 || _locationInBuffer + totalLengthIncrease >= _countof(_buffer))
			Flush();
		int written = sprintf_s(_buffer + _locationInBuffer, _countof(_buffer) - _locationInBuffer, "%d %d:%d:%d %s %s %s" newline,
			CastUint32(roughDays), CastUint32(hrNumber), minRounded, secsRounded, sz1, sz2, sz3);

		DebugOnly(AssertThrow(0, written <= totalLengthIncrease));
		_locationInBuffer += written;
	}

	void WriteLineCritical(int exceptionnumber, int lineno, const char* fnname, const char* file, const char* msg1, const char* msg2)
	{
		if (!_file)
			return;

		fprintf(_file, "exception thrown, %d %s %s (line %d function %s file %s)\n", exceptionnumber, msg1, msg2, lineno, fnname, file);
		fflush(_file);
	}

	void Flush()
	{
		if (!_file)
			return;

		fprintf_s(_file, "%s", _buffer);
		_buffer[0] = '\0';
		_locationInBuffer = 0;

		// switch to a new file if this file gets too big
		if (_counter % cHowOftenToCheckFilesize == 0 && ftell(_file) > cTargetFileSize)
		{
			StdString newFilename(GetNonExistingFilename(_filename));
			if (!Start(newFilename))
				WriteLine("failed to switch over to new log file", strlen("failed to switch over to new log file"), _filename, newFilename);
		}
	}

	StdString GetFilename()
	{
		return _filename;
	}

	~MvSimpleLogging()
	{
		Flush();
		if (_file)
			fclose(_file);
	}

	friend class MvSimpleLoggingTests;
};

// sz1 is often a string literal, so let's determine its length at compile time
extern MvSimpleLogging g_logging;
#define MvLog(szConst, ...) do { \
	static_assert(sizeof(szConst)>sizeof(const char*), "1st arg must be compile time string longer than 8 characters");\
	g_logging.WriteLine(szConst, sizeof(szConst)-1/*null term*/, __VA_ARGS__);\
	} while(false)

NORETURN inline void ThrowImpl(int exceptionnumber, int lineno, const char* fnname, const char* file, const char* msg1, const char* msg2, const char* msg3, const char* msg4, const char* msg5)
{
	// write to the log twice. first, write a succinct 'critical' message that won't allocate any memory.
	g_logging.WriteLineCritical(exceptionnumber, lineno, fnname, file, msg1, msg2);

	StdString msg;
	msg.AppendMany(StdString(msg1), StdString(msg2), StdString(msg3), StdString(msg4), StdString(msg5));
	MvLog("exception occurred", msg);
	g_logging.Flush();
	throw MvException(exceptionnumber, lineno, fnname, file, msg);
}

inline Uint64 MakeUint64(Uint32 hi, Uint32 lo)
{
	return lo | (((Uint64)hi) << 32);
}

inline Uint32 CastUint32(Uint64 n)
{
	AssertThrow(0, n <= UINT_MAX);
	return (Uint32)n;
}

template<class T>
int VectorLinearSearch(const std::vector<T>& arr, const T& searchFor)
{
	for (int i = 0; i < (int)arr.size(); i++)
		if (arr[i] == searchFor)
			return i;
	return -1;
}

inline std::vector<StdString> StrUtilSplitString(const StdString& s, char delim)
{
	std::vector<StdString> elems;
	std::istringstream ss(s.c_str());
	std::string item;
	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}

	// to be consistent with python's split, add an empty element if the string ends with delim.
	if (s.Length() > 0 && s[s.Length() - 1] == delim)
	{
		elems.push_back(StdString());
	}
	return elems;
}

inline void StrUtilSetIntFromString(const char* ptr, const char* fullstring, int& outResult)
{
	const char* ptrIter = ptr;
	while (*ptrIter != '\0')
		AssertThrowMsg(0, isdigit(*(ptrIter++)), "parameter should be numeric, but got:", fullstring);

	char* result = 0;
	int res = strtol(ptr, &result, 10);
	if (!*result && ptr[0] != '\0')
		outResult = res;
}
