

const Uint32 cHashSeed1 = chars_to_uint32('s', 'v', 'z', 'p');
const Uint32 cHashSeed2 = chars_to_uint32('S', 'V', 'Z', 'P');

typedef struct svdp_archiver {
	int a;
	int counter;
	bool preview;
	Uint64 cursize;
} svdp_archiver;

void svdp_archiver_close(svdp_archiver* self)
{
	if (self)
	{
		set_self_zero();
	}
}

Int32 svdp_archiver_count(svdp_archiver* self)
{
	check_fatal(0, "not yet implemented");
	return 2;
}

CHECK_RESULT Result svdp_archiver_addfile(svdp_archiver* self, const byte* nativepath,
	Uint32 cbnativepathlen, enum EnumFileExtType type, SnpshtID snapshot, Uint64 filesize, Int32* location)
{
	Result currenterr = {};
	*location = self->counter | (snapshot << 16);
	self->cursize += filesize;
	check_fatal(0, "not yet implemented");
	return currenterr;
}

CHECK_RESULT Result svdp_archiver_open(svdp_archiver* self, bool preview, const SvdpGlobalConfig* cfg, const SvdpGroupConfig* groupcfg)
{
	Result currenterr = {};
	set_self_zero();
	self->counter = 1;
	check_fatal(0, "not yet implemented");
	return currenterr;
}

CHECK_RESULT Result svdp_archiver_flush(svdp_archiver* self)
{
	Result currenterr = {};
	check_fatal(0, "not yet implemented");
	return currenterr;
}

typedef struct svdp_hashing {
	bcstr tmp1;
} svdp_hashing;

CHECK_RESULT Result svdp_hashing_open(svdp_hashing* self, const SvdpGroupConfig* groupcfg)
{
	Result currenterr = {};
	set_self_zero();
	check_fatal(0, "not yet implemented");
	return currenterr;
}

void svdp_hashing_close(svdp_hashing* self)
{
	if (self)
	{
		bcstr_close(&self->tmp1);
		set_self_zero();
	}
}

CHECK_RESULT Result svdp_hashing_gethash(svdp_hashing* self, bcdb* db, const byte* nativepath,
	Uint32 cbnativepathlen, hash192bits* hash, enum EnumFileExtType type, RowID* audioid)
{
	Result currenterr = {};
	if (type == FileExtTypeAudio)
	{
		hash->n[0] = 12345;
		hash->n[1] = 56789;
		bcstr_assign(&self->tmp1, "cool");
		check(svzpdb_getnameid(db,
			self->tmp1.s, bcstr_length(&self->tmp1),
			SvzpQryGetFileInfoId, SvzpQryAddFileInfo,
			audioid));
	}
	else
	{
		check(sphashfile192(nativepath, cHashSeed1, cHashSeed2, hash));
		*audioid = 0;
	}

cleanup:
	return currenterr;
}
