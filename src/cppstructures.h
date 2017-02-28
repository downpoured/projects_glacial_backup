#pragma once

const Uint32 cProgramSignature =
	PackCharactersIntoUint32('m', 'v', 't', 'k');

const Uint32 cHashSeed1 = cProgramSignature, cHashSeed2 = cProgramSignature;
enum class MvFileType : Uint32
{
	Unknown,
	SnapshotIndex_v1,
	Filepaths_v1,
	MetadataAudio_v1,
};

#pragma pack(push, 1)
struct MvStructFileHeader
{
	Uint32 fileSignature;
	MvFileType fileType;
	Uint64 checksum;
	Uint64 timeWritten;
	Uint32 numberOfArchives;
	Uint32 numberOfRecords;
	char truncatedGroupName[128];
};
static_assert(sizeof(MvStructFileHeader) == 160, "check struct size");

struct MvFilepathIdentifier
{
	Uint32 hash_filepath1;
	Uint32 hash_filepath2;
	Uint32 hash_filepath3;
	Uint32 filelength;
};
static_assert(sizeof(MvFilepathIdentifier) == 16, "check struct size");

struct MvArchiveLocation
{
	Uint16 snapshot_number;
	Uint16 archive_number;
};

struct MvStructIndexEntry
{
	MvFilepathIdentifier filepathidentifier;
	Uint32 contenthash[4];
	Uint32 lastmodifiedtime;
	MvArchiveLocation where_archived;
};
static_assert(sizeof(MvStructIndexEntry) == 40, "check struct size");

struct MvStructContentHashToPathHashEntry
{
	Uint32 firstbitsoffilepathidentifier;
	Uint32 indexIntoList;
};
static_assert(sizeof(MvStructContentHashToPathHashEntry) == 8, "check struct size");

struct MvStructFilepathsEntry
{
	MvStructIndexEntry entry;
	Uint32 filesizemod4gb;
	Uint32 cbLenStringIncludingPadding;
};
static_assert(sizeof(MvStructFilepathsEntry) == 48, "check struct size");

#pragma pack(pop)

void MvStructFileHeader_ReadFromFile(FILE* f, MvStructFileHeader* header)
{
	memset(header, 0, sizeof(*header));
	size_t nRead = fread(header, sizeof(*header), 1, f);
	AssertThrowMsg(0, nRead < 1, "file is too small");
	AssertThrowMsg(0, header->fileSignature == cProgramSignature, 
		"file does not have correct signature. (note we do not support mixing *nix and Win32).");
	header->truncatedGroupName[countof(header->truncatedGroupName)-1] = '\0';
}

UINT64 MvStructFileHeader_GetHashOfRemainingFile(FILE* f)
{
	const int cbuffersize = 64*1024;
	byte* buffer = (byte*)malloc(sizeof(byte)*cbuffersize);

	SpookyHash hash;
	hash.Init(cHashSeed1, cHashSeed2);
	size_t nRead = 0;
	while ((nRead = fread(&buffer[0], 1, cbuffersize, f)) != 0)
	{
		hash.Update(buffer, nRead);
	}
	free(buffer);

	UINT64 hashes[4];
	hash.Final(&hashes[0], &hashes[1], &hashes[2], &hashes[3]);
	return hashes[0];
}

// if record sizes are variable, pass 0 for recordSize
void MvStructFileHeader_CheckFileIntegrity(const char* filepath, MvFileType type, size_t recordSize)
{
	Uint64 filesize = OS_GetFileSize(filepath);
	CStyleFile file;
	file.Attach(fopenwrapper(filepath, "rb"));
	AssertThrowMsg(0, file, "could not open file");
	MvStructFileHeader header;
	MvStructFileHeader_ReadFromFile(file, &header);
	AssertThrowMsg(0, header.fileType == type, "incorrect file type");
	AssertThrowMsg(0, !recordSize || filesize == sizeof(header)+(Uint64)header.numberOfRecords * recordSize, "file size doesn't look right");
	UINT64 remainingHash = MvStructFileHeader_GetHashOfRemainingFile(file);
	AssertThrowMsg(0, header.checksum == remainingHash, "checksum of file does not look correct");
}

typedef int(*ComparisonFnPtr)(const void *pt1, const void *pt2);
#define MakeComparisonFn(fnname, sizeInBytes) \
	int ComparisonFunction_##fnname(const void* pt1, const void* pt2) {\
		return memcmp(pt1, pt2, sizeInBytes);\
	}

MakeComparisonFn(IndexEntry, sizeof(MvStructIndexEntry))
MakeComparisonFn(IndexEntryKeySearch, offsetof(MvFilepathIdentifier, filelength))
MakeComparisonFn(ContentHashToPathHashEntry, sizeof(MvStructContentHashToPathHashEntry))
MakeComparisonFn(ContentHashToPathHashEntryKeySearch, offsetof(MvStructContentHashToPathHashEntry, indexIntoList))

