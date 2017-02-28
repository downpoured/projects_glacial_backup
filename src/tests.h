#pragma once
// tests

template <typename T1, typename T2>
StdString TestEqImpl(const T1& a, const T2& b, bool shouldThrow=true)
{
	std::ostringstream strA, strB;
	strA << a;
	strB << b;
	AssertThrow(0, strA.str().size() > 0);
	if (strA.str() != strB.str())
	{
		std::ostringstream stm;
		stm << "expected \n" << a << "\n but got\n" << b;
		if (shouldThrow)
			Throw(0, stm.str().c_str());
		return stm.str();
	}
	return "";
}

void ExpectException(std::function<void(void)> fn, const char* expectedText=0)
{
	bool pass = false;
	try
	{
		MvException::SuppressDialogs suppress;
		fn();
	}
	catch (MvException& exc)
	{
		if (expectedText && !StrContains(exc.what(), expectedText))
			Throw(0, "exception had the wrong text");
		pass = true;
	}
	catch (...)
	{
		Throw(0, "other type of exception thrown.");
	}
	if (!pass)
		Throw(0, "no exception thrown");
}

std::vector<std::function<void(const char* szTestDirectory)>> registeredTests;

#define RegisterTestClassBegin(Classname) \
	class Classname { \
	public: \
	static void RunTestsInThisClass(const char* szTestDirectory)

#define RegisterTestClassEnd(Classname) \
	class SimulateAStaticConstructor { \
		public: SimulateAStaticConstructor() { \
		registeredTests.push_back(&RunTestsInThisClass); } }; \
	static SimulateAStaticConstructor simulateAStaticConstructor; }; \
	Classname ::SimulateAStaticConstructor Classname ::simulateAStaticConstructor;

#define TestEq(a, b) \
	do { auto sStringMessage = TestEqImpl((a), (b), false); \
	if (sStringMessage.Length()) Throw(0, sStringMessage); }  while(false)


RegisterTestClassBegin(StringTests)
{
	TestEq(true, StrEqual("abc", "abc"));
	TestEq(true, StrEqual("D", "D"));
	TestEq(true, !StrEqual("abc", "abC"));
	TestEq(true, StrStartsWith("abc", ""));
	TestEq(true, StrStartsWith("abc", "a"));
	TestEq(true, StrStartsWith("abc", "ab"));
	TestEq(true, StrStartsWith("abc", "abc"));
	TestEq(true, !StrStartsWith("abc", "abcd"));
	TestEq(true, !StrStartsWith("abc", "aB"));
	TestEq(true, !StrStartsWith("abc", "def"));
	TestEq(true, StrEndsWith("abc", ""));
	TestEq(true, StrEndsWith("abc", "c"));
	TestEq(true, StrEndsWith("abc", "bc"));
	TestEq(true, StrEndsWith("abc", "abc"));
	TestEq(true, !StrEndsWith("abc", "aabc"));
	TestEq(true, !StrEndsWith("abc", "abC"));
	TestEq(true, !StrEndsWith("abc", "aB"));
	TestEq(true, !StrEndsWith("abc", "def"));
	TestEq(true, StrStartsWith(L"abc", L"ab"));
	TestEq(true, !StrStartsWith(L"abc", L"aB"));
	TestEq(true, StrEndsWith(L"abc", L"bc"));
	TestEq(true, !StrEndsWith(L"abc", L"aB"));
	TestEq(true, StrContains("abc", ""));
	TestEq(true, StrContains("abc", "ab"));
	TestEq(true, StrContains("abc", "bc"));
	TestEq(true, StrContains("abc", "abc"));
	TestEq(true, !StrContains("abc", "abcd"));
	TestEq(true, !StrContains("abc", "x"));

	TestEq(true, StrEqual("", StdString()));
	TestEq("abcde", StdString("abcde"));
	TestEq("11233", StdString("11", "2", "33"));

	StdString s2;
	s2.AppendMany("a", "b", "", "c", "de", "");
	TestEq("abcde", s2);

	StdString s3;
	s3.AppendMany("");
	TestEq(true, StrEqual("", s3));

	TestEq("is 77 an int", StdString("is ", StrFromInt(77), " an int"));
	TestEq("is -4 an int", StdString("is ", StrFromInt(-4), " an int"));

	TestEq(0x1111000011112222ULL, MakeUint64(0x11110000, 0x11112222));
	TestEq(0x00000000FF345678ULL, MakeUint64(0x00000000, 0xFF345678));
	TestEq(0xFF345678FF345678ULL, MakeUint64(0xFF345678, 0xFF345678));
	TestEq(0xFF34567800000000ULL, MakeUint64(0xFF345678, 0x00000000));

	TestEq(true, TestEqImpl(std::vector<StdString> {"a", "b", "c"}, std::vector<StdString>{"a", "b", "c"}, false/*throw*/).Length() == 0);
	TestEq(true, TestEqImpl(std::vector<StdString> {"a", "b", "c"}, std::vector<StdString>{"a", "k", "c"}, false/*throw*/).Length() != 0);
	TestEq(true, TestEqImpl(std::vector<StdString> {"a", "b", "c"}, std::vector<StdString>{"a", "c"}, false/*throw*/).Length() != 0);
	TestEq((std::vector<StdString>{"a", "b", "c"}), (std::vector<StdString>{"a", "b", "c"}));
}
RegisterTestClassEnd(StringTests)

