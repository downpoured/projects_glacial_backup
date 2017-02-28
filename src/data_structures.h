#pragma once

/*
FastReadonlyMultimap
A fast and low-memory-usage way to map fixed-size binary data.

An example mapping int32 to int64:
  struct TStruct {
	  struct TStructKey {
      int32 n; 
    } key;
    int64 data;
  };
  FastReadonlyMultimap<TStruct, TStructKey> mymap;
  mymap.Finalize();
  TStruct* ret = mymap.Lookup(TStructKey(123));
  ret = mymap.LookupNext(TStructKey(123), *ret);
*/

template<class TStruct, class TStructKey>
class FastReadonlyMultimap
{
	static const size_t KeySize = sizeof(TStructKey);
	static const size_t RecordSize = sizeof(TStruct);
	static_assert(KeySize <= RecordSize, "key size cannot be greater than the record size");
	std::vector<TStruct> _vector;
	bool _finalized = false;

	static int ComparisonForSort(TStruct* t1, TStruct* t2)
	{
		return memcmp(t1, t2, sizeof(*t1));
	}
	static bool ComparisonForLowerBound(const TStruct& t1, const TStruct& t2)
	{
		/* returns true if the first argument is less than the second */
		return memcmp(&t1, &t2, KeySize /* don't compare the entire record */) < 0;
	}
	bool RecordHasThisKey(const TStructKey& key, const TStruct* recordToTestForMatch) const
	{
		if (recordToTestForMatch < _vector.begin() || recordToTestForMatch >= _vector.end())
		{
			return false;
		}

		return memcmp(&key, recordToTestForMatch, KeySize /* don't compare the entire record */) == 0;
	}

public:
	void Finalize(bool fStableSort)
	{
		if (fStableSort)
			std::stable_sort(_vector.begin(), _vector.end(), ComparisonForSort);
		else
			std::sort(_vector.begin(), _vector.end(), ComparisonForSort);

		AssertThrow(0, !_finalized);
		_finalized = true;
	}
	TStruct* Lookup(const TStructKey& key) const
	{
		AssertThrow(0, _finalized);

		/* make a mock record for searching */
		TStruct recordForSearching;
		memset(&recordForSearching, 0, sizeof(recordForSearching));
		memcpy(&recordForSearching, &key, sizeof(key));

		/* find the lower bound with binary search */
		auto itLowerBound = std::lower_bound(_vector.begin(), _vector.end(), recordForSearching, ComparisonForLowerBound);
		if (RecordHasThisKey(key, itLowerBound))
			return itLowerBound;
		else
			return nullptr;
	}
	TStruct* LookupNext(const TStructKey& key, const TStruct& current) const
	{
		AssertThrow(0, _finalized);

		if (RecordHasThisKey(key, &current + 1))
			return &current + 1;
		else
			return nullptr;
	}
	TStruct* GetBuffer() const
	{
		AssertThrow(0, _finalized);
		return &_vector[0];
	}
	size_t Length() const
	{
		AssertThrow(0, _finalized);
		return _vector.size();
	}
};

enum class PersistedRecordTypeIdentifier : Uint32
{
	Unknown,
	MetadataAudio,
	Filepaths,
	SnapshotIndex,
};

struct GlacialBackupArchiverFileHeader
{
	char textIdentifier[4];
	PersistedRecordTypeIdentifier fileType;
	Uint64 numberOfRecords;
	Uint64 integrityCheck;
	Uint64 reserved;
	char truncatedGroupName[128];
};

void AddRecordsFromFile(FILE* f)
{
	AssertThrowMsg(0, false, "function not yet implemented");
#if 0
	AssertThrow(0, !_finalized);
	auto requestedBufferSize = 8 * 1024 * 1024;
	SimpleBuffer<TStruct> bufferForFileIO(requestedBufferSize / RecordSize, false /*fillZeros*/);
	while (true)
	{
		size_t numread = fread(bufferForFileIO.Get(), RecordSize, buffer.GetMaxNumberOfElements(), f);
		if (numread == 0)
		{
			break;
		}

		size_t howManyRead = numread / RecordSize;
		_vector.insert(_vector.size(), bufferForFileIO.Get(), bufferForFileIO.Get() + howManyRead);
	}
#endif
}

struct MapFilepathToFileContentsHashRecord
{
	Uint32 cbStringLength;
	Uint32 filepathHashForFastLookup;
	Uint64 hash1, hash2, hash3;
	Uint64 filesize;
	Uint64 filelastmodified;
	/* immediately afterwards is a character array of length cbStringLength */
};

struct MapFileContentsHashToBackupLocationRecord
{
	struct RecordKey
	{
		Uint64 filecontentshash[4];
		Uint64 filesize;
	} key;

	Uint32 backupLocation;
	Uint32 countSeen;
};

static_assert(sizeof(MapFileContentsHashToBackupLocationRecord) == 48, "checking struct size");

struct MapTemporaryFilepathHashToFilepathRecord
{
	struct RecordKey
	{
		Uint32 filepathHashForFastLookup;
	} key;

	MapFilepathToFileContentsHashRecord* value;
};

struct CacheOfFileHashesRecord
{
	byte filenameUtf8[344];
	Uint64 hashpart1, hashpart2;
	Uint64 length;
	Uint64 ftModified1;
	Uint64 ftModified2;
	Uint64 reserved;
};