// memory-efficient data structure for data that is frequently searched and infrequently updated.
// contiguous memory buffer with fixed-size records, each record must begin with a Key of size _keySize.
// the structure maps from a Key to a set of records matching this key
// users must pass in comparison functions as there is not a portible qsort_r or bsearch_r.
struct ReadonlyMultimap
{
	size_t _keySize;
	size_t _recordSize;
	bool _ready;
	HeapArray _buffer;
	ComparisonFnPtr _compareRecords;
	void Init(size_t recordSize, ComparisonFnPtr compareRecords)
	{
		_recordSize = recordSize;
		_compareRecords = compareRecords;
		_ready = false;
	}

	void Sort()
	{
		qsort(_buffer.Get(), _buffer.LengthInRecords(), _recordSize, _compareRecords);
		_ready = true;
	}

	byte* LookupFirst(const byte* key, ComparisonFnPtr compareKeyToRecord)
	{
		AssertThrow(0, _ready);
		byte* pointed = (byte*)bsearch(key, _buffer.Get(), _buffer.LengthInRecords(), _recordSize, compareKeyToRecord);
		if (pointed)
			return WalkBackToFirstMatch(pointed, key, compareKeyToRecord);
		else
			return nullptr;
	}

	byte* WalkBackToFirstMatch(byte* ptr, const byte* key, ComparisonFnPtr compareKeyToRecord)
	{
		AssertThrow(0, _ready);
		while (true)
		{
			if (ptr == _buffer.Get() || // at the first record
				compareKeyToRecord(key, ptr-_recordSize) != 0) // the one before us doesn't match
				return ptr;
			ptr -= _recordSize;
			AssertThrow(0, ptr >= _buffer.Get());
		}
		Throw(0, "not reached");
	}
	
	byte* WalkNextMatch(byte* ptr, const byte* key, ComparisonFnPtr compareKeyToRecord)
	{
		AssertThrow(0, _ready);
		if (ptr >= _buffer.GetRecordByIndex(_buffer.LengthInRecords() - 1) || // at the last record
			compareKeyToRecord(key, ptr+_recordSize) != 0) // does the record match
			return nullptr;

		return ptr+_recordSize;
	}
};

// map from filepathhash, to information about a file
struct MapPathHashToFileInformationReader
{
	ReadonlyMultimap _map;
	void Init()
	{
		_map.Init(sizeof(MvStructIndexEntry) /* size of record */, 
			ComparisonFunction_IndexEntry /* compare 2 records */);
	}

	void LoadFromFile(const char* filepath)
	{
		Uint64 filesize = OS_GetFileSize(filepath);
		FILE* f = fopenwrapper(filepath, "rb");
		AssertThrowMsg(0, f != nullptr, "could not open file ", filepath);
		
		MvStructFileHeader header;
		MvStructFileHeader_ReadFromFile(f, &header);
		AssertThrowMsg(0, header.fileType == MvFileType::SnapshotIndex_v1, "incorrect file type");
		AssertThrowMsg(0, multiplication_is_safe64((Uint64)header.numberOfRecords, sizeof(MvStructIndexEntry)));
		const size_t bytesWanted = (Uint64)header.numberOfRecords * sizeof(MvStructIndexEntry);
		AssertThrowMsg(0, filesize == sizeof(header)+bytesWanted, "file size doesn't look right");
		_map._buffer.Reserve(bytesWanted);
		size_t nRead = fread(&_map._buffer._buffer[0], bytesWanted, 1, f);
		AssertThrowMsg(0, nRead == bytesWanted, "fread failed");
		fclose(f);
		_map.Sort();
	}

	Inline MvStructIndexEntry* Access()
	{
		return (MvStructIndexEntry*)&_map._buffer._buffer[0];
	}

	MvStructIndexEntry* FindFromFilePathHash(Uint32 pathhash1, Uint32 pathhash2, Uint32 pathhash3)
	{
		MvFilepathIdentifier obj;
		obj.hash_filepath1 = pathhash1;
		obj.hash_filepath2 = pathhash2;
		obj.hash_filepath3 = pathhash3;
		return (MvStructIndexEntry*)_map.LookupFirst((byte*) &obj, ComparisonFunction_IndexEntryKeySearch);
	}
};

struct MapContentHashToPathHash
{
	ReadonlyMultimap _mapContentHash;
	void Init(MapPathHashToFileInformationReader* mapMain)
	{
		AssertThrow(0, mapMain->_map._ready);
		_mapContentHash.Init(sizeof(MvStructContentHashToPathHashEntry) /* size of record */, 
			ComparisonFunction_ContentHashToPathHashEntry /* compare 2 records */);

		MvStructContentHashToPathHashEntry currentBatch;
		Uint32 mapSize = (Uint32) mapMain->_map._buffer.LengthInRecords();
		auto array = mapMain->Access();

		for (Uint32 i=0; i<mapSize; i++)
		{
			currentBatch.firstbitsoffilepathidentifier = array[i].contenthash[0];
			currentBatch.indexIntoList = i;
			_mapContentHash._buffer.Add((byte*)&currentBatch, sizeof(currentBatch));
		}
		_mapContentHash.Sort();
	}
};

