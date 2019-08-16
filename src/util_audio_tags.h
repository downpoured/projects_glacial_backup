/*
util_audio_tags.h

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

typedef struct sv_hasher
{
    byte *buf;
    uint32_t buflen32u;
    spooky_state state;
    const char *loggingcontext;
} sv_hasher;

typedef enum efiletype
{
    filetype_none = 0,
    filetype_ogg,
    filetype_mp3,
    filetype_mp4,
    filetype_m4a,
    filetype_flac,
    filetype_binary,
} efiletype;

extern uint64_t SvdpHashSeed1;
extern uint64_t SvdpHashSeed2;
efiletype get_file_extension_info(const char *filename, int len);
void adjustfilesize_if_audio_file(uint32_t separatemetadata, efiletype ext,
    uint64_t size_from_disk, uint64_t *outputsize);
check_result checkbinarypaths(
    ar_util *ar, uint32_t separatemetadata, const char *temp_path);
bool readhash(const char *stdout_from_proc, hash256 *out_hash);
check_result hash_of_file(os_lockedfilehandle *handle, uint32_t separateaudio,
    efiletype ext, const char *metadatabinary, hash256 *out_hash,
    uint32_t *outcrc32);
check_result sv_basic_crc32_wholefile(
    const char *file, uint32_t *crc32);
check_result get_file_checksum_string(const char *filepath, bstring s);
check_result check_ffmpeg_works(
    ar_util *ar, uint32_t separatemetadata, const char *tmpdir);
check_result writevalidmp3(
    const char *path, bool changeid3, bool changeid3length, bool changedata);

extern const hash256 hash256zeros;
inline void hash256tostr(const hash256 *h1, bstring s)
{
    bsetfmt(s, "%llx %llx %llx %llx", castull(h1->data[0]), castull(h1->data[1]),
        castull(h1->data[2]), castull(h1->data[3]));
}

#endif
