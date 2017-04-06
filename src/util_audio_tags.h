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

#ifndef UTIL_AUDIO_TAGS_H_INCLUDE
#define UTIL_AUDIO_TAGS_H_INCLUDE

#include "util_archiver.h"

typedef struct sv_hasher {
    byte *buf;
    uint32_t buflen32u;
    int32_t buflen32s;
    spooky_state state;
    const char *loggingcontext;
} sv_hasher;

check_result checkexternaltoolspresent(svdp_archiver *archiver, uint32_t separatemetadata);
bool readhash(const bstring getoutput, hash256 *out_hash);

typedef enum knownfileextension
{
    knownfileextension_none = 0,
    knownfileextension_ogg,
    knownfileextension_mp3,
    knownfileextension_mp4,
    knownfileextension_m4a,
    knownfileextension_flac,
    knownfileextension_otherbinary,
} knownfileextension;

extern uint64_t SvdpHashSeed1;
extern uint64_t SvdpHashSeed2;
check_result get_file_checksum_string(const char *filepath, bstring s);
check_result writevalidmp3(const char *path, bool changeid3, bool changeid3length, bool changedata);
knownfileextension get_file_extension_info(const char *filename, int len);
void adjustfilesize_if_audio_file(uint32_t separatemetadata, knownfileextension ext,
    uint64_t size_from_disk, uint64_t *outputsize);
check_result hash_of_file(os_exclusivefilehandle *handle, uint32_t separateaudio,
    knownfileextension ext, const char *metadatabinary, hash256 *out_hash, uint32_t *outcrc32);
check_result checkffmpegworksasexpected(svdp_archiver *archiver,
    uint32_t separatemetadata, const char *tmpdir);

#endif
