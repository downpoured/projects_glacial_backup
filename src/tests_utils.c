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

bool teststrimpl(int lineno, const char *s1, const char *s2)
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


bool testlistimpl(int lineno, const char *expected, const bstrlist *list)
{
	bstring joined = bjoin_static(list, "|");
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
		uint64_t testbuffer[10] = { 10, 0 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 3);
		TestEqn(3, arr.capacity);
		TestEqn(0, arr.length);

		sv_array_append(&arr, (byte *)&testbuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(1, arr.length);

		sv_array_append(&arr, (byte *)&testbuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(2, arr.length);

		sv_array_append(&arr, (byte *)&testbuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(3, arr.length);

		sv_array_append(&arr, (byte *)&testbuffer[0], 1);
		TestEqn(4, arr.capacity);
		TestEqn(4, arr.length);

		sv_array_append(&arr, (byte *)&testbuffer[0], 1);
		TestEqn(8, arr.capacity);
		TestEqn(5, arr.length);

		sv_array_close(&arr);
	}
	{ /* test reserve */
		uint64_t test_buffer[10] = { 10, 0 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_append(&arr, (byte *)&test_buffer[0], 1);
		TestEqn(1, arr.capacity);
		TestEqn(1, arr.length);
		TestEqn(10, *(uint64_t *)sv_array_at(&arr, 0));

		sv_array_reserve(&arr, 4000);
		TestEqn(4096, arr.capacity);
		TestEqn(1, arr.length);
		TestEqn(10, *(uint64_t *)sv_array_at(&arr, 0));
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
		uint64_t test_buffer[10] = { 10, 11, 12, 13 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 3);
		sv_array_append(&arr, (byte *)&test_buffer[0], 4);
		TestEqn(10, *(uint64_t *)sv_array_at(&arr, 0));
		TestEqn(11, *(uint64_t *)sv_array_at(&arr, 1));
		TestEqn(12, *(uint64_t *)sv_array_at(&arr, 2));
		TestEqn(13, *(uint64_t *)sv_array_at(&arr, 3));
		TestEqn(4, arr.length);
		sv_array_append(&arr, (byte *)&test_buffer[0], 1);
		TestEqn(10, *(uint64_t *)sv_array_at(&arr, 4));
		TestEqn(5, arr.length);
		sv_array_close(&arr);
	}
	{ /* valid truncate */
		uint64_t test_buffer[10] = { 10, 11 };
		sv_array arr = sv_array_open(sizeof32u(uint64_t), 0);
		sv_array_append(&arr, (byte *)&test_buffer[0], 2);
		TestEqn(2, arr.length);

		sv_array_truncatelength(&arr, 2);
		TestEqn(2, arr.length);

		sv_array_truncatelength(&arr, 1);
		TestEqn(1, arr.length);
		TestEqn(10, *(uint64_t *)sv_array_at(&arr, 0));
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
		sv_array *subarr = (sv_array *)sv_array_at(&arr.arr, 0);
		TestEqn(1, subarr->length);
		TestEqn(0, *sv_array_at(subarr, 0));
		TestTrue(sv_2d_array_at(&arr, 0, 0) == sv_array_at(subarr, 0));
		sv_2d_array_close(&arr);
	}
	{ /* hold data at 8x4 */
		sv_2d_array arr = sv_2d_array_open(sizeof32u(byte));
		sv_2d_array_ensure_space(&arr, 7, 3);
		TestEqn(8, arr.arr.length);
		sv_array *subarr = (sv_array *)sv_array_at(&arr.arr, 7);
		TestEqn(4, subarr->length);
		TestEqn(0, *sv_array_at(subarr, 0));
		TestEqn(0, *sv_array_at(subarr, 1));
		TestEqn(0, *sv_array_at(subarr, 2));
		TestEqn(0, *sv_array_at(subarr, 3));
		TestTrue(sv_2d_array_at(&arr, 7, 0) == sv_array_at(subarr, 0));
		TestTrue(sv_2d_array_at(&arr, 7, 3) == sv_array_at(subarr, 3));

		/* it's a jagged array, so the length at other dimensions is unaffected */
		subarr = (sv_array *)sv_array_at(&arr.arr, 0);
		TestEqn(0, subarr->length);

		/* allocate space at 4,8 */
		sv_2d_array_ensure_space(&arr, 3, 7);
		subarr = (sv_array *)sv_array_at(&arr.arr, 3);
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
	TestEqn(0x12341234ULL, upper32(0x1234123466667777ULL));
	TestEqn(0x66667777ULL, lower32(0x1234123466667777ULL));
	TestEqn(0, round_up_to_multiple(0, 8));
	TestEqn(8, round_up_to_multiple(1, 8));
	TestEqn(48, round_up_to_multiple(47, 8));
	TestEqn(48, round_up_to_multiple(48, 8));
	TestEqn(56, round_up_to_multiple(49, 8));
	TestEqn(1, nearest_power_of_two32(0));
	TestEqn(1, nearest_power_of_two32(1));
	TestEqn(2, nearest_power_of_two32(2));
	TestEqn(4, nearest_power_of_two32(3));
	TestEqn(16, nearest_power_of_two32(15));
	TestEqn(16, nearest_power_of_two32(16));
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
		bstrclear(str);
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
	{ /* digits added if necessary */
		bstring str = bformat("%08llx %08llx %08llx", 0x123ULL, 0x12341234ULL, 0x123412349999ULL);
		TestEqs("00000123 12341234 123412349999", cstr(str));
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
		tagbstring s = { 0 };
		TestEqn(BSTR_ERR, bdestroy(&s));
	}
	{ /* bstr_replaceall */
		bstring s = bfromcstr("abaBabaBa");
		bstr_replaceall(s, "b", "111");
		TestEqs(cstr(s), "a111aBa111aBa");
		bdestroy(s);
	}
	{ /* bstr_replaceall */
		bstring s = bfromcstr("abaBabaBa");
		bstr_replaceall(s, "ab", "");
		TestEqs(cstr(s), "aBaBa");
		bdestroy(s);
	}
	{ /* bstr_equal (note--not part of bstr) */
		bstring s1 = bstring_open();
		bstring s2 = bstring_open();
		TestTrue(bstr_equal(s1, s2));
		bstr_assignstatic(s1, "");
		bstr_assignstatic(s2, " ");
		TestTrue(!bstr_equal(s1, s2));
		bstr_assignstatic(s1, " ");
		bstr_assignstatic(s2, "");
		TestTrue(!bstr_equal(s1, s2));
		bstr_assignstatic(s1, " ");
		bstr_assignstatic(s2, " ");
		TestTrue(bstr_equal(s1, s2));
		bstr_assignstatic(s1, "abcdef");
		bstr_assignstatic(s2, "abcdeF");
		TestTrue(!bstr_equal(s1, s2));
		bstr_assignstatic(s1, "abcdefg");
		bstr_assignstatic(s2, "abcde");
		TestTrue(!bstr_equal(s1, s2));
		bstr_assignstatic(s1, "abcdef");
		bstr_assignstatic(s2, "abcdef");
		TestTrue(bstr_equal(s1, s2));
		bdestroy(s1);
		bdestroy(s2);
	}
}

void test_util_stringlist()
{
	bstrlist *list = bstrlist_open();

	{ /* simple split and join */
		tagbstring s = bstr_static("a|b|c");
		tagbstring delim = bstr_static("|");
		bstrlist_split(list, &s, &delim);
		TestEqn(3, list->qty);
		TestEqs("a", cstr(list->entry[0]));
		TestEqs("b", cstr(list->entry[1]));
		TestEqs("c", cstr(list->entry[2]));
		bstring joined = bjoin_static(list, "|");
		TestEqs("a|b|c", cstr(joined));
		bdestroy(joined);
	}
	{ /* split */
		bstrlist_splitcstr(list, "a||b|c", "|");
		TestEqList("a||b|c", list);
	}
	{ /* split */
		bstrlist_splitcstr(list, "aabcbabccabc", "abc");
		TestEqList("a|b|c|", list);
	}
	{ /* split corner cases */
		bstrlist_splitcstr(list, "", "|");
		TestEqn(1, list->qty);
		TestEqList("", list);
		bstrlist_splitcstr(list, "|", "|");
		TestEqn(2, list->qty);
		TestEqList("|", list);
		bstrlist_splitcstr(list, "||", "|");
		TestEqList("||", list);
		bstrlist_splitcstr(list, "||a||b|c||", "|");
		TestEqList("||a||b|c||", list);
	}
	{ /* append and view */
		bstrlist_clear(list);
		bstrlist_appendcstr(list, "abc");
		bstrlist_appendcstr(list, "123");
		bstrlist_appendcstr(list, "def");
		TestEqList("abc|123|def", list);
		TestEqn(3, list->qty);
		TestEqs("abc", bstrlist_view(list, 0));
		TestEqs("123", bstrlist_view(list, 1));
		TestEqs("def", bstrlist_view(list, 2));
	}
	{ /* append bstring and concat lists */
		tagbstring s = bstr_static("x|y|z");
		bstrlist *otherlist = bsplit(&s, '|');
		bstring otherstring = bstr_fromstatic("other");
		bstrlist_clear(list);
		bstrlist_appendcstr(list, "1");
		bstrlist_append(list, otherstring);
		bstrlist_concat(list, otherlist);
		TestEqList("1|other|x|y|z", list);
		bdestroy(otherstring);
		bstrlist_close(otherlist);
	}
	{ /* cannot remove out-of-bounds */
		bstrlist_clear(list);
		TestEqn(BSTR_ERR, bstrlist_remove_at(list, 0));
		TestEqn(BSTR_ERR, bstrlist_remove_at(list, 1));
		bstrlist_appendcstr(list, "abc");
		TestEqn(BSTR_ERR, bstrlist_remove_at(list, -1));
		TestEqn(BSTR_ERR, bstrlist_remove_at(list, 1));
		TestEqn(BSTR_ERR, bstrlist_remove_at(list, 2));
	}
	{ /* append and remove */
		bstrlist_clear(list);
		bstrlist_appendcstr(list, "a");
		bstrlist_appendcstr(list, "b");
		bstrlist_appendcstr(list, "c");
		bstrlist_appendcstr(list, "d");
		bstrlist_appendcstr(list, "e");
		TestEqn(BSTR_OK, bstrlist_remove_at(list, 4));
		TestEqList("a|b|c|d", list);
		TestEqn(BSTR_OK, bstrlist_remove_at(list, 0));
		TestEqList("b|c|d", list);
		TestEqn(BSTR_OK, bstrlist_remove_at(list, 1));
		TestEqList("b|d", list);
		TestEqn(BSTR_OK, bstrlist_remove_at(list, 0));
		TestEqList("d", list);
		TestEqn(BSTR_OK, bstrlist_remove_at(list, 0));
		TestEqList("", list);
		bstrlist_appendcstr(list, "a");
		bstrlist_appendcstr(list, "b");
		TestEqList("a|b", list);
	}
	{ /* copy list. the copy should be completely independent. */
		bstrlist_splitcstr(list, "a|b|c", "|");
		bstrlist *listcopy = bstrlist_copy(list);
		TestEqList("a|b|c", listcopy);
		bcatcstr(list->entry[0], "added");
		TestEqList("a|b|c", listcopy);
		bstrlist_close(listcopy);
	}
	{ /* test clearing a list */
		bstrlist_clear(list);
		TestEqList("", list);
		TestEqn(0, list->qty);
		bstrlist_appendcstr(list, "a");
		bstrlist_appendcstr(list, "b");
		TestEqList("a|b", list);
		bstrlist_clear(list);
		TestEqList("", list);
		TestEqn(0, list->qty);
	}
	{ /* test sort */
		bstrlist_splitcstr(list, "apple2|apple1|pear|peach", "|");
		bstrlist_sort(list);
		TestEqList("apple1|apple2|peach|pear", list);
		bstrlist_appendcstr(list, "aaa");
		TestEqn(BSTR_OK, bstrlist_remove_at(list, 4));
		bcatcstr(list->entry[2], "a");
		bstrlist_sort(list);
		TestEqList("apple1|apple2|peacha|pear", list);
	}
	{ /* test join on empty list */
		bstrlist_clear(list);
		bstring all = bjoin_static(list, "|");
		TestEqs("", cstr(all));
		bdestroy(all);
	}

	bstrlist_close(list);
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

void test_os_getbinarypath(void)
{
	/* find full path of executable on the system $path */
	const char *binname = islinux ? "grep" : "calc.exe";
	bstring fullpath = bstring_open();
	check_warn(os_binarypath(binname, fullpath), "", exit_on_err);
	TestTrue(os_isabspath(cstr(fullpath)));
	TestTrue(os_file_exists(cstr(fullpath)));

	/* if executable is not found, should return empty string */
	check_warn(os_binarypath("binary-doesn't-exist", fullpath), "", exit_on_err);
	TestEqs("", cstr(fullpath));
	bdestroy(fullpath);
}

check_result tmpwritetextfile(const char *dir, const char *leaf, bstring fullpath, const char *contents)
{
	sv_result currenterr = {};
	bassignformat(fullpath, "%s%s%s", dir, pathsep, leaf);
	check(sv_file_writefile(cstr(fullpath), contents, "wb"));
cleanup:
	return currenterr;
}

void expect_err_with_message(sv_result res, const char *msgcontains)
{
	TestTrue(res.code != 0);
	if (!s_contains(cstr(res.msg), msgcontains))
	{
		printf("message \n%s\n did not contain '%s'", cstr(res.msg), msgcontains);
		TestTrue(false);
	}

	sv_result_close(&res);
}

#ifndef __linux__
void wide_to_utf8(const wchar_t *wide, bstring output);
void utf8_to_wide(const char *utf8, sv_wstr *output);
#endif

check_result test_util_widestr()
{
	sv_result currenterr = {};

	{ /* test append */
		sv_wstr ws = sv_wstr_open(4);
		TestTrue(ws_equal(L"", wcstr(ws)));
	}
	{ /* add one char at a time */
		sv_wstr ws = sv_wstr_open(4);
		TestTrue(ws_equal(L"", wcstr(ws)));
		sv_wstr_append(&ws, L"");
		TestTrue(ws_equal(L"", wcstr(ws)));
		sv_wstr_append(&ws, L"a");
		TestTrue(ws_equal(L"a", wcstr(ws)));
		sv_wstr_append(&ws, L"b");
		TestTrue(ws_equal(L"ab", wcstr(ws)));
		sv_wstr_append(&ws, L"c");
		TestTrue(ws_equal(L"abc", wcstr(ws)));
		sv_wstr_append(&ws, L"d");
		TestTrue(ws_equal(L"abcd", wcstr(ws)));
		sv_wstr_append(&ws, L"e");
		TestTrue(ws_equal(L"abcde", wcstr(ws)));
		sv_wstr_append(&ws, L"f");
		TestTrue(ws_equal(L"abcdef", wcstr(ws)));
		sv_wstr_close(&ws);
	}
	{ /* add long string */
		sv_wstr ws = sv_wstr_open(4);
		sv_wstr_append(&ws, L"123456781234567812345678");
		TestTrue(ws_equal(L"123456781234567812345678", wcstr(ws)));
		sv_wstr_append(&ws, L"123456781234567812345678");
		TestTrue(ws_equal(L"123456781234567812345678123456781234567812345678", wcstr(ws)));
		sv_wstr_close(&ws);
	}
	{ /* test truncate */
		sv_wstr ws = sv_wstr_open(4);
		sv_wstr_append(&ws, L"abc");
		sv_wstr_truncate(&ws, 2);
		TestTrue(ws_equal(L"ab", wcstr(ws)));
		sv_wstr_truncate(&ws, 0);
		TestTrue(ws_equal(L"", wcstr(ws)));
		sv_wstr_close(&ws);
	}
	{ /* test clear */
		sv_wstr ws = sv_wstr_open(4);
		sv_wstr_append(&ws, L"ab");
		TestTrue(ws_equal(L"ab", wcstr(ws)));
		sv_wstr_clear(&ws);
		TestTrue(ws_equal(L"", wcstr(ws)));
		sv_wstr_append(&ws, L"cd");
		TestTrue(ws_equal(L"cd", wcstr(ws)));
		sv_wstr_close(&ws);
	}
#ifndef __linux__
	sv_wstr s_utf16 = sv_wstr_open(MAX_PATH);
	bstring s_utf8 = bstring_open();
	{ /* round-trip encoding ascii string */
		utf8_to_wide("abcd", &s_utf16);
		TestTrue(ws_equal(L"abcd", wcstr(s_utf16)));
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("abcd", cstr(s_utf8));
		TestEqn(4, blength(s_utf8));
	}
	{ /* round-trip encoding empty string */
		utf8_to_wide("", &s_utf16);
		TestTrue(ws_equal(L"", wcstr(s_utf16)));
		wide_to_utf8(wcstr(s_utf16), s_utf8);
		TestEqs("", cstr(s_utf8));
		TestEqn(0, blength(s_utf8));
	}
	{ /* encoding 2-byte sequence (accented e) */
		utf8_to_wide("a\xC3\xA9", &s_utf16);
		TestTrue(ws_equal(L"a\xE9", wcstr(s_utf16)));
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
		sv_wstr_append(&s_utf16, L"a");
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

void test_os_split(const char *testdir)
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
	TestTrue((int)-1 == (int)(ssize_t)-1); /* allow cast */
	TestTrue((int)-1 == (int)(off_t)-1); /* allow cast */
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

	TestTrue(os_create_dir(testdir));
	TestTrue(os_issubdirof(pathsep "a", pathsep "a" pathsep "test"));
	TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a"));
	TestTrue(!os_issubdirof(pathsep "a", pathsep "ab" pathsep "test"));
	TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a" pathsep "testnotsame"));
	TestTrue(!os_issubdirof(pathsep "a" pathsep "test", pathsep "a" pathsep "tes0"));
	TestTrue(os_issubdirof(pathsep "a" pathsep "test", pathsep "a" pathsep "test"));

	bdestroy(dir);
	bdestroy(filename);
}


check_result test_util_readwritefiles(const char *testdirfileio)
{
	sv_result currenterr = {};
	bstring file = bstring_open();
	bstring contents = bstring_open();

	/* write plain text to filename with utf-8 chars */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "write\xED\x95\x9C.txt", file, "contents"));
	check(sv_file_readfile(cstr(file), contents));
	TestEqn(8, contents->slen);
	TestEqs("contents", cstr(contents));

	/* write plain text */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "write.txt", file, "contents"));
	check(sv_file_readfile(cstr(file), contents));
	TestEqn(8, contents->slen);
	TestEqs("contents", cstr(contents));

	/* write text with newline characters */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "write.txt", file, "a\n\n\n\nb\r\nc\r\r"));
	check(sv_file_readfile(cstr(file), contents));
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
		bstring contents_with_nulls = bstring_open();
		check(sv_file_open(&f, cstr(file), "wb"));
		fwrite("a\0b\0c", sizeof(char), 5, f.file);
		sv_file_close(&f);
		check(sv_file_readfile(cstr(file), contents_with_nulls));
		TestEqn(5, contents_with_nulls->slen);
		TestEqn(6, contents_with_nulls->mlen);
		TestEqn('b', contents_with_nulls->data[2]);
		TestEqn('\0', contents_with_nulls->data[3]);
		TestEqn('c', contents_with_nulls->data[4]);
		TestEqn('\0', contents_with_nulls->data[5]);
		bdestroy(contents_with_nulls);
	}

