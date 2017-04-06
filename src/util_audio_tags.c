/*
util_audio_tags.c

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
const uint64_t AudioFilesizePlaceholder = 1;

check_result get_audio_hash_without_tags_impl(
    const char *ffmpeg,
    const char *path,
    hash256 *hash)
{
    sv_result currenterr = {};
    *hash = hash256zeros;
    const char *args[] = {
        linuxonly(ffmpeg)
        "-i",
        path,
        /* disable video, don't read album art as a video stream */
        "-vn",
        /* use mp3 bitstream, not converted to signed 16-bit */
        "-c:a",
        "copy",
        "-threads",
        "1",
        "-f",
        "md5",
        /* send to stdout */
        "-",
        NULL};

    bstring getoutput = bformat("Context: get hash %s", path);
    bstring useargscombined = bstring_open();
    check(os_tryuntil_run(ffmpeg, args, getoutput,
        useargscombined, true, 0, 0));

    check_b(readhash(cstr(getoutput), hash),
        "ffmpeg failed for file %s output=%s", path, cstr(getoutput));

cleanup:
    bdestroy(getoutput);
    bdestroy(useargscombined);
    return currenterr;
}

bool readhash(const char *output, hash256 *hash)
{
    *hash = hash256zeros;
    byte *hashbytes = (byte *)hash;

    /* don't search for the string "Press [q] to stop, [?] for help",
    it's not always present. */
    const char *search = "\nMD5=";
    const char *found = strstr(output, search);

    if (found)
    {
        found += strlen(search);
        for (int j = 0; j < 32; j++)
        {
            if (!isxdigit((unsigned char)found[j]))
            {
                sv_log_fmt("not enough hex digits. %s", output);
                return false;
            }
        }

        int results[16] = { 0 };
        int matched = sscanf(found,
            "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
            &results[0], &results[1], &results[2], &results[3], &results[4],
            &results[5], &results[6], &results[7], &results[8], &results[9],
            &results[10], &results[11], &results[12], &results[13],
            &results[14], &results[15]);

        if (matched == 16)
        {
            for (int j = 0; j < 16; j++)
            {
                hashbytes[j] = (byte)results[j];
            }

            return true;
        }
        else
        {
            sv_log_fmt("not all hex digits read %d %s", matched, output);
            return false;
        }
    }
    else
    {
        sv_log_fmt("string '%s' not seen. %s", search, output);
        return false;
    }
}

check_result get_audio_hash_without_tags(const char *ffmpeg,
    const char *path,
    hash256 *hash)
{
    sv_result currenterr = {};

    /* ffmpeg sometimes returns md5(''), in which case we should retry. */
    extern uint32_t max_tries;
    const uint64_t md5empty1 = 0x4b2008fd98c1dd4ULL,
        md5empty2 = 0x7e42f8ec980980e9ULL;
    for (uint32_t attempt = 0; attempt < max_tries; attempt++)
    {
        check(get_audio_hash_without_tags_impl(ffmpeg, path, hash));
        if (hash->data[0] == md5empty1 && hash->data[1] == md5empty2)
        {
            log_b(0, "get_audio_hash_without_tags got md5('') hash.");
        }
        else
        {
            break;
        }
    }

cleanup:
    return currenterr;
}

/* if using ffmpeg, ignore filesize changes for audio files;
the changed filesize could be just due to the audio tag changing. */
void adjustfilesize_if_audio_file(uint32_t use_ffmpeg,
    efiletype ext,
    uint64_t size_from_disk,
    uint64_t *outputsize)
{
    if (use_ffmpeg && ext != filetype_none && ext != filetype_binary)
    {
        *outputsize = AudioFilesizePlaceholder;
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
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
     };

    unsigned char *bytebuf = 0;
    uint32_t crc32 = incrc32 ^ 0xFFFFFFFF;
    bytebuf = (unsigned char *)buf;
    staticassert(countof(crc_table) == 256);
    for (int i = 0; i < buflen; i++)
    {
        uint32_t index = (crc32 ^ bytebuf[i]) & 0xFF;
        crc32 = (crc32 >> 8) ^ crc_table[index];
    }

    return (crc32 ^ 0xFFFFFFFF);
}