RegisterTestClassBegin(MvSimpleLoggingTests)
{
	MvSimpleLogging g_logging; //shadows the actual global logging
	auto testLogPath = StdString(szTestDirectory, pathsep, "testlogging.txt");
	auto testLogPathTmp = StdString(szTestDirectory, pathsep, "testloggingTmp.txt");
	auto reset = [&]() { 
		TestEq(true, g_logging.Start(testLogPathTmp));
		AssertThrow(0, OS_Remove(testLogPath));
		TestEq(true, g_logging.Start(testLogPath));
		AssertThrow(0, OS_Remove(testLogPathTmp)); };
	auto getLoggedText = [&](){
		TestEq(true, g_logging.Start(testLogPathTmp));
		return OS_ReadTextfile(testLogPath); };

	// simple log entries
	reset();
	MvLog("test_test_test", "test1", "test2");
	TestEq(true, StrContains(getLoggedText(), " test_test_test test1 test2"));
	TestEq(false, StrContains(getLoggedText(), "(too much text to log)"));

	// test exact text written
	reset();
	MvLog("test_test_test", "test1");
	auto parts = StrUtilSplitString(getLoggedText(), ' ');
	TestEq(5, parts.size());
	auto partsTimestamp = StrUtilSplitString(parts[1], ':');
	TestEq(3, partsTimestamp.size());
	int days = 0, hours = 0, minutes = 0, seconds = 0;
	StrUtilSetIntFromString(parts[0], "", days);
	StrUtilSetIntFromString(partsTimestamp[0], "", hours);
	StrUtilSetIntFromString(partsTimestamp[1], "", minutes);
	StrUtilSetIntFromString(partsTimestamp[2], "", seconds);
	TestEq(true, days > 0);
	TestEq(true, hours < 24);
	TestEq(true, minutes < 60);
	TestEq(true, seconds < 60);
	TestEq("test_test_test", parts[2]);
	TestEq("test1", parts[3]);

	// try to write almost too much
	reset();
	auto testText = std::string(sizeof(g_logging._buffer) - 36, 'a');
	g_logging.WriteLine(testText.c_str(), testText.size());
	TestEq(testText, StrUtilSplitString(getLoggedText(), ' ')[2]);

	// try to write too much (there needs to be room for null terminator)
	reset();
	testText = std::string(sizeof(g_logging._buffer) -  35, 'a');
	g_logging.WriteLine(testText.c_str(), testText.size());
	TestEq("(too", StrUtilSplitString(getLoggedText(), ' ')[2]);
	TestEq("much", StrUtilSplitString(getLoggedText(), ' ')[3]);

	// try to write way too much to the file
	reset();
	auto bytesToWrite = sizeof(g_logging._buffer) * 3 + 30;
	MvLog("test too much", std::string(bytesToWrite, 'a').c_str());
	TestEq(true, StrContains(getLoggedText(), "(too much text to log)"));

	// try to write way too much to the file after logging something
	reset();
	bytesToWrite = sizeof(g_logging._buffer) * 3 + 30;
	MvLog("test_test_test", "test1", "test2");
	MvLog("test too much", std::string(bytesToWrite, 'a').c_str());
	TestEq(true, StrContains(getLoggedText(), " test_test_test test1 test2"));
	TestEq(true, StrContains(getLoggedText(), "(too much text to log)"));

	// don't flush if landing before the boundary
	reset();
	memset(g_logging._buffer, 'x', sizeof(g_logging._buffer));
	g_logging._buffer[sizeof(g_logging._buffer) - 1] = '\0';
	g_logging._locationInBuffer = sizeof(g_logging._buffer) - 36;
	g_logging.WriteLine("", 0);
	TestEq(true, g_logging._locationInBuffer > sizeof(g_logging._buffer)/2);

	// do flush if landing right on the boundary
	reset();
	memset(g_logging._buffer, 'x', sizeof(g_logging._buffer));
	g_logging._buffer[sizeof(g_logging._buffer) - 1] = '\0';
	g_logging._locationInBuffer = sizeof(g_logging._buffer) - 36;
	g_logging.WriteLine("a", 1);
	TestEq(false, g_logging._locationInBuffer > sizeof(g_logging._buffer) / 2);

	// do flush if going past the boundary
	reset();
	memset(g_logging._buffer, 'x', sizeof(g_logging._buffer));
	g_logging._buffer[sizeof(g_logging._buffer) - 1] = '\0';
	g_logging._locationInBuffer = sizeof(g_logging._buffer) - 36;
	g_logging.WriteLine("abcde", 5);
	TestEq(false, g_logging._locationInBuffer > sizeof(g_logging._buffer) / 2);

	// test GetNonExistingFilename
	StdString firstName(szTestDirectory, pathsep, "scratch" pathsep "getflname.txt");
	TestEq(true, OS_CreateDirectory(StdString(szTestDirectory, pathsep, "scratch")));
	TestEq(false, OS_FileExists(firstName));
	TestEq(firstName, GetNonExistingFilename(firstName));
	OS_WriteTextfile(firstName, "created a text file");
	TestEq(StdString(szTestDirectory, pathsep, "scratch" pathsep "getflname.1.txt"), GetNonExistingFilename(firstName));
	OS_WriteTextfile(StdString(szTestDirectory, pathsep, "scratch" pathsep "getflname.1.txt"), "created a text file");
	TestEq(StdString(szTestDirectory, pathsep, "scratch" pathsep "getflname.2.txt"), GetNonExistingFilename(firstName));
}
RegisterTestClassEnd(MvSimpleLoggingTests)

