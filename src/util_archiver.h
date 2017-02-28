/*
GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef WRAP_ARCHIVER_H_INCLUDE
#define WRAP_ARCHIVER_H_INCLUDE

#include "util_os.h"
#include "lib_sphash.h"

typedef struct svdp_archiverstrings {
	bstring paramInput;
	bstring paramEncryption;
	bstring paramOutput;
	bstring errmsg;
	bstring useforargscombined;
	bstring pathparentdir;
	bstring pathfilename;
} svdp_archiverstrings;

typedef struct svdp_archiver {
	bstring workingdir;
	bstring readydir;
	uint64_t target_archive_size_bytes;
	bstring encryption;
	bstring path_archiver_binary;
	bstring path_compression_binary;
	bstring path_audiometadata_binary;
	svdp_archiverstrings strings;
	bstring currentarchive;
	uint32_t currentcollectionid;
	uint32_t currentarchivecountfiles;
	uint32_t currentarchivenumber;
	bstrList* list;
	sv_file friendlyfilenamesfile;
} svdp_archiver;

typedef struct hash256 {
	uint64_t data[4];
} hash256;

check_result svdp_archiver_open(svdp_archiver* self, const char* pathapp, const char* groupname,
	uint32_t currentcollectionid, uint32_t targetsizemb, const char* encryption);
void svdp_archiver_close(svdp_archiver* self);
check_result svdp_archiver_addfile(svdp_archiver* self, const char* pathinput, 
	bool iscompressed, uint64_t contentsid, uint32_t* archivenumber);
check_result svdp_archiver_beginarchiving(svdp_archiver* self);
check_result svdp_archiver_finisharchiving(svdp_archiver* self);
check_result svdp_archiver_delete_from_archive(svdp_archiver* self, const char* archive, 
	const sv_array* contentids, bstrList* messages);
check_result svdp_archiver_restore_from_archive(svdp_archiver* self, const char* archive, 
	uint64_t contentid, const char* workingdir, const char* subworkingdir, const char* dest);
check_result svdp_archiver_tar_list_string_testsonly(svdp_archiver* self, const char* archive,
	bstrList* results, const char* subworkingdir);

check_result svdp_tar_add(svdp_archiver* self, const char* tarname, const char* inputfile);
check_result svdp_7z_add(svdp_archiver* self, const char* path7zfile, 
	const char* inputfilename, bool is_compressed);
check_result svdp_7z_extract_overwrite_ok(svdp_archiver* self, const char* archive, 
	const char* encryption, const char* internalarchivepath, const char* outputdir);
check_result svdp_archiver_7z_list_string_testsonly(svdp_archiver* self, const char* archive,
	bstrList* results);
check_result svdp_archiver_7z_list_testsonly(svdp_archiver* self, const char* archive,
	bstrList* paths, sv_array* sizes, sv_array* crcs, sv_array* methods);

inline void hash256tostr(const hash256* h1, bstring s)
{
	bassignformat(s, "%llx %llx %llx %llx",
		castull(h1->data[0]), castull(h1->data[1]), castull(h1->data[2]), castull(h1->data[3]));
}

inline const char* get7zerr(int retcode)
{
	switch (retcode)
	{
	case 0:
		return "";
	case 1:
		return "Partial failure";
	case 2:
		return "Non-recoverable failure";
	case 7:
		return "Command-line failure";
	case 8:
		return "Out-of-memory";
	case 255:
		return "User stopped the process";
	default:
		return "";
	}
}


#endif
