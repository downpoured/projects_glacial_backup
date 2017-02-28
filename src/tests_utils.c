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

#include "tests_utils.h"

bool testintimpl(int lineno, unsigned long long n1, unsigned long long n2)
{
	if (n1 == n2)
	{
		return true;
	}

	bstring s = bformat("line %d, expected %lld, got %lld", lineno, n1, n2);
	alertdialog(cstr(s));
	bdestroy(s);
	return false;
}

bool teststrimpl(int lineno, const char* s1, const char* s2)
{
	if (s_equal(s1, s2))
	{
		return true;
	}

	bstring s = bformat("line %d, expected \n\n%s, got \n\n%s", lineno, s1, s2);
	alertdialog(cstr(s));
	bdestroy(s);
	return false;
}


bool testlistimpl(int lineno, const char* expected, const bstrList* list)
{
	bstring joined = bjoinStatic(list, "|");
	if (s_equal(expected, cstr(joined)))
	{
		return true;
	}

	bstring s = bformat("line %d, expected \n\n%s, got \n\n%s", lineno, expected, cstr(joined));
	alertdialog(cstr(s));
	bdestroy(s);
	bdestroy(joined);
	return false;
}


void test_util_array()
{
	{ /* we shouldn't allocate when initializing 0 elements */
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		TestEqn(NULL, arr.buffer);
		TestEqn(sizeof32u(uint64_t), arr.elementsize);
		TestEqn(0, arr.capacity);
		TestEqn(0, arr.length);
		sv_array_close(&arr);
	}
	{ /* intialize with 3 elements */
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 3);
		TestTrue(NULL != arr.buffer);
		TestEqn(sizeof32u(uint64_t), arr.elementsize);
		TestEqn(3, arr.capacity);
		TestEqn(0, arr.length);
		sv_array_close(&arr);
	}
	{ /* capacity and length increase as expected */
		uint64_t testBuffer[10] = { 10, 0 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 3);
		TestEqn(3, arr.capacity);
		TestEqn(0, arr.length);

		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(1, arr.length);

		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(2, arr.length);

		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(3, arr.length);

		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(4, arr.capacity);
		TestEqn(4, arr.length);

		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(8, arr.capacity);
		TestEqn(5, arr.length);

		sv_array_close(&arr);
	}
	{ /* test reserve */
		uint64_t testBuffer[10] = { 10, 0 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(1, arr.capacity);
		TestEqn(1, arr.length);
		TestEqn(10, *(uint64_t*)sv_array_at(&arr, 0));

		sv_array_reserve(&arr, 4000);
		TestEqn(4096, arr.capacity);
		TestEqn(1, arr.length);
		TestEqn(10, *(uint64_t*)sv_array_at(&arr, 0));
		sv_array_close(&arr);
	}
	{ /* test addzeros */
		sv_array arr = sv_array_open(sizeof32u(byte), 0);
		sv_array_appendzeros(&arr, 15);
		TestEqn(16, arr.capacity);
		TestEqn(15, arr.length);
		for (uint32_t i = 0; i < arr.length; i++)
		{
			TestEqn(0, *sv_array_at(&arr, i));
		}

		sv_array_close(&arr);
	}
	{ /* test contents*/
		uint64_t testBuffer[10] = { 10, 11, 12, 13 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 3);
		sv_array_append(&arr, (byte*)&testBuffer[0], 4);
		TestEqn(10, *(uint64_t*)sv_array_at(&arr, 0));
		TestEqn(11, *(uint64_t*)sv_array_at(&arr, 1));
		TestEqn(12, *(uint64_t*)sv_array_at(&arr, 2));
		TestEqn(13, *(uint64_t*)sv_array_at(&arr, 3));
		TestEqn(4, arr.length);
		sv_array_append(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(10, *(uint64_t*)sv_array_at(&arr, 4));
		TestEqn(5, arr.length);
		sv_array_close(&arr);
	}
	{ /* valid truncate */
		uint64_t testBuffer[10] = { 10, 11 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_append(&arr, (byte*)&testBuffer[0], 2);
		TestEqn(2, arr.length);

		sv_array_truncatelength(&arr, 2);
		TestEqn(2, arr.length);

		sv_array_truncatelength(&arr, 1);
		TestEqn(1, arr.length);
		TestEqn(10, *(uint64_t*)sv_array_at(&arr, 0));
		sv_array_close(&arr);
	}
	{ /* helpers for uint64_t*/
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_add64u(&arr, 123);
		TestEqn(123, sv_array_at64u(&arr, 0));
		sv_array_add64u(&arr, 456);
		TestEqn(456, sv_array_at64u(&arr, 1));
		sv_array_close(&arr);
	}
	{ /* binary search on array with one element */
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_add64u(&arr, 10);
		TestEqn(0, sv_array_64ulowerbound(&arr, 10));
		TestEqn(0, sv_array_64ulowerbound(&arr, 15));
		sv_array_close(&arr);
	}
	{ /* binary search on array with four elements */
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_add64u(&arr, 10);
		sv_array_add64u(&arr, 20);
		sv_array_add64u(&arr, 21);
		sv_array_add64u(&arr, 30);
		TestEqn(0, sv_array_64ulowerbound(&arr, 10));
		TestEqn(0, sv_array_64ulowerbound(&arr, 15));
		TestEqn(0, sv_array_64ulowerbound(&arr, 19));
		TestEqn(1, sv_array_64ulowerbound(&arr, 20));
		TestEqn(2, sv_array_64ulowerbound(&arr, 21));
		TestEqn(2, sv_array_64ulowerbound(&arr, 22));
		TestEqn(3, sv_array_64ulowerbound(&arr, 30));
		TestEqn(3, sv_array_64ulowerbound(&arr, 100));
		sv_array_close(&arr);
	}
	{ /* binary search on array with redundant elements */
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_add64u(&arr, 10);
		sv_array_add64u(&arr, 20);
		sv_array_add64u(&arr, 20);
		sv_array_add64u(&arr, 20);
		sv_array_add64u(&arr, 20);
		TestEqn(0, sv_array_64ulowerbound(&arr, 10));
		TestEqn(0, sv_array_64ulowerbound(&arr, 19));
		TestEqn(1, sv_array_64ulowerbound(&arr, 20));
		TestEqn(4, sv_array_64ulowerbound(&arr, 21));
		TestEqn(4, sv_array_64ulowerbound(&arr, 100));
		sv_array_close(&arr);
	}
}

void test_util_2d_array()
{
	{ /* hold data at 1x1 */
		sv_2d_array arr = sv_2d_array_open(sizeof32u(byte));
		sv_2d_array_ensure_space(&arr, 0, 0);
		TestEqn(1, arr.arr.length);
		sv_array* subarr = (sv_array*)sv_array_at(&arr.arr, 0);
		TestEqn(1, subarr->length);
		TestEqn(0, *sv_array_at(subarr, 0));
		TestTrue(sv_2d_array_at(&arr, 0, 0) == sv_array_at(subarr, 0));
		sv_2d_array_close(&arr);
	}
	{ /* hold data at 8x4 */
		sv_2d_array arr = sv_2d_array_open(sizeof32u(byte));
		sv_2d_array_ensure_space(&arr, 7, 3);
		TestEqn(8, arr.arr.length);
		sv_array* subarr = (sv_array*)sv_array_at(&arr.arr, 7);
		TestEqn(4, subarr->length);
		TestEqn(0, *sv_array_at(subarr, 0));
		TestEqn(0, *sv_array_at(subarr, 1));
		TestEqn(0, *sv_array_at(subarr, 2));
		TestEqn(0, *sv_array_at(subarr, 3));
		TestTrue(sv_2d_array_at(&arr, 7, 0) == sv_array_at(subarr, 0));
		TestTrue(sv_2d_array_at(&arr, 7, 3) == sv_array_at(subarr, 3));

		/* it's a jagged array, so the length at other dimensions is unaffected */
		subarr = (sv_array*)sv_array_at(&arr.arr, 0);
		TestEqn(0, subarr->length);

		/* allocate space at 4,8 */
		sv_2d_array_ensure_space(&arr, 3, 7);
		subarr = (sv_array*)sv_array_at(&arr.arr, 3);
		TestEqn(8, subarr->length);
		TestEqn(0, *sv_array_at(subarr, 0));
		TestEqn(0, *sv_array_at(subarr, 7));
		TestTrue(sv_2d_array_at(&arr, 3, 0) == sv_array_at(subarr, 0));
		TestTrue(sv_2d_array_at(&arr, 3, 7) == sv_array_at(subarr, 7));
		sv_2d_array_close(&arr);
	}
	{ /* fill with data, and then read back again */
		sv_2d_array arr = sv_2d_array_open(sizeof32u(byte));
		for (uint32_t i = 0; i < 8; i++)
		{
			for (uint32_t j = 0; j < 4; j++)
			{
				sv_2d_array_ensure_space(&arr, i, j);
				*sv_2d_array_at(&arr, i, j) = (byte)(i * i + j * j);
			}
		}
		for (uint32_t i = 0; i < 8; i++)
		{
			for (uint32_t j = 0; j < 4; j++)
			{
				TestEqn(*sv_2d_array_at(&arr, i, j), (byte)(i * i + j * j));
			}
		}
		sv_2d_array_close(&arr);
	}
}

void test_util_basicarithmatic()
{
	TestEqn(2070, checkedmul32(45, 46));
	TestEqn(4294910784ULL, checkedmul32(123456, 34789));
	TestEqn(4294967295, checkedmul32(1, 4294967295));
	TestEqn(0, checkedmul32(0, 4294967295));
	TestEqn(690, checkedadd32(123, 567));
	TestEqn(4294967295, checkedadd32(2147483647, 2147483648));
	TestEqn(4294967295, checkedadd32(1, 4294967294));
	TestEqn(690, checkedadd32s(123, 567));
	TestEqn(2147483647, checkedadd32s(1073741824, 1073741823));
	TestEqn(2147483647, checkedadd32s(1, 2147483646));
	TestEqn(-112, checkedadd32s(-45, -67));
	TestEqn(INT_MIN, checkedadd32s(-1073741824, -1073741824));
	TestEqn(INT_MIN, checkedadd32s(-1, -2147483647));
	TestEqn(5, countof("abcd"));
	TestEqn(5, sizeof("abcd"));
	TestEqn(0x1111000011112222ULL, make_uint64(0x11110000, 0x11112222));
	TestEqn(0x00000000FF345678ULL, make_uint64(0x00000000, 0xFF345678));
	TestEqn(0xFF345678FF345678ULL, make_uint64(0xFF345678, 0xFF345678));
	TestEqn(0xFF34567800000000ULL, make_uint64(0xFF345678, 0x00000000));
	TestEqn(0, round_up_to_multiple(0, 8));
	TestEqn(8, round_up_to_multiple(1, 8));
	TestEqn(48, round_up_to_multiple(47, 8));
	TestEqn(48, round_up_to_multiple(48, 8));
	TestEqn(56, round_up_to_multiple(49, 8));
	TestEqn(1, nearest_power_of_two32(0));
	TestEqn(1, nearest_power_of_two32(1));
	TestEqn(2, nearest_power_of_two32(2));
	TestEqn(4, nearest_power_of_two32(3));
}

void test_util_string()
{
	{ /* test empty string*/
		bstring str = bstring_open();
		TestEqs("", cstr(str));
		TestEqn(0, blength(str));
		bdestroy(str);
	}
	{ /* simple string*/
		bstring str = bfromcstr("abcde");
		TestEqs("abcde", cstr(str));
		TestEqn(5, blength(str));
		bdestroy(str);
	}
	{ /* assign string*/
		bstring str = bfromcstr("abc");
		bassigncstr(str, "def");
		TestEqs("def", cstr(str));
		bdestroy(str);
	}
	{ /* clear string*/
		bstring str = bfromcstr("abc");
		bstrClear(str);
		TestEqs("", cstr(str));
		bdestroy(str);
	}
	{ /* append formatting */
		bstring str = bfromcstr("abc");
		bassignformat(str, "%d, %s, %S, and hex %x", 12, "test", L"wide", 0xff);
		TestEqs("12, test, wide, and hex ff", cstr(str));
		bdestroy(str);
	}
	{ /* formatting */
		bstring str = bformat("%llu, %llu, %.3f", 0ULL, 12345678987654321ULL, 0.123456);
		TestEqs("0, 12345678987654321, 0.123", cstr(str));
		bassignformat(str, "%s", "");
		TestEqs("", cstr(str));
		bdestroy(str);
	}
	{ /* concat empty strings */
		bstring str = bfromcstr("abc");
		bcatcstr(str, "");
		TestEqs("abc", cstr(str));
		bcatblk(str, "", 0);
		TestEqs("abc", cstr(str));
		bdestroy(str);
	}
	{ /* trim */
		bstring str = bfromcstr(" a     ");
		btrimws(str);
		TestEqs("a", cstr(str));
		bdestroy(str);
	}
	{ /* truncate */
		bstring str = bfromcstr("abc");
		btrunc(str, 2);
		TestEqs("ab", cstr(str));
		bdestroy(str);
	}
	{ /* int from string */
		uint64_t n = 0;
		TestTrue(uintfromstring("12", &n) && n == 12);
		TestTrue(!uintfromstring("", &n) && n == 0);
		TestTrue(uintfromstring("0", &n) && n == 0);
		TestTrue(uintfromstring("000", &n) && n == 0);
		TestTrue(!uintfromstring("12a", &n) && n == 0);
		TestTrue(!uintfromstring("abc", &n) && n == 0);
		TestTrue(!uintfromstring(" ", &n) && n == 0);
		TestTrue(uintfromstring("0001", &n) && n == 1);
		TestTrue(uintfromstring("000123", &n) && n == 123);
	}
	{ /* uintfromstring fails for very large numbers */
		uint64_t n = 0;
		TestTrue(uintfromstring("11111111111111111111", &n) && n == 11111111111111111111ULL);
		TestTrue(!uintfromstring("18446744073709551619", &n) && n == 0);
		TestTrue(!uintfromstring("99999999999999999999", &n) && n == 0);
	}
	{ /* uint from string hex */
		uint64_t n = 0;
		TestTrue(uintfromstringhex("12", &n) && n == 0x12);
		TestTrue(!uintfromstringhex("", &n) && n == 0);
		TestTrue(uintfromstringhex("0", &n) && n == 0);
		TestTrue(uintfromstringhex("000", &n) && n == 0);
		TestTrue(uintfromstringhex("abc", &n) && n == 0xabc);
		TestTrue(uintfromstringhex("00012aa1", &n) && n == 0x12aa1);
		TestTrue(!uintfromstringhex("abcx", &n) && n == 0);
	}
	{ /* uint from string hex, large numbers */
		uint64_t n = 0;
		TestTrue(uintfromstringhex("1111111111111111", &n) && n == 0x1111111111111111ULL);
		TestTrue(!uintfromstringhex("11111111111111111", &n) && n == 0);
		TestTrue(!uintfromstringhex("999999999999999999", &n) && n == 0);
	}
	{ /* should be ok to destroy a zeroed out string */
		struct tagbstring s = { 0 };
		TestEqn(BSTR_ERR, bdestroy(&s));
	}
}

void test_util_stringlist()
{
	bstrList* list = bstrListCreate();

	{ /* simple split and join */
		struct tagbstring s = bsStatic("a|b|c");
		struct tagbstring delim = bsStatic("|");
		bstrListSplit(list, &s, &delim);
		TestEqn(3, list->qty);
		TestEqs("a", cstr(list->entry[0]));
		TestEqs("b", cstr(list->entry[1]));
		TestEqs("c", cstr(list->entry[2]));
		bstring joined = bjoinStatic(list, "|");
		TestEqs("a|b|c", cstr(joined));
		bdestroy(joined);
	}
	{ /* split */
		bstrListSplitCstr(list, "a||b|c", "|");
		TestEqList("a||b|c", list);
	}
	{ /* split */
		bstrListSplitCstr(list, "aabcbabccabc", "abc");
		TestEqList("a|b|c|", list);
	}
	{ /* split corner cases */
		bstrListSplitCstr(list, "", "|");
		TestEqn(1, list->qty);
		TestEqList("", list);
		bstrListSplitCstr(list, "|", "|");
		TestEqn(2, list->qty);
		TestEqList("|", list);
		bstrListSplitCstr(list, "||", "|");
		TestEqList("||", list);
		bstrListSplitCstr(list, "||a||b|c||", "|");
		TestEqList("||a||b|c||", list);
	}
	{ /* append and view */
		bstrListClear(list);
		bstrListAppendCstr(list, "abc");
		bstrListAppendCstr(list, "123");
		bstrListAppendCstr(list, "def");
		TestEqList("abc|123|def", list);
		TestEqn(3, list->qty);
		TestEqs("abc", bstrListViewAt(list, 0));
		TestEqs("123", bstrListViewAt(list, 1));
		TestEqs("def", bstrListViewAt(list, 2));
	}
	{ /* cannot remove out-of-bounds */
		bstrListClear(list);
		TestEqn(BSTR_ERR, bstrListRemoveAt(list, 0));
		TestEqn(BSTR_ERR, bstrListRemoveAt(list, 1));
		bstrListAppendCstr(list, "abc");
		TestEqn(BSTR_ERR, bstrListRemoveAt(list, -1));
		TestEqn(BSTR_ERR, bstrListRemoveAt(list, 1));
		TestEqn(BSTR_ERR, bstrListRemoveAt(list, 2));
	}
	{ /* append and remove */
		bstrListClear(list);
		bstrListAppendCstr(list, "a");
		bstrListAppendCstr(list, "b");
		bstrListAppendCstr(list, "c");
		bstrListAppendCstr(list, "d");
		bstrListAppendCstr(list, "e");
		TestEqn(BSTR_OK, bstrListRemoveAt(list, 4));
		TestEqList("a|b|c|d", list);
		TestEqn(BSTR_OK, bstrListRemoveAt(list, 0));
		TestEqList("b|c|d", list);
		TestEqn(BSTR_OK, bstrListRemoveAt(list, 1));
		TestEqList("b|d", list);
		TestEqn(BSTR_OK, bstrListRemoveAt(list, 0));
		TestEqList("d", list);
		TestEqn(BSTR_OK, bstrListRemoveAt(list, 0));
		TestEqList("", list);
		bstrListAppendCstr(list, "a");
		bstrListAppendCstr(list, "b");
		TestEqList("a|b", list);
	}
	{ /* copy list. the copy should be completely independent. */
		bstrListSplitCstr(list, "a|b|c", "|");
		bstrList* listcopy = bstrListCopy(list);
		TestEqList("a|b|c", listcopy);
		bcatcstr(list->entry[0], "added");
		TestEqList("a|b|c", listcopy);
		bstrListDestroy(listcopy);
	}
	{ /* test clearing a list */
		bstrListClear(list);
		TestEqList("", list);
		TestEqn(0, list->qty);
		bstrListAppendCstr(list, "a");
		bstrListAppendCstr(list, "b");
		TestEqList("a|b", list);
		bstrListClear(list);
		TestEqList("", list);
		TestEqn(0, list->qty);
	}
	{ /* test sort */
		bstrListSplitCstr(list, "apple2|apple1|pear|peach", "|");
		bstrListSort(list);
		TestEqList("apple1|apple2|peach|pear", list);
		bstrListAppendCstr(list, "aaa");
		TestEqn(BSTR_OK, bstrListRemoveAt(list, 4));
		bcatcstr(list->entry[2], "a");
		bstrListSort(list);
		TestEqList("apple1|apple2|peacha|pear", list);
	}
	{ /* test join on empty list */
		bstrListClear(list);
		bstring all = bjoinStatic(list, "|");
		TestEqs("", cstr(all));
		bdestroy(all);
	}

	bstrListDestroy(list);
}

void test_util_char_ptr_builder()
{
	char_ptr_array_builder builder = char_ptr_array_builder_open(0);
	{ /* typical use */
		char_ptr_array_builder_reset(&builder);
		char_ptr_array_builder_add(&builder, str_and_len32s("ab"));
		char_ptr_array_builder_add(&builder, str_and_len32s("cdef"));
		char_ptr_array_builder_finalize(&builder);
		TestTrue(memcmp("ab\0cdef\0", &builder.buffer->data, 8));
		TestEqn(2, builder.offsets.length);
		TestEqn(0, sv_array_at64u(&builder.offsets, 0));
		TestEqn(3, sv_array_at64u(&builder.offsets, 1));
		TestEqn(3, builder.char_ptrs.length);
		TestEqs("ab", *(const char**)(sv_array_at(&builder.char_ptrs, 0)));
		TestEqs("cdef", *(const char**)(sv_array_at(&builder.char_ptrs, 1)));
		TestEqn(NULL, *(sv_array_at(&builder.char_ptrs, 2)));
	}
	{ /* it's valid to give no arguments */
		char_ptr_array_builder_reset(&builder);
		char_ptr_array_builder_finalize(&builder);
		TestEqn(0, builder.offsets.length);
		TestEqn(1, builder.char_ptrs.length);
		TestEqn(NULL, *(sv_array_at(&builder.char_ptrs, 0)));
	}
	{ /* it's valid to give an empty argument */
		char_ptr_array_builder_reset(&builder);
		char_ptr_array_builder_add(&builder, str_and_len32s("ab"));
		char_ptr_array_builder_add(&builder, str_and_len32s(""));
		char_ptr_array_builder_add(&builder, str_and_len32s(""));
		char_ptr_array_builder_finalize(&builder);
		TestTrue(memcmp("ab\0\0\0", &builder.buffer->data, 5));
		TestEqn(3, builder.offsets.length);
		TestEqn(0, sv_array_at64u(&builder.offsets, 0));
		TestEqn(3, sv_array_at64u(&builder.offsets, 1));
		TestEqn(4, sv_array_at64u(&builder.offsets, 2));
		TestEqn(4, builder.char_ptrs.length);
		TestEqs("ab", *(const char**)(sv_array_at(&builder.char_ptrs, 0)));
		TestEqs("", *(const char**)(sv_array_at(&builder.char_ptrs, 1)));
		TestEqs("", *(const char**)(sv_array_at(&builder.char_ptrs, 2)));
		TestEqn(NULL, *(sv_array_at(&builder.char_ptrs, 3)));
	}

	char_ptr_array_builder_close(&builder);
}

void test_util_pseudosplit()
{
	{ /* basic usage */
		sv_pseudosplit spl = sv_pseudosplit_open("a|b|c");
		sv_pseudosplit_split(&spl, '|');
		TestEqn(3, spl.splitpoints.length);
		TestEqs("a", sv_pseudosplit_viewat(&spl, 0));
		TestEqs("b", sv_pseudosplit_viewat(&spl, 1));
		TestEqs("c", sv_pseudosplit_viewat(&spl, 2));
		sv_pseudosplit_close(&spl);
	}
	{ /* empty entries */
		sv_pseudosplit spl = sv_pseudosplit_open("");
		sv_pseudosplit_split(&spl, '|');
		TestEqn(1, spl.splitpoints.length);
		TestEqs("", sv_pseudosplit_viewat(&spl, 0));

		bassigncstr(spl.text, "||");
		sv_pseudosplit_split(&spl, '|');
		TestEqn(3, spl.splitpoints.length);
		TestEqs("", sv_pseudosplit_viewat(&spl, 0));
		TestEqs("", sv_pseudosplit_viewat(&spl, 1));
		TestEqs("", sv_pseudosplit_viewat(&spl, 2));

		bassigncstr(spl.text, "aaaa|");
		sv_pseudosplit_split(&spl, '|');
		TestEqn(2, spl.splitpoints.length);
		TestEqs("aaaa", sv_pseudosplit_viewat(&spl, 0));
		TestEqs("", sv_pseudosplit_viewat(&spl, 1));
		sv_pseudosplit_close(&spl);
	}
}

void test_util_c_string()
{
	TestTrue(s_equal("abc", "abc"));
	TestTrue(s_equal("D", "D"));
	TestTrue(!s_equal("abc", "abC"));
	TestTrue(s_startswith("abc", ""));
	TestTrue(s_startswith("abc", "a"));
	TestTrue(s_startswith("abc", "ab"));
	TestTrue(s_startswith("abc", "abc"));
	TestTrue(!s_startswith("abc", "abcd"));
	TestTrue(!s_startswith("abc", "aB"));
	TestTrue(!s_startswith("abc", "def"));
	TestTrue(s_endswith("abc", ""));
	TestTrue(s_endswith("abc", "c"));
	TestTrue(s_endswith("abc", "bc"));
	TestTrue(s_endswith("abc", "abc"));
	TestTrue(!s_endswith("abc", "aabc"));
	TestTrue(!s_endswith("abc", "abC"));
	TestTrue(!s_endswith("abc", "aB"));
	TestTrue(!s_endswith("abc", "def"));
	TestTrue(s_contains("abcde", ""));
	TestTrue(s_contains("abcde", "abc"));
	TestTrue(s_contains("abcde", "cde"));
	TestTrue(s_contains("abcde", "abcde"));
	TestTrue(!s_contains("abcde", "abcdef"));
	TestTrue(!s_contains("abcde", "fabcde"));
	TestTrue(!s_contains("abcde", "abCde"));
}

void test_util_fnmatch()
{
	TestTrue(fnmatch_simple("*", ""));
	TestTrue(fnmatch_simple("*", "abc"));
	TestTrue(fnmatch_simple("*", "a/b/c"));
	TestTrue(fnmatch_simple("*", "/a/"));
	TestTrue(fnmatch_simple("*.*", "a.a"));
	TestTrue(fnmatch_simple("*.*", "a."));
	TestTrue(!fnmatch_simple("*.*", "a"));
	TestTrue(!fnmatch_simple("*.*", ""));
	TestTrue(fnmatch_simple("a?c", "a0c"));
	TestTrue(!fnmatch_simple("a?c", "a00c"));
	TestTrue(fnmatch_simple("a\\b\\c", "a\\b\\c"));
	TestTrue(fnmatch_simple("a\\*", "a\\abcde"));
	TestTrue(fnmatch_simple("a\\c", "a\\c"));
	TestTrue(fnmatch_simple("a\\*c", "a\\000c"));
	TestTrue(fnmatch_simple("a\\???d", "a\\abcd"));
	TestTrue(!fnmatch_simple("a\\\\c", "a\\c"));
	TestTrue(!fnmatch_simple("prefix*", "pref"));
	TestTrue(fnmatch_simple("prefix*", "prefix"));
	TestTrue(!fnmatch_simple("prefix*", "Aprefix"));
	TestTrue(!fnmatch_simple("prefix*", "AprefixB"));
	TestTrue(fnmatch_simple("prefix*", "prefixB"));
	TestTrue(fnmatch_simple("prefix*", "prefix/file.txt"));
	TestTrue(fnmatch_simple("prefix*", "prefixdir/file.txt"));
	TestTrue(!fnmatch_simple("*contains*", "contain"));
	TestTrue(fnmatch_simple("*contains*", "contains"));
	TestTrue(fnmatch_simple("*contains*", "Acontains"));
	TestTrue(fnmatch_simple("*contains*", "AcontainsB"));
	TestTrue(fnmatch_simple("*contains*", "containsB"));
	TestTrue(fnmatch_simple("*contains*", "d/contains/file.txt"));
	TestTrue(fnmatch_simple("*contains*", "d/dircontainsdir/file.txt"));
	TestTrue(!fnmatch_simple("*suffix", "suff"));
	TestTrue(fnmatch_simple("*suffix", "suffix"));
	TestTrue(fnmatch_simple("*suffix", "Asuffix"));
	TestTrue(!fnmatch_simple("*suffix", "AsuffixB"));
	TestTrue(!fnmatch_simple("*suffix", "suffixB"));
	TestTrue(fnmatch_simple("*suffix", "a/suffix"));
	TestTrue(fnmatch_simple("*suffix", "a/dsuffix"));
	TestTrue(fnmatch_simple("aa*contains*bb", "aa things contains things bb"));

	bstring msg = bstring_open();
	fnmatch_checkvalid("*.*", msg);
	TestTrue(blength(msg) == 0);
	fnmatch_checkvalid("aba", msg);
	TestTrue(blength(msg) == 0);
	fnmatch_checkvalid("ab[a", msg);
	TestTrue(blength(msg) != 0);
	fnmatch_checkvalid("ab[ab]", msg);
	TestTrue(blength(msg) != 0);
	fnmatch_checkvalid("ab\x81", msg);
	TestTrue(blength(msg) != 0);
	bdestroy(msg);
}


check_result tmpwritetextfile(const char* dir, const char* leaf, bstring fullpath, const char* contents)
{
	sv_result currenterr = {};
	bassignformat(fullpath, "%s%s%s", dir, pathsep, leaf);
	check(sv_file_writefile(cstr(fullpath), contents, "wb"));
cleanup:
	return currenterr;
}

void expect_err_with_message(sv_result res, const char* msgcontains)
{
	TestTrue(res.code != 0);
	TestTrue(s_contains(cstr(res.msg), msgcontains));
	sv_result_close(&res);
}

#ifndef __linux__
void wide_to_utf8(const WCHAR* wide, bstring output);
void utf8_to_wide(const char* utf8, sv_wstr* output);
#endif

check_result test_util_widestr()
{
	sv_result currenterr = {};

	{ /* test append */
		sv_wstr ws = sv_wstr_open(4);
		TestTrue(wcscmp(L"", wcstr(ws)) == 0);
	}
	{ /* add one char at a time */
		sv_wstr ws = sv_wstr_open(4);
		TestTrue(wcscmp(L"", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"");
		TestTrue(wcscmp(L"", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"a");
		TestTrue(wcscmp(L"a", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"b");
		TestTrue(wcscmp(L"ab", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"c");
		TestTrue(wcscmp(L"abc", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"d");
		TestTrue(wcscmp(L"abcd", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"e");
		TestTrue(wcscmp(L"abcde", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"f");
		TestTrue(wcscmp(L"abcdef", wcstr(ws)) == 0);
		sv_wstr_close(&ws);
	}
	{ /* add long string */
		sv_wstr ws = sv_wstr_open(4);
		sv_wstrAppend(&ws, L"123456781234567812345678");
		TestTrue(wcscmp(L"123456781234567812345678", wcstr(ws)) == 0);
		sv_wstr_close(&ws);
	}
	{ /* test truncate */
		sv_wstr ws = sv_wstr_open(4);
		sv_wstrAppend(&ws, L"abc");
		sv_wstrTruncateLength(&ws, 2);
		TestTrue(wcscmp(L"ab", wcstr(ws)) == 0);
		sv_wstrTruncateLength(&ws, 0);
		TestTrue(wcscmp(L"", wcstr(ws)) == 0);
		sv_wstr_close(&ws);
	}
	{ /* test clear */
		sv_wstr ws = sv_wstr_open(4);
		sv_wstrAppend(&ws, L"ab");
		TestTrue(wcscmp(L"ab", wcstr(ws)) == 0);
		sv_wstrClear(&ws);
		TestTrue(wcscmp(L"", wcstr(ws)) == 0);
		sv_wstrAppend(&ws, L"cd");
		TestTrue(wcscmp(L"cd", wcstr(ws)) == 0);
		sv_wstr_close(&ws);
	}
#ifndef __linux__
	sv_wstr s_utf16 = sv_wstr_open(MAX_PATH);
	bstring s_utf8 = bstring_open();
	{ /* round-trip encoding ascii string */
		utf8_to_wide("abcd", &s_utf16);
		TestTrue(wcscmp(L"abcd", wcstr(s_utf16)) == 0);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("abcd", cstr(s_utf8));
		TestEqn(4, blength(s_utf8));
	}
	{ /* round-trip encoding empty string */
		utf8_to_wide("", &s_utf16);
		TestTrue(wcscmp(L"", wcstr(s_utf16)) == 0);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("", cstr(s_utf8));
		TestEqn(0, blength(s_utf8));
	}
	{ /* encoding 2-byte sequence (accented e) */
		utf8_to_wide("a\xC3\xA9", &s_utf16);
		TestTrue(wcscmp(L"a\xE9", wcstr(s_utf16)) == 0);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("a\xC3\xA9", cstr(s_utf8));
		TestEqn(3, blength(s_utf8));
	}
	{ /* encoding 3-byte sequence (hangul) */
		utf8_to_wide("a\xE1\x84\x81", &s_utf16);
		TestEqn(L'a', wcstr(s_utf16)[0]);
		TestEqn(0x1101, wcstr(s_utf16)[1]);
		TestEqn(L'\0', wcstr(s_utf16)[2]);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("a\xE1\x84\x81", cstr(s_utf8));
		TestEqn(4, blength(s_utf8));
	}
	{ /* encoding 4-byte sequence (musical symbol g clef) */
		utf8_to_wide("a\xF0\x9D\x84\x9E", &s_utf16);
		TestEqn(L'a', wcstr(s_utf16)[0]);
		TestEqn(0xD834, wcstr(s_utf16)[1]);
		TestEqn(0xDD1E, wcstr(s_utf16)[2]);
		TestEqn(L'\0', wcstr(s_utf16)[3]);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("a\xF0\x9D\x84\x9E", cstr(s_utf8));
		TestEqn(5, blength(s_utf8));
	}
	{ /* encoding only a 4-byte sequence (musical symbol g clef) */
		utf8_to_wide("\xF0\x9D\x84\x9E", &s_utf16);
		TestEqn(0xD834, wcstr(s_utf16)[0]);
		TestEqn(0xDD1E, wcstr(s_utf16)[1]);
		TestEqn(L'\0', wcstr(s_utf16)[2]);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("\xF0\x9D\x84\x9E", cstr(s_utf8));
		TestEqn(4, blength(s_utf8));
	} { /* encoding several 4-byte sequences (U+1D11E),(U+1D565),(U+1D7F6),(U+2008A) */
		utf8_to_wide("\xf0\x9d\x84\x9e\xf0\x9d\x95\xa5\xf0\x9d\x9f\xb6\xf0\xa0\x82\x8a", &s_utf16);
		TestEqn(0xD834, wcstr(s_utf16)[0]);
		TestEqn(0xDD1E, wcstr(s_utf16)[1]);
		TestEqn(0xd835, wcstr(s_utf16)[2]);
		TestEqn(0xdd65, wcstr(s_utf16)[3]);
		TestEqn(0xd835, wcstr(s_utf16)[4]);
		TestEqn(0xdff6, wcstr(s_utf16)[5]);
		TestEqn(0xd840, wcstr(s_utf16)[6]);
		TestEqn(0xdc8a, wcstr(s_utf16)[7]);
		TestEqn(L'\0', wcstr(s_utf16)[8]);
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("\xf0\x9d\x84\x9e\xf0\x9d\x95\xa5\xf0\x9d\x9f\xb6\xf0\xa0\x82\x8a", cstr(s_utf8));
		TestEqn(16, blength(s_utf8));
	}
	{ /* ensure correct length after encoding */
		utf8_to_wide("a\xE1\x84\x81\xE1\x84\x81", &s_utf16);
		TestEqn(4, s_utf16.arr.length);
		sv_wstrAppend(&s_utf16, L"a");
		TestEqn(5, s_utf16.arr.length);
		TestEqn(L'a', wcstr(s_utf16)[0]);
		TestEqn(L'a', wcstr(s_utf16)[3]);
		TestEqn(L'\0', wcstr(s_utf16)[4]);
	}
	bdestroy(s_utf8);
	sv_wstr_close(&s_utf16);
#endif

	return currenterr;
}

void test_os_split()
{
	bstring dir = bstring_open();
	bstring filename = bstring_open();
	{ /* os_split_dir with many subdirs */
		os_split_dir(pathsep "test" pathsep "one" pathsep "two", dir, filename);
		TestEqs(pathsep "test" pathsep "one", cstr(dir));
		TestEqs("two", cstr(filename));
	}
	{ /* os_split_dir with one subdir */
		os_split_dir(pathsep "test", dir, filename);
		TestEqs("", cstr(dir));
		TestEqs("test", cstr(filename));
	}
	{ /* os_split_dir with no subdir */
		os_split_dir("test", dir, filename);
		TestEqs("", cstr(dir));
		TestEqs("test", cstr(filename));
	}
	{ /* os_split_dir with trailing pathsep */
		os_split_dir(pathsep "test" pathsep, dir, filename);
		TestEqs(pathsep "test", cstr(dir));
		TestEqs("", cstr(filename));
	}
	{ /* os_split_dir with no root */
		os_split_dir("dir" pathsep "file", dir, filename);
		TestEqs("dir", cstr(dir));
		TestEqs("file", cstr(filename));
	}
	{ /* os_split_dir with no root, depth=2 */
		os_split_dir("dir" pathsep "dir" pathsep "file", dir, filename);
		TestEqs("dir" pathsep "dir", cstr(dir));
		TestEqs("file", cstr(filename));
	}
	{ /* os_split_dir with empty string */
		os_split_dir("", dir, filename);
		TestEqs("", cstr(dir));
		TestEqs("", cstr(filename));
	}
	{ /* os_get_filename */
		os_get_filename(pathsep "dirname" pathsep "a.txt", filename);
		TestEqs("a.txt", cstr(filename));
	}

#if __linux__
	TestTrue(os_isabspath("/a"));
	TestTrue(os_isabspath("/a/b/c"));
	TestTrue(!os_isabspath(""));
	TestTrue(!os_isabspath("a"));
	TestTrue(!os_isabspath("a/b/c"));
	TestTrue(!os_isabspath("../a/b/c"));
	TestTrue(!os_isabspath("\\a"));
	TestTrue(!os_isabspath("\\a\\b\\c"));
	TestTrue(os_create_dir("/"));
#else
	TestTrue(os_isabspath("C:\\a"));
	TestTrue(os_isabspath("C:\\a\\b\\c"));
	TestTrue(os_isabspath("C:\\"));
	TestTrue(!os_isabspath(""));
	TestTrue(!os_isabspath("C:/a"));
	TestTrue(!os_isabspath("a"));
	TestTrue(!os_isabspath("C:"));
	TestTrue(!os_isabspath("a\\b\\c"));
	TestTrue(!os_isabspath("..\\a\\b\\c"));
	TestTrue(!os_isabspath("\\\\fileshare"));
	TestTrue(!os_isabspath("\\\\fileshare\\path"));
	TestTrue(os_create_dir("C:\\"));
#endif

	TestTrue(os_issubdirof(pathsep "a", pathsep "a" pathsep "test"));
	TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a"));
	TestTrue(!os_issubdirof(pathsep "a", pathsep "ab" pathsep "test"));
	TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a" pathsep "testnotsame"));
	TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a" pathsep "tes0"));
	TestTrue(os_issubdirof(pathsep "a" pathsep "test", pathsep "a" pathsep "test"));

	bdestroy(dir);
	bdestroy(filename);
}


check_result test_util_readwritefiles(const char* testdirfileio)
{
	sv_result currenterr = {};
	bstring tempfile = bstring_open();
	bstring contents = bstring_open();

	/* write plain text to filename with utf-8 chars */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "write\xED\x95\x9C.txt", tempfile, "contents"));
	check(sv_file_readfile(cstr(tempfile), contents));
	TestEqn(8, contents->slen);
	TestEqs("contents", cstr(contents));

	/* write plain text */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "write.txt", tempfile, "contents"));
	check(sv_file_readfile(cstr(tempfile), contents));
	TestEqn(8, contents->slen);
	TestEqs("contents", cstr(contents));

	/* write text with newline characters */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "write.txt", tempfile, "a\n\n\n\nb\r\nc\r\r"));
	check(sv_file_readfile(cstr(tempfile), contents));
	TestEqn(11, contents->slen);
	TestEqs("a\n\n\n\nb\r\nc\r\r", cstr(contents));

	/* string with nuls */
#if WARN_NUL_IN_STR
	set_debugbreaks_enabled(false);
	bassignblk(contents, "abcd", 4);
	TestTrue(s_equal(cstr(contents), "abcd"));
	bassignblk(contents, "ab\0d", 4);
	TestTrue(cstr(contents) == NULL);
	set_debugbreaks_enabled(true);
#endif

	{ /* write text with nuls */
		sv_file f = {};
		bstring contentsWithNulls = bstring_open();
		check(sv_file_open(&f, cstr(tempfile), "wb"));
		fwrite("a\0b\0c", sizeof(char), 5, f.file);
		sv_file_close(&f);
		check(sv_file_readfile(cstr(tempfile), contentsWithNulls));
		TestEqn(5, contentsWithNulls->slen);
		TestEqn(6, contentsWithNulls->mlen);
		TestEqn('b', contentsWithNulls->data[2]);
		TestEqn('\0', contentsWithNulls->data[3]);
		TestEqn('c', contentsWithNulls->data[4]);
		TestEqn('\0', contentsWithNulls->data[5]);
		bdestroy(contentsWithNulls);
	}

cleanup:
	bdestroy(tempfile);
	bdestroy(contents);
	return currenterr;
}

check_result test_util_logging(const char* testdirfileio)
{
	sv_result currenterr = {};
	bstring tempfile = bstring_open();
	bstring contents = bstring_open();
	bstring logpathFirst = bformat("%s%s%s", testdirfileio, pathsep, "log00001.txt");
	bstring logpathSecond = bformat("%s%s%s", testdirfileio, pathsep, "log00002.txt");
	sv_log testLogger = {};

	/* test generating filenames */
	appendNumberToFilename("/path/dir", "prefix", ".txt", 1, tempfile);
	TestEqs("/path/dir" pathsep "prefix00001.txt", cstr(tempfile));
	appendNumberToFilename("/path/dir", "prefix", ".txt", 123456, tempfile);
	TestEqs("/path/dir" pathsep "prefix123456.txt", cstr(tempfile));

	/* test reading number from filename */
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file01.txt2"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/2path/file01.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "2/path/file01.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/fileA01.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/fileA01A.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file01A.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file01txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file12A34.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file01\xCC.txt"));
	TestEqn(0, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file00.txt"));
	TestEqn(1, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file1.txt"));
	TestEqn(1, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file00001.txt"));
	TestEqn(12345, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file12345.txt"));
	TestEqn(123456, readNumberFromFilename(tempfile, "/path/file", ".txt", "/path/file123456.txt"));

	/* no matching files, latest number is 0 */
	uint32_t latestNumber = UINT32_MAX;
	check(readLatestNumberFromFilename(testdirfileio, "testlog", ".txt", &latestNumber));
	TestEqn(0, latestNumber);

	/* third file present, should return 3 */
	check(tmpwritetextfile(testdirfileio, "testlog00003.txt", tempfile, ""));
	check(readLatestNumberFromFilename(testdirfileio, "testlog", ".txt", &latestNumber));
	TestEqn(3, latestNumber);

	/* first, third file present, should return 3 */
	check(tmpwritetextfile(testdirfileio, "testlog00001.txt", tempfile, ""));
	check(readLatestNumberFromFilename(testdirfileio, "testlog", ".txt", &latestNumber));
	TestEqn(3, latestNumber);

	/* first, second, third file present, should return 3 */
	check(tmpwritetextfile(testdirfileio, "testlog02.txt", tempfile, ""));
	check(readLatestNumberFromFilename(testdirfileio, "testlog", ".txt", &latestNumber));
	TestEqn(3, latestNumber);

	/* high number file present */
	check(tmpwritetextfile(testdirfileio, "testlog000123456.txt", tempfile, ""));
	check(readLatestNumberFromFilename(testdirfileio, "testlog", ".txt", &latestNumber));
	TestEqn(123456, latestNumber);

	/* first file present, should return 1. */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "testlog01.txt", tempfile, ""));
	check(readLatestNumberFromFilename(testdirfileio, "testlog", ".txt", &latestNumber));
	TestEqn(1, latestNumber);

	/* skips logging if current logger is null */
	sv_log_register_active_logger(NULL);
	sv_log_writeLine("should be skipped");

	/* write some log entries */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_log_open(&testLogger, testdirfileio));
	sv_log_register_active_logger(&testLogger);
	sv_log_writeLine("");
	sv_log_writeLine("abcd");
	sv_log_writeFmt("s %s int %d", "string", 123);
	sv_log_close(&testLogger);
	check(sv_file_readfile(cstr(logpathFirst), contents));
	const char* pattern = nativenewline "????" "/" "??" "/" "??"
		nativenewline "[??:??:??:???] "
		nativenewline "[??:??:??:???] abcd"
		nativenewline "[??:??:??:???] s string int 123";
	TestTrue(fnmatch_simple(pattern, cstr(contents)));

	/* test switch file */
	check(sv_log_open(&testLogger, testdirfileio));
	TestTrue(!os_file_exists(cstr(logpathSecond)));
	testLogger.cap_filesize = 4;
	testLogger.counter = 15; /* check filesize whenever counter%16==0 */
	sv_log_writeLine("abcd");
	sv_log_close(&testLogger);
	TestTrue(os_file_exists(cstr(logpathFirst)));
	TestTrue(os_file_exists(cstr(logpathSecond)));

	/* format timestamps */
	{
		check(tmpwritetextfile(testdirfileio, "testtimeformatting.txt", tempfile, ""));
		sv_file f = {};
		check(sv_file_open(&f, cstr(tempfile), "wb"));
		int64_t day_start_sep_27 = 1474934400ULL;
		int64_t morning_sep_27 = 1474949106ULL;
		int64_t evening_sep_27 = 1475007777ULL;
		sv_log_addNewLineTime(f.file, day_start_sep_27, morning_sep_27, 1);
		sv_log_addNewLineTime(f.file, day_start_sep_27, evening_sep_27, 10);
		sv_log_addNewLineTime(f.file, 0, 123, 100);
		sv_file_close(&f);
		check(sv_file_readfile(cstr(tempfile), contents));
		TestEqs(nativenewline "[04:05:06:001] " nativenewline "[20:22:57:010] " nativenewline "[00:02:03:100] ", cstr(contents));
	}

cleanup:
	bdestroy(logpathFirst);
	bdestroy(logpathSecond);
	bdestroy(tempfile);
	bdestroy(contents);
	sv_log_close(&testLogger);
	sv_log_register_active_logger(NULL);
	return currenterr;
}

check_result test_util_os_fileops(const char* testdirfileio)
{
	sv_result currenterr = {};
	bstring contents = bstring_open();
	bstring filename1 = bformat("%s%s%s", testdirfileio, pathsep, "\xED\x95\x9C file1.txt");
	bstring filename2 = bformat("%s%s%s", testdirfileio, pathsep, "\xED\x95\x9C file2.txt");

	/* os_file_exists should return false for missing files */
	TestTrue(!os_file_exists(""));
	TestTrue(!os_file_exists("."));
	TestTrue(!os_file_exists(".."));
	TestTrue(!os_file_exists("||invalid"));
	TestTrue(!os_file_exists("//invalid"));

	/* os_remove should work on a file */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(os_file_exists(cstr(filename1)));
	TestTrue(os_remove(cstr(filename1)));
	TestTrue(!os_file_exists(cstr(filename1)));

	/* os_remove returns true for non-existent file */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	TestTrue(!os_file_exists(cstr(filename2)));
	TestTrue(os_remove(cstr(filename2)));
	TestTrue(os_remove("d" pathsep "not_exist"));

	/* os_copy from Missing */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	TestTrue(!os_copy(cstr(filename1), cstr(filename2), true));
	TestTrue(!os_copy("||invalid", cstr(filename2), true));
	TestTrue(!os_copy("_not_exist_" pathsep "a", cstr(filename2), true));

	/* os_copy from Exists to Invalid */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(!os_copy(cstr(filename1), "||invalid", true));
	TestTrue(!os_copy(cstr(filename1), "_not_exist_" pathsep "a", true));

	/* os_copy from Exists to Missing */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(os_copy(cstr(filename1), cstr(filename2), false));
	TestTrue(os_file_exists(cstr(filename1)));
	TestTrue(os_file_exists(cstr(filename2)));

	/* os_copy from Exists to Exists, overwrite not allowed */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	check(sv_file_writefile(cstr(filename2), "defg", "w"));
	TestTrue(!os_copy(cstr(filename1), cstr(filename2), false));
	check(sv_file_readfile(cstr(filename2), contents));
	TestEqs("defg", cstr(contents));

	/* os_copy from Exists to Exists, overwrite allowed */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	check(sv_file_writefile(cstr(filename2), "defg", "w"));
	TestTrue(os_copy(cstr(filename1), cstr(filename2), true));
	TestTrue(os_file_exists(cstr(filename1)));
	check(sv_file_readfile(cstr(filename2), contents));
	TestEqs("abcd", cstr(contents));

	/* os_copy supports copying file onto itself */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(os_copy(cstr(filename1), cstr(filename1), false));
	TestTrue(os_file_exists(cstr(filename1)));

	/* os_move from Missing */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	TestTrue(!os_move(cstr(filename1), cstr(filename2), true));
	TestTrue(!os_move("||invalid", cstr(filename2), true));
	TestTrue(!os_move("_not_exist_" pathsep "a", cstr(filename2), true));

	/* os_move from Exists to Invalid */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(!os_move(cstr(filename1), "||invalid", true));
	TestTrue(!os_move(cstr(filename1), "_not_exist_" pathsep "a", true));
	TestTrue(!os_file_exists("_not_exist_" pathsep "a"));
	TestTrue(os_file_exists(cstr(filename1)));

	/* os_move from Exists to Missing */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(os_move(cstr(filename1), cstr(filename2), false));
	TestTrue(!os_file_exists(cstr(filename1)));
	TestTrue(os_file_exists(cstr(filename2)));

	/* os_move from Exists to Exists, overwrite not allowed */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	check(sv_file_writefile(cstr(filename2), "defg", "w"));
	TestTrue(!os_move(cstr(filename1), cstr(filename2), false));
	TestTrue(os_file_exists(cstr(filename1)));
	check(sv_file_readfile(cstr(filename2), contents));
	TestEqs("defg", cstr(contents));

	/* os_move from Exists to Exists, overwrite allowed */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	check(sv_file_writefile(cstr(filename2), "defg", "w"));
	TestTrue(os_move(cstr(filename1), cstr(filename2), true));
	TestTrue(!os_file_exists(cstr(filename1)));
	check(sv_file_readfile(cstr(filename2), contents));
	TestEqs("abcd", cstr(contents));

	/* os_move supports moving file onto itself */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(os_move(cstr(filename1), cstr(filename1), false));
	TestTrue(os_file_exists(cstr(filename1)));

	/* os_getfilesize simple */
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestEqn(strlen("abcd"), os_getfilesize(cstr(filename1)));

	/* os_getfilesize longer */
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	bFill(contents, 'a', 20000); /* 20 thousand of the character a */
	check(sv_file_writefile(cstr(filename1), cstr(contents), "w"));
	TestEqn(20000, strlen(cstr(contents)));
	TestEqn(20000, os_getfilesize(cstr(filename1)));

	/* os_getfilesize 0 length or missing */
	check(sv_file_writefile(cstr(filename1), "", "w"));
	TestEqn(strlen(""), os_getfilesize(cstr(filename1)));
	TestEqn(0, os_getfilesize("d" pathsep "not_exist"));

cleanup:
	bdestroy(contents);
	bdestroy(filename1);
	bdestroy(filename2);
	return currenterr;
}

static check_result test_util_recurse_callback(void* context,
	const bstring filepath, uint64_t modtime, uint64_t filesize, uint64_t unused)
{
	bstrList* list = (bstrList*)context;
	bstrListAppendCstr(list, cstr(filepath));
	TestTrue(modtime != 0);
	TestTrue(filesize != 0);
	if (!os_recurse_is_dir(modtime, filesize))
	{
		/* verify filesize */
		/* when we created these files, we gave file1 a size of 1, file2 a size of 2, and so on */
		check_fatal(s_endswith(cstr(filepath), ".txt"), "expect to end with .txt");
		char lastCharacter = (cstr(filepath))[blength(filepath) - 5];
		TestEqn(lastCharacter - '0', filesize);
	}

	(void)unused;
	return OK;
}

check_result test_util_os_dirlist(const char* testdirrecurse)
{
	/* give each filename and dirname utf-8 characters */
	sv_result currenterr = {};
	bstring dirname1 = bformat("%s" pathsep "\xED\x95\x9C d1", testdirrecurse);
	bstring dirname2 = bformat("%s" pathsep "\xED\x95\x9C d2", testdirrecurse);
	bstring dirname2sub = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "sub", testdirrecurse);
	bstring filename1 = bformat("%s" pathsep "\xED\x95\x9C d1" pathsep "\xED\x95\x9C f1.txt", testdirrecurse);
	bstring filename2 = bformat("%s" pathsep "\xED\x95\x9C d1" pathsep "\xED\x95\x9C f2.txt", testdirrecurse);
	bstring filename3 = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "\xED\x95\x9C f3.txt", testdirrecurse);
	bstring filename4 = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "\xED\x95\x9C f4.txt", testdirrecurse);
	bstring filename5 = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "sub" pathsep "\xED\x95\x9C f5.txt", testdirrecurse);
	bstrList* list = bstrListCreate();

	/* clean up anything left from a previous run */
	TestTrue(os_remove(cstr(filename1)));
	TestTrue(os_remove(cstr(filename2)));
	TestTrue(os_remove(cstr(filename3)));
	TestTrue(os_remove(cstr(filename4)));
	TestTrue(os_remove(cstr(filename5)));
	TestTrue(os_remove(cstr(dirname2sub)));
	TestTrue(os_remove(cstr(dirname1)));
	TestTrue(os_remove(cstr(dirname2)));

	/* check for existing files or dirs */
	if (!os_dir_empty(testdirrecurse))
	{
		printf("the test directory contains remnants from a previous build, "
			"please delete the directory %s and run the tests again.", testdirrecurse);
		alertdialog("");
		return OK;
	}

	/* test os_dir_exists */
	TestTrue(!os_dir_exists(cstr(dirname1)));
	TestTrue(os_create_dir(cstr(dirname1)));
	TestTrue(os_dir_exists(cstr(dirname1)));
	TestTrue(!os_dir_exists(cstr(dirname2sub)));

	/* os_remove should work on an empty directory */
	TestTrue(os_create_dir(cstr(dirname1)));
	TestTrue(os_dir_exists(cstr(dirname1)));
	TestTrue(os_remove(cstr(dirname1)));
	TestTrue(!os_dir_exists(cstr(dirname1)));

	/* os_remove should not work on a non-empty directory */
	TestTrue(os_create_dir(cstr(dirname1)));
	TestTrue(os_dir_exists(cstr(dirname1)));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	TestTrue(!os_remove(cstr(dirname1)));
	TestTrue(os_dir_exists(cstr(dirname1)));
	TestTrue(os_remove(cstr(filename1)));
	TestTrue(os_remove(cstr(dirname1)));
	TestTrue(!os_dir_exists(cstr(dirname1)));

	{ /* create directory structure */
		TestTrue(os_create_dir(cstr(dirname1)));
		TestTrue(os_create_dir(cstr(dirname2)));
		TestTrue(os_create_dir(cstr(dirname2sub)));
		check(sv_file_writefile(cstr(filename1), "I", "w"));
		check(sv_file_writefile(cstr(filename2), "II", "w"));
		check(sv_file_writefile(cstr(filename3), "III", "w"));
		check(sv_file_writefile(cstr(filename4), "IIII", "w"));
		check(sv_file_writefile(cstr(filename5), "IIIII", "w"));

		/* os_listfiles (non-recursive) */
		check(os_listfiles(cstr(dirname2), list, true));
		TestEqn(2, list->qty);
		TestEqs(cstr(filename3), bstrListViewAt(list, 0));
		TestEqs(cstr(filename4), bstrListViewAt(list, 1));

		/* os_listdirs (non-recursive) */
		check(os_listdirs(testdirrecurse, list, true));
		TestEqn(2, list->qty);
		TestEqs(cstr(dirname1), bstrListViewAt(list, 0));
		TestEqs(cstr(dirname2), bstrListViewAt(list, 1));

		/* os_recurse, not limited by depth */
		bstrListClear(list);
		os_recurse_params params = { list, testdirrecurse, &test_util_recurse_callback, INT_MAX };
		check(os_recurse(&params));
		bstrListSort(list);
		TestEqn(8, list->qty);
		const char* expected[] = { cstr(dirname1), cstr(filename1), cstr(filename2),
			cstr(dirname2), cstr(dirname2sub), cstr(filename5), cstr(filename3), cstr(filename4) };
		for (int i = 0; i < countof32s(expected); i++)
			TestEqs(expected[i], bstrListViewAt(list, i));

		/* os_recurse, limited depth should throw an error */
		set_debugbreaks_enabled(false);
		params.maxRecursionDepth = 1;
		expect_err_with_message(os_recurse(&params), "recursion limit");
		set_debugbreaks_enabled(true);

		/* os_deletefiles removes files but not dirs */
		check(os_tryuntil_deletefiles(cstr(dirname2), "*"));
		check(os_listfiles(cstr(dirname2), list, true));
		TestEqn(0, list->qty);
		TestTrue(os_dir_exists(cstr(dirname2sub)));
		TestTrue(os_file_exists(cstr(filename5)));
	}

cleanup:
	bdestroy(filename1);
	bdestroy(filename2);
	bdestroy(filename3);
	bdestroy(filename4);
	bdestroy(filename5);
	bdestroy(dirname1);
	bdestroy(dirname2);
	bdestroy(dirname2sub);
	bstrListDestroy(list);
	return currenterr;
}