RegisterTestClassBegin(ExceptionTests)
{
	ExpectException([] {
		Throw(0, "foo"); }, "foo");

	// test ExpectException itself
	ExpectException([] { 
		ExpectException([] {/*don't throw anything*/}, "");
	}, "no exception thrown");
	ExpectException([] {
		ExpectException([] { throw std::runtime_error("");  }, "");
	}, "other type");
	ExpectException([] {
		ExpectException([] { Throw(0, "foo"); }, "bar");
	}, "the wrong text");
}
RegisterTestClassEnd(ExceptionTests)

RegisterTestClassBegin(UtilOSTests)
{
	// set up test data
	StdString dir(szTestDirectory, pathsep, "scratch");
	StdString subdir1(dir, pathsep, "subdir1");
	StdString subdir2(dir, pathsep, "subdir2");
	StdString textFileInSubdir1(subdir1, pathsep, "text.txt");
	std::vector<StdString> filesExpected {
		StdString(dir, pathsep, "1st file in base"),
		StdString(dir, pathsep, "file in base"),
		StdString(subdir1, pathsep, "no_extension"),
		textFileInSubdir1,
		StdString(subdir1, pathsep, "z"),
		StdString(subdir2, pathsep, "other file.other") };
	std::vector<StdString> filesSeenWhenRecursing;
	Uint64 modTimeSeenForTextFile = 0;
	auto callback = [&](const char* szFullpath, size_t lenFullpath, const wchar_t* wz, size_t lenWz, Uint64 modtime, Uint64 filesize)
	{
		// as a small perf optimization we kept the string length. make sure it matches.
		TestEq(lenFullpath, strlen(szFullpath));
		TestEq(lenWz, wz ? wcslen(wz) : 0);
		Uint32 whichFile = VectorLinearSearch(filesExpected, StdString(szFullpath));
		TestEq(true, whichFile >= 0 && whichFile < filesExpected.size());

		// test the file size (which we arbitrarily set based on the index in filesExpected)
		TestEq(whichFile + 1, filesize);

		// keep the modified time, we'll check it later
		if (StrEqual(textFileInSubdir1, szFullpath))
			modTimeSeenForTextFile = modtime;

		filesSeenWhenRecursing.push_back(StdString(szFullpath));
		std::sort(filesSeenWhenRecursing.begin(), filesSeenWhenRecursing.end());
		return true;
	};

#ifdef _MSC_VER
	// create and clear directories
	AssertThrow(0, OS_CreateDirectory(dir));
	AssertThrow(0, OS_CreateDirectory(subdir1));
	AssertThrow(0, OS_CreateDirectory(subdir2));
	AssertThrow(0, OS_ClearFilesInDirectory(dir));
	AssertThrow(0, OS_ClearFilesInDirectory(subdir1));
	AssertThrow(0, OS_ClearFilesInDirectory(subdir2));

	// confirm that directories are empty
	TestEq(0, OS_ListFilesInDirectory(dir, true /*includeFiles*/, false /*includeDirectories*/, "*").size());
	TestEq(0, OS_ListFilesInDirectory(subdir1, true /*includeFiles*/, true /*includeDirectories*/, "*").size());
	TestEq(0, OS_ListFilesInDirectory(subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*").size());
	TestEq(0, OS_ListFilesInDirectory("notexist", true /*includeFiles*/, true /*includeDirectories*/, "*").size());
	auto listDir = OS_ListFilesInDirectory(dir, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {subdir1, subdir2}), listDir);

	
	// test recursing through directories with no files
	RecurseFiles(dir, callback);
	TestEq(0, filesSeenWhenRecursing.size());
	RecurseFiles(subdir1, callback);
	TestEq(0, filesSeenWhenRecursing.size());
	RecurseFiles(subdir2, callback);
	TestEq(0, filesSeenWhenRecursing.size());
	RecurseFiles("notexist", callback);
	TestEq(0, filesSeenWhenRecursing.size());

	// test file existence
	for (size_t i = 0; i < filesExpected.size(); i++)
	{
		// we'll arbitrarily set the length of the file based on its index in filesExpected, and test it later
		OS_WriteTextfile(filesExpected[i], std::string(i + 1, 'a').c_str());
		TestEq(true, OS_FileExists(filesExpected[i]));
		TestEq(false, OS_DirectoryExists(filesExpected[i]));
	}
	TestEq(false, OS_FileExists("notexist"));
	TestEq(false, OS_FileExists(StdString(filesExpected[0], "andextracharacters")));
	TestEq(false, OS_FileExists(subdir1));
	TestEq(false, OS_DirectoryExists("notexist"));
	TestEq(true, OS_DirectoryExists(subdir1));
	TestEq(true, OS_DirectoryExists(dir));

	// test directory listing.
	listDir = OS_ListFilesInDirectory(dir, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[0], filesExpected[1], subdir1, subdir2}), listDir);
	listDir = OS_ListFilesInDirectory(dir, true /*includeFiles*/, false /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[0], filesExpected[1]}), listDir);
	listDir = OS_ListFilesInDirectory(subdir1, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[2], filesExpected[3], filesExpected[4]}), listDir);
	listDir = OS_ListFilesInDirectory(subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[5]}), listDir);

	// test recursing through a subdir
	RecurseFiles(subdir1, callback);
	TestEq((std::vector<StdString> {filesExpected[2], filesExpected[3], filesExpected[4]}), filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();

	// test recursing through everything
	RecurseFiles(dir, callback);
	TestEq(filesExpected, filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();

	// test excluded directories
	RecurseFiles(dir, callback, std::vector<StdString> {subdir1});
	TestEq((std::vector<StdString> {filesExpected[0], filesExpected[1], filesExpected[5]}), filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();
	RecurseFiles(dir, callback, std::vector<StdString> {subdir1, subdir2});
	TestEq((std::vector<StdString> {filesExpected[0], filesExpected[1]}), filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();

	// test that the modified time is updated if we modify the file
	::Sleep(500);
	TestEq(true, modTimeSeenForTextFile!=0);
	Uint64 modTimePrevious = modTimeSeenForTextFile;
	OS_WriteTextfile(textFileInSubdir1, "aaaa" /*make an identical file with same length in bytes*/);
	RecurseFiles(dir, callback);
	TestEq(filesExpected, filesSeenWhenRecursing);
	TestEq(true, modTimeSeenForTextFile > modTimePrevious);

	// test file deletion
	AssertThrow(0, OS_Remove(filesExpected[3]));
	listDir = OS_ListFilesInDirectory(subdir1, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[2], filesExpected[4]}), listDir);
	AssertThrow(0, OS_Remove(filesExpected[5]));
	listDir = OS_ListFilesInDirectory(subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq(std::vector<StdString>(), listDir);

	// test file copy
	TestEq(false, OS_CopyFileAndOverwriteExisting("notexist1", "|badpath|invalid"));
	TestEq(false, OS_CopyFileAndOverwriteExisting(filesExpected[2], "|badpath|invalid"));
	TestEq(false, OS_CopyFileAndOverwriteExisting("notexist", StdString(subdir2, pathsep, "afile")));
	TestEq(true, OS_CopyFileAndOverwriteExisting(filesExpected[2], StdString(subdir2, pathsep, "copied_file1")));
	TestEq(true, OS_CopyFileAndOverwriteExisting(StdString(subdir2, pathsep, "copied_file1"), StdString(subdir2, pathsep, "copied_file2")));
	listDir = OS_ListFilesInDirectory(subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString>{StdString(subdir2, pathsep, "copied_file1"), StdString(subdir2, pathsep, "copied_file2")}), listDir);

	// test copy overwrites
	OS_WriteTextfile(StdString(subdir2, pathsep, "different file"), "qqqq");
	TestEq(true, OS_CopyFileAndOverwriteExisting(StdString(subdir2, pathsep, "different file"), StdString(subdir2, pathsep, "copied_file2")));
	TestEq("qqqq", OS_ReadTextfile(StdString(subdir2, pathsep, "copied_file2")));

	// test file rename
	TestEq(true, OS_RenameFileWithoutOverwriteExisting(StdString(subdir2, pathsep, "different file"), StdString(subdir2, pathsep, "different file2")));
	TestEq(true, OS_RenameFileWithoutOverwriteExisting(StdString(subdir2, pathsep, "copied_file1"), StdString(subdir2, pathsep, "testOverwrite")));
	TestEq(false, OS_RenameFileWithoutOverwriteExisting(StdString(subdir2, pathsep, "copied_file2"), StdString(subdir2, pathsep, "testOverwrite")));
	listDir = OS_ListFilesInDirectory(subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString>{StdString(subdir2, pathsep, "copied_file2"), StdString(subdir2, pathsep, "different file2"), StdString(subdir2, pathsep, "testOverwrite")}), listDir);

	TestEq(true, StrEqual("", UIApproximateNarrowString(L"")));
	TestEq("a B c D", UIApproximateNarrowString(L"a B c D"));
	TestEq("c:\\test\\f", UIApproximateNarrowString(L"c:\\test\\f"));
	
	// let's test utf8 encoding.
	AssertThrow(0, OS_ClearFilesInDirectory(subdir2));
	OS_WriteTextfile(StdString(subdir2, pathsep, "the umlaut is \xf9"), "file contents");
	StdString fileSeenSz;
	std::wstring fileSeenWz;
	RecurseFiles(subdir2, [&](const char* szFullpath, size_t lenFullpath, const WCHAR* wz, size_t wzLen, Uint64, Uint64) {
		TestEq(0, fileSeenSz.Length());
		TestEq(wzLen, wcslen(wz));
		fileSeenSz = szFullpath;
		fileSeenWz = wz;
		return true; });
	std::string str(StdString(subdir2, pathsep, "the umlaut is \xf9"));
	TestEq(StdString(subdir2, pathsep, "the umlaut is \xc3\xb9"), fileSeenSz);
	TestEq(true, L'\xf9' == fileSeenWz[fileSeenWz.size()-1]);
#endif

}
RegisterTestClassEnd(UtilOSTests)


