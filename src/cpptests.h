
RegisterTestClassBegin(MvSimpleLoggingTests)
{
	auto GetLoggedText = [&](const char*szTestDirectory, std::function<void(MvSimpleLogging&)> fnLogStatements)
	{
		AssertThrow(0, OS_CreateDirectory(StdString(szTestDirectory, pathsep, "testloggingdir")));
		AssertThrow(0, OS_ClearFilesInDirectory(StdString(szTestDirectory, pathsep, "testloggingdir")));
		{
			MvSimpleLogging logger;
			AssertThrow(0, logger.Start(
				StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.txt")));
			fnLogStatements(logger);
		}
		return OS_ReadFile(StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.1.txt"));
	};

	// test GetNonExistingFilename
	StdString firstName(szTestDirectory, pathsep, "scratch" pathsep "getflname.txt");
	TestEq(true, OS_CreateDirectory(StdString(szTestDirectory, pathsep, "scratch")));
	TestEq(false, OS_FileExists(firstName));
	TestEq(StdString(
		szTestDirectory, pathsep, "scratch" pathsep "getflname.1.txt"), GetLatestNonExistingFilename(firstName));
	OS_WriteFile(StdString(
		szTestDirectory, pathsep, "scratch" pathsep "getflname.1.txt"), "created a text file");
	TestEq(StdString(
		szTestDirectory, pathsep, "scratch" pathsep "getflname.2.txt"), GetLatestNonExistingFilename(firstName));

	// simple log entries
	auto loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		logger.WriteLine(StrAndLen("test_test_test"), "test1", "test2");
	});
	TestEq(true, StrContains(loggedText, " test_test_test test1 test2"));
	TestEq(false, StrContains(loggedText, "(too much text to log)"));

	// test exact text written
	loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		logger.WriteLine(StrAndLen("test_test_test"), "test1");
	});

	auto parts = StrUtilSplitString(loggedText, ' ');
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
	auto buffersize = countof(g_logging._buffer), reservedSpace = g_logging.cEnoughSpaceForDateAndTime;
	std::string testText((buffersize - reservedSpace) - 1, 'a');
	loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		logger.WriteLine(testText.c_str(), testText.size());
	});
	TestEq(testText, StrUtilSplitString(loggedText, ' ')[2]);

	// try to write too much
	testText = std::string((buffersize - reservedSpace), 'a');
	loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		logger.WriteLine(testText.c_str(), testText.size());
	});
	TestEq("(too", StrUtilSplitString(loggedText, ' ')[2]);
	TestEq("much", StrUtilSplitString(loggedText, ' ')[3]);

	// write just enough for one buffer
	loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		TestEq(0, getLocationInBuffer(logger));
		logger.WriteLine(StrAndLen("test_test_test"), "test1");
		TestEq(true, getLocationInBuffer(logger) > 0);
		std::string testText(((buffersize - getLocationInBuffer(logger)) - reservedSpace) - 1, 'a');
		logger.WriteLine(testText.c_str(), testText.size());
		
		// ensure that we haven't flushed yet
		TestEq(3, StrUtilSplitString(getBufferContents(logger), '\n').size());
	});
	TestEq(false, StrContains(loggedText, "too much"));

	// write just enough for more than one buffer
	loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		TestEq(0, getLocationInBuffer(logger));
		logger.WriteLine(StrAndLen("test_test_test"), "test1");
		TestEq(true, getLocationInBuffer(logger) > 0);
		std::string testText(((buffersize - getLocationInBuffer(logger)) - reservedSpace), 'a');
		logger.WriteLine(testText.c_str(), testText.size());
		
		// ensure that we did flush
		TestEq(2, StrUtilSplitString(getBufferContents(logger), '\n').size());
	});
	TestEq(false, StrContains(loggedText, "too much"));


	// splitting into separate files
	auto fairlyLongString = std::string(200, '1');
	loggedText = GetLoggedText(szTestDirectory, [&](MvSimpleLogging& logger) {
		setTargetSize(logger, 128 * 1024);
		for (int i = 0; i < 2 * 1024; i++)
		{
			logger.WriteLine(fairlyLongString.c_str(), fairlyLongString.size());
		}
	});

	TestEq(true, OS_FileExists(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.1.txt")));
	TestEq(true, OS_FileExists(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.2.txt")));
	TestEq(true, OS_FileExists(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.3.txt")));
	TestEq(true, OS_FileExists(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.4.txt")));
	TestEq(false, OS_FileExists(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.5.txt")));
	TestEq(false, OS_FileExists(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.6.txt")));
	TestEq(true, OS_GetFileSize(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.1.txt")) > 90 * 1024);
	TestEq(true, OS_GetFileSize(
		StdString(szTestDirectory, pathsep, "testloggingdir", pathsep, "testlogging.1.txt")) < 190 * 1024);
}

static const char* getBufferContents(MvSimpleLogging& logger) { return logger._buffer; }
static size_t getLocationInBuffer(MvSimpleLogging& logger) { return logger._locationInBuffer; }
static void setTargetSize(MvSimpleLogging& logger, int newSize) { logger._targetFileSize = newSize;  }
RegisterTestClassEnd(MvSimpleLoggingTests)

RegisterTestClassBegin(HashingTests)
{
	const char* collide1 = "cb_koders.zip";
	const char* collide2 = "defaultmimehandler.zip";
	Uint32 hsh1 = SpookyHash::Hash32(collide1, strlen(collide1), 1/*seed*/) % 1024;
	Uint32 hsh2 = SpookyHash::Hash32(collide2, strlen(collide2), 1/*seed*/) % 1024;
	AssertThrowMsg(0, hsh1 == hsh2);
	Uint32 hsh1differentSeed = SpookyHash::Hash32(collide1, strlen(collide1), 2/*seed*/) % 1024;
	Uint32 hsh2differentSeed = SpookyHash::Hash32(collide2, strlen(collide2), 2/*seed*/) % 1024;
	AssertThrowMsg(0, hsh1differentSeed != hsh2differentSeed);
}
RegisterTestClassEnd(HashingTests)

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


#ifdef _WIN32
typedef std::function<bool(
	const char*, size_t, Uint64 /*modtime*/, Uint64 /*filesize*/)> FnRecurseFilesCallbackNarrow;
inline void RecurseFilesNarrow(const char* dir, FnRecurseFilesCallbackNarrow callback, 
	const std::vector<StdString>& disallowedDirectories = std::vector<StdString>())
{
	RecurseFiles(dir, [&](const oschartype* wz, size_t lenwz, Uint64 modtime, Uint64 filesize) {
		return callback(UIApproximateNarrowString(wz), lenwz, modtime, filesize);
	}, disallowedDirectories);
}
#else
#define RecurseFilesNarrow RecurseFiles
#endif

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
	auto callback = [&](const char* szFullpath, size_t lenFullpath, Uint64 modtime, Uint64 filesize)
	{
		// as a small perf optimization we kept the string length. make sure it matches.
		TestEq(lenFullpath, strlen(szFullpath));
		auto whichFile = VectorLinearSearch(filesExpected, StdString(szFullpath));
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
	
	// create and clear directories
	AssertThrow(0, OS_CreateDirectory(dir));
	AssertThrow(0, OS_CreateDirectory(subdir1));
	AssertThrow(0, OS_CreateDirectory(subdir2));
	AssertThrow(0, OS_ClearFilesInDirectory(dir));
	AssertThrow(0, OS_ClearFilesInDirectory(subdir1));
	AssertThrow(0, OS_ClearFilesInDirectory(subdir2));

	// confirm that directories are empty
	TestEq(0, OS_ListFilesInDirectory(
		dir, true /*includeFiles*/, false /*includeDirectories*/, "*").size());
	TestEq(0, OS_ListFilesInDirectory(
		subdir1, true /*includeFiles*/, true /*includeDirectories*/, "*").size());
	TestEq(0, OS_ListFilesInDirectory(
		subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*").size());
	TestEq(0, OS_ListFilesInDirectory(
		"notexist", true /*includeFiles*/, true /*includeDirectories*/, "*").size());
	auto listDir = OS_ListFilesInDirectory(
		dir, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {subdir1, subdir2}), listDir);

	// test recursing through directories with no files
	RecurseFilesNarrow(dir, callback);
	TestEq(0, filesSeenWhenRecursing.size());
	RecurseFilesNarrow(subdir1, callback);
	TestEq(0, filesSeenWhenRecursing.size());
	RecurseFilesNarrow(subdir2, callback);
	TestEq(0, filesSeenWhenRecursing.size());
	RecurseFilesNarrow("notexist", callback);
	TestEq(0, filesSeenWhenRecursing.size());

	// test file existence
	for (size_t i = 0; i < filesExpected.size(); i++)
	{
		// we'll arbitrarily set the length of the file based on its index in filesExpected, and test it later
		OS_WriteFile(filesExpected[i], std::string(i + 1, 'a').c_str());
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
	listDir = OS_ListFilesInDirectory(
		dir, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[0], filesExpected[1], subdir1, subdir2}), listDir);
	listDir = OS_ListFilesInDirectory(
		dir, true /*includeFiles*/, false /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[0], filesExpected[1]}), listDir);
	listDir = OS_ListFilesInDirectory(
		subdir1, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[2], filesExpected[3], filesExpected[4]}), listDir);
	listDir = OS_ListFilesInDirectory(
		subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[5]}), listDir);

	// test recursing through a subdir
	RecurseFilesNarrow(subdir1, callback);
	TestEq((std::vector<StdString> {
		filesExpected[2], filesExpected[3], filesExpected[4]}), filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();

	// test recursing through everything
	RecurseFilesNarrow(dir, callback);
	TestEq(filesExpected, filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();

	// test excluded directories
	RecurseFilesNarrow(dir, callback, std::vector<StdString> {subdir1});
	TestEq((std::vector<StdString> {
		filesExpected[0], filesExpected[1], filesExpected[5]}), filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();
	RecurseFilesNarrow(dir, callback, std::vector<StdString> {subdir1, subdir2});
	TestEq((std::vector<StdString> {
		filesExpected[0], filesExpected[1]}), filesSeenWhenRecursing);
	filesSeenWhenRecursing.clear();

	// test that the modified time is updated if we modify the file
	::Sleep(500);
	TestEq(true, modTimeSeenForTextFile != 0);
	Uint64 modTimePrevious = modTimeSeenForTextFile;
	OS_WriteFile(textFileInSubdir1, "aaaa" /*make an identical file with same length in bytes*/);
	RecurseFilesNarrow(dir, callback);
	TestEq(filesExpected, filesSeenWhenRecursing);
	TestEq(true, modTimeSeenForTextFile > modTimePrevious);

	// test file deletion
	AssertThrow(0, OS_Remove(filesExpected[3]));
	listDir = OS_ListFilesInDirectory(
		subdir1, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString> {filesExpected[2], filesExpected[4]}), listDir);
	AssertThrow(0, OS_Remove(filesExpected[5]));
	listDir = OS_ListFilesInDirectory(
		subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq(std::vector<StdString>(), listDir);

	// test file copy
	TestEq(false, OS_CopyFileAndOverwriteExisting(
		"notexist1", "|badpath|invalid"));
	TestEq(false, OS_CopyFileAndOverwriteExisting(
		filesExpected[2], "|badpath|invalid"));
	TestEq(false, OS_CopyFileAndOverwriteExisting(
		"notexist", StdString(subdir2, pathsep, "afile")));
	TestEq(true, OS_CopyFileAndOverwriteExisting(
		filesExpected[2], StdString(subdir2, pathsep, "copied_file1")));
	TestEq(true, OS_CopyFileAndOverwriteExisting(
		StdString(subdir2, pathsep, "copied_file1"), StdString(subdir2, pathsep, "copied_file2")));
	listDir = OS_ListFilesInDirectory(
		subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString>{
		StdString(subdir2, pathsep, "copied_file1"), StdString(subdir2, pathsep, "copied_file2")}), listDir);

	// test copy overwrites
	OS_WriteFile(StdString(subdir2, pathsep, "different file"), "qqqq");
	TestEq(true, OS_CopyFileAndOverwriteExisting(
		StdString(subdir2, pathsep, "different file"), StdString(subdir2, pathsep, "copied_file2")));
	TestEq("qqqq", OS_ReadFile(StdString(subdir2, pathsep, "copied_file2")));

	// test file rename
	TestEq(true, OS_RenameFileWithoutOverwriteExisting(
		StdString(subdir2, pathsep, "different file"), StdString(subdir2, pathsep, "different file2")));
	TestEq(true, OS_RenameFileWithoutOverwriteExisting(
		StdString(subdir2, pathsep, "copied_file1"), StdString(subdir2, pathsep, "testOverwrite")));
	TestEq(false, OS_RenameFileWithoutOverwriteExisting(
		StdString(subdir2, pathsep, "copied_file2"), StdString(subdir2, pathsep, "testOverwrite")));
	listDir = OS_ListFilesInDirectory(
		subdir2, true /*includeFiles*/, true /*includeDirectories*/, "*", true/*sorted*/);
	TestEq((std::vector<StdString>{StdString(subdir2, pathsep, "copied_file2"), 
		StdString(subdir2, pathsep, "different file2"), StdString(subdir2, pathsep, "testOverwrite")}), listDir);

#ifdef WIN32
	TestEq(true, StrEqual(L"c:\\test\\f", UIApproximateWideString("c:\\test\\f").c_str()));
	TestEq(true, StrEqual("", UIApproximateNarrowString(L"")));
	TestEq("a B c D", UIApproximateNarrowString(L"a B c D"));
	TestEq("c:\\test\\f", UIApproximateNarrowString(L"c:\\test\\f"));
	
	// add utf16 character
	std::wstring utf16filenameSeen, utf16filenamePrefix = UIApproximateWideString(
		StdString(szTestDirectory, pathsep, "fileutf16"));
	std::wstring utf16filename = utf16filenamePrefix + L"\u8349.txt";
	OS_Remove(utf16filename.c_str());
	FILE* f = 0;
	_wfopen_s(&f, utf16filename.c_str(), L"wb");
	fclose(f);

	RecurseFiles(szTestDirectory, [&](const oschartype* wz, size_t wzLen, Uint64, Uint64) {
		TestEq(wzLen, wcslen(wz));
		if (StrStartsWith(wz, utf16filenamePrefix.c_str()))
		{
			TestEq(0, utf16filenameSeen.size());
			utf16filenameSeen = wz;
		}
		return true; });
	TestEq(true, utf16filename == utf16filenameSeen);
	OS_Remove(utf16filename.c_str());
#endif

}
RegisterTestClassEnd(UtilOSTests)


RegisterTestClassBegin(BackupConfigurationGeneralTests)
{
	TestEq((std::vector<StdString>{}), StrUtilSplitString());
	TestEq((std::vector<StdString>{"a ", "b\r\n", "c\n", ""}), StrUtilSplitString("a |b\r\n|c\n|", '|'));
	TestEq((std::vector<StdString>{"", "", "a", "", "b", "c", "", ""}), StrUtilSplitString("||a||b|c||", '|'));
	
	TestEq((std::vector<StdString>{"aaa\n"}), StrUtilSplitString("aaa\n", '|'));
	TestEq((std::vector<StdString>{"", ""}), StrUtilSplitString("|", '|'));
	TestEq((std::vector<StdString>{"", "", ""}), StrUtilSplitString("||", '|'));
	TestEq((std::vector<StdString>{}), StrUtilSplitString("", '|'));

	// parse the demo
	BackupConfigurationGeneral::WriteDemoConfigFile(StdString(szTestDirectory, pathsep, "demo.ini"));
	TestEq(true, OS_FileExists(StdString(szTestDirectory, pathsep, "demo.ini")));
	BackupConfigurationGeneral configsDemo, configsThorough, 
		configsErrSetOutside, configsErrNotNumeric, configsErrDupeGroup;
	configsDemo.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "demo.ini"));
	TestEq(0, configsDemo._testing_countUnrecognizedParameters);
	TestEq(true, strlen(configsDemo.GetLocationForLocalIndex()) > 0);
	TestEq(true, strlen(configsDemo.GetLocationForFilesReadyToUpload()) > 0);
	TestEq(true, StrContains(configsDemo.GetPathFfmpeg(), "optionallyProvidePathTo"));
	TestEq(true, StrContains(configsDemo.GetPath7zip(), "pleaseProvidePathTo"));
	TestEq(1, configsDemo.ViewGroups().size());
	TestEq("group1", configsDemo.ViewGroups()[0].GetGroupName());
	TestEq("Password1", configsDemo.ViewGroups()[0].GetPass());
	TestEq("AES-256", configsDemo.ViewGroups()[0].GetEncryption());
	TestEq(false, configsDemo.ViewGroups()[0].GetSeparatelyTrackTaggingChangesInAudioFiles());
	TestEq(4, configsDemo.ViewGroups()[0].GetConsiderRemovingDeletedFilesAfterThisManyWeeks());
	TestEq(64 * 1024, configsDemo.ViewGroups()[0].GetTargetArchiveSizeInKb());
	TestEq(2, configsDemo.ViewGroups()[0].ViewDirs()->size());
	TestEq((std::vector<StdString>{}), *configsDemo.ViewGroups()[0].ViewExcludedDirs());
	TestEq((std::vector<StdString>{"tmp", "pyc"}), *configsDemo.ViewGroups()[0].ViewExcludedExtensions());

	// parse a more thorough ini
	std::ostringstream stm;
	stm << "locationForLocalIndex=locationForLocalIndex" "\n";
	stm << "#locationForLocalIndex=commentedOut" "\n";
	stm << "# comment 1" "\n";
	stm << "; comment 2" "\n";
	stm << "locationForFilesReadyToUpload=locationForFilesReadyToUpload" "\n";
	stm << "7zip=7zip" "\n";
	stm << "ffmpeg=ffmpeg" "\n";
	stm << "\n" << "\n" << "[groupWithMostSet]" "\n";
	stm << StdString("dirs=", pathsep, "firstDirectoryInGroup", pathsep, 
		"subdir", "|", pathsep, "otherDirectoryInGroup") << "\n";
	stm << "excludedDirs=a|b|c" "\n";
	stm << "excludedExtensions=abcd|1234|DCBA" "\n";
	stm << "separatelyTrackTaggingChangesInAudioFiles=1" "\n";
	stm << "considerRemovingDeletedFilesAfterThisManyWeeks=100" "\n";
	stm << "pass=pass" "\n";
	stm << "encryption=encryption" "\n";
	stm << "targetArchiveSizeInMb=90" "\n";
	stm << "unknown1=1000" "\n";
	stm << "unknown2=abc" "\n";
	stm << "unknownIgnored=" "\n";
	stm << "[groupWithMostBlank]" "\n";
	stm << "dirs=a" "\n";
	stm << "dirs=b" "\n";
	stm << "targetArchiveSizeInMb=" "\n";

	OS_WriteFile(StdString(szTestDirectory, pathsep, "test.ini"), stm.str().c_str(), false /*binary*/);
	configsThorough.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	TestEq(2, configsThorough._testing_countUnrecognizedParameters);
	TestEq("locationForLocalIndex", configsThorough.GetLocationForLocalIndex());
	TestEq("locationForFilesReadyToUpload", configsThorough.GetLocationForFilesReadyToUpload());
	TestEq("ffmpeg", configsThorough.GetPathFfmpeg());
	TestEq("7zip", configsThorough.GetPath7zip());
	TestEq(2, configsThorough._groups.size());
	TestEq("groupWithMostSet", configsThorough._groups[0].GetGroupName());

	TestEq((std::vector<StdString>{
		StdString(pathsep, "firstDirectoryInGroup", pathsep, "subdir"), StdString(pathsep, "otherDirectoryInGroup")}), *configsThorough._groups[0].ViewDirs());
	TestEq((std::vector<StdString>{
		"a", "b", "c"}), *configsThorough._groups[0].ViewExcludedDirs());
	TestEq((std::vector<StdString>{
		"abcd", "1234", "DCBA"}), *configsThorough._groups[0].ViewExcludedExtensions());

	TestEq(true, configsThorough.ViewGroups()[0].GetSeparatelyTrackTaggingChangesInAudioFiles());
	TestEq(100, configsThorough.ViewGroups()[0].GetConsiderRemovingDeletedFilesAfterThisManyWeeks());
	TestEq("pass", configsThorough.ViewGroups()[0].GetPass());
	TestEq("encryption", configsThorough.ViewGroups()[0].GetEncryption());
	TestEq(90 * 1024, configsThorough.ViewGroups()[0].GetTargetArchiveSizeInKb());
	TestEq("groupWithMostBlank", configsThorough.ViewGroups()[1].GetGroupName());
	TestEq((std::vector<StdString>{"b"}), *configsThorough.ViewGroups()[1].ViewDirs());
	TestEq((std::vector<StdString>{}), *configsThorough.ViewGroups()[1].ViewExcludedDirs());
	TestEq((std::vector<StdString>{}), *configsThorough.ViewGroups()[1].ViewExcludedExtensions());
	TestEq(false, configsThorough.ViewGroups()[1].GetSeparatelyTrackTaggingChangesInAudioFiles());
	TestEq(MAXINT, configsThorough.ViewGroups()[1].GetConsiderRemovingDeletedFilesAfterThisManyWeeks());
	TestEq(true, StrEqual("", configsThorough.ViewGroups()[1].GetPass()));
	TestEq(true, StrEqual("", configsThorough.ViewGroups()[1].GetEncryption()));
	TestEq(64 * 1024, configsThorough.ViewGroups()[1].GetTargetArchiveSizeInKb());

	// check for invalid ini, settings that need to be in group
	OS_WriteFile(StdString(szTestDirectory, pathsep, "test.ini"), 
		"locationForLocalIndex=a\nexcludedDirs=this is the wrong place\n[group1]\n\n");
	ExpectException([&] {
		configsErrSetOutside.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	 }, "outside of a group");
	
	// check for invalid ini, arguments that need to be numeric
	OS_WriteFile(StdString(szTestDirectory, pathsep, "test.ini"),
		"locationForLocalIndex=a\n\n[group1]\n\ndirs=a\nconsiderRemovingDeletedFilesAfterThisManyWeeks=1a");
	ExpectException([&] {
		configsErrNotNumeric.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	}, "should be numeric");

	// check for invalid ini, duplicate groups
	OS_WriteFile(StdString(szTestDirectory, pathsep, "test.ini"),
		"locationForLocalIndex=a\n\n[groupFirstName]\n\ndirs=a\n[groupSecondName]\n\ndirs=a\n[groupFirstName]\n\ndirs=a\n");
	ExpectException([&] {
		configsErrDupeGroup.ParseFromConfigFile(StdString(szTestDirectory, pathsep, "test.ini"));
	}, "duplicate group");

}
RegisterTestClassEnd(BackupConfigurationGeneralTests)


RegisterTestClassBegin(ClassifyFileExtensionTests)
{
	ExpectException([&] {
		ClassifyFileExtension lookup(std::vector<StdString>{"e1", "e2", "e3", "e4", "e5", "e6"});
	}, "currently only support 5 user-defined extensions");

	ExpectException([&] {
		ClassifyFileExtension lookup(std::vector<StdString>{"tmp", "*.pyc"});
	}, "specify just the extension");

	ExpectException([&] {
		ClassifyFileExtension lookup(std::vector<StdString>{"tmp", ".pyc"});
	}, "specify just the extension");

	ExpectException([&] {
		ClassifyFileExtension lookup(std::vector<StdString>{"tmp", ""});
	}, "only support extensions of");

	ExpectException([&] {
		ClassifyFileExtension lookup(std::vector<StdString>{"tmp", "a"});
	}, "only support extensions of");

	ExpectException([&] {
		ClassifyFileExtension lookup(std::vector<StdString>{"tmp", "abcde"});
	}, "only support extensions of");

	
	{ // every position within the array should work
		ClassifyFileExtension lookupFirst1(std::vector<StdString>{"foo"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified == 
			lookupFirst1.GetExt(StrAndLen("/path/to/testfile.foo")));

		ClassifyFileExtension lookupFirst2(std::vector<StdString>{"foo", "tmp"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified == 
			lookupFirst2.GetExt(StrAndLen("/path/to/testfile.foo")));

		ClassifyFileExtension lookupSecond(std::vector<StdString>{"tmp", "foo"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified == 
			lookupSecond.GetExt(StrAndLen("/path/to/testfile.foo")));

		ClassifyFileExtension lookup3(std::vector<StdString>{"tmp", "tmp", "foo"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified == 
			lookup3.GetExt(StrAndLen("/path/to/testfile.foo")));

		ClassifyFileExtension lookup4(std::vector<StdString>{"tmp", "tmp", "tmp", "foo"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified == 
			lookup3.GetExt(StrAndLen("/path/to/testfile.foo")));

		ClassifyFileExtension lookup5(std::vector<StdString>{"tmp", "tmp", "tmp", "tmp", "foo"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified == 
			lookup3.GetExt(StrAndLen("/path/to/testfile.foo")));
	}

	{ // user specified, every supported length
		ClassifyFileExtension lookup(std::vector<StdString>{"ab", "cde", "fghi"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.ab")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.cde")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.fghi")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.fghI")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.fgHi")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.fGhi")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.Fghi")));

		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.nab")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.ncde")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.nfghi")));

		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.dots.ab")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.dots.cde")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.dots.fghi")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.ab.dots")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.cde.dots")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.fghi.dots")));

		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.otherab")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.abcde")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.cdefghi")));
	}

	{ // strings that are too short
		ClassifyFileExtension lookup(std::vector<StdString>{"tmp"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("tmp")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen(".tmp")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testweirdextensions")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testweirdextensions.")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testweirdextensions..")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testweirdextensions...")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testweirdextensions....")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.tmp")));
	}

	{ // built-in, every supported length
		ClassifyFileExtension lookup(std::vector<StdString>{});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::NotVeryCompressable ==
			lookup.GetExt(StrAndLen("/path/to/testfile.7z")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::ContainsAudioMetadata ==
			lookup.GetExt(StrAndLen("/path/to/testfile.mp3")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::ContainsAudioMetadata ==
			lookup.GetExt(StrAndLen("/path/to/testfile.flac")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.other7z")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.othermp3")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::Other ==
			lookup.GetExt(StrAndLen("/path/to/testfile.otherflac")));
	}

	{ // user specified need to take precedence
		ClassifyFileExtension lookup(std::vector<StdString>{"7z", "mp3", "flac"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.7z")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.mp3")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile.flac")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::ContainsAudioMetadata ==
			lookup.GetExt(StrAndLen("/path/to/testfile.mp4")));
	}

	{ // corner case, we should treat "...ab" as ".ab" and not "...ab"
		ClassifyFileExtension lookup(std::vector<StdString>{"ab", "abc", "abcd"});
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile...ab")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile...abc")));
		TestEq(true, ClassifyFileExtension::ClassifyFileExtensionType::UserSpecified ==
			lookup.GetExt(StrAndLen("/path/to/testfile...abcd")));
	}

	{ // check packing
		std::vector<Uint32> seenUnique;
		seenUnique.emplace_back(PackCharactersIntoUint32(0, 0, 0, 0));
		seenUnique.emplace_back(PackCharactersIntoUint32(0, 0, 0, UCHAR_MAX));
		seenUnique.emplace_back(PackCharactersIntoUint32(0, 0, UCHAR_MAX, 0));
		seenUnique.emplace_back(PackCharactersIntoUint32(0, UCHAR_MAX, 0, 0));
		seenUnique.emplace_back(PackCharactersIntoUint32(UCHAR_MAX, 0, 0, 0));
		seenUnique.emplace_back(PackCharactersIntoUint32(UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX));
		std::sort(seenUnique.begin(), seenUnique.end());
		for (int i=0; i<seenUnique.size()-1; i++)
			TestEq(true, seenUnique[i] != seenUnique[i+1]);
		
		TestEq(0xffffffff, PackCharactersIntoUint32(UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX));
	}
}
RegisterTestClassEnd(ClassifyFileExtensionTests)

