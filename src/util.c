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

#include "util_os.h"

sv_array sv_array_open(uint32_t elementsize, uint32_t initialcapacity)
{
	sv_array self = {};
	self.elementsize = elementsize;
	if (initialcapacity)
	{
		self.buffer = sv_calloc(initialcapacity, elementsize);
		self.capacity = initialcapacity;
	}

	return self;
}

void sv_array_reserve(sv_array* self, uint32_t requestedcapacity)
{
	if (requestedcapacity <= self->capacity)
	{
		return;
	}

	/* double container size until large enough.*/
	requestedcapacity = nearest_power_of_two32(requestedcapacity);
	self->buffer = sv_realloc(self->buffer, requestedcapacity, self->elementsize);
	self->capacity = requestedcapacity;
}

void sv_array_append(sv_array* self, const void* inbuffer, uint32_t incount)
{
	sv_array_reserve(self, checkedadd32(self->length, incount));
	uint32_t index = checkedmul32(self->length, self->elementsize);
	memcpy(&self->buffer[index], inbuffer, checkedmul32(self->elementsize, incount));
	self->length += incount;
}

void sv_array_appendzeros(sv_array* self, uint32_t incount)
{
	sv_array_reserve(self, checkedadd32(self->length, incount));
	uint32_t index = checkedmul32(self->length, self->elementsize);
	memset(&self->buffer[index], 0, checkedmul32(self->elementsize, incount));
	self->length += incount;
}

void sv_array_truncatelength(sv_array* self, uint32_t newlength)
{
	check_fatal(newlength <= self->length, "out-of-bounds read");
	self->length = newlength;
}

void sv_array_clear(sv_array* self)
{
	sv_array_truncatelength(self, 0);
}

byte* sv_array_at(sv_array* self, uint32_t index)
{
	return (byte*)sv_array_atconst(self, index);
}

const byte* sv_array_atconst(const sv_array* self, uint32_t index)
{
	check_fatal(index < self->length, "out-of-bounds read");
	uint32_t index_bytes = checkedmul32(index, self->elementsize);
	return &self->buffer[index_bytes];
}

/* helper for making an array of uint64_t. */
uint64_t sv_array_at64u(const sv_array* self, uint32_t index)
{
	check_fatal(self->elementsize == sizeof32u(uint64_t), "unexpected elementsize");
	return *(const uint64_t*)sv_array_atconst(self, index);
}

/* helper for making an array of uint64_t. */
void sv_array_add64u(sv_array* self, uint64_t n)
{
	check_fatal(self->elementsize == sizeof32u(uint64_t), "unexpected elementsize");
	sv_array_append(self, (byte*)&n, 1);
}

/* nb: all warnings resolved in next checkin */
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

/* binary search (list must be sorted), returns index of lower bound
input must be >= at least one array element
*/
uint32_t sv_array_64ulowerbound(sv_array* self, uint64_t n)
{
	check_fatal(self->elementsize == sizeof32u(uint64_t), "unexpected elementsize");
	check_fatal(self->length > 0, "list cannot be empty");
	check_fatal(n >= sv_array_at64u(self, 0), "input must be >= least one array element");
	uint32_t lo = 0;
	uint32_t hi = cast32u32s(self->length);
	while (lo < hi)
	{
		int32_t mid = lo + (hi - lo) / 2;
		uint64_t current = sv_array_at64u(self, mid);
		if (current >= n)
		{
			hi = mid;
		}
		else
		{
			lo = mid + 1;
		}
	}

	if (lo < self->length && sv_array_at64u(self, lo) == n)
	{
		return lo;
	}
	else
	{
		return lo - 1;
	}
}

void sv_array_close(sv_array* self)
{
	if (self)
	{
		sv_freenull(self->buffer);
		set_self_zero();
	}
}

sv_2d_array sv_2d_array_open(uint32_t elementsize)
{
	sv_2d_array ret = {};
	ret.arr = sv_array_open(sizeof32u(sv_array), 0);
	ret.elementsize = elementsize;
	return ret;
}

void sv_2d_array_ensure_space(sv_2d_array* self, uint32_t d1, uint32_t d2)
{
	if (d1 + 1 > self->arr.length)
	{
		uint32_t formerlength = self->arr.length;
		sv_array_appendzeros(&self->arr, (d1 + 1) - formerlength);
		for (uint32_t i = formerlength; i < self->arr.length; i++)
		{
			sv_array* subarr = (sv_array*)sv_array_at(&self->arr, i);
			*subarr = sv_array_open(self->elementsize, 0);
		}
	}

	sv_array* subarr = (sv_array*)sv_array_at(&self->arr, d1);
	if (d2 + 1 > subarr->length)
	{
		sv_array_appendzeros(subarr, (d2 + 1) - subarr->length);
	}
}

byte* sv_2d_array_at(sv_2d_array* self, uint32_t d1, uint32_t d2)
{
	return (byte*)sv_2d_array_atconst(self, d1, d2);
}

const byte* sv_2d_array_atconst(const sv_2d_array* self, uint32_t d1, uint32_t d2)
{
	check_fatal(d1 < self->arr.length, "out-of-bounds read");
	const sv_array* subarr = (const sv_array*)sv_array_atconst(&self->arr, d1);
	check_fatal(d2 < subarr->length, "out-of-bounds read");
	return sv_array_atconst(subarr, d2);
}