RegisterTestClassBegin(BackupConfigurationGeneralTests)
{
	TestEq((std::vector<StdString>{"a", "bb", "c"}), StrUtilSplitString("a|bb|c", '|'));
	TestEq((std::vector<StdString>{"a", "", "b", "c"}), StrUtilSplitString("a||b|c", '|'));
	TestEq((std::vector<StdString>{"a ", "b\r\n", "c\n", ""}), StrUtilSplitString("a |b\r\n|c\n|", '|'));
	TestEq((std::vector<StdString>{"", "", "a", "", "b", "c", "", ""}), StrUtilSplitString("||a||b|c||", '|'));
	TestEq((std::vector<StdString>{"a"}), StrUtilSplitString("a", '|'));
	TestEq((std::vector<StdString>{"aaa\n"}), StrUtilSplitString("aaa\n", '|'));
	TestEq((std::vector<StdString>{"", ""}), StrUtilSplitString("|", '|'));
	TestEq((std::vector<StdString>{"", "", ""}), StrUtilSplitString("||", '|'));
	TestEq((std::vector<StdString>{"a|bb|c"}), StrUtilSplitString("a|bb|c", '?'));
	TestEq((std::vector<StdString>{}), StrUtilSplitString("", '|'));

	int integerTest = 0;
	StrUtilSetIntFromString("12", "", integerTest);
	TestEq(12, integerTest);

	ExpectException([&] { 
		StrUtilSetIntFromString("12a", "", integerTest); }, "should be numeric");
	ExpectException([&] { 
		StrUtilSetIntFromString("abc", "", integerTest); }, "should be numeric");
	ExpectException([&] { 
		StrUtilSetIntFromString(" ", "", integerTest); }, "should be numeric");
	
	// passing in an empty string should result in the integer being left unchanged
	StrUtilSetIntFromString("", "", integerTest);
	TestEq(12, integerTest);

	// parse the demo
	BackupConfigurationGeneral::WriteDemoConfigFile(StdString(szTestDirectory, pathsep, "demo.ini"));
	TestEq(true, OS_FileExists(StdString(szTestDirectory, pathsep, "demo.ini")));
	BackupConfigurationGeneral configsDemo, configsThorough, configsErrSetOutside, configsErrNotNumeric, configsErrDupeGroup;
	configsDemo.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "demo.ini"));
	TestEq(0, configsDemo._unknownOptionsSeen);
	TestEq(true, configsDemo._location_for_local_index.Length() > 0);
	TestEq(true, configsDemo._location_for_files_ready_to_upload.Length() > 0);
	TestEq(true, StrContains(configsDemo._pathFfmpeg, "optionallyProvidePathTo"));
	TestEq(true, StrContains(configsDemo._path7zip, "pleaseProvidePathTo"));
	TestEq(1, configsDemo._groups.size());
	TestEq("group1", configsDemo._groups[0].GetGroupName());
	TestEq("Password1", configsDemo._groups[0].GetPass());
	TestEq("AES-256", configsDemo._groups[0].GetEncryption());
	TestEq(false, configsDemo._groups[0].GetSeparatelyTrackTaggingChangesInAudioFiles());
	TestEq(4, configsDemo._groups[0].GetConsiderRemovingDeletedFilesAfterThisManyWeeks());
	TestEq(64 * 1024, configsDemo._groups[0].GetTargetArchiveSizeInKb());
	TestEq(2, configsDemo._groups[0].ViewDirs()->size());
	TestEq((std::vector<StdString>{}), *configsDemo._groups[0].ViewExcludedDirs());
	TestEq((std::vector<StdString>{".tmp", ".pyc"}), *configsDemo._groups[0].ViewExcludedExtensions());

	// parse a more thorough ini
	std::ostringstream stm;
	stm << "location_for_local_index=location_for_local_index" newline;
	stm << "#location_for_local_index=commentedOut" newline;
	stm << "# comment 1" newline;
	stm << "; comment 2" newline;
	stm << "location_for_files_ready_to_upload=location_for_files_ready_to_upload" newline;
	stm << "7zip=7zip" newline;
	stm << "ffmpeg=ffmpeg" newline;
	stm << newline << newline << "[groupWithMostSet]" newline;
	stm << StdString("dirs=", pathsep, "firstDirectoryInGroup", pathsep, "subdir", "|", pathsep, "otherDirectoryInGroup") << newline;
	stm << "excludedDirs=a|b|c" newline;
	stm << "excludedExtensions=.abcd|.1234|.DCBA" newline;
	stm << "separatelyTrackTaggingChangesInAudioFiles=1" newline;
	stm << "considerRemovingDeletedFilesAfterThisManyWeeks=100" newline;
	stm << "pass=pass" newline;
	stm << "encryption=encryption" newline;
	stm << "targetArchiveSizeInMb=90" newline;
	stm << "unknown1=1000" newline;
	stm << "unknown2=abc" newline;
	stm << "unknownIgnored=" newline;
	stm << "[groupWithMostBlank]" newline;
	stm << "dirs=a" newline;
	stm << "dirs=b" newline;
	stm << "targetArchiveSizeInMb=" newline;
	OS_WriteTextfile(StdString(szTestDirectory, pathsep, "test.ini"), stm.str().c_str());
	configsThorough.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	TestEq(2, configsThorough._unknownOptionsSeen);
	TestEq("location_for_local_index", configsThorough._location_for_local_index);
	TestEq("location_for_files_ready_to_upload", configsThorough._location_for_files_ready_to_upload);
	TestEq("ffmpeg", configsThorough._pathFfmpeg);
	TestEq("7zip", configsThorough._path7zip);
	TestEq(2, configsThorough._groups.size());
	TestEq("groupWithMostSet", configsThorough._groups[0].GetGroupName());
	TestEq((std::vector<StdString>{StdString(pathsep, "firstDirectoryInGroup", pathsep, "subdir"), StdString(pathsep, "otherDirectoryInGroup")}), *configsThorough._groups[0].ViewDirs());
	TestEq((std::vector<StdString>{"a", "b", "c"}), *configsThorough._groups[0].ViewExcludedDirs());
	TestEq((std::vector<StdString>{".abcd", ".1234", ".DCBA"}), *configsThorough._groups[0].ViewExcludedExtensions());
	TestEq(true, configsThorough._groups[0].GetSeparatelyTrackTaggingChangesInAudioFiles());
	TestEq(100, configsThorough._groups[0].GetConsiderRemovingDeletedFilesAfterThisManyWeeks());
	TestEq("pass", configsThorough._groups[0].GetPass());
	TestEq("encryption", configsThorough._groups[0].GetEncryption());
	TestEq(90 * 1024, configsThorough._groups[0].GetTargetArchiveSizeInKb());
	TestEq("groupWithMostBlank", configsThorough._groups[1].GetGroupName());
	TestEq((std::vector<StdString>{"b"}), *configsThorough._groups[1].ViewDirs());
	TestEq((std::vector<StdString>{}), *configsThorough._groups[1].ViewExcludedDirs());
	TestEq((std::vector<StdString>{}), *configsThorough._groups[1].ViewExcludedExtensions());
	TestEq(false, configsThorough._groups[1].GetSeparatelyTrackTaggingChangesInAudioFiles());
	TestEq(INT_MAX, configsThorough._groups[1].GetConsiderRemovingDeletedFilesAfterThisManyWeeks());
	TestEq(true, StrEqual("", configsThorough._groups[1].GetPass()));
	TestEq(true, StrEqual("", configsThorough._groups[1].GetEncryption()));
	TestEq(64 * 1024, configsThorough._groups[1].GetTargetArchiveSizeInKb());

	// check for invalid ini, settings that need to be in group
	OS_WriteTextfile(StdString(szTestDirectory, pathsep, "test.ini"), 
		"location_for_local_index=a\nexcludedDirs=this is the wrong place\n[group1]\n\n");
	ExpectException([&] {
		configsErrSetOutside.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	 }, "outside of a group");
	
	// check for invalid ini, arguments that need to be numeric
	OS_WriteTextfile(StdString(szTestDirectory, pathsep, "test.ini"),
		"location_for_local_index=a\n\n[group1]\n\ndirs=a\nconsiderRemovingDeletedFilesAfterThisManyWeeks=1a");
	ExpectException([&] {
		configsErrNotNumeric.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	}, "should be numeric");

	// check for invalid ini, duplicate groups
	OS_WriteTextfile(StdString(szTestDirectory, pathsep, "test.ini"),
		"location_for_local_index=a\n\n[groupFirstName]\n\ndirs=a\n[groupSecondName]\n\ndirs=a\n[groupFirstName]\n\ndirs=a\n");
	ExpectException([&] {
		configsErrDupeGroup.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	}, "duplicate group");

}
RegisterTestClassEnd(BackupConfigurationGeneralTests)

