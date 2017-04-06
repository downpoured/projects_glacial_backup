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

#include "util_audio_tags.h"

uint64_t SvdpHashSeed1 = 0;
uint64_t SvdpHashSeed2 = 0;
const uint64_t AUDIOFILESIZEPLACEHOLDER = 1;

check_result get_audiostream_hash_excluding_metadata_once(const char *ffmpeg,
    const char *path, hash256 *out_hash)
{
    sv_result currenterr = {};
    memset(out_hash, 0, sizeof(*out_hash));
    const char *args[] = {
        linuxonly(ffmpeg)
        "-i",
        path,
        "-vn", /* disable video; otherwise id3 artwork is used as video stream */
        "-c:a",
        "copy", /* we want the mp3 bitstream and not the audio converted to signed 16-bit raw */
        "-threads",
        "1",
        "-f",
        "md5",
        "-", /* stdout */
        NULL};

    bstring getoutput = bstring_open();
    bstring useargscombined = bstring_open();
    int retcode = os_tryuntil_run_process(ffmpeg, args, false, useargscombined, getoutput);
    check_b(retcode == 0, "ffmpeg failed for file %s ret=%d output=%s", path, retcode, cstr(getoutput));
    check_b(readhash(getoutput, out_hash), "ffmpeg failed for file %s output=%s", path, cstr(getoutput));

cleanup:
    bdestroy(getoutput);
    bdestroy(useargscombined);
    return currenterr;
}

bool readhash(const bstring getoutput, hash256 *out_hash)
{
    bool ret = false;
    *out_hash = hash256zeros;
    byte *hashbytes = (byte *)out_hash;
    tagbstring delim = bstr_static("\nMD5=");
    bstrlist *parts = bsplitstr(getoutput, &delim);
    sv_log_writefmt("reading md5 #parts=%d", parts->qty);
    for (int i = 1; i < parts->qty; i++)
    {
        bool isgood = true;
        if (blength(parts->entry[i]) >= 32)
        {
            for (int j = 0; j < 32; j++)
            {
                if (!isxdigit((unsigned char)bstrlist_view(parts, i)[j]))
                {
                    isgood = false;
                    sv_log_writefmt("not enough hex digits. %s", cstr(getoutput));
                    break;
                }
            }
        }
        else
        {
            sv_log_writefmt("string too short. %s", cstr(getoutput));
            isgood = false;
        }

        if (isgood)
        {
            int results[16] = { 0 };
            int matched = sscanf(bstrlist_view(parts, i),
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                &results[0], &results[1], &results[2], &results[3], &results[4],
                &results[5], &results[6], &results[7], &results[8], &results[9],
                &results[10], &results[11], &results[12], &results[13], &results[14], &results[15]);
            log_b(matched == 16, "not all hex digits read %d %s", matched, cstr(getoutput));
            if (matched == 16)
            {
                for (int j = 0; j < 16; j++)
                {
                    hashbytes[j] = (byte)results[j];
                }

                ret = true;
                break;
            }
        }
    }

    bstrlist_close(parts);
    return ret;
}

check_result get_audiostream_hash_excluding_metadata(const char *ffmpeg,
    const char *path, hash256 *out_hash)
{
    sv_result currenterr = {};
    const uint64_t md5emptystring1 = 0x4b2008fd98c1dd4ULL, md5emptystring2 = 0x7e42f8ec980980e9ULL;
    extern uint32_t max_tries;
    for (uint32_t attempt = 0; attempt < max_tries; attempt++)
    {
        /* ffmpeg sometimes returns md5(''), in which case we should retry. */
        check(get_audiostream_hash_excluding_metadata_once(ffmpeg, path, out_hash));
        if (out_hash->data[0] == md5emptystring1 && out_hash->data[1] == md5emptystring2)
        {
            log_b(0, "get_audiostream_hash_excluding_metadata got md5('') hash.");
        }
        else
        {
            break;
        }
    }

cleanup:
    return currenterr;
}