void sv_2d_array_close(sv_2d_array* self)
{
	if (self)
	{
		for (uint32_t i = 0; i < self->arr.length; i++)
		{
			sv_array* subarr = (sv_array*)sv_array_at(&self->arr, i);
			sv_array_close(subarr);
		}
		sv_array_close(&self->arr);
		set_self_zero();
	}
}

sv_pseudosplit sv_pseudosplit_open(const char* s)
{
	sv_pseudosplit ret = {};
	ret.currentline = bstring_open();
	ret.text = bfromcstr(s);
	ret.splitpoints = sv_array_open(sizeof32u(uint64_t), 1);
	return ret;
}

void sv_pseudosplit_split(sv_pseudosplit* self, char delim)
{
	sv_array_truncatelength(&self->splitpoints, 0);
	sv_array_add64u(&self->splitpoints, 0); /* the first line begins at offset 0. */
	for (int i = 0; i < blength(self->text); i++)
	{
		if (cstr(self->text)[i] == delim)
		{
			sv_array_add64u(&self->splitpoints, cast32s32u(i + 1));
		}
	}
}

const char* sv_pseudosplit_viewat(sv_pseudosplit* self, uint32_t linenumber)
{
	sv_pseudosplit_at(self, linenumber, self->currentline);
	return cstr(self->currentline);
}

void sv_pseudosplit_at(sv_pseudosplit* self, uint32_t linenumber, bstring s)
{
	check_fatal(linenumber < self->splitpoints.length, "attempted read out-of-bounds");
	uint64_t offset1 = sv_array_at64u(&self->splitpoints, linenumber);
	uint64_t offset2 = (linenumber == self->splitpoints.length-1) ? blength(self->text) :
		(sv_array_at64u(&self->splitpoints, linenumber + 1) - 1);
	bassignblk(s, cstr(self->text) + offset1, cast64u32s(offset2 - offset1));
}

uint32_t sv_pseudosplit_pos_to_linenumber(sv_pseudosplit* self, uint64_t offset)
{
	return sv_array_64ulowerbound(&self->splitpoints, offset);
}

void sv_pseudosplit_close(sv_pseudosplit* self)
{
	bdestroy(self->currentline);
	bdestroy(self->text);
	sv_array_close(&self->splitpoints);
}

sv_result OK = {};

void sv_result_close(sv_result* self)
{
	if (self)
	{
		bdestroy(self->msg);
		set_self_zero();
	}
}

uint32_t cast64u32u(uint64_t n)
{
	check_fatal(n <= (uint64_t)UINT32_MAX, "overflow");
	return (uint32_t)n;
}

int32_t cast64s32s(int64_t n)
{
	check_fatal(n <= (int64_t)INT32_MAX, "overflow");
	return (int32_t)n;
}

int32_t cast64u32s(uint64_t n)
{
	check_fatal(n <= (uint64_t)INT32_MAX, "overflow");
	return (int32_t)n;
}

int64_t cast64u64s(uint64_t n)
{
	check_fatal(n <= (uint64_t)INT64_MAX, "overflow");
	return (int64_t)n;
}

uint64_t cast64s64u(int64_t n)
{
	check_fatal(n >= 0, "overflow");
	return (uint64_t)n;
}

int32_t cast32u32s(uint32_t n)
{
	check_fatal((int64_t)n <= (int64_t)INT32_MAX, "overflow");
	return (int32_t)n;
}

uint32_t cast32s32u(int32_t n)
{
	check_fatal(n >= 0, "overflow");
	return (uint32_t)n;
}

uint32_t checkedmul32(uint32_t a, uint32_t b)
{
	uint32_t ret = a * b;
	check_fatal(a == 0 || ret / a == b, "overflow");
	return ret;
}

uint32_t checkedadd32(uint32_t a, uint32_t b)
{
	uint32_t ret = a + b;
	check_fatal(ret >= a && ret >= b, "overflow");
	return ret;
}

int32_t checkedadd32s(int32_t a, int32_t b)
{
	check_fatal(
		!(((b > 0) && (a > INT_MAX - b)) ||
		((b < 0) && (a < INT_MIN - b))),
		"overflow");

	return a + b;
}

uint32_t round_up_to_multiple(uint32_t a, uint32_t mod)
{
	uint32_t sum = checkedadd32(a, mod);
	if (sum == 0)
		return mod;
	else
		return checkedmul32((sum - 1) / mod, mod);
}

uint32_t round_down_to_multiple(uint32_t a, uint32_t mod)
{
	return round_up_to_multiple(a, mod) - mod;
}

uint32_t nearest_power_of_two32(uint32_t a)
{
	check_fatal(a < UINT32_MAX / 4, "overflow");
	if (a == 0)
		return 1;

	uint32_t result = 1;
	while (result < a)
		result <<= 1;
	return result;
}

uint64_t make_uint64(uint32_t hi, uint32_t lo)
{
	return lo | (((uint64_t)hi) << 32);
}

uint32_t upper32(uint64_t n)
{
	return (uint32_t)(n >> 32);
}

uint32_t lower32(uint64_t n)
{
	return (uint32_t)n;
}