check_result test_util_os_runprocess(const char* dir)
{
	sv_result currenterr = {};
	bstring script_noexitcode = bformat("%s%sno exit%s", dir, pathsep, islinux ? ".sh" : ".bat");
	bstring script_exitcode = bformat("%s%shas exit%s", dir, pathsep, islinux ? ".sh" : ".bat");
	bstring script_printarg = bformat("%s%sprint arg%s", dir, pathsep, islinux ? ".sh" : ".bat");
	bstring script_printstdin = bformat("%s%sprint stdin%s", dir, pathsep, islinux ? ".sh" : ".bat");
	bstring getoutput = bstring_open();
	bstring useforargs = bstring_open();
	bstring cmd = bstring_open();
	check(os_tryuntil_deletefiles(dir, "*"));

#if __linux__
	check(sv_file_writefile(cstr(script_noexitcode), "echo 'hi'\necho 'oh' 1>&2\necho 'yay'", "wb"));
	check(sv_file_writefile(cstr(script_exitcode), "echo 'hi'\necho 'oh' 1>&2\nexit 22", "wb"));
	check(sv_file_writefile(cstr(script_printarg), "echo 'arg'\necho $1", "wb"));
	check(sv_file_writefile(cstr(script_printstdin), "read var\necho $var > gotfromstdin.txt\n", "wb")); /* nb: reads only the first line */
#define FIRST_PARAM() 
#else
	check(sv_file_writefile(cstr(script_noexitcode), "@echo off\r\necho hi\r\necho oh 1>&2\r\necho yay", "wb"));
	check(sv_file_writefile(cstr(script_exitcode), "@echo off\r\necho hi\r\necho oh 1>&2\r\nEXIT /b 22", "wb"));
	check(sv_file_writefile(cstr(script_printarg), "@echo off\r\necho arg\r\necho %1", "wb"));
	check(sv_file_writefile(cstr(script_printstdin), "@echo off\r\n"
		"set /p var=Enter:\r\n"
		"echo val is \"%var%\" > gotfromstdin.txt", "wb"));  /* nb: reads the first line */
#define FIRST_PARAM() "/c",
#endif
	const char* sh = islinux ? "/bin/sh" : getenv("comspec");

	{ /* read 0 exit code, read output */
		const char* args[] = { FIRST_PARAM() cstr(script_noexitcode), NULL };
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "ghghj" : "hi\r\noh \r\nyay\r\n", cstr(getoutput));
		TestTrue(retcode == 0);
	}
	{ /* read non-0 exit code, read output */
		const char* args[] = { FIRST_PARAM() cstr(script_exitcode), NULL };
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "ghghj" : "hi\r\noh \r\n", cstr(getoutput));
		TestTrue(retcode == 22);
	}
	{ /* read non-0 exit code, read output only on error */
		const char* args[] = { FIRST_PARAM() cstr(script_exitcode), NULL };
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "ghghj" : "hi\r\noh \r\n", cstr(getoutput));
		TestTrue(retcode == 22);
	}
	{ /* read given parameter */
		const char* args[] = { FIRST_PARAM() cstr(script_printarg), "paramgiven", NULL };
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "ghghj" : "arg\r\n\"paramgiven\r\n", cstr(getoutput));
		TestTrue(retcode == 0);
	}