/* editing an id3 tag can change the filesize. and so if we are in separate-metadata mode,
a different filesize doesn't mean the file has actually changed, maybe only the metadata changed. */
void adjustfilesize_if_audio_file(uint32_t separatemetadata,
    knownfileextension ext, uint64_t size_from_disk, uint64_t *outputsize)
{
    if (separatemetadata && ext != knownfileextension_none &&
        ext != knownfileextension_otherbinary)
    {
        *outputsize = AUDIOFILESIZEPLACEHOLDER;
    }
    else
    {
        *outputsize = size_from_disk;
    }
}

/* CRC-32 version 2.0.0 by Craig Bruce, 2006-04-29. */
/* http://www.csbruce.com/~csbruce/software/crc32.c */
/* PUBLIC-DOMAIN SOFTWARE. */
static uint32_t Crc32_ComputeBuf(uint32_t incrc32, const void *buf, int buflen)
{
    static const uint32_t crc_table[] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535,
        0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD,
        0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D,
        0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
        0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4,
        0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC,
        0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
        0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
        0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB,
        0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA,
        0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE,
        0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
        0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409,
        0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739,
        0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
        0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344, 0x8708A3D2, 0x1E01F268,
        0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0,
        0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8,
        0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF,
        0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703,
        0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
        0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
        0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE,
        0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6,
        0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D,
        0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5,
        0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605,
        0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D };
    uint32_t crc32 = 0;
    unsigned char *bytebuf = 0;

    /* accumulate crc32 for buffer */
    crc32 = incrc32 ^ 0xFFFFFFFF;
    bytebuf = (unsigned char *)buf;
    for (int i = 0; i < buflen; i++) {
        crc32 = (crc32 >> 8) ^ crc_table[(crc32 ^ bytebuf[i]) & 0xFF];
    }
    return (crc32 ^ 0xFFFFFFFF);
}

sv_hasher sv_hasher_open(const char *loggingcontext)
{
    sv_hasher ret = {0};
    spooky_init(&ret.state, SvdpHashSeed1, SvdpHashSeed2);
    ret.buflen32u = 64 * 1024;
    /* benchmarks for my config say 64k is faster than 4k and slower than 256k */
    ret.buflen32s = cast32u32s(ret.buflen32u);
    check_fatal(ret.buflen32u % 4096 == 0, "buffer length should be multiple of 4096.");
    ret.buf = os_aligned_malloc(ret.buflen32u, 4096);
    ret.loggingcontext = loggingcontext;
    return ret;
}

void sv_hasher_close(sv_hasher *self)
{
    if (self)
    {
        os_aligned_free(&self->buf);
        set_self_zero();
    }
}

check_result sv_hasher_wholefile(sv_hasher *self, int fd, hash256 *out_hash, uint32_t *outcrc32)
{
    sv_result currenterr = {};
    *outcrc32 = 0;
    if (out_hash)
    {
        memset(out_hash, 0, sizeof(*out_hash));
        spooky_init(&self->state, SvdpHashSeed1, SvdpHashSeed2);
    }

    check_b(fd > 0, "bad file handle for %s", self->loggingcontext);
    check_errno(_, cast64s32s(lseek(fd, 0, SEEK_SET)), self->loggingcontext);
    while (true)
    {
        check_errno(int bytes_read, cast64s32s(read(
            fd, self->buf, self->buflen32u)), self->loggingcontext);
        if (bytes_read <= 0)
        {
            break;
        }

        if (out_hash)
        {
            spooky_update(&self->state, self->buf, cast32s32u(bytes_read));
        }

        *outcrc32 = Crc32_ComputeBuf(*outcrc32, self->buf, bytes_read);
    }

    if (out_hash)
    {
        spooky_final(&self->state,
            &out_hash->data[0], &out_hash->data[1], &out_hash->data[2], &out_hash->data[3]);
    }
cleanup:
    return currenterr;
}