struct MvFilepathsFileReader
{
	FILE* _file;
	Uint32 _rowsExpected;
	void Init(const char* filepath, MvFileType typeExpected)
	{
		_file = fopenwrapper(filepath, "rb");
		AssertThrowMsg(0, _file, "could not open file ", filepath);
		MvStructFileHeader header;
		MvStructFileHeader_ReadFromFile(_file, &header);
		AssertThrowMsg(0, header.fileType == typeExpected, "incorrect file type");
		_rowsExpected = header.numberOfRecords;
	}

	Uint32 GetNumberOfExpectedRows()
	{
		return _rowsExpected;
	}

	bool ReadNext(MvStructFilepathsEntry* record, oschartype** strAllocced)
	{
		AssertThrow(0, strAllocced && !*strAllocced);
		memset(record, 0, sizeof(*record));
		size_t nRead = fread(record, sizeof(*record), 1, _file);
		if (nRead != sizeof(*record))
			return false;

		*strAllocced = (oschartype*)calloc(record->cbLenStringIncludingPadding + sizeof(oschartype), 1);
		nRead = fread(*strAllocced, record->cbLenStringIncludingPadding, 1, _file);
		AssertThrow(0, nRead == record->cbLenStringIncludingPadding);
		return true;
	}

	~MvFilepathsFileReader() { fclose(_file); }
};

struct MvFileWriter
{
	FILE* _file;
	Uint32 _rowsAdded;
	SpookyHash _hasher;
	StdString _filename;
	MvFileType _filetype;
	void Init(const char* filepath, MvFileType type, const char* szGroupname, Uint64 timeWritten)
	{
		_filename = filepath;
		_file = fopenwrapper(filepath, "wb");
		AssertThrowMsg(0, _file, "could not open file ", filepath);
		MvStructFileHeader header;
		memset(&header, 0, sizeof(header));
		header.checksum = 0; //we'll set this later
		header.fileSignature = cProgramSignature;
		header.fileType = type;
		strncpy(&header.truncatedGroupName[0], szGroupname, sizeof(header.truncatedGroupName)-1);
		header.timeWritten = timeWritten;
		fwrite(&header, sizeof(header), 1, _file);
		_hasher.Init(cHashSeed1, cHashSeed2);
	}

	void AddVariableSizeRecord(MvStructFilepathsEntry* record, const byte* str, size_t strLenInBytes)
	{
		AssertThrowMsg(0, _filetype == MvFileType::Filepaths_v1 || _filetype == MvFileType::MetadataAudio_v1);
		AssertThrowMsg(0, strLenInBytes < INT_MAX);
		record->cbLenStringIncludingPadding = (Uint32)alignUpToNearest(strLenInBytes, 8);
		fwrite(record, sizeof(*record), 1, _file);
		_hasher.Update(record, sizeof(*record));

		fwrite(str, strLenInBytes, 1, _file);
		_hasher.Update(str, strLenInBytes);

		byte bufferZeros[8] = {};
		AssertThrowMsg(0, record->cbLenStringIncludingPadding-strLenInBytes < sizeof(bufferZeros));
		fwrite(&bufferZeros[0], record->cbLenStringIncludingPadding-strLenInBytes, 1, _file);
		_hasher.Update(&bufferZeros[0], record->cbLenStringIncludingPadding-strLenInBytes);

		_rowsAdded++;
	}

	void AddFixedSizeRecord(const byte* record, size_t recordSize)
	{
		fwrite(record, recordSize, 1, _file);
		_hasher.Update(record, recordSize);
		_rowsAdded++;
	}

	void Finalize(Uint32 numberOfArchives=0)
	{
		fclose(_file);
		_file = 0;
		Uint64 hash[4] = {};
		_hasher.Final(&hash[0], &hash[1], &hash[2], &hash[3]);
		Uint64 finalHashForChecksum = hash[0];

		// check file integrity
		_file = fopenwrapper(_filename, "rb+");
		fseek(_file, 0, SEEK_SET);
		MvStructFileHeader headerPrevious;
		MvStructFileHeader_ReadFromFile(_file, &headerPrevious);
		Uint64 hashReceived = MvStructFileHeader_GetHashOfRemainingFile(_file);
		AssertThrowMsg(0, hashReceived == finalHashForChecksum, "file io error");
		fseek(_file, 0, SEEK_SET);
		headerPrevious.checksum = finalHashForChecksum;
		headerPrevious.numberOfRecords = _rowsAdded;
		headerPrevious.numberOfArchives = numberOfArchives;
		fwrite(&headerPrevious, sizeof(headerPrevious), 1, _file);
		fclose(_file);
		_file = 0;
	}

	~MvFileWriter()
	{ 
		if (_file)
		{
			fclose(_file);
		}
	}
};