#if defined(_LP64) || defined(_AMD64_) || defined(_IA64_)
uint32_t cast_size_t_32u(size_t n)
{
	staticassert(sizeof(size_t) == 8);
	staticassert(sizeof(intptr_t) == 8);
	return cast64u32u(n);
}
int32_t cast_size_t_32s(size_t n)
{
	staticassert(sizeof(size_t) == 8);
	staticassert(sizeof(intptr_t) == 8);
	return cast64u32s(n);
}
#else
uint32_t cast_size_t_32u(size_t n)
{
	staticassert(sizeof(size_t) == 4);
	staticassert(sizeof(intptr_t) == 4);
	return (uint32_t)n; /* allow cast */
}
int32_t cast_size_t_32s(size_t n)
{
	staticassert(sizeof(size_t) == 4);
	staticassert(sizeof(intptr_t) == 4);
	return (int32_t)n; /* allow cast */
}
#endif

#if INT_MAX == LONG_MAX
int32_t castls32s(long n)
{
	return (int32_t)n;
}
uint32_t castlu32s(unsigned long n)
{
	return (uint32_t)n;
}
#else
int32_t castls32s(long n)
{
	return cast64s32s(n);
}
uint32_t castlu32s(unsigned long n)
{
	return cast64u32u(n);
}
#endif

long long castll(int64_t n)
{
	staticassert(sizeof(long long) >= sizeof(int64_t));
	return (long long)n;
}

unsigned long long castull(uint64_t n)
{
	staticassert(sizeof(unsigned long long) >= sizeof(uint64_t));
	return (unsigned long long)n;
}

byte* sv_calloc(uint32_t a, uint32_t b)
{
	void* ret = calloc(a, b);
	check_fatal(ret != NULL, "calloc returned NULL");
	return (byte*)ret;
}

byte* sv_realloc(byte* ptr, uint32_t a, uint32_t b)
{
	void* ret = realloc(ptr, checkedmul32(a, b));
	check_fatal(ret != NULL, "realloc returned NULL");
	return (byte*)ret;
}

bool s_equal(const char* s1, const char* s2)
{
	return strcmp(s1, s2) == 0;
}

bool s_startswithlen(const char* s1, int len1, const char* s2, int len2)
{
	return (len1 < len2) ?
		false :
		memcmp(s1, s2, len2) == 0;
}

bool s_startswith(const char* s1, const char* s2)
{
	return s_startswithlen(s1, strlen32s(s1), s2, strlen32s(s2));
}

bool s_endswithlen(const char* s1, int len1, const char* s2, int len2)
{
	return (len1 < len2) ?
		false :
		memcmp(s1 + (len1 - len2), s2, len2) == 0;
}

bool s_endswith(const char* s1, const char* s2)
{
	return s_endswithlen(s1, strlen32s(s1), s2, strlen32s(s2));
}

bool s_contains(const char* s1, const char* s2)
{
	return strstr(s1, s2) != 0;
}

bool s_isalphanum_paren_or_underscore(const char* s)
{
	while (*s)
	{
		if (*s != '_' && *s != '-' && *s != '(' && *s != ')' && !isalnum((unsigned char) *s))
			return false;
		s++;
	}

	return true;
}

/* input must consist of only numerals.*/
bool uintfromstring(const char* s, uint64_t* result)
{
	staticassert(sizeof(unsigned long long) == sizeof(uint64_t));
	*result = 0;
	if (!s || !s[0] || strspn(s, "1234567890") != strlen(s))
	{
		return false;
	}

	/* we must check errno, the number could be greater than UINT64_MAX */
	errno = 0;
	char* ptrresult = NULL;
	uint64_t res = (uint64_t)strtoull(s, &ptrresult, 10);
	if (!*ptrresult && errno == 0)
	{
		*result = res;
		return true;
	}

	return false;
}

/* input must consist of only numerals.*/
bool uintfromstringhex(const char* s, uint64_t* result)
{
	staticassert(sizeof(unsigned long long) == sizeof(uint64_t));
	*result = 0;
	if (!s || !s[0] || strspn(s, "1234567890abcdefABCDEF") != strlen(s))
	{
		return false;
	}

	/* we must check errno, the number could be greater than UINT64_MAX */
	errno = 0;
	char* ptrresult = NULL;
	uint64_t res = (uint64_t)strtoull(s, &ptrresult, 16);
	if (!*ptrresult && errno == 0)
	{
		*result = res;
		return true;
	}

	return false;
}

/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.
www.rpi.edu/dept/acm/packages/Bin-Man/1.1/distrib/libglob/fnmatch.c

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

const int FNM_PATHNAME = (1 << 0); /* No wildcard can ever match `/'.  */
const int FNM_NOESCAPE = (1 << 1); /* Backslashes don't quote special chars.  */
const int FNM_PERIOD = (1 << 2); /* Leading `.' is matched only explicitly.  */
const int FNM_NOMATCH = 1;