static const uint32_t extensions_audio[] = {
    chars_to_uint32('\0', 'o', 'g', 'g'),
    chars_to_uint32('\0', 'm', 'p', '3'),
    chars_to_uint32('\0', 'm', 'p', '4'),
    chars_to_uint32('\0', 'm', '4', 'a'),
    chars_to_uint32('f', 'l', 'a', 'c')
};

static const uint32_t extensions_already_compressed[] = {
    chars_to_uint32('\0', '\0', '7', 'z'),
    chars_to_uint32('\0', '\0', 'g', 'z'),
    chars_to_uint32('\0', '\0', 'x', 'z'),
    chars_to_uint32('\0', 'a', 'c', 'e'),
    chars_to_uint32('\0', 'a', 'p', 'e'),
    chars_to_uint32('\0', 'a', 'r', 'j'),
    chars_to_uint32('\0', 'b', 'z', '2'),
    chars_to_uint32('\0', 'c', 'a', 'b'),
    chars_to_uint32('\0', 'j', 'p', 'g'),
    chars_to_uint32('\0', 'l', 'h', 'a'),
    chars_to_uint32('\0', 'l', 'z', 'h'),
    chars_to_uint32('\0', 'r', 'a', 'r'),
    chars_to_uint32('\0', 't', 'a', 'z'),
    chars_to_uint32('\0', 't', 'g', 'z'),
    chars_to_uint32('\0', 'z', 'i', 'p'),
    chars_to_uint32('\0', 'p', 'n', 'g'),
    chars_to_uint32('\0', 'a', 'v', 'i'),
    chars_to_uint32('\0', 'm', 'o', 'v'),
    chars_to_uint32('\0', 'm', 'p', '4'),
    chars_to_uint32('\0', 'f', 'l', 'v'),
    chars_to_uint32('\0', 'w', 'm', 'a'),
    chars_to_uint32('j', 'p', 'e', 'g'),
    chars_to_uint32('w', 'e', 'b', 'p'),
    chars_to_uint32('w', 'e', 'b', 'm'),
    chars_to_uint32('a', 'l', 'a', 'c')
};

uint32_t extension_into_uint32(const char *s, int len)
{
    const char *last_char = s + (len - 1);
    if (len < 5)
        return 0;
    else if (*(last_char - 2) == '.')
        return chars_to_uint32('\0', '\0', *(last_char - 1), *(last_char));
    else if (*(last_char - 3) == '.')
        return chars_to_uint32('\0', *(last_char - 2), *(last_char - 1), *(last_char));
    else if (*(last_char - 4) == '.')
        return chars_to_uint32(*(last_char - 3), *(last_char - 2), *(last_char - 1), *(last_char));
    else
        return 0;
}

knownfileextension get_file_extension_info(const char *filename, int len)
{
    uint32_t extuint32 = extension_into_uint32(filename, len);
    knownfileextension ret = knownfileextension_none;
    if (extuint32)
    {
        staticassert(countof(extensions_audio) == 5);
        if (extuint32 == extensions_audio[0])
            ret = knownfileextension_ogg;
        if (extuint32 == extensions_audio[1])
            ret = knownfileextension_mp3;
        if (extuint32 == extensions_audio[2])
            ret = knownfileextension_mp4;
        if (extuint32 == extensions_audio[3])
            ret = knownfileextension_m4a;
        if (extuint32 == extensions_audio[4])
            ret = knownfileextension_flac;

        for (size_t i = 0; i < countof(extensions_already_compressed); i++)
        {
            if (extuint32 == extensions_already_compressed[i])
            {
                ret = knownfileextension_otherbinary;
            }
        }
    }
    return ret;
}