class IterThroughDataSet
{
	CStyleFile _f;
	char _buffer[1000];
	const char* _szFilename = "listoffiles52k.txt";
	const char* Root = "/path/test";
public:
	IterThroughDataSet()
	{
		FILE* f = 0;
		fopen_s(&f, _szFilename, "rb");
		AssertThrow(0, f != 0);
		_f.Attach(f);

	}

	const bool Next(const char*& outStrPtr, size_t& outStrLen)
	{
		outStrPtr = nullptr;
		outStrLen = 0;
		memset(&_buffer[0], 0, sizeof(_buffer));
		int strLenWithPadding = 0, strLen = 0;
		size_t nread = fread(&strLenWithPadding, sizeof(strLenWithPadding), 1, _f);
		if (!nread) return false;
		nread = fread(&strLen, sizeof(strLen), 1, _f);
		AssertThrow(0, nread > 0);
		AssertThrow(0, strLenWithPadding < sizeof(_buffer) - 1);
		AssertThrow(0, strLen < sizeof(_buffer) - 1);

		nread = fread(&_buffer[0], strLenWithPadding, 1, _f);
		AssertThrow(0, nread > 0);
		AssertThrow(0, StrStartsWith(_buffer, "/path/test"));
		outStrPtr = _buffer;
		outStrLen = strLen;
		return true;
	}
};