sv_hasher sv_hasher_open(const char *loggingcontext)
{
    sv_hasher ret = {0};

    /* for benchmarks on my machines, buffer size of 64k is
    slightly faster than 4k and slightly slower than 256k */
    ret.buflen32u = 64 * 1024;
    ret.buflen32s = cast32u32s(ret.buflen32u);
    check_fatal(ret.buflen32u % 4096 == 0, "must be multiple of 4096.");
    ret.buf = os_aligned_malloc(ret.buflen32u, 4096);
    ret.loggingcontext = loggingcontext;
    spooky_init(&ret.state, SvdpHashSeed1, SvdpHashSeed2);
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

check_result sv_hasher_wholefile(sv_hasher *self,
    int fd,
    hash256 *hash,
    uint32_t *crc32)
{
    sv_result currenterr = {};
    *crc32 = 0;
    if (hash)
    {
        *hash = hash256zeros;
        spooky_init(&self->state, SvdpHashSeed1, SvdpHashSeed2);
    }

    check_b(fd > 0, "bad file handle %s", self->loggingcontext);
    check_errno(_, cast64s32s(lseek(
        fd, 0, SEEK_SET)), "%s", self->loggingcontext);

    while (true)
    {
        check_errno(int bytes, cast64s32s(read(fd, self->buf,
            self->buflen32u)), "%s", self->loggingcontext);

        if (bytes <= 0)
        {
            break;
        }

        if (hash)
        {
            spooky_update(&self->state, self->buf, cast32s32u(bytes));
        }

        *crc32 = Crc32_ComputeBuf(*crc32, self->buf, bytes);
    }

    if (hash)
    {
        spooky_final(&self->state, &hash->data[0], &hash->data[1],
            &hash->data[2], &hash->data[3]);
    }

cleanup:
    return currenterr;
}

static const uint32_t extensions_compressed[] = {
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
    {
        return 0;
    }
    else if (*(last_char - 2) == '.')
    {
        return chars_to_uint32('\0', '\0', *(last_char - 1),
            *(last_char));
    }
    else if (*(last_char - 3) == '.')
    {
        return chars_to_uint32('\0', *(last_char - 2),
            *(last_char - 1), *(last_char));
    }
    else if (*(last_char - 4) == '.')
    {
        return chars_to_uint32(*(last_char - 3),
            *(last_char - 2), *(last_char - 1), *(last_char));
    }
    else
    {
        return 0;
    }
}

efiletype get_file_extension_info(const char *filename, int len)
{
    uint32_t ext = extension_into_uint32(filename, len);
    efiletype ret = filetype_none;
    if (ext)
    {
        switch (ext)
        {
        case chars_to_uint32('\0', 'o', 'g', 'g'):
            return filetype_ogg;
        case chars_to_uint32('\0', 'm', 'p', '3'):
            return filetype_mp3;
        case chars_to_uint32('\0', 'm', 'p', '4'):
            return filetype_mp4;
        case chars_to_uint32('\0', 'm', '4', 'a'):
            return filetype_m4a;
        case chars_to_uint32('f', 'l', 'a', 'c'):
            return filetype_flac;
        }

        for (size_t i = 0; i < countof(extensions_compressed); i++)
        {
            if (ext == extensions_compressed[i])
            {
                ret = filetype_binary;
            }
        }
    }

    return ret;
}

check_result hash_of_file(os_lockedfilehandle *handle,
    uint32_t useffmpeg,
    efiletype ext,
    const char *ffmpeg,
    hash256 *out_hash,
    uint32_t *outcrc32)
{
    sv_result currenterr = {};
    sv_hasher hasher = sv_hasher_open(cstr(handle->loggingcontext));
    if (ext != filetype_none && ext != filetype_binary && useffmpeg)
    {
        const char *path = cstr(handle->loggingcontext);
        check_b(os_file_exists(path), "not found %s", path);
        check_b(os_file_exists(ffmpeg), "not found %s", ffmpeg);
        sv_result res = get_audio_hash_without_tags(
            ffmpeg, path, out_hash);

        if (res.code)
        {
            /* ok, maybe it's an audio format ffmpeg doesn't support,
            fall back to hashing data. */
            sv_result_close(&res);
            sv_log_fmt("%s type=%d falling back to hash", path, ext);
            check(sv_hasher_wholefile(&hasher, handle->fd,
                out_hash, outcrc32));
        }
        else
        {
            /* get the crc32. */
            check(sv_hasher_wholefile(&hasher, handle->fd,
                NULL, outcrc32));
        }
    }
    else
    {
        /* normal hash of the bytes on disk */
        check(sv_hasher_wholefile(&hasher, handle->fd,
            out_hash, outcrc32));
    }

cleanup:
    sv_hasher_close(&hasher);
    return currenterr;
}

check_result writevalidmp3(const char *path,
    bool changeid3,
    bool changeid3length,
    bool changedata)
{
    sv_result currenterr = {};
    byte firstdata[] = { 'I', 'D', '3', 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 'x', 'T', 'A', 'L', 'B', 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x01, 0xff, 0xfe, 'A', 0x00, 'l', 0x00, 'b', 0x00, 'u', 0x00, 'm', 0x00, 'T', 'P', 'E', '1', 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x01, 0xff, 0xfe, 'A', 0x00, 'r', 0x00, 't', 0x00, 'i', 0x00, 's', 0x00, 't', 0x00, 'C', 'O', 'M', 'M', 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x01, 'e', 'n', 'g', 0xff, 0xfe, 0x00, 0x00, 0xff, 0xfe, 'C', 0x00, 'o', 0x00, 'm', 0x00, 'm', 0x00, 'e', 0x00, 'n', 0x00, 't', 0x00, 'T', 'I', 'T', '2', 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x01, 0xff, 0xfe, 'T', 0x00, 'i', 0x00, 't', 0x00, 'l', 0x00, 'e', 0x00, 'T', 'Y', 'E', 'R', 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 'Y', 'e', 'a', 'r', };
    byte seconddata[] = { 0xff, 0xfb, 0x92, 0x60, 0xb7, 0x80, 0x05, 'd', 0x3b, 'T', 0xd3, 'X', 0xc2, 0xdc, 0x1b, 0xa0, 0x0a, 0xf3, 0x00, 0x00, 0x01, 0x94, 'D', 0xe3, 'Q', 'M', 0xe7, 0x0b, 'p', 'l', 0x80, 'l', 0x0c, 0x10, 0x00, 0x06, 0xaf, 0xff, 0xff, 0xff, 0xff, 0xfa, 0xc6, 0xaa, 'l', 'h', 0x95, '7', 0x3c, 'p', 'u', 0xc4, 'a', 0x95, 0xa9, 0x2c, 'u', 'r', 0xc0, 'Q', 'J', 0xa9, '7', 0x24, 'i', 0x09, 0xe3, 0xb0, 0x25, 0x1d, 'G', '0', 0xd1, '4', 0x3b, 0xa5, 0x29, 0xad, 0x8b, 0x1a, 0x60, 0x18, 0x88, 0x08, 0xcd, 0xc5, 'Q', 'q', 0xca, 'l', 0xab, 0x21, 0x2b, 0x09, 0x83, 'r', 0x0a, 'D', 0x9e, 0xbd, 'm', 0xd6, 0x85, 0xb2, 0x26, 0xf9, 'q', 0x23, 0xea, 0xfd, 'h', '8', 0xc2, 0xcd, 'c', 0x00, 0xf1, 0xa1, 0x95, 0x1c, '1', 0x2f, 'e', 0x0c, 0xc4, 0x1c, 'V', 0x1f, 0x2a, 'K', 0xf9, 'C', 0xb6, 0xc8, 0xdb, 0xbb, 0x08, 0x81, '3', 'N', 0x19, 0x14, 'F', 0x25, 'j', 0x3b, 'Q', 0xc6, 0x96, 'L', 0x3b, 'm', 0x99, 'M', 0xdb, 0x12, 0xc4, 0xcd, 'i', 0xa4, 0xdc, 0x0d, '3', 0x2a, 0x80, 0xa0, 0x8a, 0x93, 0xd2, 0x9c, 0xe5, 't', 0xb4, 0x7f, 0xac, 'u', '3', 'n', 0x9e, 0xde, '2', 0xaa, 0x1a, 0x1d, 'G', 0xff, 0x5c, 0xb3, 0xab, 0xbb, 0xb9, 0x86, 'Z', 0xc3, 0x2c, 0xf2, 0xdd, 0xb1, 0xc6, 0x04, 0xc5, 'X', 'E', 0xa9, 0xed, 0xf9, 'V', 'W', '1', 'W', 0xa2, 0x40, 0x00, 0x00, 0xb2, 0x01, 0x5e, 0xe3, 0x7f, 0xff, 0xff, 0xff, 0xfe, 0xb5, 0xdc, 0xed, 0xfc, 0xc8, 0xaf, 0xe7, 0x5f, 0xea, 0x24, 0x92, 'T', 'I', 0xa7, 0x2d, 0xb1, 0xa1, 'x', 0xe5, 'M', 0x86, 0x0d, 0xc7, 0x88, 0x16, 0x14, 0x2e, 'J', 'h', 0xe3, 'F', 'F', 0x16, 'X', 0x06, 0x11, 0x92, 0x17, 0x3a, 0x12, '5', '7', '9', 0xaf, 0x83, 'J', 0xca, 0xda, 0xfb, '5', 0xe4, 0xc8, 0xa0, 0x1e, 0x09, 'r', 0xd7, 0x12, 'j', 0x96, 0x05, 0xc0, 'H', 'D', 0xea, 0x08, 0xfc, 0x1e, 0x24, 0xc0, 0xb1, 0x5f, 0x5c, 'c', 'G', 0x82, 't', 0x05, 0xc7, 0xc8, 'p', 0xb3, 0x1c, 0x93, 0x03, 0x28, 0x21, 0x23, 0x90, 0x16, 'j', 0xb2, '4', 't', 0x90, 0xe7, 0x83, 0xf8, 'O', 0x13, 0xc3, 0x14, 0xfc, 0x2d, 0xa3, 0x2c, 0x19, 'b', 0x90, 0x3a, 'D', 0xe0, 'x', 0x8f, 0xd3, 0xcd, 0xed, 'E', 0xfa, 'y', 0xc4, 0xc8, 0x60, 0xc2, 0x89, '8', 0xfc, 0xf2, 'f', 'W', 0xb8, 0xc7, 'S', 0xbe, 'Y', 0xb5, 0xe0, 0x2c, 0xb0, 'R', 'C', 0x8a, 0xfb, 0x87, '4', 0xf2, 'F', 0xdc, '7', 0xd3, 'i', 0xf4, 0x8f, 0x2f, 0x3e, 0xb7, 0xd9, 0xe1, 0x03, 0x60, 0x00, 'Y', 'M', 0x1e, 0x27, 0x10, 0xb0, 0x8b, 0x9c, 0xa6, 0x0b, 0x5e, 0xb5, 'z', 'r', 0x82, 0x00, 0x00, 0x06, 0x90, 0x0c, 0xf8, 0x87, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0x0e, 0xad, 'v', 0x0c, 0xe3, 0xff, 0xfb, 0x92, 0x60, 0xb3, 0x80, 0x04, 0xbe, '7', 0xd5, 'S', 'x', 0xc2, 0xdc, 0x15, 0x40, 0x9b, 0x17, 0x04, 0x40, 0x13, 0x94, 0xcc, 0xeb, 'W', 0xad, 0xe1, 0xeb, 'p', 'r', 0x18, 'l', 0x1c, 0x10, 0x09, 'n', 'j', 0x97, 'K', 'N', 0xb9, 'r', 'o', 0xc6, 0xd0, 'U', 0x5b, 0xed, '9', 'e', 'm', 0x88, 0x2c, 0xe3, 0x93, 'H', 'J', 0xcc, 0x20, 'A', 0x98, 0x05, 'M', 'A', 0xc5, 0xc0, 0x27, 0xa2, 0xf3, 0x28, 'p', 0x60, 'j', 0xb6, 0xa3, 0xd2, 0x89, 0xb0, 0xc0, 0xf1, 'C', 0x2e, 0x9a, 0x7d, 0xf6, 0xba, 'v', 0xae, 0x89, 0x95, 0xf4, 0x89, 'o', 0xe0, 0x89, 'i', 0x1c, 0x2c, 0xf4, 0xd8, 'Z', 0xca, 0xc8, 0x95, 0x92, 0xf8, 0xa2, 0x95, 0xb7, 'G', 0x0b, 'Q', 0xd6, 0x86, 0x13, 0xc1, 0x25, 'A', 'P', 0x3c, 0x07, 0x20, 0x7e, 0x13, 0xe1, 0x21, 0x1a, 0xa8, 'R', 'U', 0x0d, 0x0f, 0x28, 'm', 0x07, 0xe6, 0x89, 0x08, 'I', 0x05, 0x88, 0x0a, 0x24, 0xb0, 0x15, 0xc7, 0x28, 0xb2, 0x3e, 'i', 'l', 0x95, 0x89, 0x3b, 0x13, 'R', 0xd7, 0x7e, 'W', 'W', 0x7b, 0x15, 0x26, 0xfd, 'z', 0x1d, 0xa4, 0x8c, 0xe1, 0x1a, 0x0b, 0xe8, 0xf1, 'P', 0x13, 0x1d, 0xa6, 0xa3, 0xc6, 0x85, 0x8c, 'K', 0x9f, 0x81, 0xd4, 'w', 0x85, 0xe8, 0xea, 0xfa, 0xea, 0x8f, 0xfe, '0', 0xe6, 0xbd, 'T', 0xf6, 0x5d, 0xb0, 0x00, 0x96, 0x40, 0x21, '5', 0xb7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 'N', 0xaf, 0x5b, 0x80, 0xc4, 0x0c, 0x7b, 0xc7, 0x12, 'Q', 0xe4, 0x9f, 0xce, 0x88, 0xc5, 0xf3, 0xee, 0x8d, 0x96, 'P', 0x1a, 0xe9, 0x27, 0x24, 'i', 0x07, 0xd0, 0xec, 'b', 0x81, 0x85, 'f', 0x84, 0x3e, 0xc0, 0x07, 'E', 'G', 0x90, 0x00, 0xc4, 0x92, 0xa0, 0xa0, 0xea, 'P', 0xc0, 0xe0, 0xf3, 0x8d, 'X', 0xb6, 0xe0, 0xe0, 0x12, 0xc8, 0xda, 0xb8, 0x87, 0xab, 0x17, 0x99, 0x24, 0x20, 0xf4, 0x12, 0x81, 0x84, 0xb6, '1', 0x80, 0x0a, 0xde, 0x98, 0xc4, 0xa3, 0xa0, 0x05, 0x03, 0x1e, 0x00, '4', 'B', 0x2a, 0x97, 0xa3, 0xc9, 0x10, 0x16, 0xa0, 0x8b, 's', 0x7c, 0x3e, 'V', 0xd3, 0x88, 'I', 0x14, 'F', 0x82, '0', 0xe2, 0x94, '8', 0x14, 0x8c, 0x81, 0x3e, 'y', '9', 0x9f, 0xe6, 0xcd, 0x82, 0xfc, 0xe2, 'S', 0x89, 0xd0, 0xe7, 0x2b, 0x07, 0x22, 'j', 'T', 0xf2, 0xa6, 0xee, 0xcf, '7', 'u', 0x7f, 'x', 0xea, 0xe8, 'R', 'E', 0x9a, 0x8a, 0x8d, 'V', 0x90, 'l', 0xdf, 0x15, 0xb5, 'G', 0x8c, 'n', 0x0f, 0xb5, '3', 0x8a, 0x7f, 'K', 'Z', 0xd7, 0xbc, 0x2f, 0xe6, 0x60, 0xa9, 'r', 0x87, 0x28, 'u', 's', 0x99, '7', 'c', 0xd8, 0xb5, 0x87, 0xcf, 0x0f, 0xe8, 0x00, 0x01, 'u', 0xa0, 'E', 0xcf, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd3, 0xfc, 0xaf, 0x9d, 0x1e, 0xb9, 0xea, 0xb3, 0xaf, 't', 0xd4, 0x1d, 'F', 0x90, 'A', 0x16, 'l', 'T', 'A', 'G', 'T', 'i', 't', 'l', 'e', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'A', 'r', 't', 'i', 's', 't', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'A', 'l', 'b', 'u', 'm', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'Y', 'e', 'a', 'r', 'C', 'o', 'm', 'm', 'e', 'n', 't', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, };
    staticassert(countof(firstdata) == 0x082);
    staticassert(countof(seconddata) == 0x3c4);

    if (changeid3)
    {
        firstdata[0x052] = 'x'; /* change id3v2 comment */
        seconddata[0x368] = 'x'; /* change id3v1 artist */
    }

    if (changedata)
    {
        seconddata[0x057] = 'r'; /* change mp3 data */
    }

    sv_file file = {};
    check(sv_file_open(&file, path, "wb"));
    fwrite(firstdata, sizeof(byte), countof(firstdata), file.file);
    const uint32_t lengthzeros = 2048;
    byte *zeros = sv_calloc(lengthzeros, sizeof(byte));
    fwrite(zeros, sizeof(byte), lengthzeros, file.file);
    fwrite(seconddata, sizeof(byte), countof(seconddata), file.file);
    if (changeid3length)
    {
        /* add 4 zero bytes, does not change mp3 bitstream */
        fwrite(zeros, sizeof(byte), 4, file.file);
    }

    sv_freenull(zeros);
    sv_file_close(&file);
    check_b(os_getfilesize(path) == (changeid3length ? 3146 : 3142),
        "wrong filesize writing %s", path);

cleanup:
    return currenterr;
}

const hash256 hash256zeros = {};
extern inline void hash256tostr(const hash256 *h1, bstring s);

check_result get_file_checksum_string(const char *path, bstring s)
{
    sv_result currenterr = {};
    hash256 hash = {};
    uint32_t crc = 0;
    uint64_t filesize = os_getfilesize(path);
    os_lockedfilehandle handle = {};
    check(os_lockedfilehandle_open(&handle, path, true, NULL));
    check(hash_of_file(&handle, 0, filetype_binary, "", &hash, &crc));
    bstrclear(s);
    hash256tostr(&hash, s);
    bformata(s, ",crc32-%08X,size-%llu", crc, filesize);

cleanup:
    os_lockedfilehandle_close(&handle);
    return currenterr;
}

check_result check_ffmpeg_works(ar_util *ar,
    uint32_t useffmpeg,
    const char *tmpdir)
{
    sv_result currenterr = {};
    hash256 hash = {};
    sv_result res = {};
    bstring path = bformat("%s%stest.mp3", tmpdir, pathsep);
    if (useffmpeg)
    {
        check_fatal(tmpdir, "null ptr");
        check_b(os_create_dirs(tmpdir), "could not create %s", tmpdir);
        check(writevalidmp3(cstr(path), false, false, false));
        const char *msg = "\n\nWe were not able to verify that ffmpeg "
            "is installed. Possible reasons: 1) ffmpeg is not correctly "
            "installed \n2) an incompatable version of ffmpeg is installed\n"
            "To continue without using this feature, go to 'Edit Group' "
            "and turn off the 'separate audio metadata' feature.\n\n";
        res = get_audio_hash_without_tags(
            cstr(ar->audiotag_binary), cstr(path), &hash);

        check_b(res.code == 0, "%s. %s", msg, cstr(res.msg));
        check_b(hash.data[0] == 0x611c9c9e3d1e62ccULL &&
            hash.data[1] == 0x1ffaa7d05b187124ULL &&
            hash.data[2] == 0 && hash.data[3] == 0,
            "%llx %llx\nhashes not equal. %s",
            castull(hash.data[0]), castull(hash.data[1]), msg);
    }

cleanup:
    sv_result_close(&res);
    bdestroy(path);
    return currenterr;
}

void get_tar_version_from_string(bstring s, double *version)
{
    *version = 0.0;
    bstr_replaceall(s, "\r\n", "\n");
    bstrlist *lines = bsplit(s, '\n');
    if (lines->qty > 0)
    {
        const char *line1 = blist_view(lines, 0);
        if (s_startwith(line1, "tar (GNU tar) "))
        {
            const char *sversion = line1 + strlen("tar (GNU tar) ");
            const char *point = strchr(sversion, '.');
            if (point)
            {
                (void)sscanf(sversion, "%lf", version);
            }
        }
    }

    bstrlist_close(lines);
}

check_result verify_tar_version(ar_util *ar)
{
    sv_result currenterr = {};
    const char *args[] = { linuxonly(cstr(ar->tar_binary))
        "--version", NULL };

    bassigncstr(ar->tmp_results, "Checking GNU tar version");
    check(os_tryuntil_run(cstr(ar->tar_binary), args, ar->tmp_results,
        ar->tmp_combined, true, 0, 0));

    double version = 0, needed = 1.19;
    get_tar_version_from_string(ar->tmp_results, &version);
    check_b(version > 1.19, "needed GNU tar version greater than %.3f "
        "but saw version %.3f.", needed, version);

cleanup:
    return currenterr;
}

#ifdef __linux__
check_result checkbinarypaths(ar_util *ar,
    uint32_t useffmpeg,
    const char *temp_path)
{
    sv_result currenterr = {};
    check(os_binarypath("xz", ar->xz_binary));
    check_b(os_file_exists(cstr(ar->xz_binary)), "We are not able to "
        "continue. This program requires 'xz', please install the "
        "package 'xz-utils' and try again.");

    check(os_binarypath("tar", ar->tar_binary));
    check_b(os_file_exists(cstr(ar->tar_binary)), "We are not able to "
        "continue. This program requires GNU 'tar'.");
    check(verify_tar_version(ar));
    if (useffmpeg)
    {
        /* example instructions for mint/ubuntu
            cat /etc/os-release, see that we are on ubuntu 14.04
            name for this version is 'trusty'
            sudo apt-add-repository ppa:mc3man/trusty-media
            sudo apt-get update
            sudo apt-get install ffmpeg */
        check(os_binarypath("ffmpeg", ar->audiotag_binary));
        check_b(os_file_exists(cstr(ar->audiotag_binary)),
            "When the use_ffmpeg feature is used, ffmpeg is needed. "
            "Please either install ffmpeg or disable the "
            "'Skip metadata changes' feature (under Edit Group).");
        check(check_ffmpeg_works(ar, useffmpeg, temp_path));
    }

cleanup:
    return currenterr;
}
#else
check_result checkbinarypaths(ar_util *ar,
    uint32_t useffmpeg,
    const char *temp_path)
{
    sv_result currenterr = {};
    bstring dir = os_getthisprocessdir();
    bstring tools = bstring_open();
    check_b(blength(dir), "We were not able to read the directory "
        "containing this exe.");

    bsetfmt(tools, "%s\\..\\..\\..\\tools", cstr(dir));
    if (!os_dir_exists(cstr(tools)))
    {
        bsetfmt(tools, "%s\\..\\..\\tools", cstr(dir));
        if (!os_dir_exists(cstr(tools)))
        {
            bsetfmt(tools, "%s\\..\\tools", cstr(dir));
            if (!os_dir_exists(cstr(tools)))
            {
                bsetfmt(tools, "%s\\tools", cstr(dir));
            }
        }
    }

    bsetfmt(ar->tar_binary, "%s%star.exe", cstr(tools), pathsep);
    bsetfmt(ar->xz_binary, "%s%sxz.exe", cstr(tools), pathsep);
    check_b(os_dir_exists(cstr(tools)), "We were not able to find "
        "the tools directory.");
    check_b(os_file_exists(cstr(ar->tar_binary)), "We were not able "
        "to find the program 'tools\\tar.exe'");
    check_b(os_file_exists(cstr(ar->xz_binary)),
        "We were not able to find the program 'tools\\xz.exe'");
    check(verify_tar_version(ar));

    if (useffmpeg)
    {
        bsetfmt(ar->audiotag_binary, "%s%sffmpeg.exe", cstr(tools), pathsep);
        check_b(os_file_exists(cstr(ar->audiotag_binary)), "We were not "
            "able to find tools\\ffmpeg.exe. Please either download "
            "ffmpeg.exe and place it in the tools directory, or disable "
            "the 'Skip metadata changes' feature (under Edit Group).");
        check(check_ffmpeg_works(ar, useffmpeg, temp_path));
    }

cleanup:
    bdestroy(dir);
    bdestroy(tools);
    return currenterr;
}
#endif