cleanup:
	bdestroy(file);
	bdestroy(contents);
	return currenterr;
}

check_result test_util_logging(const char *testdirfileio)
{
	sv_result currenterr = {};
	bstring file = bstring_open();
	bstring contents = bstring_open();
	bstring logpathfirst = bformat("%s%s%s", testdirfileio, pathsep, "log00001.txt");
	bstring logpathsecond = bformat("%s%s%s", testdirfileio, pathsep, "log00002.txt");
	sv_log testlogger = {};

	/* test generating filenames */
	appendnumbertofilename("/path/dir", "prefix", ".txt", 1, file);
	TestEqs("/path/dir" pathsep "prefix00001.txt", cstr(file));
	appendnumbertofilename("/path/dir", "prefix", ".txt", 123456, file);
	TestEqs("/path/dir" pathsep "prefix123456.txt", cstr(file));

	/* test reading number from filename */
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file01.txt2"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/2path/file01.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "2/path/file01.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/fileA01.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/fileA01A.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file01A.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file01txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file12A34.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file01\xCC.txt"));
	TestEqn(0, readnumberfromfilename(file, "/path/file", ".txt", "/path/file00.txt"));
	TestEqn(1, readnumberfromfilename(file, "/path/file", ".txt", "/path/file1.txt"));
	TestEqn(1, readnumberfromfilename(file, "/path/file", ".txt", "/path/file00001.txt"));
	TestEqn(12345, readnumberfromfilename(file, "/path/file", ".txt", "/path/file12345.txt"));
	TestEqn(123456, readnumberfromfilename(file, "/path/file", ".txt", "/path/file123456.txt"));

	/* no matching files, latest number is 0 */
	uint32_t latestnumber = UINT32_MAX;
	check(readlatestnumberfromfilename(testdirfileio, "testlog", ".txt", &latestnumber));
	TestEqn(0, latestnumber);

	/* third file present, should return 3 */
	check(tmpwritetextfile(testdirfileio, "testlog00003.txt", file, ""));
	check(readlatestnumberfromfilename(testdirfileio, "testlog", ".txt", &latestnumber));
	TestEqn(3, latestnumber);

	/* first, third file present, should return 3 */
	check(tmpwritetextfile(testdirfileio, "testlog00001.txt", file, ""));
	check(readlatestnumberfromfilename(testdirfileio, "testlog", ".txt", &latestnumber));
	TestEqn(3, latestnumber);

	/* first, second, third file present, should return 3 */
	check(tmpwritetextfile(testdirfileio, "testlog02.txt", file, ""));
	check(readlatestnumberfromfilename(testdirfileio, "testlog", ".txt", &latestnumber));
	TestEqn(3, latestnumber);

	/* high number file present */
	check(tmpwritetextfile(testdirfileio, "testlog000123456.txt", file, ""));
	check(readlatestnumberfromfilename(testdirfileio, "testlog", ".txt", &latestnumber));
	TestEqn(123456, latestnumber);

	/* first file present, should return 1. */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(tmpwritetextfile(testdirfileio, "testlog01.txt", file, ""));
	check(readlatestnumberfromfilename(testdirfileio, "testlog", ".txt", &latestnumber));
	TestEqn(1, latestnumber);

	/* skips logging if current logger is null */
	sv_log_register_active_logger(NULL);
	sv_log_writeline("should be skipped");

	/* write some log entries */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_log_open(&testlogger, testdirfileio));
	sv_log_register_active_logger(&testlogger);
	sv_log_writeline("");
	sv_log_writeline("abcd");
	sv_log_writefmt("s %s int %d", "string", 123);
	sv_log_close(&testlogger);
	check(sv_file_readfile(cstr(logpathfirst), contents));
	const char *pattern = nativenewline "????" "/" "??" "/" "??"
		nativenewline "??:??:??:??? "
		nativenewline "??:??:??:??? abcd"
		nativenewline "??:??:??:??? s string int 123";
	TestTrue(fnmatch_simple(pattern, cstr(contents)));

	/* test switch file */
	check(sv_log_open(&testlogger, testdirfileio));
	TestTrue(!os_file_exists(cstr(logpathsecond)));
	testlogger.cap_filesize = 4;
	testlogger.counter = 15; /* check filesize whenever counter%16==0 */
	sv_log_writeline("abcd");
	sv_log_close(&testlogger);
	TestTrue(os_file_exists(cstr(logpathfirst)));
	TestTrue(os_file_exists(cstr(logpathsecond)));

	/* format timestamps */
	{
		check(tmpwritetextfile(testdirfileio, "testtimeformatting.txt", file, ""));
		sv_file f = {};
		check(sv_file_open(&f, cstr(file), "wb"));
		int64_t day_start_sep_27 = 1474934400ULL;
		int64_t morning_sep_27 = 1474949106ULL;
		int64_t evening_sep_27 = 1475007777ULL;
		sv_log_addnewlinetime(f.file, day_start_sep_27, morning_sep_27, 1);
		sv_log_addnewlinetime(f.file, day_start_sep_27, evening_sep_27, 10);
		sv_log_addnewlinetime(f.file, 0, 123, 100);
		sv_file_close(&f);
		check(sv_file_readfile(cstr(file), contents));
		TestEqs(nativenewline "04:05:06:001 " nativenewline "20:22:57:010 "
			nativenewline "00:02:03:100 ", cstr(contents));
	}