void GetPathStatistics()
{
	int totalSeen = 0;
	Uint32 hashSeed = 0x12345, hashEverything = 0;
	size_t totalbytes = 0;
	PerfTimer timeIt;
	for (int ii=0; ii<400; ii++)
	{
		const int cbuffersize = 64*1024;
		byte* buffer = new byte[cbuffersize];
		const char* szFilename = "large data.mp4";

		// read by chunks
		FILE* f = fopenwrapper(szFilename, "rb");
		size_t nRead = 0;
		SpookyHash hash;
		hash.Init(hashSeed, hashSeed);
		while ((nRead = fread(&buffer[0], 1, cbuffersize, f)) != 0)
		{
			totalbytes += nRead;
			hash.Update(buffer, nRead);
		}

		fclose(f);
		delete[] buffer;
		UINT64 hashes[4];
		hash.Final(&hashes[0], &hashes[1], &hashes[2], &hashes[3]);
		hashEverything += (Uint32)hashes[0];
	}

	printf("read %f Mb totalhash=%d \n", totalbytes/(1024.0*1024.0), (Uint32)hashEverything);
	printf("\ncompleted in %f seconds\n", timeIt.stop());
}

void Tests_AllTests()
{
#if __linux__
	StdString testDirectory(OS_GetDirectoryOfCurrentModule(), pathsep, "tests");
#else
	StdString testDirectory("/tmp/downpoured_glacial_backup_tests");
#endif
	
	AssertThrow(0, testDirectory.Length() > 10);
	AssertThrow(0, OS_CreateDirectory(testDirectory));
	AssertThrow(0, OS_ClearFilesInDirectory(testDirectory));
	AssertThrow(0, registeredTests.size() > 0);
	OS_Remove(StdString(testDirectory, pathsep, "tests.txt"));
	auto restoreLogLocation = g_logging.GetFilename();
	RunWhenLeavingScope restore([&]() { 
		if (restoreLogLocation.Length()) {
			AssertThrow(0, g_logging.Start(restoreLogLocation));
		}
	});

	AssertThrow(0, g_logging.Start(StdString(testDirectory, pathsep, "tests.txt")));
	for (auto it = registeredTests.begin(); it != registeredTests.end(); ++it)
	{
		(*it)(testDirectory);
	}

	printf("tests complete.\n");
}