/* Match STRING against the filename pattern PATTERN, returning zero if
it matches, FNM_NOMATCH if not. *removed support for char groups* */
static int fnmatch(const char *pattern, const char *string, int flags)
{
	register const char *p = pattern, *n = string;
	register char c;
	const int __FNM_FLAGS = (FNM_PATHNAME | FNM_NOESCAPE | FNM_PERIOD);
	if ((flags & ~__FNM_FLAGS) != 0)
	{
		errno = EINVAL;
		return (-1);
	}

	while ((c = *p++) != '\0')
	{
		switch (c)
		{
		case '?':
			if (*n == '\0')
				return (FNM_NOMATCH);
			else if ((flags & FNM_PATHNAME) && *n == '/')
				return (FNM_NOMATCH);
			else if ((flags & FNM_PERIOD) && *n == '.' &&
				(n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
				return (FNM_NOMATCH);
			break;

		case '\\':
			if (!(flags & FNM_NOESCAPE))
				c = *p++;
			if (*n != c)
				return (FNM_NOMATCH);
			break;

		case '*':
			if ((flags & FNM_PERIOD) && *n == '.' &&
				(n == string || ((flags & FNM_PATHNAME) && n[-1] == '/')))
				return (FNM_NOMATCH);

			for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
				if (((flags & FNM_PATHNAME) && *n == '/') ||
					(c == '?' && *n == '\0'))
					return (FNM_NOMATCH);

			if (c == '\0')
				return (0);

			{
				char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;
				for (--p; *n != '\0'; ++n)
					if ((c == '[' || *n == c1) &&
						fnmatch(p, n, flags & ~FNM_PERIOD) == 0)
						return (0);
				return (FNM_NOMATCH);
			}

		default:
			if (c != *n)
				return (FNM_NOMATCH);
		}

		++n;
	}

	if (*n == '\0')
		return (0);

	return (FNM_NOMATCH);
}

bool fnmatch_simple(const char* pattern, const char* string)
{
	return s_equal(pattern, "*") ? 
		true :
		fnmatch(pattern, string, FNM_NOESCAPE) != FNM_NOMATCH;
}

void fnmatch_checkvalid(const char* pattern, bstring response)
{
	bstrClear(response);
	if (pattern[0] == '\0')
	{
		bassigncstr(response, "pattern cannot be empty");
	}

	for (int i = 0; i < strlen32s(pattern); i++)
	{
		if (pattern[i] == '[' || pattern[i] == ']')
		{
			bassigncstr(response, "character groups [] not supported");
		}
		else if ((unsigned char)pattern[i] >= 128)
		{
			bassigncstr(response, "non-ansi chars not supported");
		}
	}
}

const char* bstrListViewAt(const bstrList* list, int index)
{
	check_fatal(list && index >= 0 && index < list->qty, "read out-of-bounds");
	return cstr(list->entry[index]);
}

bstrList* bstrListCopy(const bstrList* list)
{
	bstrList* ret = bstrListCreate();
	bstrListAlloc(ret, list->qty);
	for (int i = 0; i < list->qty; i++)
	{
		ret->entry[i] = bstrcpy(list->entry[i]);
	}

	ret->qty = list->qty;
	return ret;
}

void bstrListSplit(bstrList* list, const bstring str, const bstring bdelim)
{
	bstrListClear(list);
	struct genBstrList g = {};
	g.b = (bstring)str;
	g.bl = list;
	if (bsplitstrcb(str, bdelim, 0, bscb, &g) < 0) {
		bstrListClear(list);
	}
}

void bstrListSplitCstr(bstrList* list, const char* s, const char* delim)
{
	bstring bs = bfromcstr(s);
	bstring bdelim = bfromcstr(delim);
	bstrListSplit(list, bs, bdelim);
	bdestroy(bs);
	bdestroy(bdelim);
}

int bstrListAppend(bstrList* list, const bstring bs)
{
	if (!list)
		return BSTR_ERR;
	bstrListAlloc(list, list->qty + 1);
	list->entry[list->qty] = bstrcpy(bs);
	list->qty++;
	return BSTR_OK;
}

int bstrListAppendCstr(bstrList* list, const char* s)
{
	if (!list)
		return BSTR_ERR;
	bstrListAlloc(list, list->qty + 1);
	list->entry[list->qty] = bfromcstr(s);
	list->qty++;
	return BSTR_OK;
}

int bstrListAppendList(bstrList* list, const bstrList* otherlist)
{
	if (!list || !otherlist)
	{
		return BSTR_ERR;
	}

	for (int i = 0; i < otherlist->qty; i++)
	{
		bstrListAppend(list, otherlist->entry[i]);
	}

	return BSTR_OK;
}

int bstrListRemoveAt(bstrList* list, int index)
{
	if (!list || index < 0 || index >= list->qty)
		return BSTR_ERR;

	bdestroy(list->entry[index]);
	for (int i = index + 1; i < list->qty; i++)
	{
		list->entry[i - 1] = list->entry[i];
	}

	list->qty--;
	return BSTR_OK;
}

void bstrListClear(bstrList* list)
{
	for (int i = 0; i < list->qty; i++)
	{
		bdestroy(list->entry[i]);
	}

	list->qty = 0;
}

static int qsortfn_bstrListSort(const void* pt1, const void* pt2)
{
	bstring *left = (bstring*)pt1;
	bstring *right = (bstring*)pt2;
	return strcmp(cstr(*left), cstr(*right));
}

void bstrListSort(bstrList* list)
{
	qsort(&list->entry[0], (size_t)list->qty,
		sizeof(list->entry[0]), &qsortfn_bstrListSort);
}

int bstringAllocZeros(bstring s, int len)
{
	len += 1; /* room for nul char */
	if (BSTR_OK != ballocmin(s, len)) return BSTR_ERR;
	check_fatal(s->mlen >= len, "did not allocate enough space %d %d", s->mlen, len);
	memset(&s->data[0], 0, (size_t)s->mlen);
	return BSTR_OK;
}

void bstringFindAll(bstring haystack, const char* needle, sv_array* arr)
{
	sv_array_truncatelength(arr, 0);
	bstring bneedle = bfromcstr(needle);
	int pos = -1;
	while (true)
	{
		pos = binstr(haystack, pos + 1, bneedle);
		if (pos < 0)
			break;
		else
			sv_array_add64u(arr, cast32s32u(pos));
	}
	bdestroy(bneedle);
}

void bytes_to_string(const void* b, uint32_t len, bstring s)
{
	bstrClear(s);
	char buf[32] = "";
	const byte* bytes = (const byte*)b;
	for (uint32_t i = 0; i < len; i++)
	{
		int lenchars = snprintf(buf, countof(buf) - 1, "%02x", bytes[i]);
		bcatblk(s, buf, lenchars);
	}
}

static bool g_debugbreaks_enabled = true;

const char* bstrWarnNullToCstr(const bstring s)
{
	check_fatal(s->data, "Don't use cstr() on a null string");
	if (strlen32s((const char*)s->data) != s->slen)
	{
		if (g_debugbreaks_enabled)
		{
			alertdialog("Warning: nul byte found. Don't use cstr() for binary data because it will truncate.");
		}
		return NULL;
	}
	return (const char*)s->data;
}

const wchar_t* wcstrWarnNullToCstr(const sv_wstr* s)
{
	check_fatal(s->arr.length, "Don't use cstr() on a null string");
	for (uint32_t i = 0; i < s->arr.length - 1; i++)
	{
		if (*(const wchar_t*)sv_array_atconst(&s->arr, i) == L'\0')
		{
			if (g_debugbreaks_enabled)
			{
				alertdialog("Warning: nul byte found. Don't use cstr() for binary data because it will truncate.");
			}
			return NULL;
		}
	}
	return (const wchar_t*)sv_array_atconst(&s->arr, 0);
}

sv_wstr sv_wstr_open(uint32_t initialLength)
{
	initialLength += 1; /* room for nul char */
	check_fatal(initialLength >= 1 && initialLength < INT_MAX, "invalid length");
	sv_wstr self = {};
	self.arr = sv_array_open(sizeof(wchar_t), initialLength);
	sv_array_appendzeros(&self.arr, 1); /* add initial nul char*/
	return self;
}

void sv_wstr_close(sv_wstr* self)
{
	if (self)
	{
		sv_array_close(&self->arr);
		set_self_zero();
	}
}

void sv_wstrAllocZeros(sv_wstr* self, uint32_t len)
{
	len += 1; /* room for nul char */
	sv_wstrClear(self);
	sv_array_appendzeros(&self->arr, len);
	/* user should call sv_wstrRefreshLength after placing text into buffer */
}

void sv_wstrTruncateLength(sv_wstr* self, uint32_t len)
{
	sv_array_truncatelength(&self->arr, len + 1 /*nul char*/);
	*(wchar_t*)sv_array_at(&self->arr, len) = L'\0';
}

void sv_wstrClear(sv_wstr* self)
{
	sv_array_clear(&self->arr);
	sv_wstrAppend(self, L""); /*adds nul char*/
}

void sv_wstrAppend(sv_wstr* self, const wchar_t* s)
{
	/* erase the old nul term */
	if (self->arr.length > 0)
	{
		sv_array_truncatelength(&self->arr, self->arr.length - 1);
	}

	/* add the new data and new nul term */
	sv_array_append(&self->arr, (const byte*)s, wcslen32u(s));
	sv_array_appendzeros(&self->arr, 1);
}

void sv_wstrListClear(sv_array* arr)
{
	for (uint32_t i = 0; i < arr->length; i++)
	{
		sv_wstr_close((sv_wstr*)sv_array_at(arr, i));
	}

	sv_array_truncatelength(arr, 0);
}

char_ptr_array_builder char_ptr_array_builder_open(uint32_t estimatedsize)
{
	char_ptr_array_builder ret = {};
	ret.buffer = bstring_open();
	balloc(ret.buffer, 4 * estimatedsize);
	ret.offsets = sv_array_open(sizeof32u(uint64_t), estimatedsize);
	ret.char_ptrs = sv_array_open(sizeof32u(const char*), estimatedsize);
	return ret;
}

void char_ptr_array_builder_add(char_ptr_array_builder* self, const char* s, int len)
{
	sv_array_add64u(&self->offsets, cast32s32u(blength(self->buffer)));
	bcatblk(self->buffer, s, len + 1 /* get the nul term too! */);
}

void char_ptr_array_builder_finalize(char_ptr_array_builder* self)
{
	for (uint32_t i = 0; i < self->offsets.length; i++)
	{
		uint64_t offset = sv_array_at64u(&self->offsets, i);
		const char* s = (const char*)(self->buffer->data + offset);
		sv_array_append(&self->char_ptrs, &s, 1);
	}
	/* add one last pointer, to NULL */
	sv_array_appendzeros(&self->char_ptrs, 1);
}

void char_ptr_array_builder_close(char_ptr_array_builder* self)
{
	bdestroy(self->buffer);
	sv_array_close(&self->char_ptrs);
	sv_array_close(&self->offsets);
	set_self_zero();
}

void char_ptr_array_builder_reset(char_ptr_array_builder* self)
{
	bstrClear(self->buffer);
	sv_array_truncatelength(&self->char_ptrs, 0);
	sv_array_truncatelength(&self->offsets, 0);
}

void ask_user_prompt(const char* prompt, bool allow_empty, bstring out)
{
	bstrClear(out);
	while (1)
	{
		char buffer[1024] = "";
		puts(prompt);
		fflush(stdout);
		if (fgets(buffer, countof(buffer) - 1, stdin) == NULL || buffer[0] == '\n' || buffer[0] == '\0')
		{
			if (allow_empty)
				break;
			else
				continue;
		}

		if (buffer[strlen(buffer) - 1] == '\n')
		{
			bassignblk(out, buffer, cast64s32s(strlen(buffer) - 1));
			break;
		}
		else
		{
			/* eat extra characters */
			while (getchar() != '\n');
			printf("String entered was too long, max of %d characters\n", countof32s(buffer) - 1);
			continue;
		}
	}
}

bool ask_user_yesno(const char* prompt)
{
	bstring result = bstring_open();
	bool ret = false;
	while (1)
	{
		ask_user_prompt(prompt, false, result);
		if (s_equal("y", cstr(result)))
		{
			ret = true;
			break;
		}
		else if (s_equal("n", cstr(result)))
		{
			ret = false;
			break;
		}
	}
	bdestroy(result);
	return ret;
}

uint32_t ask_user_int(const char* prompt, int valmin, int valmax)
{
	bstring userinput = bstring_open();
	int ret = 0;
	while (true)
	{
		uint64_t n = 0;
		ask_user_prompt(prompt, false, userinput);
		if (uintfromstring(cstr(userinput), &n) && n >= valmin && n <= valmax)
		{
			ret = cast64u32u(n);
			break;
		}
		else
		{
			printf("Must be between %d and %d.\n", valmin, valmax);
		}
	}
	bdestroy(userinput);
	return ret;
}

void alertdialog(const char* message)
{
	fprintf(stderr, "%s\nPress any key to continue...\n", message);
	(void)getchar();
}

int ui_numbered_menu_pick_from_list(const char* msg, const bstrList* list, const char* formatEach, 
	const char* additionalOpt1, const char* additionalOpt2)
{
	int ret = 0;
	bstring userinput = bstring_open();
	while (true)
	{
		os_clr_console();
		printf("%s\n", msg);
		const char* spec = formatEach ? formatEach : "%d) %s\n";
		int listindex = 0;
		while (listindex < list->qty)
		{
			printf(spec, listindex + 1, bstrListViewAt(list, listindex));
			++listindex;
		}
		if (additionalOpt1)
		{
			printf("%d) %s\n", ++listindex, additionalOpt1);
		}
		if (additionalOpt2)
		{
			printf("%d) %s\n", ++listindex, additionalOpt2);
		}
		ask_user_prompt("\n\nPlease type a number from the list above and press Enter", false, userinput);
		uint64_t n = 0;
		if (uintfromstring(cstr(userinput), &n) && n >= 1 && n <= listindex)
		{
			ret = cast64u32s(n - 1);
			break;
		}
	}
	bdestroy(userinput);
	return ret;
}

int ui_numbered_menu_pick_from_long_list(const bstrList* list, int groupsize)
{
	int ret = -1;
	int ngroups = (list->qty / groupsize) + 1;
	bstrList* listGroups = bstrListCreate();
	bstrList* listSubgroup = bstrListCreate();
	for (int i = 0; i < ngroups; i++)
	{
		char buf[BUFSIZ] = { 0 };
		snprintf(buf, countof(buf) - 1, "Items %d to %d", (i*groupsize)+1, ((i + 1)*groupsize));
		bstrListAppendCstr(listGroups, buf);
	}

	while (true)
	{
		int chosengroup = ui_numbered_menu_pick_from_list("", listGroups, NULL, "Back", NULL);
		if (chosengroup >= listGroups->qty)
		{
			break;
		}
		bstrListClear(listSubgroup);
		for (int i = chosengroup*groupsize; i < chosengroup * groupsize + groupsize && i < list->qty; i++)
		{
			const char* subitem = bstrListViewAt(list, i);
			bstrListAppendCstr(listSubgroup, subitem);
		}
		int chosensubitem = ui_numbered_menu_pick_from_list("", listSubgroup, NULL, "Back", NULL);
		if (chosensubitem < listSubgroup->qty)
		{
			ret = chosengroup * groupsize + chosensubitem;
			break;
		}
	}

	bstrListDestroy(listSubgroup);
	bstrListDestroy(listGroups);
	return ret;
}

struct svdp_application;
check_result ui_numbered_menu_show(const char* msg, const ui_numbered_menu_spec_entry* entries, 
	struct svdp_application* app, FnMenuGetNextMenu fngetnextmenu)
{
	bstring userinput = bstring_open();

	while (true)
	{
		if (fngetnextmenu)
		{
			entries = fngetnextmenu(app);
		}

		os_clr_console();
		printf("%s\n", msg);
		uint64_t number = 1;
		const ui_numbered_menu_spec_entry* iter = entries;
		while (iter->callback || iter->message)
		{
			printf("%llu) %s\n", castull(number), iter->message);
			number++;
			iter++;
		}

		ask_user_prompt("\n\nPlease type a number from the list above and press Enter", false, userinput);
		uint64_t n = 0;
		if (uintfromstring(cstr(userinput), &n) && n >= 1 && n < number)
		{
			const ui_numbered_menu_spec_entry* action = entries + (n-1);
			if (action->callback)
			{
				os_clr_console();
				action->callback(app, action->arg);
				/* go back into the loop */
			}
			else
			{
				break;
				/* if the callback is null, return and go back up the callstack. */
			}
		}
	}
	bdestroy(userinput);
	return OK;
}


static sv_log* p_sv_log = NULL;
void sv_log_register_active_logger(sv_log* logger)
{
	p_sv_log = logger;
}

static check_result sv_log_start_attach_file(const char* dir, uint32_t number, sv_file* file, int64_t* start_of_day)
{
	sv_result currenterr = {};
	bstring filename = bstring_open();
	appendNumberToFilename(dir, "log", ".txt", number, filename);
	check(sv_file_open(file, cstr(filename), 
		/* the 'e' means to pass O_CLOEXEC so that the fd is closed on calls to exec. */
		islinux ? "abe" : "ab"));
	
	/* get the start of the day in seconds */
	time_t curtime = time(NULL);
	struct tm* tmstruct = localtime(&curtime);
	*start_of_day = curtime;
	*start_of_day -= 60 * 60 * tmstruct->tm_hour;
	*start_of_day -= 60 * tmstruct->tm_min;
	*start_of_day -= tmstruct->tm_sec;

	/* write time */
	fprintf(file->file, nativenewline "%04d/%02d/%02d",
		tmstruct->tm_year + 1900, tmstruct->tm_mon + 1, tmstruct->tm_mday);

cleanup:
	bdestroy(filename);
	return currenterr;
}

check_result sv_log_open(sv_log* self, const char* dir)
{
	sv_result currenterr = {};
	set_self_zero();
	self->dir = bfromcstr(dir);
	self->cap_filesize = 8 * 1024 * 1024;
	
	check(readLatestNumberFromFilename(dir, "log", ".txt", &self->logfilenumber));
	if (self->logfilenumber == 0)
		self->logfilenumber = 1;

	check(sv_log_start_attach_file(cstr(self->dir), self->logfilenumber, &self->logfile, &self->start_of_day));
cleanup:
	return currenterr;
}

void sv_log_close(sv_log* self)
{
	if (self)
	{
		bdestroy(self->dir);
		sv_file_close(&self->logfile);
		set_self_zero();
	}
}

FILE* sv_log_currentFile()
{
	return p_sv_log ? p_sv_log->logfile.file : NULL;
}

void sv_log_addNewLineTime(FILE* f, int64_t start_of_day, int64_t totalseconds, long milliseconds)
{
	int64_t seconds_since_midnight = totalseconds - start_of_day;
	int64_t hours = seconds_since_midnight / (60 * 60);
	int64_t secondspasthour = seconds_since_midnight - hours * 60 * 60;
	int64_t minutes = (secondspasthour) / 60;
	int64_t seconds = secondspasthour - minutes * 60;
	fprintf(f, nativenewline "[%02d:%02d:%02d:%03d] ", (int)hours, (int)minutes, (int)seconds, (int)milliseconds);
}

static void sv_log_check_switch_nextfile(sv_log* self)
{
	self->counter++;
	if ((self->counter & 0xf) == 0) /* check filesize whenever counter%16==0 */
	{
		fseek(self->logfile.file, 0, SEEK_END);
		if (ftell(self->logfile.file) > self->cap_filesize)
		{
			sv_file next_file = {};
			int64_t next_start_of_day = 0;
			sv_result try_switch_file = sv_log_start_attach_file(cstr(self->dir),
				self->logfilenumber + 1, &next_file, &next_start_of_day);

			if (try_switch_file.code == 0)
			{
				sv_file_close(&self->logfile);
				self->logfile = next_file;
				self->start_of_day = next_start_of_day;
			}
			else
			{
				sv_file_close(&next_file);
				sv_result_close(&try_switch_file);
			}
		}
	}
}

static void sv_log_addNewLine()
{
	sv_log_check_switch_nextfile(p_sv_log);
	int64_t seconds;
	int32_t milliseconds;
	os_clock_gettime(&seconds, &milliseconds);
	sv_log_addNewLineTime(sv_log_currentFile(), p_sv_log->start_of_day, seconds, milliseconds);
}

void sv_log_writeLine(const char* s)
{
	if (sv_log_currentFile())
	{
		sv_log_addNewLine();
		fputs(s, sv_log_currentFile());
	}
}

void sv_log_writeLines(const char* s1, const char* s2)
{
	if (sv_log_currentFile())
	{
		sv_log_addNewLine();
		fputs(s1, sv_log_currentFile());
		fputs(s2, sv_log_currentFile());
	}
}

void sv_log_flush()
{
	if (sv_log_currentFile())
	{
		fflush(sv_log_currentFile());
	}
}

void sv_log_writeFmt(const char* fmt, ...)
{
	if (sv_log_currentFile())
	{
		sv_log_addNewLine();
		va_list args;
		va_start(args, fmt);
		vfprintf(sv_log_currentFile(), fmt, args);
		va_end(args);
	}
}

void set_debugbreaks_enabled(bool b)
{
	g_debugbreaks_enabled = b;
}

void check_b_hit(void)
{
	fflush(stdout);
	fflush(stderr);
	sv_log_flush();
	if (g_debugbreaks_enabled)
	{
		debugbreak();
	}
}

void debugbreak(void)
{
#if _DEBUG
	alertdialog("a recoverable error occurred, attach debugger to investigate.");
	DEBUGBREAK();
#endif
}

void die(void)
{
	fflush(stdout);
	fflush(stderr);
	sv_log_flush();
	alertdialog("");
	debugbreak();
	exit(1);
}

void check_warn_impl(sv_result res, const char* msg, const char* function, erespondtoerr respondtoerr)
{
	if (res.code)
	{
		log_b(0, "%s %s", function, cstr(res.msg));
		sv_log_flush();
		if (g_debugbreaks_enabled)
		{
			printf("%s\n%s\n", msg ? msg : "", cstr(res.msg));
			alertdialog("");
		}
		sv_result_close(&res);
		if (respondtoerr == exit_on_err)
		{
			die();
		}
	}
}

void log_errno_impl(const char* exp, int nerrno, const char* context[4], const char* fn, int lineno)
{
	char errorname[BUFSIZ] = { 0 };
	os_errno_to_buffer(nerrno, errorname, countof(errorname));
	sv_log_writeFmt("'%s' returned errno=%s(%d) in %s line %d %s %s %s %s",
		exp, errorname, nerrno, fn, lineno,
		context[0] ? context[0] : "",
		context[1] ? context[1] : "",
		context[2] ? context[2] : "",
		context[3] ? context[3] : "");
}

void check_errno_impl(sv_result* currenterr, const char* exp,
	int nerrno, const char* context[4], const char* fn)
{
	char errorname[BUFSIZ] = { 0 };
	os_errno_to_buffer(nerrno, errorname, countof(errorname));
	currenterr->code = 1;
	currenterr->msg = bformat("Got %s(%d) in %s %s %s %s %s (%s)", 
		errorname, nerrno, fn,
		context[0] ? context[0] : "",
		context[1] ? context[1] : "",
		context[2] ? context[2] : "",
		context[3] ? context[3] : "",
		exp);
	check_b_hit();
}

void log_errwin32_impl(const char* exp, unsigned long nerrno, 
	const char* context[4], const char* fn, int lineno)
{
	char errorname[BUFSIZ] = { 0 };
	os_win32err_to_buffer(nerrno, errorname, countof(errorname));
	sv_log_writeFmt("'%s' returned errno=%s(%d) in %s line %d, %s %s %s %s",
		exp, errorname, nerrno, fn, lineno,
		context[0] ? context[0] : "",
		context[1] ? context[1] : "",
		context[2] ? context[2] : "",
		context[3] ? context[3] : "");
}

void check_errwin32_impl(sv_result* currenterr, const char* exp, 
	unsigned long nerrno, const char* context[4], const char* fn)
{
	char errorname[BUFSIZ] = { 0 };
	os_win32err_to_buffer(nerrno, errorname, countof(errorname));
	currenterr->code = 1;
	currenterr->msg = bformat("Got %s(%d) in %s, %s %s %s %s (%s)",
		errorname, nerrno, fn,
		context[0] ? context[0] : "",
		context[1] ? context[1] : "",
		context[2] ? context[2] : "",
		context[3] ? context[3] : "",
		exp);
	check_b_hit();
}

int _ = 0;
bstring testCheckBformatStrings = NULL;

/* from 1 to "/path/file001.txt" */
void appendNumberToFilename(const char* dir, const char* prefix, const char* suffix, uint32_t number, bstring out)
{
	bassignformat(out, "%s%s%s%05d%s", dir, pathsep, prefix, number, suffix);
}

/* from "/path/file001.txt" to 1. */
uint32_t readNumberFromFilename(bstring tmp, const char* prefix, const char* suffix, const char* candidate)
{
	int lenprefix = strlen32s(prefix), lensuffix = strlen32s(suffix), candlen = strlen32s(candidate);
	if (s_startswithlen(candidate, candlen, prefix, lenprefix) && 
		s_endswithlen(candidate, candlen, suffix, lensuffix))
	{
		/* get chars between the template and extension. */
		int len = candlen - (lensuffix + lenprefix);
		bassignblk(tmp, candidate + lenprefix, len);
		uint64_t n = 0;
		if (blength(tmp) > 0 && uintfromstring(cstr(tmp), &n))
			return cast64u32u(n);
	}
	return 0;
}

/* from "/path/file001.txt" to 1. */
check_result readLatestNumberFromFilename(const char* dir, const char* prefix, const char* suffix, uint32_t* latestnumber)
{
	*latestnumber = 0;
	sv_result currenterr = {};
	bstring fullprefix = bformat("%s%s%s", dir, pathsep, prefix);
	bstring tmp = bstring_open();
	bstrList* files_seen = bstrListCreate();
	check_b(os_dir_exists(dir), "dir not found %s", dir);
	check(os_listfiles(dir, files_seen, false));
	for (int i = 0; i < files_seen->qty; i++)
	{
		/* don't use string sorting, which would put 02 after 000123456 */
		uint32_t number = readNumberFromFilename(tmp, cstr(fullprefix), suffix, bstrListViewAt(files_seen, i));
		*latestnumber = MAX(*latestnumber, number);
	}

cleanup:
	bdestroy(fullprefix);
	bdestroy(tmp);
	bstrListDestroy(files_seen);
	return currenterr;
}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