cleanup:
	bdestroy(logpathfirst);
	bdestroy(logpathsecond);
	bdestroy(file);
	bdestroy(contents);
	sv_log_close(&testlogger);
	sv_log_register_active_logger(NULL);
	return currenterr;
}

check_result test_os_fileops(const char *testdirfileio)
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
	TestTrue(!os_dir_exists(""));
	TestTrue(!os_dir_exists("||invalid"));
	TestTrue(!os_dir_exists("//invalid"));

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
	set_debugbreaks_enabled(false);
	TestTrue(!os_copy(cstr(filename1), cstr(filename2), true));
	TestTrue(!os_copy("||invalid", cstr(filename2), true));
	TestTrue(!os_copy("_not_exist_" pathsep "a", cstr(filename2), true));
	set_debugbreaks_enabled(true);

	/* os_copy from Exists to Invalid */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	set_debugbreaks_enabled(false);
	TestTrue(!os_copy(cstr(filename1), "||invalid", true));
	TestTrue(!os_copy(cstr(filename1), "_not_exist_" pathsep "a", true));
	set_debugbreaks_enabled(true);

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
	set_debugbreaks_enabled(false);
	TestTrue(!os_copy(cstr(filename1), cstr(filename2), false));
	set_debugbreaks_enabled(true);
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
	set_debugbreaks_enabled(false);
	TestTrue(!os_move(cstr(filename1), cstr(filename2), true));
	TestTrue(!os_move("||invalid", cstr(filename2), true));
	TestTrue(!os_move("_not_exist_" pathsep "a", cstr(filename2), true));
	set_debugbreaks_enabled(true);

	/* os_move from Exists to Invalid */
	check(os_tryuntil_deletefiles(testdirfileio, "*"));
	check(sv_file_writefile(cstr(filename1), "abcd", "w"));
	set_debugbreaks_enabled(false);
	TestTrue(!os_move(cstr(filename1), "||invalid", true));
	TestTrue(!os_move(cstr(filename1), "_not_exist_" pathsep "a", true));
	set_debugbreaks_enabled(true);
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
	set_debugbreaks_enabled(false);
	TestTrue(!os_move(cstr(filename1), cstr(filename2), false));
	set_debugbreaks_enabled(true);
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
	bstr_fill(contents, 'a', 20000); /* 20 thousand of the character a */
	check(sv_file_writefile(cstr(filename1), cstr(contents), "w"));
	TestEqn(20000, strlen(cstr(contents)));
	TestEqn(20000, os_getfilesize(cstr(filename1)));

	/* os_getfilesize 0 length or missing */
	check(sv_file_writefile(cstr(filename1), "", "w"));
	TestEqn(0, os_getfilesize(cstr(filename1)));
	TestEqn(0, os_getfilesize("d" pathsep "not_exist"));

	/* os_setmodifiedtime and os_getmodifiedtime */
	TestTrue(os_setmodifiedtime_nearestsecond(cstr(filename1), 0x0111111155555555LL));
	long long timegot = (long long)os_getmodifiedtime(cstr(filename1));
	TestTrue(llabs(timegot - 0x0111111155555555LL) < 10);
	TestTrue(os_setmodifiedtime_nearestsecond(cstr(filename1), 0x0222222255555555LL));
	timegot = (long long)os_getmodifiedtime(cstr(filename1));
	TestTrue(llabs(timegot - 0x0222222255555555LL) < 10);