void Tests_AllTests()
{
#if __linux__
	StdString testDirectory("/tmp/downpoured_glacial_backup_tests");
	AssertThrow(0, testDirectory.Length() > 10);
	if (OS_DirectoryExists(testDirectory))
	{
		printf("please delete the directory %s before continuing", testDirectory.c_str());
		return;
	}
	else
	{
		AssertThrow(0, OS_CreateDirectory(testDirectory));
	}
	
#else
	StdString testDirectory(OS_GetDirectoryOfCurrentModule(), pathsep, "tests");
	AssertThrow(0, testDirectory.Length() > 10);
	AssertThrow(0, OS_CreateDirectory(testDirectory));
	AssertThrow(0, OS_ClearFilesInDirectory(testDirectory));
#endif
	
	AssertThrow(0, registeredTests.size() > 0);
	OS_Remove(StdString(testDirectory, pathsep, "tests.txt"));
	auto restoreLogLocation = g_logging.GetFilename();
	RunWhenLeavingScope restore([&]() { 
		if (restoreLogLocation.Length()) 
			AssertThrow(0, g_logging.Start(restoreLogLocation)); 
	});

	AssertThrow(0, g_logging.Start(StdString(testDirectory, pathsep, "tests.txt")));
	for (auto it = registeredTests.begin(); it != registeredTests.end(); ++it)
	{
		(*it)(testDirectory);
	}

	puts("tests complete.");
}
