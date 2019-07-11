/*
tests_util.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "tests.h"

SV_BEGIN_TEST_SUITE(tests_c_string)
{
    SV_TEST_()
    {
        TestTrue(s_equal("abc", "abc"));
    }

    SV_TEST_()
    {
        TestTrue(s_equal("D", "D"));
    }

    SV_TEST_()
    {
        TestTrue(!s_equal("abc", "abC"));
    }

    SV_TEST_()
    {
        TestTrue(s_startwith("abc", ""));
    }

    SV_TEST_()
    {
        TestTrue(s_startwith("abc", "a"));
    }

    SV_TEST_()
    {
        TestTrue(s_startwith("abc", "ab"));
    }

    SV_TEST_()
    {
        TestTrue(s_startwith("abc", "abc"));
    }

    SV_TEST_()
    {
        TestTrue(!s_startwith("abc", "abcd"));
    }

    SV_TEST_()
    {
        TestTrue(!s_startwith("abc", "aB"));
    }

    SV_TEST_()
    {
        TestTrue(!s_startwith("abc", "def"));
    }

    SV_TEST_()
    {
        TestTrue(s_endwith("abc", ""));
    }

    SV_TEST_()
    {
        TestTrue(s_endwith("abc", "c"));
    }

    SV_TEST_()
    {
        TestTrue(s_endwith("abc", "bc"));
    }

    SV_TEST_()
    {
        TestTrue(s_endwith("abc", "abc"));
    }

    SV_TEST_()
    {
        TestTrue(!s_endwith("abc", "aabc"));
    }

    SV_TEST_()
    {
        TestTrue(!s_endwith("abc", "abC"));
    }

    SV_TEST_()
    {
        TestTrue(!s_endwith("abc", "aB"));
    }

    SV_TEST_()
    {
        TestTrue(!s_endwith("abc", "def"));
    }

    SV_TEST_()
    {
        TestTrue(s_contains("abcde", ""));
    }

    SV_TEST_()
    {
        TestTrue(s_contains("abcde", "abc"));
    }

    SV_TEST_()
    {
        TestTrue(s_contains("abcde", "cde"));
    }

    SV_TEST_()
    {
        TestTrue(s_contains("abcde", "abcde"));
    }

    SV_TEST_()
    {
        TestTrue(!s_contains("abcde", "abcdef"));
    }

    SV_TEST_()
    {
        TestTrue(!s_contains("abcde", "fabcde"));
    }

    SV_TEST_()
    {
        TestTrue(!s_contains("abcde", "abCde"));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_fnmatch)
{
    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*", ""));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*", "abc"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*", "a/b/c"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*", "/a/"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*.*", "a.a"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*.*", "a."));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("*.*", "a"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("*.*", ""));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("a?c", "a0c"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("a?c", "a00c"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("a\\b\\c", "a\\b\\c"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("a\\*", "a\\abcde"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("a\\c", "a\\c"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("a\\*c", "a\\000c"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("a\\???d", "a\\abcd"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("a\\\\c", "a\\c"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("prefix*", "pref"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("prefix*", "prefix"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("prefix*", "Aprefix"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("prefix*", "AprefixB"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("prefix*", "prefixB"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("prefix*", "prefix/file.txt"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("prefix*", "prefixdir/file.txt"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("*contains*", "contain"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*contains*", "contains"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*contains*", "Acontains"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*contains*", "AcontainsB"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*contains*", "containsB"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*contains*", "d/contains/file.txt"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*contains*", "d/dircontainsdir/file.txt"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("*suffix", "suff"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*suffix", "suffix"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*suffix", "Asuffix"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("*suffix", "AsuffixB"));
    }

    SV_TEST_()
    {
        TestTrue(!fnmatch_simple("*suffix", "suffixB"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*suffix", "a/suffix"));
    }

    SV_TEST_()
    {
        TestTrue(fnmatch_simple("*suffix", "a/dsuffix"));
    }

    SV_TEST_()
    {
        TestTrue(
            fnmatch_simple("aa*contains*bb", "aa things contains things bb"));
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, msg);
        fnmatch_isvalid("*.*", msg);
        TestTrue(blength(msg) == 0);
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, msg);
        fnmatch_isvalid("aba", msg);
        TestTrue(blength(msg) == 0);
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, msg);
        fnmatch_isvalid("ab[a", msg);
        TestTrue(blength(msg) != 0);
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, msg);
        fnmatch_isvalid("ab[ab]", msg);
        TestTrue(blength(msg) != 0);
    }

    SV_TEST_()
    {
        TEST_OPEN(bstring, msg);
        fnmatch_isvalid("ab\x81", msg);
        TestTrue(blength(msg) != 0);
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_string_bstr)
{
    SV_TEST("init as empty string")
    {
        TEST_OPEN(bstring, s);
        TestEqs("", cstr(s));
    }

    SV_TEST("init as simple string")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abcde"));
        TestEqs("abcde", cstr(s));
    }

    SV_TEST("assign string")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bassigncstr(s, "def");
        TestEqs("def", cstr(s));
    }

    SV_TEST("clear string")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bstrclear(s);
        TestEqs("", cstr(s));
    }

    SV_TEST("append formatting")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bsetfmt(s, "%d, %s, %S, and hex %x", 12, "test", L"wide", 0xff);
        TestEqs("12, test, wide, and hex ff", cstr(s));
    }

    SV_TEST("init with formatting")
    {
        TEST_OPEN_EX(bstring, s,
            bformat("%llu, %llu, %.3f", 0ULL, 12345678987654321ULL, 0.123456));
        TestEqs("0, 12345678987654321, 0.123", cstr(s));
    }

    SV_TEST("replace with formatting")
    {
        TEST_OPEN_EX(bstring, s, bformat("%ld", 1));
        bsetfmt(s, "%s", "");
        TestEqs("", cstr(s));
    }

    SV_TEST("digits added if necessary")
    {
        TEST_OPEN_EX(bstring, s,
            bformat("%08llx %08llx %08llx", 0x123ULL, 0x12341234ULL,
                0x123412349999ULL));
        TestEqs("00000123 12341234 123412349999", cstr(s));
    }

    SV_TEST("concat empty strings")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bcatcstr(s, "");
        TestEqs("abc", cstr(s));
    }

    SV_TEST("concat empty strings")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bcatblk(s, "", 0);
        TestEqs("abc", cstr(s));
    }

    SV_TEST("trim")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr(" a     "));
        btrimws(s);
        TestEqs("a", cstr(s));
    }

    SV_TEST("truncate")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        btrunc(s, 2);
        TestEqs("ab", cstr(s));
    }

    SV_TEST("assign static to empty")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bstr_assignstatic(s, "");
        TestEqs("", cstr(s));
    }

    SV_TEST("assign static to longer")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        bstr_assignstatic(s, "abcdef");
        TestEqs("abcdef", cstr(s));
    }

    SV_TEST("length of normal string")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abc"));
        TestEqn(3, blength(s));
    }

    SV_TEST("length of empty string")
    {
        TEST_OPEN(bstring, s);
        TestEqn(0, blength(s));
    }

    SV_TEST("length of string containing null bytes")
    {
        TEST_OPEN(bstring, s);
        bcatblk(s, "abc", 3);
        bcatblk(s, "d\0e", 3);
        TestEqn(6, blength(s));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_string_bstr_extensions)
{
    SV_TEST("replace all should be case sensitive")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abaBabaBa"));
        bstr_replaceall(s, "b", "111");
        TestEqs(cstr(s), "a111aBa111aBa");
    }

    SV_TEST("replace all")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("abaBabaBa"));
        bstr_replaceall(s, "ab", "");
        TestEqs(cstr(s), "aBaBa");
    }

    SV_TEST("replace all even")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("aaaaaa"));
        bstr_replaceall(s, "aa", "");
        TestEqs(cstr(s), "");
    }

    SV_TEST("replace all odd")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("aaaaaaa"));
        bstr_replaceall(s, "aa", "");
        TestEqs(cstr(s), "a");
    }

    SV_TEST("should be ok to free zero'd string")
    {
        tagbstring s = {0};
        TestEqn(BSTR_ERR, bdestroy(&s));
    }

    SV_TEST("empty strings should compare equal")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr(""));
        TEST_OPEN_EX(bstring, s2, bfromcstr(""));
        TestTrue(bstr_equal(s1, s2));
    }

    SV_TEST("space is not empty string")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr(""));
        TEST_OPEN_EX(bstring, s2, bfromcstr(" "));
        TestTrue(!bstr_equal(s1, s2));
    }

    SV_TEST("empty string is not space")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr(" "));
        TEST_OPEN_EX(bstring, s2, bfromcstr(""));
        TestTrue(!bstr_equal(s1, s2));
    }

    SV_TEST("comparison is case sensitive")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr("abcdef"));
        TEST_OPEN_EX(bstring, s2, bfromcstr("abcdeF"));
        TestTrue(!bstr_equal(s1, s2));
    }

    SV_TEST("compare with differing lengths 1")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr("abcdefg"));
        TEST_OPEN_EX(bstring, s2, bfromcstr("abcdef"));
        TestTrue(!bstr_equal(s1, s2));
    }

    SV_TEST("compare with differing lengths 2")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr("abcdef"));
        TEST_OPEN_EX(bstring, s2, bfromcstr("abcdefg"));
        TestTrue(!bstr_equal(s1, s2));
    }

    SV_TEST("compare with same ptrs")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr("abc"));
        TestTrue(bstr_equal(s1, s1));
    }

    SV_TEST("compare with same contents")
    {
        TEST_OPEN_EX(bstring, s1, bfromcstr("abc"));
        TEST_OPEN_EX(bstring, s2, bfromcstr("abc"));
        TestTrue(bstr_equal(s1, s2));
    }

    SV_TEST("test assign block")
    {
        TEST_OPEN(bstring, s);
        bassignblk(s, "abcde", 4);
        TestTrue(s_equal(cstr(s), "abcd"));
    }

    SV_TEST("test warning for embedded null bytes")
    {
        if (WARN_NUL_IN_STR)
        {
            TEST_OPEN(bstring, swithnulls);
            quiet_warnings(true);
            bassignblk(swithnulls, "ab\0de", 4);
            TestTrue(cstr(swithnulls) == NULL);
            quiet_warnings(false);
        }
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_string_conversions)
{
    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstr("12", &n) && n == 12);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(!uintfromstr("", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstr("0", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstr("000", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(!uintfromstr("12a", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(!uintfromstr("abc", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(!uintfromstr(" ", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstr("0001", &n) && n == 1);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstr("000123", &n) && n == 123);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstrhex("12", &n) && n == 0x12);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(!uintfromstrhex("", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstrhex("0", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstrhex("000", &n) && n == 0);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstrhex("abc", &n) && n == 0xabc);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(uintfromstrhex("00012aa1", &n) && n == 0x12aa1);
    }

    SV_TEST_()
    {
        uint64_t n = 0;
        TestTrue(!uintfromstrhex("abcx", &n) && n == 0);
    }

    SV_TEST("we should support numbers larger than MAXINT32")
    {
        uint64_t n = 0;
        TestTrue(uintfromstr("11111111111111111111", &n) &&
            n == 11111111111111111111ULL);
    }

    SV_TEST("we do not support numbers larger than MAXINT64")
    {
        uint64_t n = 0;
        TestTrue(!uintfromstr("18446744073709551619", &n) && n == 0);
    }

    SV_TEST("we do not support numbers larger than MAXUINT64")
    {
        uint64_t n = 0;
        TestTrue(!uintfromstr("99999999999999999999", &n) && n == 0);
    }

    SV_TEST("we should support hex numbers larger than MAXINT32")
    {
        uint64_t n = 0;
        TestTrue(uintfromstrhex("1111111111111111", &n) &&
            n == 0x1111111111111111ULL);
    }

    SV_TEST("we do not support hex numbers larger than MAXINT64")
    {
        uint64_t n = 0;
        TestTrue(!uintfromstrhex("11111111111111111", &n) && n == 0);
    }

    SV_TEST("we do not support hex numbers larger than MAXUINT64")
    {
        uint64_t n = 0;
        TestTrue(!uintfromstrhex("999999999999999999", &n) && n == 0);
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_string_list)
{
    SV_TEST("simple split")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(bstring, s, bfromcstr("aa|bb|cc"));
        bstrlist_split(list, s, '|');
        TestEqs("aa", cstr(list->entry[0]));
        TestEqs("bb", cstr(list->entry[1]));
        TestEqs("cc", cstr(list->entry[2]));
        TestEqn(3, list->qty);
    }

    SV_TEST("simple join")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_appendcstr(list, "aa");
        bstrlist_appendcstr(list, "b");
        bstrlist_appendcstr(list, "cc");
        TEST_OPEN_EX(bstring, s, bjoin_static(list, "|"));
        TestEqs("aa|b|cc", cstr(s));
    }

    SV_TEST("split with empty element")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a--b-c", '-');
        TestEqList("a||b|c", list);
    }

    SV_TEST("split start with empty")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "-aaa", '-');
        TestEqList("|aaa", list);
    }

    SV_TEST("split end with empty")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "aaa-", '-');
        TestEqList("aaa|", list);
    }

    SV_TEST("split start and end with one empty")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "-a-a-", '-');
        TestEqList("|a|a|", list);
    }

    SV_TEST("split start and end with two empty")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "--a-a--", '-');
        TestEqList("||a|a||", list);
    }

    SV_TEST("split all delims 1")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "-", '-');
        TestEqList("|", list);
    }

    SV_TEST("split all delims 2")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "--", '-');
        TestEqList("||", list);
    }

    SV_TEST("split empty string")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "", '-');
        TestEqList("", list);
        TestEqn(1, list->qty);
        TestEqs("", blist_view(list, 0));
    }

    SV_TEST("view empty list")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TestEqList("", list);
        TestEqn(0, list->qty);
    }

    SV_TEST("blist_view")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("aa|b|cc"));
        TEST_OPEN_EX(bstrlist *, list, bsplit(s, '|'));
        TestEqs("aa", blist_view(list, 0));
        TestEqs("b", blist_view(list, 1));
        TestEqs("cc", blist_view(list, 2));
    }

    SV_TEST("bstrlist_appendcstr")
    {
        TEST_OPEN_EX(bstring, s, bfromcstr("BBB"));
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_appendcstr(list, "AA");
        bstrlist_append(list, s);
        bstrlist_appendcstr(list, "CC");
        TestEqList("AA|BBB|CC", list);
    }

    SV_TEST("remove at invalid index")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a|b|c|d", '|');
        TestEqn(BSTR_ERR, bstrlist_remove_at(list, -1));
        TestEqn(BSTR_ERR, bstrlist_remove_at(list, 4));
    }

    SV_TEST("remove")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a|b|c|d", '|');
        TestEqn(BSTR_OK, bstrlist_remove_at(list, 0));
        TestEqList("b|c|d", list);
    }

    SV_TEST("remove")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a|b|c|d", '|');
        TestEqn(BSTR_OK, bstrlist_remove_at(list, 1));
        TestEqList("a|c|d", list);
    }

    SV_TEST("remove")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a|b|c|d", '|');
        TestEqn(BSTR_OK, bstrlist_remove_at(list, 2));
        TestEqList("a|b|d", list);
    }

    SV_TEST("remove")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a|b|c|d", '|');
        TestEqn(BSTR_OK, bstrlist_remove_at(list, 3));
        TestEqList("a|b|c", list);
    }

    SV_TEST("remove last item")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a", '|');
        TestEqn(BSTR_OK, bstrlist_remove_at(list, 0));
        TestEqList("", list);
        TestEqn(0, list->qty);
    }

    SV_TEST("clear list")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "a|b|c", '|');
        bstrlist_clear(list);
        TestEqList("", list);
        TestEqn(0, list->qty);
    }

    SV_TEST("sort list")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "apple2|apple1|pear|peach", '|');
        bstrlist_sort(list);
        TestEqList("apple1|apple2|peach|pear", list);
    }

    SV_TEST("sort list should not ressurrect removed items")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_splitcstr(list, "apple2|apple1|pear|peach", '|');
        bstrlist_sort(list);
        bstrlist_appendcstr(list, "aaa");
        bstrlist_remove_at(list, 4);
        bcatcstr(list->entry[2], "a");
        bstrlist_sort(list);
        TestEqList("apple1|apple2|peacha|pear", list);
    }

    SV_TEST("sort empty list")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        bstrlist_sort(list);
        TestEqn(0, list->qty);
    }

    SV_TEST("join empty list")
    {
        TEST_OPEN_EX(bstrlist *, list, bstrlist_open());
        TEST_OPEN_EX(bstring, joined, bjoin_static(list, "|"));
        TestEqs("", cstr(joined));
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_string_pseudosplit)
{
    SV_TEST("simple split")
    {
        TEST_OPENA(sv_pseudosplit, list, "aa-bb-cc");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("aa|bb|cc", &list);
    }

    SV_TEST("split with empty element")
    {
        TEST_OPENA(sv_pseudosplit, list, "a--b-c");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("a||b|c", &list);
    }

    SV_TEST("split start with empty")
    {
        TEST_OPENA(sv_pseudosplit, list, "-aaa");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("|aaa", &list);
    }

    SV_TEST("split end with empty")
    {
        TEST_OPENA(sv_pseudosplit, list, "aaa-");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("aaa|", &list);
    }

    SV_TEST("split start and end with one empty")
    {
        TEST_OPENA(sv_pseudosplit, list, "-a-a-");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("|a|a|", &list);
    }

    SV_TEST("split start and end with two empty")
    {
        TEST_OPENA(sv_pseudosplit, list, "--a-a--");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("||a|a||", &list);
    }

    SV_TEST("split all delims 1")
    {
        TEST_OPENA(sv_pseudosplit, list, "-");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("|", &list);
    }

    SV_TEST("split all delims 2")
    {
        TEST_OPENA(sv_pseudosplit, list, "--");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("||", &list);
    }

    SV_TEST("split empty string")
    {
        TEST_OPENA(sv_pseudosplit, list, "");
        sv_pseudosplit_split(&list, '-');
        TestEqPsList("", &list);
        TestEqn(1, list.splitpoints.length);
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_widestrings)
{
    SV_TEST("init as empty")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        TestTrue(ws_equal(L"", wcstr(ws)));
    }

    SV_TEST("append very short string")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"a");
        sv_wstr_append(&ws, L"bc");
        TestTrue(ws_equal(L"abc", wcstr(ws)));
    }

    SV_TEST("append short string")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"");
        sv_wstr_append(&ws, L"abcd");
        TestTrue(ws_equal(L"abcd", wcstr(ws)));
    }

    SV_TEST("append long string")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"abc");
        sv_wstr_append(&ws, L"123456781234567812345678");
        TestTrue(ws_equal(L"abc123456781234567812345678", wcstr(ws)));
    }

    SV_TEST("append long string twice")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"123456781234567812345678");
        sv_wstr_append(&ws, L"123456781234567812345678");
        TestTrue(ws_equal(L"123456781234567812345678"
                          L"123456781234567812345678",
            wcstr(ws)));
    }

    SV_TEST("add one char at a time")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"a");
        sv_wstr_append(&ws, L"b");
        sv_wstr_append(&ws, L"c");
        sv_wstr_append(&ws, L"d");
        sv_wstr_append(&ws, L"e");
        sv_wstr_append(&ws, L"f");
        TestTrue(ws_equal(L"abcdef", wcstr(ws)));
    }

    SV_TEST("truncate same")
    {
        TEST_OPENA(sv_wstr, ws, 1);
        sv_wstr_append(&ws, L"abcd");
        sv_wstr_truncate(&ws, 4);
        TestTrue(ws_equal(L"abcd", wcstr(ws)));
    }

    SV_TEST("truncate shorter")
    {
        TEST_OPENA(sv_wstr, ws, 1);
        sv_wstr_append(&ws, L"abcd");
        sv_wstr_truncate(&ws, 2);
        TestTrue(ws_equal(L"ab", wcstr(ws)));
    }

    SV_TEST("truncate to nothing")
    {
        TEST_OPENA(sv_wstr, ws, 1);
        sv_wstr_append(&ws, L"abcd");
        sv_wstr_truncate(&ws, 0);
        TestTrue(ws_equal(L"", wcstr(ws)));
    }

    SV_TEST("clear")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"ab");
        sv_wstr_clear(&ws);
        TestTrue(ws_equal(L"", wcstr(ws)));
    }

    SV_TEST("append after clear")
    {
        TEST_OPENA(sv_wstr, ws, 4);
        sv_wstr_append(&ws, L"ab");
        sv_wstr_clear(&ws);
        sv_wstr_append(&ws, L"cd");
        TestTrue(ws_equal(L"cd", wcstr(ws)));
    }

#ifndef __linux__
    void wide_to_utf8(const wchar_t *wide, bstring output);
    void utf8_to_wide(const char *utf8, sv_wstr *output);
    SV_TEST("round-trip ascii string")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("abcd", &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestTrue(ws_equal(L"abcd", wcstr(s_16)));
        TestEqs("abcd", cstr(s_8));
        TestEqn(4, blength(s_8));
    }

    SV_TEST("round-trip encoding empty string")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("", &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestTrue(ws_equal(L"", wcstr(s_16)));
        TestEqs("", cstr(s_8));
        TestEqn(0, blength(s_8));
    }

    SV_TEST("encoding 2-byte sequence (accented e)")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("a\xC3\xA9", &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestTrue(ws_equal(L"a\xE9", wcstr(s_16)));
        TestEqs("a\xC3\xA9", cstr(s_8));
        TestEqn(3, blength(s_8));
    }

    SV_TEST("encoding 3-byte sequence (hangul)")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("a\xE1\x84\x81", &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestEqn(L'a', wcstr(s_16)[0]);
        TestEqn(0x1101, wcstr(s_16)[1]);
        TestEqn(L'\0', wcstr(s_16)[2]);
        TestEqs("a\xE1\x84\x81", cstr(s_8));
        TestEqn(4, blength(s_8));
    }

    SV_TEST("encoding 4-byte sequence (musical symbol g clef)")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("a\xF0\x9D\x84\x9E", &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestEqn(L'a', wcstr(s_16)[0]);
        TestEqn(0xD834, wcstr(s_16)[1]);
        TestEqn(0xDD1E, wcstr(s_16)[2]);
        TestEqn(L'\0', wcstr(s_16)[3]);
        TestEqs("a\xF0\x9D\x84\x9E", cstr(s_8));
        TestEqn(5, blength(s_8));
    }

    SV_TEST("encoding only a 4-byte sequence (musical symbol g clef)")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("\xF0\x9D\x84\x9E", &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestEqn(0xD834, wcstr(s_16)[0]);
        TestEqn(0xDD1E, wcstr(s_16)[1]);
        TestEqn(L'\0', wcstr(s_16)[2]);
        TestEqs("\xF0\x9D\x84\x9E", cstr(s_8));
        TestEqn(4, blength(s_8));
    }

    SV_TEST("encoding several 4-byte sequences "
            "(U+1D11E),(U+1D565),(U+1D7F6),(U+2008A)")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        TEST_OPEN(bstring, s_8);
        utf8_to_wide("\xf0\x9d\x84\x9e\xf0\x9d\x95"
                     "\xa5\xf0\x9d\x9f\xb6\xf0\xa0\x82\x8a",
            &s_16);
        wide_to_utf8(wcstr(s_16), s_8);
        TestEqn(0xD834, wcstr(s_16)[0]);
        TestEqn(0xDD1E, wcstr(s_16)[1]);
        TestEqn(0xd835, wcstr(s_16)[2]);
        TestEqn(0xdd65, wcstr(s_16)[3]);
        TestEqn(0xd835, wcstr(s_16)[4]);
        TestEqn(0xdff6, wcstr(s_16)[5]);
        TestEqn(0xd840, wcstr(s_16)[6]);
        TestEqn(0xdc8a, wcstr(s_16)[7]);
        TestEqn(L'\0', wcstr(s_16)[8]);
        TestEqs("\xf0\x9d\x84\x9e\xf0\x9d\x95\xa5\xf0"
                "\x9d\x9f\xb6\xf0\xa0\x82\x8a",
            cstr(s_8));
        TestEqn(16, blength(s_8));
    }

    SV_TEST("ensure correct length after encoding")
    {
        TEST_OPENA(sv_wstr, s_16, MAX_PATH);
        utf8_to_wide("a\xE1\x84\x81\xE1\x84\x81", &s_16);
        TestEqn(4, s_16.arr.length);
        sv_wstr_append(&s_16, L"a");
        TestEqn(5, s_16.arr.length);
        TestEqn(L'a', wcstr(s_16)[0]);
        TestEqn(L'a', wcstr(s_16)[3]);
        TestEqn(L'\0', wcstr(s_16)[4]);
    }
#endif
}
SV_END_TEST_SUITE()