cleanup:
	bdestroy(contents);
	bdestroy(filename1);
	bdestroy(filename2);
	return currenterr;
}

static check_result test_util_recurse_callback(void *context,
	const bstring filepath, uint64_t modtime, uint64_t filesize, unused(const bstring))
{
	bstrlist *list = (bstrlist *)context;
	bstrlist_appendcstr(list, cstr(filepath));
	TestTrue(modtime != 0);
	TestTrue(filesize != 0);
	if (!os_recurse_is_dir(modtime, filesize))
	{
		/* verify filesize */
		/* when we created these files, we gave file1 a size of 1, file2 a size of 2, and so on */
		check_fatal(s_endswith(cstr(filepath), ".txt"), "expect to end with .txt");
		char last_character = (cstr(filepath))[blength(filepath) - 5];
		TestEqn(last_character - '0', filesize);
	}

	return OK;
}

check_result test_os_dirlist(const char *testdirrecurse)
{
	/* give each filename and dirname utf-8 characters */
	sv_result currenterr = {};
	bstring dirname1 = bformat("%s" pathsep "\xED\x95\x9C d1", testdirrecurse);
	bstring dirname2 = bformat("%s" pathsep "\xED\x95\x9C d2", testdirrecurse);
	bstring dirname2sub = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "sub",
		testdirrecurse);
	bstring filename1 = bformat("%s" pathsep "\xED\x95\x9C d1" pathsep "\xED\x95\x9C f1.txt",
		testdirrecurse);
	bstring filename2 = bformat("%s" pathsep "\xED\x95\x9C d1" pathsep "\xED\x95\x9C f2.txt",
		testdirrecurse);
	bstring filename3 = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "\xED\x95\x9C f3.txt",
		testdirrecurse);
	bstring filename4 = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "\xED\x95\x9C f4.txt",
		testdirrecurse);
	bstring filename5 = bformat("%s" pathsep "\xED\x95\x9C d2" pathsep "sub" pathsep 
		"\xED\x95\x9C f5.txt", testdirrecurse);
	bstrlist *list = bstrlist_open();

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

		/* os_file_exists for relative paths */
		TestTrue(os_setcwd(cstr(dirname1)));
		TestTrue(os_file_exists("\xED\x95\x9C f1.txt"));
		TestTrue(!os_file_exists("\xED\x95\x9C f3.txt"));
		TestTrue(os_setcwd(cstr(dirname2)));
		TestTrue(!os_file_exists("\xED\x95\x9C f1.txt"));
		TestTrue(os_file_exists("\xED\x95\x9C f3.txt"));

		/* os_listfiles (non-recursive) */
		check(os_listfiles(cstr(dirname2), list, true));
		TestEqn(2, list->qty);
		TestEqs(cstr(filename3), bstrlist_view(list, 0));
		TestEqs(cstr(filename4), bstrlist_view(list, 1));

		/* os_listdirs (non-recursive) */
		check(os_listdirs(testdirrecurse, list, true));
		TestEqn(2, list->qty);
		TestEqs(cstr(dirname1), bstrlist_view(list, 0));
		TestEqs(cstr(dirname2), bstrlist_view(list, 1));

		/* os_recurse, not limited by depth */
		bstrlist_clear(list);
		os_recurse_params params = { list, testdirrecurse, &test_util_recurse_callback, INT_MAX };
		check(os_recurse(&params));
		bstrlist_sort(list);
		TestEqn(8, list->qty);
		const char *expected[] = { cstr(dirname1), cstr(filename1), cstr(filename2),
			cstr(dirname2), cstr(dirname2sub), cstr(filename5), cstr(filename3), cstr(filename4) };
		for (int i = 0; i < countof32s(expected); i++)
			TestEqs(expected[i], bstrlist_view(list, i));

		/* os_recurse, limited depth should throw an error */
		set_debugbreaks_enabled(false);
		params.max_recursion_depth = 1;
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
	bstrlist_close(list);
	return currenterr;
}