check_result hash_of_file(os_exclusivefilehandle *handle, uint32_t separateaudio,
    knownfileextension ext, const char *metadatabinary, hash256 *out_hash, uint32_t *outcrc32)
{
    sv_result currenterr = {};
    sv_hasher hasher = sv_hasher_open(cstr(handle->loggingcontext));
    if (ext != knownfileextension_none && ext != knownfileextension_otherbinary && separateaudio)
    {
        const char *path = cstr(handle->loggingcontext);
        check_b(os_file_exists(path), "file not found %s", path);
        check_b(os_file_exists(metadatabinary), "file not found %s", metadatabinary);
        sv_result res = get_audiostream_hash_excluding_metadata(metadatabinary, path, out_hash);
        if (res.code)
        {
            /* this is ok, maybe it's an audio format ffmpeg doesn't support yet.
            we'll fall back to sphash. */
            sv_log_writefmt("for %s type=%d falling back to sphash", path, ext);
            check(sv_hasher_wholefile(&hasher, handle->fd, out_hash, outcrc32));
            sv_result_close(&res);
        }
        else
        {
            /* we successfully got the md5. get the crc32. */
            check(sv_hasher_wholefile(&hasher, handle->fd, NULL, outcrc32));
        }
    }
    else
    {
        check(sv_hasher_wholefile(&hasher, handle->fd, out_hash, outcrc32));
    }
cleanup:
    sv_hasher_close(&hasher);
    return currenterr;
}



check_result get_file_checksum_string(const char *filepath, bstring s)
{
    sv_result currenterr = {};
    hash256 hash = {};
    uint32_t crc = 0;
    uint64_t filesize = os_getfilesize(filepath);
    os_exclusivefilehandle handle = {};
    check(os_exclusivefilehandle_open(&handle, filepath, true, NULL));
    check(hash_of_file(&handle, 0, knownfileextension_otherbinary, "", &hash, &crc));
    bstrclear(s);
    hash256tostr(&hash, s);
    bformata(s, ",crc32-%08X,size-%llu", crc, filesize);
cleanup:
    os_exclusivefilehandle_close(&handle);
    return currenterr;
}

check_result checkffmpegworksasexpected(svdp_archiver *archiver,
    uint32_t separatemetadata, const char *tmpdir)
{
    sv_result currenterr = {};
    if (separatemetadata)
    {
        bstring path = bformat("%s%stest.mp3", tmpdir, pathsep);
        hash256 hash = {};
        check_b(os_create_dirs(tmpdir), "could not create %s", tmpdir);
        check(writevalidmp3(cstr(path), false, false, false));
        const char *msg = "\n\nWe were not able to verify that ffmpeg is installed. "
            "Possible reasons: 1) ffmpeg is not correctly installed 2) an incompatable "
            "version of ffmpeg is installed\n"
            "To continue without using this feature, go to 'Edit Group' and turn off the "
            "'separate audio metadata' feature.";
        sv_result res = get_audiostream_hash_excluding_metadata(
            cstr(archiver->path_audiometadata_binary), cstr(path), &hash);
        check_b(res.code == 0, "%s. %s", cstr(res.msg), msg);
        check_b(hash.data[0] == 0x611c9c9e3d1e62ccULL && hash.data[1] == 0x1ffaa7d05b187124ULL
            && hash.data[2] == 0 && hash.data[3] == 0,
            "%llx %llx\nhashes not equal. %s", castull(hash.data[0]), castull(hash.data[1]), msg);

    cleanup:
        sv_result_close(&res);
        bdestroy(path);
    }
    return currenterr;
}

void get_tar_version_from_string(bstring s, double *outversion)
{
    *outversion = 0.0;
    bstr_replaceall(s, "\r\n", "\n");
    bstrlist *lines = bsplit(s, '\n');
    if (lines->qty > 0)
    {
        const char *line1 = bstrlist_view(lines, 0);
        if (s_startswith(line1, "tar (GNU tar) "))
        {
            const char *version = line1 + strlen("tar (GNU tar) ");
            const char *point = strchr(version, '.');
            if (point)
            {
                (void)sscanf(version, "%lf", outversion);
            }
        }
    }

    bstrlist_close(lines);
}