#ifndef __linux__
	{ /* read stdin */
		os_setcwd(dir);
		bassignformat(useforargs, "%s%stextfile.txt", dir, pathsep);
		check(sv_file_writefile(cstr(useforargs), "abcde", "w"));
		os_exclusivefilehandle handle = {};
		check(os_exclusivefilehandle_open(&handle, cstr(useforargs), true, NULL));
		check_b(handle.fd, "did not set fd");
		const char* args[] = { FIRST_PARAM() cstr(script_printstdin), NULL };
		int retcode = 0;
		check(os_run_process_ex(sh, args, os_proc_stdin, &handle, true, useforargs, getoutput, &retcode));
		TestTrue(retcode == 0);
		TestEqs("", cstr(getoutput));
		bassignformat(useforargs, "%s%sgotfromstdin.txt", dir, pathsep);
		check(sv_file_readfile(cstr(useforargs), getoutput));
		TestEqs("val is \"abcde\" \r\n", cstr(getoutput));
		os_exclusivefilehandle_close(&handle);
	}
#endif

cleanup:
	bdestroy(script_exitcode);
	bdestroy(script_noexitcode);
	bdestroy(script_printarg);
	bdestroy(script_printstdin);
	bdestroy(getoutput);
	bdestroy(useforargs);
	bdestroy(cmd);
	return currenterr;
}