check_result test_os_runprocess(const char *dir)
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
	check(sv_file_writefile(cstr(script_noexitcode), 
		"echo 'hi'\necho 'oh' 1>&2\necho 'yay'", "wb"));
	check(sv_file_writefile(cstr(script_exitcode), 
		"echo 'hi'\necho 'oh' 1>&2\nexit 22", "wb"));
	check(sv_file_writefile(cstr(script_printarg), 
		"echo 'arg'\necho $1", "wb"));
	check(sv_file_writefile(cstr(script_printstdin), 
		"read var\necho $var > gotfromstdin.txt\n", "wb")); /* nb: reads only the first line */
#define MAKE_ARGS_LIST(...) { sh, __VA_ARGS__ }
#else
	check(sv_file_writefile(cstr(script_noexitcode), 
		"@echo off\r\necho hi\r\necho oh 1>&2\r\necho yay", "wb"));
	check(sv_file_writefile(cstr(script_exitcode), 
		"@echo off\r\necho hi\r\necho oh 1>&2\r\nEXIT /b 22", "wb"));
	check(sv_file_writefile(cstr(script_printarg), 
		"@echo off\r\necho arg\r\necho %1", "wb"));
	check(sv_file_writefile(cstr(script_printstdin), 
		"@echo off\r\n"
		"set /p var=Enter:\r\n"
		"echo val is \"%var%\" > gotfromstdin.txt", "wb"));  /* nb: reads the first line */