check_result verify_tar_version(svdp_archiver *archiver)
{
    sv_result currenterr = {};
    int retcode = -1;
    const char *args[] = { linuxonly(cstr(archiver->path_archiver_binary)) "--version", NULL };
    check(os_run_process(cstr(archiver->path_archiver_binary), args, true, archiver->tmp_argscombined,
        archiver->tmp_resultstring, &retcode));
    check_b(retcode == 0, "needed GNU tar version greater than 1.19. got return code %d", retcode);
    double version = 0;
    get_tar_version_from_string(archiver->tmp_resultstring, &version);
    check_b(version > 1.19, "needed GNU tar version greater than 1.19 but saw version %.3f.", version);
cleanup:
    return currenterr;
}

#ifdef __linux__
check_result checkexternaltoolspresent(svdp_archiver *archiver, uint32_t separatemetadata)
{
    sv_result currenterr = {};
    check(os_binarypath("7z", archiver->path_compression_binary));
    check_b(os_file_exists(cstr(archiver->path_compression_binary)),
        "We are not able to continue. This program requires '7zip' to be installed. "
        "Please install the package called 'p7zip-full' and try again.");

    check(os_binarypath("tar", archiver->path_archiver_binary));
    check_b(os_file_exists(cstr(archiver->path_archiver_binary)),
        "We are not able to continue. This program requires GNU 'tar' to be installed.");
    check(verify_tar_version(archiver));

    if (separatemetadata)
    {
        /* cat /etc/os-release to confirm we are on ubuntu 14.04, which is 'trusty'
            sudo apt-add-repository ppa:mc3man/trusty-media
            sudo apt-get update
            sudo apt-get install ffmpeg
        */
        check(os_binarypath("ffmpeg", archiver->path_audiometadata_binary));
        check_b(os_file_exists(cstr(archiver->path_audiometadata_binary)),
            "When the separatemetadata feature is used, ffmpeg is needed. Please either install "
            "'ffmpeg' or edit this group "
            "to disable the separatemetadata feature.");
        check(checkffmpegworksasexpected(archiver, separatemetadata,
            cstr(archiver->path_intermediate_archives)));
    }
cleanup:
    return currenterr;
}
#else
check_result checkexternaltoolspresent(svdp_archiver *archiver, uint32_t separatemetadata)
{
    sv_result currenterr = {};
    bstring thisdir = os_getthisprocessdir();
    bstring toolsdir = bstring_open();
    check_b(blength(thisdir), "We were not able to read the directory containing this exe.");

    bassignformat(toolsdir, "%s\\..\\..\\..\\tools", cstr(thisdir));
    if (!os_dir_exists(cstr(toolsdir)))
    {
        bassignformat(toolsdir, "%s\\..\\..\\tools", cstr(thisdir));
        if (!os_dir_exists(cstr(toolsdir)))
        {
            bassignformat(toolsdir, "%s\\..\\tools", cstr(thisdir));
            if (!os_dir_exists(cstr(toolsdir)))
            {
                bassignformat(toolsdir, "%s\\tools", cstr(thisdir));
            }
        }
    }

    check_b(os_dir_exists(cstr(toolsdir)), "We were not able to find the 'tools' directory.");
    bassignformat(archiver->path_archiver_binary, "%s%star.exe", cstr(toolsdir), pathsep);
    check_b(os_file_exists(cstr(archiver->path_archiver_binary)), "We were not able to find the "
        "program 'tools\\tar.exe'");
    check(verify_tar_version(archiver));

    bassignformat(archiver->path_compression_binary, "%s%s7z.exe", cstr(toolsdir), pathsep);
    check_b(os_file_exists(cstr(archiver->path_compression_binary)),
        "We were not able to find the program 'tools\\7z.exe'");

    if (separatemetadata)
    {
        bassignformat(archiver->path_audiometadata_binary, "%s%sffmpeg.exe", cstr(toolsdir), pathsep);
        check_b(os_file_exists(cstr(archiver->path_audiometadata_binary)),
            "We were not able to find tools\\ffmpeg.exe. Please either add this file, or edit this "
            "group to disable the separatemetadata feature.");
        check(checkffmpegworksasexpected(archiver, separatemetadata,
            cstr(archiver->path_intermediate_archives)));
    }

cleanup:
    bdestroy(thisdir);
    bdestroy(toolsdir);
    return currenterr;
}
#endif