void createsearchspec(sv_wstr* dir, sv_wstr* buffer);

check_result tests_requiring_interaction(void)
{
	bstrList* list = bstrListCreate();
	while (ask_user_yesno("Run pick-number-from-long list test?"))
	{
		bstrListClear(list);
		bstring s = bstring_open();
		for (int i = 0; i < 30; i++)
		{
			bassignformat(s, "a%da", i + 1);
			bstrListAppendCstr(list, cstr(s));
		}

		int chosen = ui_numbered_menu_pick_from_long_list(list, 4);
		if (chosen >= 0)
			printf("you picked %d (%s)\n", chosen + 1, bstrListViewAt(list, chosen));
		else
			printf("you decided to cancel\n");

		alertdialog("");
		bdestroy(s);
	}

	alertdialog("For this test we'll intentionally trigger integer overflow and verify that it causes program exit(). "
		"Please attach a debugger and place a breakpoint in the function void die(void) and confirm that the breakpoint is hit "
		"for each of the statements below. You can use your debugger to skip over the exit() call.");
	if (ask_user_yesno("Run this test? Is a debugger attached?"))
	{
		checkedmul32(123456, 34790);
		checkedmul32(4294967295, 2);
		checkedmul32(4294967295, 2);
		checkedadd32(1, 4294967295);
		checkedadd32(4294967295, 4294967295);
		checkedadd32s(1, 2147483647);
		checkedadd32s(2147483647, 2147483647);
		checkedadd32s(-1, INT_MIN);
		checkedadd32s(INT_MIN, INT_MIN);
		cast32u32s(UINT32_MAX - 1);
		cast32s32u(-1);
		cast64u64s(0xffffffff11111111ULL);
		cast64s64u(-1);
		cast64u32u(0xffffffffULL + 1);
		cast64s32s(0xffffffffULL / 2 + 1);
	}
	
	bstrListDestroy(list);
	return OK;
}