#define MAKE_ARGS_LIST(...) { "/c", __VA_ARGS__ }
#endif
	const char *sh = islinux ? "/bin/sh" : getenv("comspec");

	{ /* read 0 exit code, read output */
		const char *args[] = MAKE_ARGS_LIST(cstr(script_noexitcode), NULL);
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "hi\noh\nyay\n" : "hi\r\noh \r\nyay\r\n", cstr(getoutput));
		TestTrue(retcode == 0);
	}
	{ /* read non-0 exit code, read output */
		const char *args[] = MAKE_ARGS_LIST(cstr(script_exitcode), NULL);
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "hi\noh\n" : "hi\r\noh \r\n", cstr(getoutput));
		TestTrue(retcode == 22);
	}
	{ /* read given parameter */
		const char *args[] = MAKE_ARGS_LIST(cstr(script_printarg), "paramgiven", NULL);
		int retcode = os_tryuntil_run_process(sh, args, true, useforargs, getoutput);
		TestEqs(islinux ? "arg\nparamgiven\n" : "arg\r\n\"paramgiven\r\n", cstr(getoutput));
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
		const char *args[] = MAKE_ARGS_LIST(cstr(script_printstdin), NULL);
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
#undef MAKE_ARGS_LIST
	
	{ /* building normal arguments */
		const char *args[] = { "arg1", "arg2", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, true));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2\"", cstr(cmd));
	}
	{ /* building arguments with spaces and interesting characters */
		const char *args[] = { "  arg 1  ", "\\a\\/|? '`~!@#$%^&*()", NULL };
		TestTrue(argvquote("C:\\path\\the binary", args, cmd, true));
		TestEqs("\"C:\\path\\the binary\" \"  arg 1  \" \"\\a\\/|? '`~!@#$%^&*()\"", cstr(cmd));
	}
	{ /* reject args containing double quotes because on windows 
	  this requires 'thorough argument join' */
		const char *args[] = { "arg1", "arg2with\"quote", NULL };
		TestTrue(!argvquote("C:\\path\\binary", args, cmd, true));
	}
	{ /* reject args containing trailing backslash because on windows 
	  this requires 'thorough argument join' */
		const char *args[] = { "arg1", "arg2with\\", NULL };
		TestTrue(!argvquote("C:\\path\\binary", args, cmd, true));
	}
	{ /* thorough argument join, building normal arguments */
		const char *args[] = { "arg1", "arg2", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2\"", cstr(cmd));
	}
	{ /* thorough argument join, building arguments with spaces and interesting characters */
		const char *args[] = { "  arg 1  ", "\\a\\/|? '`~!@#$%^&*()", NULL };
		TestTrue(argvquote("C:\\path\\the binary", args, cmd, false));
		TestEqs("\"C:\\path\\the binary\" \"  arg 1  \" \"\\a\\/|? '`~!@#$%^&*()\"", cstr(cmd));
	}
	{ /* thorough argument join, containing double quotes */
		const char *args[] = { "arg1", "arg2with\"quote", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2with\\\"quote\"", cstr(cmd));
	}
	{ /* thorough argument join, containing trailing backslash */
		const char *args[] = { "arg1", "arg2with\\", NULL }; //1
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2with\\\\\"", cstr(cmd));
	}
	{ /* thorough argument join, containing just two double quotes */
		const char *args[] = { "arg1", "\"\"", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"\\\"\\\"\"", cstr(cmd));
	}
	{ /* thorough argument join, containing just three double quotes */
		const char *args[] = { "arg1", "\"\"\"", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"\\\"\\\"\\\"\"", cstr(cmd));
	}
	{ /* thorough argument join, containing backslashes before double quotes */
		const char *args[] = { "arg1", "arg2with\\\\\"quote", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2with\\\\\\\\\\\"quote\"", cstr(cmd));
	}
	{ /* thorough argument join, containing trailing backslashes */
		const char *args[] = { "arg1", "arg2with\\\\\\", NULL };
		TestTrue(argvquote("C:\\path\\binary", args, cmd, false));
		TestEqs("\"C:\\path\\binary\" \"arg1\" \"arg2with\\\\\\\\\\\\\"", cstr(cmd));
	}

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

check_result test_os_movebypattern(const char *dir)
{
	sv_result currenterr = {};
	bstring subdir1 = bformat("%s%sd1", dir, pathsep);
	bstring subdir2 = bformat("%s%sd2", dir, pathsep);
	bstring path = bstring_open();
	bstrlist *listfiles = bstrlist_open();
	TestTrue(os_create_dirs(cstr(subdir1)));
	TestTrue(os_create_dirs(cstr(subdir2)));
	check(os_tryuntil_deletefiles(cstr(subdir1), "*"));
	check(os_tryuntil_deletefiles(cstr(subdir2), "*"));
	check(tmpwritetextfile(cstr(subdir1), "a.txt", path, "contents"));
	check(tmpwritetextfile(cstr(subdir1), "b.txt", path, "contents"));
	check(tmpwritetextfile(cstr(subdir1), "ctxt", path, "contents"));
	check(tmpwritetextfile(cstr(subdir1), "d.jpg", path, "contents"));

	/* pattern that matches no files */
	int moved = 0;
	check(os_tryuntil_movebypattern(cstr(subdir1), "txt", cstr(subdir2), false, &moved));
	TestEqn(0, moved);
	TestTrue(os_dir_empty(cstr(subdir2)));

	/* pattern that matches some files */
	check(os_tryuntil_movebypattern(cstr(subdir1), "*.txt", cstr(subdir2), false, &moved));
	TestEqn(2, moved);
	check(os_listfiles(cstr(subdir1), listfiles, true));
	TestEqn(2, listfiles->qty);
	TestTrue(s_endswith(bstrlist_view(listfiles, 0), pathsep "ctxt"));
	TestTrue(s_endswith(bstrlist_view(listfiles, 1), pathsep "d.jpg"));
	check(os_listfiles(cstr(subdir2), listfiles, true));
	TestEqn(2, listfiles->qty);
	TestTrue(s_endswith(bstrlist_view(listfiles, 0), pathsep "a.txt"));
	TestTrue(s_endswith(bstrlist_view(listfiles, 1), pathsep "b.txt"));

	/* pattern that matches all files */
	check(os_tryuntil_movebypattern(cstr(subdir1), "*", cstr(subdir2), false, &moved));
	TestEqn(2, moved);
	TestTrue(os_dir_empty(cstr(subdir1)));
	check(os_listfiles(cstr(subdir2), listfiles, true));
	TestEqn(4, listfiles->qty);

	/* delete, pattern that matches some files */
	check(os_tryuntil_deletefiles(cstr(subdir2), "*.txt"));
	check(os_listfiles(cstr(subdir2), listfiles, true));
	TestEqn(2, listfiles->qty);

	/* delete, pattern that matches all files */
	check(os_tryuntil_deletefiles(cstr(subdir2), "*"));
	TestTrue(os_dir_empty(cstr(subdir2)));

cleanup:
	bdestroy(subdir1);
	bdestroy(subdir2);
	bdestroy(path);
	bstrlist_close(listfiles);
	return currenterr;
}

check_result test_os_createdirs(const char *dir)
{
	bstring subdir1 = bformat("%s%ssub1", dir, pathsep);
	bstring subdir2 = bformat("%s%ssub1%ssub2", dir, pathsep, pathsep);
	bstring subdir3 = bformat("%s%ssub1%ssub2%ssub3", dir, pathsep, pathsep, pathsep);
	TestTrue(os_remove(cstr(subdir3)));
	TestTrue(os_remove(cstr(subdir2)));
	TestTrue(os_remove(cstr(subdir1)));
	
	/* create directory tree */
	TestTrue(!os_dir_exists(cstr(subdir3)));
	TestTrue(os_create_dirs(cstr(subdir3)));
	TestTrue(os_dir_exists(cstr(subdir3)));

	/* create just one directory */
	TestTrue(os_remove(cstr(subdir3)));
	TestTrue(os_create_dirs(cstr(subdir3)));
	TestTrue(os_dir_exists(cstr(subdir3)));

	/* ok if dir already exists */
	TestTrue(os_create_dirs(cstr(subdir3)));
	bdestroy(subdir1);
	bdestroy(subdir2);
	bdestroy(subdir3);
	return OK;
}

check_result test_os_filelocks(const char *dir)
{
	sv_result currenterr = {};
	sv_file filewrapper = {};
	os_exclusivefilehandle handle1 = {};
	os_exclusivefilehandle handle2 = {};
	bstring path = bformat("%s%sfile.txt", dir, pathsep);
	bstring pathnofile = bformat("%s%sfile-notexist.txt", dir, pathsep);
	bstring pathnodir = bformat("%s%snot-exist%sfile.txt", dir, pathsep, pathsep);
	bstring contents = bstring_open();
	bstring permissions = bstring_open();

	/* exclusivefilehandle gets stats and releases lock after closed */
	check(sv_file_writefile(cstr(path), "contents", "wb"));
	uint64_t lmt = os_getmodifiedtime(cstr(path));
	uint64_t stat_lmt = 0, stat_filesize = 0;
	check(os_exclusivefilehandle_open(&handle1, cstr(path), true, NULL));
	check(os_exclusivefilehandle_stat(&handle1, &stat_filesize, &stat_lmt, permissions));
	TestTrue(handle1.fd > 0);
	TestEqn(8, stat_filesize);
	TestEqn(lmt, stat_lmt);
	os_exclusivefilehandle_close(&handle1);
	check(sv_file_writefile(cstr(path), "contents-changed", "wb"));
	TestEqn(16, os_getfilesize(cstr(path)));

#if __linux__
	/* most of the tests here are checks for insufficient permissions, but when
	we have root access we have permissions for just about everything. */
	if (geteuid() == 0)
	{
		goto cleanup;
	}
	
	/* insufficient permissions to get an exclusivefilehandle */
	TestTrue(chmod(cstr(path), 0111) == 0);
	set_debugbreaks_enabled(false);
	expect_err_with_message(os_exclusivefilehandle_open(&handle1, cstr(path), true, NULL), 
		"Permission denied");

	os_exclusivefilehandle_close(&handle1);
	TestTrue(chmod(cstr(path), 0777) == 0);
	
	/* permissions string should have this format */
	TestTrue(fnmatch_simple("p*|g*|u*", cstr(permissions)));
	bstrlist *splitbypipecharacter = bsplit(permissions, '|');
	TestEqn(3, splitbypipecharacter->qty);
	bstrlist_close(splitbypipecharacter);
#else
	
	/* exclusivefilehandle locks out a FILE read and write */
	check(os_exclusivefilehandle_open(&handle1, cstr(path), false, NULL));
	set_debugbreaks_enabled(false);
	expect_err_with_message(sv_file_open(&filewrapper, cstr(path), "w"), "failed to open ");
	sv_file_close(&filewrapper);
	expect_err_with_message(sv_file_open(&filewrapper, cstr(path), "r"), "failed to open ");
	sv_file_close(&filewrapper);
	set_debugbreaks_enabled(true);
	os_exclusivefilehandle_close(&handle1);

	/* exclusivefilehandle locks out a FILE write, reading is ok */
	check(os_exclusivefilehandle_open(&handle1, cstr(path), true, NULL));
	set_debugbreaks_enabled(false);
	expect_err_with_message(sv_file_open(&filewrapper, cstr(path), "w"), "failed to open ");
	sv_file_close(&filewrapper); 
	set_debugbreaks_enabled(true);
	check(sv_file_readfile(cstr(path), contents));
	TestEqs("contents-changed", cstr(contents));
	os_exclusivefilehandle_close(&handle1);

	/* FILE can lock out a exclusivefilehandle */
	check(sv_file_open(&filewrapper, cstr(path), "ab"));
	set_debugbreaks_enabled(false);
	expect_err_with_message(os_exclusivefilehandle_open(&handle1, cstr(path), true, NULL), "Bad file");
	os_exclusivefilehandle_close(&handle1);
	set_debugbreaks_enabled(true);
	sv_file_close(&filewrapper);
#endif

	/* exclusivefilehandle (doesn't) lock out another exclusivefilehandle 
	we shouldn't use this pattern in the product; 
	posix locks are released after the first close which is counterintuitive */
	check(os_exclusivefilehandle_open(&handle1, cstr(path), true, NULL));
	check(os_exclusivefilehandle_open(&handle2, cstr(path), true, NULL));
	os_exclusivefilehandle_close(&handle2);
	os_exclusivefilehandle_close(&handle1);

	/* os_exclusivefilehandle_tryuntil_open when path is found */
	extern uint32_t sleep_between_tries;
	sleep_between_tries = 999999;
	bool notfound = true;
	check(os_exclusivefilehandle_tryuntil_open(&handle1, cstr(path), true, &notfound));
	TestTrue(!notfound && handle1.fd > 0);
	os_exclusivefilehandle_close(&handle1);

	/* os_exclusivefilehandle_tryuntil_open must return quickly if file-not-found  */
	notfound = false;
	check(os_exclusivefilehandle_tryuntil_open(&handle1, cstr(pathnofile), true, &notfound));
	TestTrue(notfound && handle1.fd <= 0);
	os_exclusivefilehandle_close(&handle1);

	/* os_exclusivefilehandle_tryuntil_open must return quickly if directory-not-found  */
	notfound = false;
	check(os_exclusivefilehandle_tryuntil_open(&handle1, cstr(pathnodir), true, &notfound));
	TestTrue(notfound && handle1.fd <= 0);
	os_exclusivefilehandle_close(&handle1);

	/* os_is_dir_writable when it is writable */
	TestTrue(os_is_dir_writable(dir));

	/* mimic a non-writable directory by locking the same file it happens to use */
	bassignformat(path, "%s%stestwrite.tmp", dir, pathsep);
	check(sv_file_writefile(cstr(path), "contents", "wb"));
	if (islinux)
	{
		TestTrue(chmod(cstr(path), 0555) == 0);
	}
	else
	{
		check(os_exclusivefilehandle_open(&handle1, cstr(path), false, NULL));
	}

	set_debugbreaks_enabled(false);
	TestTrue(!os_is_dir_writable(dir));
	set_debugbreaks_enabled(true);
	if (islinux)
	{
		TestTrue(chmod(cstr(path), 0777) == 0);
	}
	else
	{
		os_exclusivefilehandle_close(&handle1);
	}

cleanup:
	if (islinux)
	{
		TestTrue(chmod(cstr(path), 0777) == 0);
	}
	sleep_between_tries = 1;
	sv_file_close(&filewrapper);
	os_exclusivefilehandle_close(&handle1);
	os_exclusivefilehandle_close(&handle2);
	bdestroy(path);
	bdestroy(pathnofile);
	bdestroy(pathnodir);
	bdestroy(contents);
	bdestroy(permissions);
	return currenterr;
}