check_result run_utils_tests(void)
{
	sv_result currenterr = {};
	bstring dir = os_get_tmpdir("tmpglacial_backup");
	bstring testdir = os_make_subdir(cstr(dir), "tests");
	bstring testdirfileio = os_make_subdir(cstr(testdir), "fileio");
	bstring testdirrecurse = os_make_subdir(cstr(testdir), "recurse");
	bstring testdirdb = os_make_subdir(cstr(testdir), "testdb");

	test_util_basicarithmatic();
	test_util_array();
	test_util_2d_array();
	test_util_string();
	test_util_c_string();
	test_util_stringlist();
	test_util_char_ptr_builder();
	test_util_pseudosplit();
	test_util_fnmatch();
	test_os_split();
	check(test_util_widestr());
	check(test_util_readwritefiles(cstr(testdirfileio)));
	check(test_util_logging(cstr(testdirfileio)));
	check(test_util_os_fileops(cstr(testdirfileio)));
	check(test_util_os_dirlist(cstr(testdirrecurse)));
	check(test_util_os_runprocess(cstr(testdirfileio)));
#if 0
	check(tests_requiring_interaction());
#endif

cleanup:
	bdestroy(dir);
	bdestroy(testdir);
	bdestroy(testdirfileio);
	bdestroy(testdirrecurse);
	bdestroy(testdirdb);
	return currenterr;
}