void createsearchspec(const sv_wstr *dir, sv_wstr *buffer);
check_result os_listfiles_callback(void *context,
	const bstring filepath, uint64_t modtime, uint64_t filesize, const bstring);

check_result test_os_dir_iter_cornercases(unused_ptr(const char))
{
	sv_result currenterr = {};
	bstring s = bstring_open();
	sv_wstr wtmp = sv_wstr_open(1);
	bstrlist *list = bstrlist_open();
	bstrlist *list_messages = bstrlist_open();

	{ /* recurse on empty path */
		os_recurse_params params = { list, "",
			&os_listfiles_callback, 0 /* max depth */, list_messages };
		set_debugbreaks_enabled(false);
		expect_err_with_message(os_recurse(&params), "expected full path");
		set_debugbreaks_enabled(true);
		TestEqn(0, list->qty);
		TestEqn(0, list_messages->qty);
	}
	{ /* recurse on nonexistant path */
		os_recurse_params params = { list, 
			islinux ? "/path--does--not--exist" : "C:\\path--does--not--exist",
			&os_listfiles_callback, 0 /* max depth */, list_messages };
		set_debugbreaks_enabled(false);
		check(os_recurse(&params));
		set_debugbreaks_enabled(true);
		TestEqn(0, list->qty);
		TestEqn(0, list_messages->qty);
	}
#ifndef __linux__
	{ /* test with invalid path */
		os_recurse_params params = { list, "C:\\|||invalid|||",
			&os_listfiles_callback, 0 /* max depth */, list_messages };
		set_debugbreaks_enabled(false);
		check(os_recurse(&params));
		set_debugbreaks_enabled(true);
		TestEqn(0, list->qty);
		TestEqn(1, list_messages->qty);
		TestTrue(s_contains(bstrlist_view(list_messages, 0), "The filename, directory name, or"));
	}
	{ /* make search spec for normal path */
		sv_wstr input = sv_wstr_open(1);
		sv_wstr_append(&input, L"C:\\a\\normal\\path");
		createsearchspec(&input, &wtmp);
		TestTrue(ws_equal(L"C:\\a\\normal\\path\\*", wcstr(wtmp)));
		sv_wstr_close(&input);
	}
	{ /* make search spec for path with trailing backslash */
		sv_wstr input = sv_wstr_open(1);
		sv_wstr_append(&input, L"C:\\a\\path\\trailing\\");
		createsearchspec(&input, &wtmp);
		TestTrue(ws_equal(L"C:\\a\\path\\trailing\\*", wcstr(wtmp)));
		sv_wstr_close(&input);
	}
	{ /* make search spec for root path */
		sv_wstr input = sv_wstr_open(1);
		sv_wstr_append(&input, L"D:\\");
		createsearchspec(&input, &wtmp);
		TestTrue(ws_equal(L"D:\\*", wcstr(wtmp)));
		sv_wstr_close(&input);
	}
	{ /* win32 error to string when there is no error */
		char buf[BUFSIZ] = "";
		os_win32err_to_buffer(0, buf, countof(buf));
		TestEqs("The operation completed successfully.\r\n", buf);
	}
	{ /* win32 error to string for access denied */
		char buf[BUFSIZ] = "";
		os_win32err_to_buffer(5, buf, countof(buf));
		TestEqs("Access is denied.\r\n", buf);
	}
#endif
cleanup:
	sv_wstr_close(&wtmp);
	bdestroy(s);
	return currenterr;
}

check_result tests_requiring_interaction(void)
{
	bstrlist *list = bstrlist_open();
	while (ask_user_yesno("Run pick-number-from-long list test?"))
	{
		bstrlist_clear(list);
		bstring s = bstring_open();
		for (int i = 0; i < 30; i++)
		{
			bassignformat(s, "a%da", i + 1);
			bstrlist_appendcstr(list, cstr(s));
		}

		int chosen = ui_numbered_menu_pick_from_long_list(list, 4);
		if (chosen >= 0)
			printf("you picked %d (%s)\n", chosen + 1, bstrlist_view(list, chosen));
		else
			printf("you decided to cancel\n");

		alertdialog("");
		bdestroy(s);
	}

	alertdialog("For this test we'll intentionally trigger integer overflow and verify that "
		"it causes program exit(). Please attach a debugger and place a breakpoint in the "
		"function void die(void) and confirm that the breakpoint is hit "
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
	
	bstrlist_close(list);
	return OK;
}


check_result run_utils_tests(void)
{
	sv_result currenterr = {};
	bstring dir = os_get_tmpdir("tmpglacial_backup");
	
	bstring testdir = os_make_subdir(cstr(dir), "tests");
	bstring testdirfileio = os_make_subdir(cstr(testdir), "fileio");
	bstring testdirrecurse = os_make_subdir(cstr(testdir), "recurse");
	bstring testdirwithfiles = os_make_subdir(cstr(testdir), "dirwithfiles");
	bstring testdirdb = os_make_subdir(cstr(testdir), "testdb");

	test_util_basicarithmatic();
	test_util_array();
	test_util_2d_array();
	test_util_string();
	test_util_c_string();
	test_util_stringlist();
	test_util_pseudosplit();
	test_util_fnmatch();
	test_os_getbinarypath();
	test_os_split(cstr(testdirfileio));
	check(test_util_widestr());
	check(test_util_readwritefiles(cstr(testdirfileio)));
	check(test_util_logging(cstr(testdirfileio)));
	check(test_os_fileops(cstr(testdirfileio)));
	check(test_os_dirlist(cstr(testdirrecurse)));
	check(test_os_dir_iter_cornercases(cstr(testdirrecurse)));
	check(test_os_runprocess(cstr(testdirfileio)));
	check(test_os_movebypattern(cstr(testdirwithfiles)));
	check(test_os_createdirs(cstr(testdirwithfiles)));
	check(test_os_filelocks(cstr(testdirwithfiles)));
#if 0
	check(tests_requiring_interaction());
#endif

cleanup:
	os_setcwd(cstr(dir));
	bdestroy(dir);
	bdestroy(testdir);
	bdestroy(testdirfileio);
	bdestroy(testdirrecurse);
	bdestroy(testdirwithfiles);
	bdestroy(testdirdb);
	return currenterr;
}

