#pragma once

bool testintimpl(int lineno, unsigned long long n1, unsigned long long n2)
{
	if (n1 == n2)
		return true;

	bcstr s = bcstr_open();
	bcstr_addfmt(&s, "line %d, expected %lld but got %lld", lineno, n1, n2);
	alertdialog(s.s);
	bcstr_close(&s);
	return false;
}

bool teststrimpl(int lineno, const char* s1, const char* s2)
{
	if (s_equal(s1, s2))
		return true;

	bcstr s = bcstr_open();
	bcstr_addfmt(&s, "line %d, expected %s but got %s", lineno, s1, s2);
	alertdialog(s.s);
	bcstr_close(&s);
	return false;
}

#define TestEqs(s1, s2) do { \
	if (!teststrimpl(__LINE__, (s1), (s2))) { \
		DEBUGBREAK(); \
		exit(1); \
	} } while(0)

#define TestEqn(n1, n2) do { \
	if (!testintimpl(__LINE__, (unsigned long long)(n1), \
		(unsigned long long)(n2))) { \
		DEBUGBREAK(); \
		exit(1); \
	} } while(0)

#define TestTrue(cond) \
	TestEqn(1, (cond)!=0)

#define TestEqslist(par_received, ...) do { \
	bcstrlist ar_expected = bcstrlist_openwith(__VA_ARGS__);  \
	bcstr srec = bcstrlist_tostring(par_received); \
	bcstr sexpt = bcstrlist_tostring(&ar_expected); \
	TestEqs(sexpt.s, srec.s); \
	bcstr_close(&srec); \
	bcstr_close(&sexpt); \
	bcstrlist_close(&ar_expected); \
	} while (0)

#define TestErrMsgAndClose(currenterr, s) do { \
	TestTrue(currenterr.errormsg && s_contains(currenterr.errormsg, s)); \
	Result_close(&currenterr); \
	} while(0)

void test_dynamicarray()
{
	Uint64 testBuffer[10] = {};
	{ /* we shouldn't allocate when told to start with 0 elements */
		bcarray arr = bcarray_open(sizeof32(Uint64), 0);
		TestEqn(NULL, arr.buffer);
		TestEqn(sizeof32(Uint64), arr.elementsize);
		TestEqn(0, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_close(&arr);
	}
	{ /* we should allocate */
		bcarray arr = bcarray_open(sizeof32(Uint64), 3);
		TestTrue(NULL != arr.buffer);
		TestEqn(sizeof32(Uint64), arr.elementsize);
		TestEqn(3, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_close(&arr);
	}
	{ /* capacity should remain at initial value until < length, and then double. */
		bcarray arr = bcarray_open(sizeof32(Uint64), 3);
		TestEqn(3, arr.capacity);
		TestEqn(0, arr.length);
		testBuffer[0] = 10;

		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(1, arr.length);

		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(2, arr.length);

		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(3, arr.capacity);
		TestEqn(3, arr.length);

		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(4, arr.capacity);
		TestEqn(4, arr.length);

		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(8, arr.capacity);
		TestEqn(5, arr.length);

		bcarray_close(&arr);
	}
	{ /* don't allocate space until needed.*/
		bcarray arr = bcarray_open(sizeof32(Uint64), 0);
		TestTrue(NULL == arr.buffer);
		TestTrue(!arr.buffer);
		TestEqn(0, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_reserve(&arr, 15);
		TestTrue(NULL != arr.buffer);
		TestEqn(16, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_close(&arr);
	}
	{ /*reserve space when has content */
		bcarray arr = bcarray_open(sizeof32(Uint64), 0);
		testBuffer[0] = 10;
		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(1, arr.capacity);
		TestEqn(1, arr.length);
		TestEqn(10, *(Uint64*)bcarray_at(&arr, 0));

		bcarray_reserve(&arr, 4000);
		TestEqn(4096, arr.capacity);
		TestEqn(1, arr.length);
		TestEqn(10, *(Uint64*)bcarray_at(&arr, 0));
		bcarray_close(&arr);
	}
	{ /* test that addzeros adds zeros */
		bcarray arr = bcarray_open(sizeof32(byte), 0);
		bcarray_addzeros(&arr, 15);
		TestEqn(16, arr.capacity);
		TestEqn(15, arr.length);
		for (Uint32 i=0; i<arr.length; i++)
			TestEqn(0, *bcarray_at(&arr, i));

		bcarray_close(&arr);
	}
	{ /* test contents*/
		bcarray arr = bcarray_open(sizeof32(Uint64), 3);
		testBuffer[0] = 10;
		testBuffer[1] = 11;
		testBuffer[2] = 12;
		testBuffer[3] = 13;
		bcarray_add(&arr, (byte*)&testBuffer[0], 4);
		TestEqn(10, *(Uint64*)bcarray_at(&arr, 0));
		TestEqn(11, *(Uint64*)bcarray_at(&arr, 1));
		TestEqn(12, *(Uint64*)bcarray_at(&arr, 2));
		TestEqn(13, *(Uint64*)bcarray_at(&arr, 3));
		TestEqn(4, arr.length);
		bcarray_add(&arr, (byte*)&testBuffer[0], 1);
		TestEqn(10, *(Uint64*)bcarray_at(&arr, 4));
		TestEqn(5, arr.length);
		bcarray_close(&arr);
	}
	{ /* valid truncate */
		bcarray arr = bcarray_open(sizeof32(Uint64), 0);
		testBuffer[0] = 10;
		testBuffer[1] = 11;
		bcarray_add(&arr, (byte*)&testBuffer[0], 2);
		TestEqn(2, arr.length);

		bcarray_truncatelength(&arr, 2);
		TestEqn(2, arr.length);

		bcarray_truncatelength(&arr, 1);
		TestEqn(1, arr.length);
		TestEqn(10, *(Uint64*)bcarray_at(&arr, 0));
		bcarray_close(&arr);
	}
	{ /* helpers for Uint64*/
		bcarray arr = bcarray_open(sizeof32(Uint64), 0);
		bcarray_add64u(&arr, 123);
		TestEqn(123, *bcarray_at64u(&arr, 0));
		bcarray_add64u(&arr, 456);
		TestEqn(456, *bcarray_at64u(&arr, 1));
		bcarray_close(&arr);
	}
}

void test_dynamicarraysort()
{
	/* why macros like MAKEKEY? we want to sort with memcmp, 
	but on a little-endian machine, bytes are in the wrong order.*/

	{ /* we currently require little endian 32 */
		Uint32 n = 0x12345678;
		bcstr s = bcstr_open();
		bcstr_appendszlen(&s, (char*)&n, sizeof(n));
		TestEqs("\x78\x56\x34\x12", s.s);
		bcstr_close(&s);
	}
	{ /* we currently require little endian 64 */
		Uint64 n = 0x1122334455667788ULL;
		bcstr s = bcstr_open();
		bcstr_appendszlen(&s, (char*)&n, sizeof(n));
		TestEqs("\x88\x77\x66\x55\x44\x33\x22\x11", s.s);
		bcstr_close(&s);
	}
	{ /* test makekey32*/
		Uint32 n = MAKEKEY32(0x12345678);
		bcstr s = bcstr_open();
		bcstr_appendszlen(&s, (char*)&n, sizeof(n));
		TestEqs("\x12\x34\x56\x78", s.s);
		bcstr_close(&s);
	}
	{ /* test makekey64*/
		Uint64 n = MAKEKEY64(0x1122334455667788ULL);
		bcstr s = bcstr_open();
		bcstr_appendszlen(&s, (char*)&n, sizeof(n));
		TestEqs("\x11\x22\x33\x44\x55\x66\x77\x88", s.s);
		bcstr_close(&s);
	}

	{ /* test that memcmp works well with makekey32  */
		Uint32 a100 = MAKEKEY32(100), a200 = MAKEKEY32(200),
			a300 = MAKEKEY32(300), a400 = MAKEKEY32(400);
		TestTrue(memcmp(&a100, &a100, sizeof(Uint32)) == 0);
		TestTrue(memcmp(&a100, &a200, sizeof(Uint32)) < 0);
		TestTrue(memcmp(&a100, &a300, sizeof(Uint32)) < 0);
		TestTrue(memcmp(&a100, &a400, sizeof(Uint32)) < 0);
		TestTrue(memcmp(&a200, &a100, sizeof(Uint32)) > 0);
		TestTrue(memcmp(&a200, &a200, sizeof(Uint32)) == 0);
		TestTrue(memcmp(&a200, &a300, sizeof(Uint32)) < 0);
		TestTrue(memcmp(&a200, &a400, sizeof(Uint32)) < 0);
	}
	
	{ /* sorting int64 numbers */
		bcarray arr = bcarray_open(sizeof32(Uint64), 0);
		bcarray_add64u(&arr, MAKEKEY64(1000));
		bcarray_add64u(&arr, MAKEKEY64(3000));
		bcarray_add64u(&arr, MAKEKEY64(4000));
		bcarray_add64u(&arr, MAKEKEY64(2000));
		bcarray_sort(&arr);

		TestEqn(MAKEKEY64(1000), *bcarray_at64u(&arr, 0));
		TestEqn(MAKEKEY64(2000), *bcarray_at64u(&arr, 1));
		TestEqn(MAKEKEY64(3000), *bcarray_at64u(&arr, 2));
		TestEqn(MAKEKEY64(4000), *bcarray_at64u(&arr, 3));
		bcarray_close(&arr);
	}
	{ /* sorting int32 numbers */
		bcarray arr = bcarray_open(sizeof32(Uint32), 0);
		bcarray_addzeros(&arr, 4);
		*(Uint32*)bcarray_at(&arr, 0) = MAKEKEY32(100);
		*(Uint32*)bcarray_at(&arr, 1) = MAKEKEY32(300);
		*(Uint32*)bcarray_at(&arr, 2) = MAKEKEY32(400);
		*(Uint32*)bcarray_at(&arr, 3) = MAKEKEY32(200);
		bcarray_sort(&arr);

		TestEqn(MAKEKEY32(100), *(Uint32*)bcarray_at(&arr, 0));
		TestEqn(MAKEKEY32(200), *(Uint32*)bcarray_at(&arr, 1));
		TestEqn(MAKEKEY32(300), *(Uint32*)bcarray_at(&arr, 2));
		TestEqn(MAKEKEY32(400), *(Uint32*)bcarray_at(&arr, 3));
		bcarray_close(&arr);
	}

	typedef struct MockKeyValue {
		Uint32 key;
		char value[256];
	} MockKeyValue;

	bcarray kvarr = bcarray_open(sizeof(MockKeyValue), 0);
	bcarray_addzeros(&kvarr, 5);
	*(MockKeyValue*)bcarray_at(&kvarr, 0) = CAST(MockKeyValue) { MAKEKEY32(123), "aaa" };
	*(MockKeyValue*)bcarray_at(&kvarr, 1) = CAST(MockKeyValue) { MAKEKEY32(125), "zzz" };
	*(MockKeyValue*)bcarray_at(&kvarr, 2) = CAST(MockKeyValue) { MAKEKEY32(125), "aaa" };
	*(MockKeyValue*)bcarray_at(&kvarr, 3) = CAST(MockKeyValue) { MAKEKEY32(125), "ccc" };
	*(MockKeyValue*)bcarray_at(&kvarr, 4) = CAST(MockKeyValue) { MAKEKEY32(200), "bbb" };

	{ /* search by key, too low */
		Uint32 key = MAKEKEY32(100);
		TestTrue(-1 == bcarray_bsearch(&kvarr, &key, sizeof(key)));
	}
	{ /* search by key, lowest */
		Uint32 key = MAKEKEY32(123);
		TestEqn(0, bcarray_bsearch(&kvarr, &key, sizeof(key)));
	}
	{ /* search by key, medium */
		Uint32 key = MAKEKEY32(125);
		TestEqn(1, bcarray_bsearch(&kvarr, &key, sizeof(key)));
	}
	{ /* search by key, highest */
		Uint32 key = MAKEKEY32(200);
		TestEqn(4, bcarray_bsearch(&kvarr, &key, sizeof(key)));
	}
	{ /* search by key, too high */
		Uint32 key = MAKEKEY32(900);
		TestTrue(-1 == bcarray_bsearch(&kvarr, &key, sizeof(key)));
	}
	{ /* walk back, too low and hits*/
		Uint32 key = MAKEKEY32(123);
		TestTrue(-20 == bcarray_walk_first_match(&kvarr, -20, &key, sizeof(key)));
	}
	{ /* walk back, first and hits*/
		Uint32 key = MAKEKEY32(123);
		TestTrue(0 == bcarray_walk_first_match(&kvarr, 0, &key, sizeof(key)));
	}
	{ /* walk back, first and does not hit*/
		Uint32 key = MAKEKEY32(900);
		TestTrue(0 == bcarray_walk_first_match(&kvarr, 0, &key, sizeof(key)));
	}
	{ /* walk back, back to first match */
		Uint32 key = MAKEKEY32(125);
		TestTrue(1 == bcarray_walk_first_match(&kvarr, 3, &key, sizeof(key)));
	}
	{ /* walk back, already at first match */
		Uint32 key = MAKEKEY32(200);
		TestTrue(4 == bcarray_walk_first_match(&kvarr, 4, &key, sizeof(key)));
	}
	bcarray_close(&kvarr);
}

void test_basicarithmatic()
{
	TestEqn(2070, safemul32(45, 46));
	TestEqn(4294910784ULL, safemul32(123456, 34789));
	TestEqn(4294967295, safemul32(1, 4294967295));
	TestEqn(0, safemul32(0, 4294967295));
	TestEqn(690, safeadd32(123, 567));
	TestEqn(4294967295, safeadd32(2147483647, 2147483648));
	TestEqn(4294967295, safeadd32(1, 4294967294));
	TestEqn(690, safeadd32s(123, 567));
	TestEqn(2147483647, safeadd32s(1073741824, 1073741823));
	TestEqn(2147483647, safeadd32s(1, 2147483646));
	TestEqn(-112, safeadd32s(-45, -67));
	TestEqn(INT_MIN, safeadd32s(-1073741824, -1073741824));
	TestEqn(INT_MIN, safeadd32s(-1, -2147483647));

	/* if enabled, each of these lines trigger overflow error: */
#ifdef EXPECT_OVERFLOW_FAILURE
	TestEqn(4294910784ULL, safemul32(123456, 34790));
	TestEqn(8589934590ULL, safemul32(4294967295, 2));
	TestEqn(18446744065119617025ULL, safemul32(4294967295, 2));
	TestEqn(4294967296, safeadd32(1, 4294967295));
	TestEqn(8589934590ULL, safeadd32(4294967295, 4294967295));
	TestEqn(2147483648ULL, safeadd32s(1, 2147483647));
	TestEqn(4294967294ULL, safeadd32s(2147483647, 2147483647));
	TestEqn(((Int64)INT_MIN)-1, safeadd32s(-1, INT_MIN));
	TestEqn(-4294967296LL, safeadd32s(INT_MIN, INT_MIN));
#endif

	TestEqn(5, countof("abcd"));
	TestEqn(5, sizeof("abcd"));
	TestEqn(0x1111000011112222ULL, make_uint64(0x11110000, 0x11112222));
	TestEqn(0x00000000FF345678ULL, make_uint64(0x00000000, 0xFF345678));
	TestEqn(0xFF345678FF345678ULL, make_uint64(0xFF345678, 0xFF345678));
	TestEqn(0xFF34567800000000ULL, make_uint64(0xFF345678, 0x00000000));
	TestEqn(0, align_to_multiple(0, 8));
	TestEqn(8, align_to_multiple(1, 8));
	TestEqn(48, align_to_multiple(47, 8));
	TestEqn(48, align_to_multiple(48, 8));
	TestEqn(56, align_to_multiple(49, 8));
	TestEqn(1, nearest_power_of_two32(0));
	TestEqn(1, nearest_power_of_two32(1));
	TestEqn(2, nearest_power_of_two32(2));
	TestEqn(4, nearest_power_of_two32(3));
}

void test_basic()
{
	TestEqs("", "");
	TestEqs("abc", "abc");
	TestEqn(0, 0);
	TestEqn(123, 123);
	TestTrue(true);
	TestTrue(1);
	TestTrue(1+1 == 2);
}

void test_string()
{
	{ /* test initempty creates a valid empty string*/
		bcstr str = bcstr_open();
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test init can create empty string*/
		bcstr str = bcstr_openwith("");
		bcstr_append(&str, "");
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test init with ordinary string*/
		bcstr str = bcstr_openwith("abcde");
		TestEqs("abcde", str.s);
		TestEqn(5, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test init with many ordinary strings*/
		bcstr str = bcstr_openwith("aa", "B", "cc", "d");
		TestEqs("aaBccd", str.s);
		TestEqn(6, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test append with length*/
		bcstr str = bcstr_openwith("");
		bcstr_appendszlen(&str, "abcd", 2);
		TestEqs("ab", str.s);
		TestEqn(2, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test append with zero-length*/
		bcstr str = bcstr_openwith("");
		bcstr_appendszlen(&str, "abcd", 0);
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test append with many strings, some empty strings*/
		bcstr str = bcstr_openwith(""); 
		bcstr_append(&str, "", "aa", "B");
		TestEqs("aaB", str.s);
		TestEqn(3, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test append with many strings, all empty strings*/
		bcstr str = bcstr_openwith("");
		bcstr_append(&str, "", "", "", "");
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_append(&str, "", "", "", "");
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test that clear() sets to empty string*/
		bcstr str = bcstr_openwith("abc");
		TestEqs("abc", str.s);
		bcstr_clear(&str);
		TestEqs("", str.s);
		TestEqn(1, str.__buffer.length /*includes nul term*/);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test that assign() clears previous content*/
		bcstr str = bcstr_openwith("abc");
		TestEqs("abc", str.s);
		bcstr_assign(&str, "def");
		TestEqs("def", str.s);
		TestEqn(4, str.__buffer.length /*includes nul term*/);
		TestEqn(3, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test addchar() */
		bcstr str = bcstr_openwith("abc");
		bcstr_addchar(&str, 'D');
		bcstr_addchar(&str, 'E');
		TestEqs("abcDE", str.s);
		TestEqn(5, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing a uint64 0 */
		bcstr str = bcstr_open();
		bcstr_addfmt(&str, "%llu", 0ULL);
		TestEqs("0", str.s);
		TestEqn(1, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing a large uint64 */
		bcstr str = bcstr_open();
		bcstr_addfmt(&str, "%llu", 12345678987654321ULL);
		TestEqs("12345678987654321", str.s);
		TestEqn(17, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing a wide string */
		bcstr str = bcstr_open();
		bcstr_addfmt(&str, "s='%S'", L"wide");
		TestEqs("s='wide'", str.s);
		TestEqn(8, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing some formatted stuff */
		bcstr str = bcstr_open();
		bcstr_addfmt(&str, "there is %d, %s, and hex %x.", 12, "abc", 0xf00);
		TestEqs("there is 12, abc, and hex f00.", str.s);
		bcstr_close(&str);
	}
	{ /* test that addfmt() appends correctly */
		bcstr str = bcstr_openwith("abc");
		bcstr_addfmt(&str, "deff");
		bcstr_addfmt(&str, "ghi123456789123456789");
		TestEqs("abcdeffghi123456789123456789", str.s);
		bcstr_close(&str);
	}
	{ /* test trim end */
		bcstr str = bcstr_openwith("abc");
		bcstr_trimwhitespace(&str);
		TestEqs("abc", str.s);

		bcstr_assign(&str, " a     ");
		bcstr_trimwhitespace(&str);
		TestEqs(" a", str.s);

		bcstr_assign(&str, "   ");
		bcstr_trimwhitespace(&str);
		TestEqs("", str.s);

		bcstr_assign(&str, "");
		bcstr_trimwhitespace(&str);
		TestEqs("", str.s);

		bcstr_close(&str);
	}
	{ /* bcstr_truncate should create valid string */
		bcstr str = bcstr_openwith("abc");
		bcstr_truncate(&str, 3);
		TestEqs("abc", str.s);
		TestEqn(3, bcstr_length(&str));
		bcstr_truncate(&str, 2);
		TestEqs("ab", str.s);
		TestEqn(2, bcstr_length(&str));
		bcstr_truncate(&str, 0);
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* int from string */
		int n = 0;
		TestTrue(s_intfromstring("12", &n) && n == 12);
		TestTrue(!s_intfromstring("", &n) && n == 0);
		TestTrue(s_intfromstring("0", &n) && n == 0);
		TestTrue(!s_intfromstring("12a", &n) && n == 0);
		TestTrue(!s_intfromstring("abc", &n) && n == 0);
		TestTrue(!s_intfromstring(" ", &n) && n == 0);
	}
	{ /* int from string should fail for very large numbers */
		int n = 0;
		if (sizeof(long) == 4)
		{
			TestTrue(!s_intfromstring("4294967299", &n) && n == 0);
			TestTrue(!s_intfromstring("99999999999", &n) && n == 0);
		}
		else if (sizeof(long) == 8)
		{
			TestTrue(!s_intfromstring("18446744073709551619", &n) && n == 0);
			TestTrue(!s_intfromstring("99999999999999999999", &n) && n == 0);
		}
	}

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

void test_stringlist()
{
	{ /* simple strarray example */
		bcstrlist list = bcstrlist_openwith("a", "b", "c");
		bcstr str = bcstrlist_tostring(&list);
		TestEqs("length=3\nindex 0:'a'\nindex 1:'b'\nindex 2:'c'\n", str.s);
		bcstr_close(&str);
		bcstrlist_close(&list);
	}
	{ /* manually retrieve from strarray*/
		bcstrlist list = bcstrlist_open();
		bcstrlist_pushback(&list, "first");
		bcstrlist_pushback(&list, "");
		bcstrlist_pushback(&list, "third");

		bcstr received = bcstr_open();
		bcstrlist_at(&list, 0, &received);
		TestEqs("first", received.s);
		bcstrlist_at(&list, 1, &received);
		TestEqs("", received.s);
		bcstrlist_at(&list, 2, &received);
		TestEqs("third", received.s);

		TestEqs("first", bcstrlist_viewat(&list, 0));
		TestEqs("", bcstrlist_viewat(&list, 1));
		TestEqs("third", bcstrlist_viewat(&list, 2));

		bcstr_close(&received);
		bcstrlist_close(&list);
	}
	{ /* push and pop from array*/
		bcstrlist list = bcstrlist_open();
		TestEqn(0, list.length);
		bcstrlist_pushback(&list, "a");
		TestEqn(1, list.length);
		bcstrlist_pushback(&list, "b");
		TestEqn(2, list.length);

		bcstrlist_pop(&list);
		TestEqn(1, list.length);
		bcstrlist_pop(&list);
		TestEqn(0, list.length);

		/* re-use allocated strings */
		bcstrlist_pushback(&list, "aa");
		TestEqn(1, list.length);
		bcstrlist_pushback(&list, "bb");
		TestEqn(2, list.length);

		/* have to allocate a new string */
		bcstrlist_pushback(&list, "cc");
		TestEqn(3, list.length);
		bcstrlist_close(&list);
	}
	{ /*clear should remove all elments*/
		bcstrlist list = bcstrlist_open();
		bcstrlist_pushback(&list, "first");
		bcstrlist_pushback(&list, "second");
		TestEqn(2, list.length);
		bcstrlist_clear(&list, false);
		TestEqn(0, list.length);
		bcstrlist_close(&list);
	}
	{ /* strarray to string */
		bcstrlist sarr1 = bcstrlist_openwith("first", "", "third");
		bcstr str1 = bcstrlist_tostring(&sarr1);
		TestEqslist(&sarr1, "first", "", "third");
		TestEqs("length=3\nindex 0:'first'\nindex 1:''\nindex 2:'third'\n", str1.s);

		bcstrlist sarr2 = bcstrlist_openwith("", "");
		bcstr str2 = bcstrlist_tostring(&sarr2);
		TestEqslist(&sarr2, "", "");
		TestEqs("length=2\nindex 0:''\nindex 1:''\n", str2.s);

		bcstr_close(&str1);
		bcstr_close(&str2);
		bcstrlist_close(&sarr1);
		bcstrlist_close(&sarr2);
	}
	{ /* test sorting*/
		bcstrlist list = bcstrlist_openwith("apple2", "apple1", "pear", "peach");
		TestEqn(4, list.length);
		bcstrlist_sort(&list);
		TestEqslist(&list, "apple1", "apple2", "peach", "pear");
		bcstrlist_close(&list);
	}
	{ /* test sorting after pop. make sure 'apple' doesn't come back. */
		bcstrlist list = bcstrlist_openwith("peach", "pear", "apple");
		bcstrlist_pop(&list);
		TestEqn(2, list.length);
		bcstrlist_sort(&list);
		TestEqslist(&list, "peach", "pear");
		bcstrlist_close(&list);
	}
	{ /* test sorting after clear. make sure 'apple' doesn't come back. */
		bcstrlist list = bcstrlist_openwith("peach", "apple");
		bcstrlist_clear(&list, false);
		bcstrlist_pushback(&list, "plum");
		TestEqn(1, list.length);
		bcstrlist_sort(&list);
		TestEqslist(&list, "plum");
		bcstrlist_close(&list);
	}
	{ /* test split common cases*/
		bcstrlist list = bcstrlist_open();
		bcstrlist_add_str_split(&list, "a|bb|c", '|');
		TestEqslist(&list, "a", "bb", "c");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "a||b|c", '|');
		TestEqslist(&list, "a", "", "b", "c");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "a||b|c", '|');
		TestEqslist(&list, "a", "", "b", "c");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "a||b|c", '?');
		TestEqslist(&list, "a||b|c");
		bcstrlist_clear(&list, false);

		bcstrlist_close(&list);
	}
	{ /* test split less common cases*/
		bcstrlist list = bcstrlist_open();
		bcstrlist_add_str_split(&list, "", '|');
		TestEqslist(&list, "");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "|", '|');
		TestEqslist(&list, "", "");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "||", '|');
		TestEqslist(&list, "", "", "");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "a |b\r\n|c\n|", '|');
		TestEqslist(&list, "a ", "b\r\n", "c\n", "");
		bcstrlist_clear(&list, false);

		bcstrlist_add_str_split(&list, "||a||b|c||", '|');
		TestEqslist(&list, "", "", "a", "", "b", "c", "", "");
		bcstrlist_clear(&list, false);

		bcstrlist_close(&list);
	}
}

void test_getfileextension()
{
#define STR_AND_LEN(pas) (pas), strlen32(pas)
#define TESTGETTYPE(type, path) \
	TestEqn((type), svdpgetfileextensiontype_get( \
	STR_AND_LEN(path), customtypes, countof(customtypes)))

	Result currenterr = {};
	Uint32 customtypes[4] = {};

	/* test that errors are triggered when they should be */
	{
		g_log.suppress_dialogs = true;

		{ /* provide too many extensions*/
			bcstrlist list = bcstrlist_openwith("ab", "ab", "ab", "ab", "ab");
			Result err = svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes));
			bcstrlist_close(&list);
			TestErrMsgAndClose(err, "number of extensions. 4");
		}
		{ /* use wrong format */
			bcstrlist list = bcstrlist_openwith("tmp", ".pyc");
			Result err = svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes));
			bcstrlist_close(&list);
			TestErrMsgAndClose(err, "write just the extension");
		}
		/* test wrong # of chars */
		const char* tests[] = { "", "a", "12345", "123456" };
		for (size_t i = 0; i < countof(tests); i++)
		{
			bcstrlist list = bcstrlist_openwith("tmp", tests[i]);
			Result err = svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes));
			bcstrlist_close(&list);
			TestErrMsgAndClose(err, "must be 2, 3");
		}

		g_log.suppress_dialogs = false;
	}

	{ /* normal lookup, it's the first in the array */
		bcstrlist list = bcstrlist_openwith("foo", "tmp");
		check(svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes)));
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.foo");
		TESTGETTYPE(FileExtTypeAudio, "/path/to/testfile.mp3");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.bar");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile_noext");
		TESTGETTYPE(FileExtTypeOther, ".foo");
		TESTGETTYPE(FileExtTypeOther, "/a");
		bcstrlist_close(&list);
	}
	{ /* it's the second in the array */
		bcstrlist list = bcstrlist_openwith("tmp", "foo");
		check(svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes)));
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.foo");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.tmp");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.bar");
		bcstrlist_close(&list);
	}
	{ /* test for lengths 2,3,and 4 */
		bcstrlist list = bcstrlist_openwith("ab", "cde", "fghi");
		check(svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes)));
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.ab");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.cde");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.fghi");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.nab");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.ncde");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.nfghi");

		/* differing case or prefix means not a match*/
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.fghI");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.abcde");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.abab");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.00ab");

		/* need make sure we treat ..ab as ".ab" not "..ab" */
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.ab");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile..ab");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile...ab");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile..cde");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile..fghi");

		/* dots after are not ok*/
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.ab.");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.cde.");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.fghi.");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.fghi..");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.fghi...");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.fghi....");

		bcstrlist_close(&list);
	}
	{ /* test the built-in ones*/
		bcstrlist list = bcstrlist_open();
		check(svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes)));
		TestEqn(0, customtypes[0]);
		TESTGETTYPE(FileExtTypeNotCompressable, "/path/to/testfile.7z");
		TESTGETTYPE(FileExtTypeNotCompressable, "/path/to/testfile.zip");
		TESTGETTYPE(FileExtTypeNotCompressable, "/path/to/testfile.webm");
		TESTGETTYPE(FileExtTypeAudio, "/path/to/testfile.m4a");
		TESTGETTYPE(FileExtTypeAudio, "/path/to/testfile.flac");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.o7z");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.omp3");
		TESTGETTYPE(FileExtTypeOther, "/path/to/testfile.oflac");
		bcstrlist_close(&list);
	}
	{ /* user-specified need to take precedence */
		bcstrlist list = bcstrlist_openwith("7z", "mp3", "flac");
		check(svdpgetfileextensiontype_add(&list, customtypes, countof(customtypes)));
		TESTGETTYPE(FileExtTypeAudio, "/path/to/testfile.m4a");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.mp3");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.7z");
		TESTGETTYPE(FileExtTypeExclude, "/path/to/testfile.flac");
		bcstrlist_close(&list);
	}
	{ /* test comparison fn */
		Uint32 arr[] = {4, 2, 3, 1};
		Uint32 elementsize = sizeof(arr[0]);
		qsort_r(&arr[0], countof(arr), sizeof(arr[0]), &bcarray_sort_memcmp, &elementsize);
		TestEqn(1, arr[0]);
		TestEqn(2, arr[1]);
		TestEqn(3, arr[2]);
		TestEqn(4, arr[3]);
	}

	{ /* every single Uint32 in this arr should be unique. 
	  to verify, sort and compare adjacents.*/
		Uint32 arr[] = { chars_to_uint32(0, 0, 0, 0),
			chars_to_uint32(0, 0, 0, UCHAR_MAX),
			chars_to_uint32(0, 0, UCHAR_MAX, 0),
			chars_to_uint32(0, UCHAR_MAX, 0, 0),
			chars_to_uint32(UCHAR_MAX, 0, 0, 0),
			chars_to_uint32(UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX) };

		/* after sorting, no adjacent entries should be equal */
		Uint32 elementsize = sizeof(arr[0]);
		qsort_r(&arr[0], countof(arr), sizeof(arr[0]), &bcarray_sort_memcmp, &elementsize);
		for (int i=0; i<countof(arr)-1; i++)
			TestTrue(arr[i] != arr[i+1]);
	}
	{ /* should fill all bits. */
		TestEqn(0xffffffff, chars_to_uint32(UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX));
	}

cleanup:
	print_error_if_ocurred("", &currenterr);
	TestTrue(currenterr.errortag == 0);
#undef STR_AND_LEN
#undef TESTGETTYPE
}

void test_logging(const char* dir)
{
	Result currenterr = {};
	bcstr tmp = bcstr_open();
	bcstr result = bcstr_open();
	bcstr logpattern = bcstr_openwith(dir, pathsep "testlog");
	bcstr logpath1 = bcstr_openwith(dir, pathsep "testlog00001.txt");
	bcstr logpath2 = bcstr_openwith(dir, pathsep "testlog00002.txt");
	bcstr logpath3 = bcstr_openwith(dir, pathsep "testlog00003.txt");
	int number = 0;

	{ /* generate names */
		filenamepattern_gen("/path/file", ".txt", 0, &result);
		TestEqs("/path/file00000.txt", result.s);
		filenamepattern_gen("/path/file", ".txt", 1, &result);
		TestEqs("/path/file00001.txt", result.s);
		filenamepattern_gen("/path/file", ".txt", 12345, &result);
		TestEqs("/path/file12345.txt", result.s);
	}
	{ /* wrong prefix or suffix*/
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file.txt2", &tmp));
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/2path/file.txt", &tmp));
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/fileb00001.txt", &tmp));
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file00001txt", &tmp));
	}
	{ /* wrong number of digits*/
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file.txt", &tmp));
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file1234.txt", &tmp));
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file123456.txt", &tmp));
	}
	{ /* we should reject non-numeric chars */
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file12a45.txt", &tmp));
	}
	{ /* we should reject chars that are negative when char is signed */
		TestEqn(-1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file0000\xCC.txt", &tmp));
	}
	{ /* read valid names */
		TestEqn(1, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file00001.txt", &tmp));
		TestEqn(12345, filenamepattern_matches_template(
			"/path/file", ".txt", "/path/file12345.txt", &tmp));
	}
	{ /* no matching files should return 1. */
		check(os_removechildren(dir));
		check(filenamepattern_getlatest(logpattern.s, ".txt", &number, &result));
		TestEqn(1, number);
		TestEqs(logpath1.s, result.s);
	}
	{ /* third file present, should return 3. */
		check(os_removechildren(dir));
		check(bcfile_writefile(logpath3.s, "abcd", "w"));
		check(filenamepattern_getlatest(logpattern.s, ".txt", &number, &result));
		TestEqn(3, number);
		TestEqs(logpath3.s, result.s);
	}
	{ /* first, second, third file present, should return 3. */
		check(os_removechildren(dir));
		check(bcfile_writefile(logpath1.s, "abcd", "w"));
		check(bcfile_writefile(logpath2.s, "abcd", "w"));
		check(bcfile_writefile(logpath3.s, "abcd", "w"));
		check(filenamepattern_getlatest(logpattern.s, ".txt", &number, &result));
		TestEqn(3, number);
		TestEqs(logpath3.s, result.s);
	}
	{ /* first file present, should return 1. */
		check(os_removechildren(dir));
		check(bcfile_writefile(logpath1.s, "abcd", "w"));
		check(filenamepattern_getlatest(logpattern.s, ".txt", &number, &result));
		TestEqn(1, number);
		TestEqs(logpath1.s, result.s);
	}
	{ /* don't move to next file when not too big. */
		bc_log log = {};
		check(os_removechildren(dir));
		check(bc_log_open(&log, logpattern.s));
		log.how_often_checkfilesize = 1;
		bc_log_write(&log, "test");
		bc_log_write(&log, "test");
		bc_log_write(&log, "test");
		bc_log_close(&log);

		TestTrue(os_file_exists(logpath1.s));
		TestTrue(!os_file_exists(logpath2.s));
	}
	{ /* do move to next file when too big. */
		bc_log log = {};
		check(os_removechildren(dir));
		check(bc_log_open(&log, logpattern.s));
		log.how_often_checkfilesize = 1;
		log.approx_max_filesize = 10; /* new file every 10 bytes*/
		bc_log_write(&log, "test");
		bc_log_write(&log, "test");
		bc_log_write(&log, "test");
		bc_log_close(&log);

		TestTrue(os_file_exists(logpath1.s));
		TestTrue(os_file_exists(logpath2.s));
	}

cleanup:
	bcstr_close(&tmp);
	bcstr_close(&result);
	bcstr_close(&logpattern);
	bcstr_close(&logpath1);
	bcstr_close(&logpath2);
	bcstr_close(&logpath3);
	print_error_if_ocurred("", &currenterr);
	TestTrue(currenterr.errortag == 0);
}

void test_readconfigs(const char* dir)
{
	Result currenterr = {};
	bcstr inipath = bcstr_open();
	SvdpGlobalConfig tmpconfig = svdpglobalconfig_open();

	{
		/* write the demo */
		bcstr_append(&inipath, dir, pathsep, "config_demo.ini");
		check(svdpglobalconfig_writedemo(inipath.s, "userdir1", "userdir2"));

		/* parse the demo */
		SvdpGlobalConfig config = svdpglobalconfig_open();
		check(svdpglobalconfig_read(&config, inipath.s));
		TestEqn(1, config.groups.length);
		TestEqs("userdir1", config.locationForLocalIndex.s);
		TestEqs("userdir2", config.locationForFilesReadyToUpload.s);

		/* check group settings*/
		SvdpGroupConfig* group = (SvdpGroupConfig*) bcarray_at(&config.groups, 0);
		TestEqs("group1", group->groupname.s);
		TestEqs("(ask)", group->pass.s);
#if __linux__
		TestEqs("gpg", group->encryption.s);
		TestEqslist(&group->arDirs, "/home/a_directory", "/home/another_directory");
		TestEqslist(&group->arExcludedDirs, "/home/exclude_this_directory");
#elif _WIN32
		TestEqs("AES-256", group->encryption.s);
		TestEqslist(&group->arDirs, "C:\\a_directory", "C:\\another_directory");
		TestEqslist(&group->arExcludedDirs, "C:\\exclude_this_directory");
		TestTrue(bcstr_length(&config.path7zip) > 0);
		TestTrue(bcstr_length(&config.pathFfmpeg) > 0);
#endif
		TestEqn(0, group->fSeparatelyTrackTaggingChangesInAudioFiles);
		TestEqn(64, group->targetArchiveSizeInMb);
		
		TestEqslist(&group->arExcludedExtensions, "tmp", "pyc");
		TestEqn(1, group->arExcludedDirs.length);

		svdpglobalconfig_close(&config);
	}
	{ /* a more complex test*/
		bcstr_clear(&inipath);
		bcstr_append(&inipath, dir, pathsep, "config_test.ini");
		check(bcfile_writefile(inipath.s, 
			"locationForLocalIndex=actual\n"
			"locationForLocalIndex=\n" /* ignore setting to empty */
			"locationForLocalIndex=       \n" /* ignore setting to whitespace */
			"#locationForLocalIndex=commentedOut1\n" /* ignore comments */
			";locationForLocalIndex=commentedOut2\n" /* ignore comments */
			"7zip=whitespace stripped       \n"
			"unknown=unknown\n"
			"[groupFirst]\n"
			"excludedDirs=justOneExcluded\n"
			"[groupSecond]\n"
			"excludedDirs=dir_1|dir_2\n"
			"targetArchiveSizeInMb=20\n","w"));

		SvdpGlobalConfig config = svdpglobalconfig_open();
		check(svdpglobalconfig_read(&config, inipath.s));
		TestEqs("actual", config.locationForLocalIndex.s);
		TestEqs("whitespace stripped", config.path7zip.s);

		TestEqn(2, config.groups.length);
		SvdpGroupConfig* group1 = (SvdpGroupConfig*)bcarray_at(&config.groups, 0);
		SvdpGroupConfig* group2 = (SvdpGroupConfig*)bcarray_at(&config.groups, 1);
		TestEqs("groupFirst", group1->groupname.s);
		TestEqs("groupSecond", group2->groupname.s);
		TestEqn(64, group1->targetArchiveSizeInMb);
		TestEqn(20, group2->targetArchiveSizeInMb);
		TestEqslist(&group1->arExcludedDirs, "justOneExcluded");
		TestEqslist(&group2->arExcludedDirs, "dir_1", "dir_2");

		svdpglobalconfig_close(&config);
	}

	/* check that invalid configs cause errors */
	g_log.suppress_dialogs = true;

	{ /* settings that need to be in a group */
		check(bcfile_writefile(inipath.s,
			"locationForLocalIndex=dir\n"
			"excludedDirs=incorrectly outside group\n", "w"));
		Result err = svdpglobalconfig_read(&tmpconfig, inipath.s);
		TestErrMsgAndClose(err, "need to be in a group to");
	}
	{ /* settings that need to be numeric */
		check(bcfile_writefile(inipath.s,
			"[group1]\n"
			"targetArchiveSizeInMb=1a\n", "w"));
		Result err = svdpglobalconfig_read(&tmpconfig, inipath.s);
		TestErrMsgAndClose(err, "parameter requires a number");
	}
	{ /* no duplicate groups */
		check(bcfile_writefile(inipath.s,
			"[groupName]\n"
			"targetArchiveSizeInMb=1\n"
			"[groupName]\n", "w"));
		Result err = svdpglobalconfig_read(&tmpconfig, inipath.s);
		TestErrMsgAndClose(err, "duplicate group name");
	}
	{ /* no duplicate groups casing */
		check(bcfile_writefile(inipath.s,
			"[groupName]\n"
			"targetArchiveSizeInMb=1\n"
			"[groupNamE]\n", "w"));
		Result err = svdpglobalconfig_read(&tmpconfig, inipath.s);
		TestErrMsgAndClose(err, "duplicate group name");
	}

	g_log.suppress_dialogs = false;

cleanup:
	svdpglobalconfig_close(&tmpconfig);
	bcstr_close(&inipath);
	print_error_if_ocurred("", &currenterr);
	TestTrue(currenterr.errortag == 0);
}

typedef struct Results_testosrecurse_entry
{
	char filepath[PATH_MAX]; /* filepath first, for sorting*/
	Uint32 filepathlen;
	byte nativepath[PATH_MAX];
	Uint32 cbnativepathlen;
	Uint64 filesize;
	int dirnumber;
} Results_testosrecurse_entry;

typedef struct Results_testosrecurse
{
	int index;
	Results_testosrecurse_entry entries[10];
} Results_testosrecurse;

Result testosrecurse_callback(void* context, int dirnumber,
	const char* filepath, Uint32 filepathlen, const byte* nativepath, 
	Uint32 cbnativepathlen, Uint64 modtime, Uint64 filesize)
{
	Results_testosrecurse* results = (Results_testosrecurse*)context;
	Results_testosrecurse_entry* entry = &results->entries[results->index];
	entry->dirnumber = dirnumber;
	entry->filepathlen = filepathlen;
	entry->cbnativepathlen = cbnativepathlen;
	entry->filesize = filesize;

	/* check string lengths and get strings */
	TestTrue(filepathlen < countof(entry->filepath) &&
		cbnativepathlen < countof(entry->nativepath));
	memcpy(&entry->filepath[0], filepath, filepathlen);
	memcpy(&entry->nativepath[0], nativepath, cbnativepathlen);
	
	/* these shouldn't point to the same place */
	TestTrue((byte*)filepath != (byte*)nativepath);

	results->index++;
	TestTrue(results->index < countof(results->entries));
	TestTrue(modtime != 0);
	return ResultOk;
}

void test_os_util(const char* dirtest)
{
	Result currenterr = {};
	bcstr dir = bcstr_openwith(dirtest, pathsep "testfiles");
	bcstr path = bcstr_openwith(dir.s, pathsep "readwrite.txt");
	bcstr path2 = bcstr_openwith(dir.s, pathsep "zreadwrite.txt");
	bcstr subdir = bcstr_openwith(dir.s, pathsep "subdir");
	bcstr subsubdir = bcstr_openwith(subdir.s, pathsep "sub");
	bcstr subsubdirfile = bcstr_openwith(subsubdir.s, pathsep "s.txt");
	bcstr subdirfile = bcstr_openwith(subdir.s, pathsep "file");
	bcstr subdirfile2 = bcstr_openwith(subdir.s, pathsep "file2");
	bcstr content = bcstr_open();
	bcstrlist seen = bcstrlist_open();

	bcstr fairlylongstring = bcstr_open();
	for (int i = 0; i < 1000; i++)
	{
		bcstr_append(&fairlylongstring, "abcde");
	}

	/* clean up directory in case a previous run was interrupted */
	check(os_removechildren(dir.s));
	check(os_removechildren(subdir.s));
	check(os_removechildren(subsubdir.s));
	TestTrue(os_remove(subsubdir.s));
	TestTrue(os_remove(subdir.s));
	check_fatal(os_create_dir(dir.s), "could not create test dir");

	{ /* test write file*/
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestTrue(os_file_exists(path.s));
		TestEqn(4, os_getfilesize(path.s));
		TestTrue(os_remove(path.s));
	}
	{ /* test read file */
		check(bcfile_writefile(path.s, "abcd", "w"));
		check(bcfile_readfile(path.s, &content, "r"));
		TestEqs("abcd", content.s);
		TestEqn(strlen("abcd"), bcstr_length(&content));
		TestTrue(os_remove(path.s));
	}
	{ /* reading a fairly long string, string.s ptr should be correct*/
		check(bcfile_writefile(path.s, fairlylongstring.s, "w"));
		check(bcfile_readfile(path.s, &content, "r"));
		TestEqs(fairlylongstring.s, content.s);
		TestEqn(1000*strlen("abcde"), bcstr_length(&fairlylongstring));
		TestEqn(1000*strlen("abcde"), bcstr_length(&content));
		TestTrue(os_remove(path.s));
	}
	{ /* os_file_exists */
		TestTrue(os_create_dir(subdir.s));
		TestTrue(!os_file_exists(path.s)); /* non existent file */
		TestTrue(!os_file_exists("d" pathsep "not_exist")); /* non existent file */
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestTrue(os_file_exists(path.s)); /* existent file */
		TestTrue(!os_file_exists(subdir.s)); /* existent dir */
		TestTrue(os_remove(path.s));
		TestTrue(os_remove(subdir.s));
	}
	{ /* os_dir_exists */
		TestTrue(os_create_dir(subdir.s));
		TestTrue(!os_dir_exists(path.s)); /* non existent file */
		TestTrue(!os_dir_exists("d" pathsep "not_exist")); /* non existent file */
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestTrue(!os_dir_exists(path.s)); /* existent file */
		TestTrue(os_dir_exists(subdir.s)); /* existent dir */
		TestTrue(os_remove(path.s));
		TestTrue(os_remove(subdir.s));
	}
	{ /* os_remove should work on a file */
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestTrue(os_file_exists(path.s));
		TestTrue(os_remove(path.s));
		TestTrue(!os_file_exists(path.s));
	}
	{ /* os_remove returns true for non-existent file */
		TestTrue(!os_file_exists(path.s));
		TestTrue(os_remove(path.s));
		TestTrue(os_remove("d" pathsep "not_exist"));
	}
	{ /* os_remove should work on an empty directory */
		TestTrue(os_create_dir(subdir.s));
		TestTrue(os_dir_exists(subdir.s));
		TestTrue(os_remove(subdir.s));
		TestTrue(!os_dir_exists(subdir.s));
	}
	{ /* os_remove should not work on a non-empty directory */
		TestTrue(os_create_dir(subdir.s));
		TestTrue(os_dir_exists(subdir.s));
		check(bcfile_writefile(subdirfile.s, "abcd", "w"));
		TestTrue(!os_remove(subdir.s));
		TestTrue(os_dir_exists(subdir.s));
		TestTrue(os_remove(subdirfile.s));
		TestTrue(os_remove(subdir.s));
		TestTrue(!os_dir_exists(subdir.s));
	}
	{ /* os_copy should fail if src not exist */
		check(os_removechildren(dir.s));
		TestTrue(!os_copy(path.s, path2.s, true));
		TestTrue(!os_copy("///invalid", path2.s, true));
	}
	{ /* os_copy should fail if dest is unaccessible */
		check(os_removechildren(dir.s));
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestTrue(!os_copy(path.s, "///invalid", true));
		TestTrue(!os_copy(path.s, "_not_exist_" pathsep "a", true));
	}
	{ /* os_copy should succeed if target does not exist */
		check(os_removechildren(dir.s));
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestTrue(os_copy(path.s, path2.s, false));
		TestTrue(os_file_exists(path2.s));
	}
	{ /* os_copy should fail if overwrite disallowed and target exists */
		check(os_removechildren(dir.s));
		check(bcfile_writefile(path.s, "abcd", "w"));
		check(bcfile_writefile(path2.s, "defg", "w"));
		TestTrue(!os_copy(path.s, path2.s, false));
		check(bcfile_readfile(path2.s, &content, "r"));
		TestEqs("defg", content.s);
	}
	{ /* os_copy should succeed if overwrite allowed and target exists */
		check(os_removechildren(dir.s));
		check(bcfile_writefile(path.s, "abcd", "w"));
		check(bcfile_writefile(path2.s, "defg", "w"));
		TestTrue(os_copy(path.s, path2.s, true));
		check(bcfile_readfile(path2.s, &content, "r"));
		TestEqs("abcd", content.s);
	}
	{ /* os_getfilesize on typical files*/
		check(bcfile_writefile(path.s, "abcd", "w"));
		TestEqn(strlen("abcd"), os_getfilesize(path.s));
		check(bcfile_writefile(path.s, fairlylongstring.s, "w"));
		TestEqn(strlen(fairlylongstring.s), os_getfilesize(path.s));
		TestTrue(os_remove(path.s));
	}
	{ /* os_getfilesize on zero-length or non-existent files */
		check(bcfile_writefile(path.s, "", "w"));
		TestEqn(strlen(""), os_getfilesize(path.s));
		TestEqn(0, os_getfilesize("d" pathsep "not_exist"));
		TestTrue(os_remove(path.s));
	}
	{ /* os_split_dir with many subdirs */
		char testpath[] = pathsep "test" pathsep "one" pathsep "two";
		char *dir=NULL, *filename=NULL;
		os_split_dir(testpath, &dir, &filename);
		TestEqs(pathsep "test" pathsep "one", dir);
		TestEqs("two", filename);
	}
	{ /* os_split_dir with one subdir */
		char testpath[] = pathsep "test";
		char *dir=NULL, *filename=NULL;
		os_split_dir(testpath, &dir, &filename);
		TestEqs("", dir);
		TestEqs("test", filename);
	}
	{ /* os_split_dir with no subdir */
		char testpath[] = "test";
		char *dir=NULL, *filename=NULL;
		os_split_dir(testpath, &dir, &filename);
		TestEqs("", dir);
		TestEqs("test", filename);
	}
	{ /* splitting just name with two subdirs should give correct length */
		char testpath[] = pathsep "test" pathsep "one" pathsep "two";
		const char *filename = NULL;
		Uint32 filenamelen = 0;
		os_split_getfilename(testpath, strlen32(testpath), &filename, &filenamelen);
		TestEqs("two", filename);
		TestEqn(strlen32(filename), filenamelen);
	}
	{ /* splitting just name with no subdirs should give correct length */
		char testpath[] = "two";
		const char *filename = NULL;
		Uint32 filenamelen = 0;
		os_split_getfilename(testpath, strlen32(testpath), &filename, &filenamelen);
		TestEqs("two", filename);
		TestEqn(strlen32(filename), filenamelen);
	}
	{ /* splitting with two subdirs should give correct lengths */
		char testpath[] = pathsep "test" pathsep "one" pathsep "two";
		char *dir=NULL, *filename=NULL;
		Uint32 dirlen=0, filenamelen=0;
		os_split_dir_len(testpath, strlen32(testpath), &dir, &dirlen, &filename, &filenamelen);
		TestEqs(pathsep "test" pathsep "one", dir);
		TestEqs("two", filename);
		TestEqn(strlen32(dir), dirlen);
		TestEqn(strlen32(filename), filenamelen);
	}
	{ /* splitting with one subdirs should give correct lengths */
		char testpath[] = pathsep "test" pathsep "one";
		char *dir=NULL, *filename=NULL;
		Uint32 dirlen=0, filenamelen=0;
		os_split_dir_len(testpath, strlen32(testpath), &dir, &dirlen, &filename, &filenamelen);
		TestEqs(pathsep "test", dir);
		TestEqs("one", filename);
		TestEqn(strlen32(dir), dirlen);
		TestEqn(strlen32(filename), filenamelen);
	}
	{ /* os_removechildren should not recurse into subdirs */
		/* make files ./readwrite.txt, ./subdir/file */
		TestTrue(os_create_dir(subdir.s));
		check(bcfile_writefile(subdirfile.s, "abcd", "w"));
		check(bcfile_writefile(path.s, "abcd", "w"));

		check(os_removechildren(dir.s));
		TestTrue(!os_file_exists(path.s));
		TestTrue(os_file_exists(subdirfile.s));
		TestTrue(os_remove(subdirfile.s));
		TestTrue(os_remove(subdir.s));
	}
	{ /* os_listdirfiles should not recurse into subdirs*/
		/* make files ./readwrite.txt, ./subdir/file */
		TestTrue(os_create_dir(subdir.s));
		check(bcfile_writefile(subdirfile.s, "abcd", "w"));
		check(bcfile_writefile(path.s, "abcd", "w"));

		check(os_listdirfiles(dir.s, &seen, true));
		TestEqslist(&seen, path.s);
		check(os_removechildren(dir.s));
		TestTrue(os_remove(subdirfile.s));
		TestTrue(os_remove(subdir.s));
	}
	{ /* os_listdirfiles common usage */
		check(os_listdirfiles(dir.s, &seen, true));
		TestEqn(0, seen.length);

		check(bcfile_writefile(path.s, "abcd", "w"));
		check(os_listdirfiles(dir.s, &seen, true));
		TestEqslist(&seen, path.s);

		check(bcfile_writefile(path2.s, "defg", "w"));
		check(os_listdirfiles(dir.s, &seen, true));
		TestEqslist(&seen, path.s, path2.s);
		check(os_removechildren(dir.s));
	}
	{ /* recursion */
		TestTrue(os_create_dir(subdir.s));
		TestTrue(os_create_dir(subsubdir.s));
		check(bcfile_writefile(path.s, "1", "w"));
		check(bcfile_writefile(path2.s, "12", "w"));
		check(bcfile_writefile(subdirfile.s, "123", "w"));
		check(bcfile_writefile(subdirfile2.s, "1234", "w"));
		check(bcfile_writefile(subsubdirfile.s, "12345", "w"));

		/* run recurse with low maxdepth*/
		Results_testosrecurse results = {};
		int symskipped = 0;
		g_log.suppress_dialogs = true;
		Result err = os_recurse(&results, dir.s, &symskipped, 1 /*maxdepth*/, testosrecurse_callback);
		g_log.suppress_dialogs = false;
		TestErrMsgAndClose(err, "recursion limit");
		TestEqn(4, results.index);

		/* run recurse and get everything*/
		results = CAST(Results_testosrecurse) {};
		check(os_recurse(&results, dir.s, &symskipped, 1000 /*maxdepth*/, testosrecurse_callback));
		TestEqn(5, results.index);

		/* sort the results, since traversal order not guarenteed*/
		Uint32 elementsize = sizeof(results.entries[0]);
		qsort_r(&results.entries[0], results.index, 
			sizeof(results.entries[0]), &bcarray_sort_memcmp, &elementsize);

		TestEqs(path.s, results.entries[0].filepath);
		TestEqs(subdirfile.s, results.entries[1].filepath);
		TestEqs(subdirfile2.s, results.entries[2].filepath);
		TestEqs(subsubdirfile.s, results.entries[3].filepath);
		TestEqs(path2.s, results.entries[4].filepath);
		TestEqn(1, results.entries[0].filesize);
		TestEqn(3, results.entries[1].filesize);
		TestEqn(4, results.entries[2].filesize);
		TestEqn(5, results.entries[3].filesize);
		TestEqn(2, results.entries[4].filesize);
		TestEqn(0, results.entries[0].dirnumber);
		TestEqn(1, results.entries[1].dirnumber);
		TestEqn(1, results.entries[2].dirnumber);
		TestEqn(2, results.entries[3].dirnumber);
		TestEqn(0, results.entries[4].dirnumber);
		for (int i=0; i<results.index; i++)
		{
			TestEqn(strlen(results.entries[i].filepath), results.entries[i].filepathlen);

#ifdef __linux__
			/* nativepath is a nul-terminated char string*/
			TestEqn(results.entries[i].filepathlen, 
				results.entries[i].cbnativepathlen * sizeof(char));
			TestEqn(results.entries[i].cbnativepathlen,
				strlen((const char*)results.entries[i].nativepath) * sizeof(char));

			/* nativepath is same as filepath*/
			TestTrue(s_equal(results.entries[i].filepath, 
				(const char*)results.entries[i].nativepath));
#elif _WIN32
			/* nativepath is a nul-terminated WCHAR string*/
			TestEqn(results.entries[i].cbnativepathlen, 
				results.entries[i].filepathlen * sizeof(WCHAR));
			TestEqn(results.entries[i].cbnativepathlen,
				wcslen((const WCHAR*)results.entries[i].nativepath) * sizeof(WCHAR));

			/* nativepath is utf16, filepath is utf8 */
			bcwstr wide = bcwstr_openwz(L"");
			ascii_to_bcwstr(results.entries[i].filepath, 
				results.entries[i].filepathlen, &wide);
			TestTrue(wcscmp(wide.s, 
				(const WCHAR*)results.entries[i].nativepath) == 0);
			bcwstr_close(&wide);
#endif
		}

		/* cleanup */
		check(os_removechildren(subsubdir.s));
		check(os_removechildren(subdir.s));
		check(os_removechildren(dir.s));
		TestTrue(os_remove(subsubdir.s));
		TestTrue(os_remove(subdir.s));
	}

cleanup:
	bcstr_close(&dir);
	bcstr_close(&path);
	bcstr_close(&path2);
	bcstr_close(&subdir);
	bcstr_close(&subsubdir);
	bcstr_close(&subsubdirfile);
	bcstr_close(&subdirfile);
	bcstr_close(&subdirfile2);
	bcstr_close(&content);
	bcstr_close(&fairlylongstring);
	bcstrlist_close(&seen);
	print_error_if_ocurred("", &currenterr);
	TestTrue(currenterr.errortag == 0);
}

void test_string_w()
{
	bcstr narrow = bcstr_open();
	bcwstr wide = bcwstr_openwz(L"");

	{ /*wide strings should be able to append */
		bcwstr str = bcwstr_openwz(L"");
		TestTrue(wcscmp(L"", str.s)==0);
		bcwstr_appendwz(&str, L"abcd");
		TestTrue(wcscmp(L"abcd", str.s)==0);
		bcwstr_close(&str);
	}
	{ /*wide strings should be able to clear */
		bcwstr str = bcwstr_openwz(L"abc");
		TestTrue(wcscmp(L"abc", str.s)==0);
		bcwstr_clear(&str);
		TestTrue(wcscmp(L"", str.s)==0);
		bcwstr_close(&str);
	}
#ifdef _WIN32
	{ /* wide_to_utf8 on common string */
		wide_to_utf8(L"abcd", 4, &narrow);
		TestEqs("abcd", narrow.s);
		TestEqn(4, bcstr_length(&narrow));
	}
	{ /* wide_to_utf8 on part of string */
		wide_to_utf8(L"abcd", 2, &narrow);
		TestEqs("ab", narrow.s);
		TestEqn(2, bcstr_length(&narrow));
	}
	{ /* wide_to_utf8 on empty string */
		wide_to_utf8(L"", 0, &narrow);
		TestEqs("", narrow.s);
		TestEqn(0, bcstr_length(&narrow));
	}
	{ /* append works on string from wide_to_utf8 */
		wide_to_utf8(L"abcd", 4, &narrow);
		bcstr_append(&narrow, "ABC");
		bcstr_append(&narrow, "012");
		TestEqs("abcdABC012", narrow.s);
	}
	{ /* wide_to_utf8 with a 2-byte sequence */
		WCHAR abc_accentedE[] = {L'a', L'b', L'c', 0xE9, L'\0'};
		wide_to_utf8(abc_accentedE, wcslen32(abc_accentedE), &narrow);
		TestEqn(5, bcstr_length(&narrow));
		TestEqn('a', narrow.s[0]);
		TestEqn((char)0xC3, narrow.s[3]);
		TestEqn((char)0xA9, narrow.s[4]);
		TestEqn('\0', narrow.s[5]);
	}
	{ /* wide_to_utf8 with a 3-byte sequence */
		WCHAR abc_hangul[] = {L'a', 0x1101, L'\0'};
		wide_to_utf8(abc_hangul, wcslen32(abc_hangul), &narrow);
		TestEqn(4, bcstr_length(&narrow));
		TestEqn('a', narrow.s[0]);
		TestEqn((char)0xE1, narrow.s[1]);
		TestEqn((char)0x84, narrow.s[2]);
		TestEqn((char)0x81, narrow.s[3]);
		TestEqn('\0', narrow.s[4]);
	}
	{ /* wide_to_utf8 with a 4-byte sequence (musical symbol g clef) */
		WCHAR abc_symbol[] = {L'a', 0xD834, 0xDD1E, L'\0'};
		wide_to_utf8(abc_symbol, wcslen32(abc_symbol), &narrow);
		TestEqn(5, bcstr_length(&narrow));
		TestEqn('a', narrow.s[0]);
		TestEqn((char)0xF0, narrow.s[1]);
		TestEqn((char)0x9D, narrow.s[2]);
		TestEqn((char)0x84, narrow.s[3]);
		TestEqn((char)0x9E, narrow.s[4]);
		TestEqn('\0', narrow.s[5]);
	}
	{ /* ascii_to_bcwstr on common string */
		ascii_to_bcwstr("abcd", 4, &wide);
		TestTrue(wcscmp(L"abcd", wide.s) == 0);
		TestEqn(4, wide.buffer.length-1);
	}
	{ /* ascii_to_bcwstr on part of string */
		ascii_to_bcwstr("abcd", 2, &wide);
		TestTrue(wcscmp(L"ab", wide.s) == 0);
		TestEqn(2, wide.buffer.length-1);
	}
	{ /* ascii_to_bcwstr on empty string */
		ascii_to_bcwstr("", 0, &wide);
		TestTrue(wcscmp(L"", wide.s) == 0);
		TestEqn(0, wide.buffer.length-1);
	}
	{ /* ascii_to_bcwstr should not accept extended ascii */
		ascii_to_bcwstr("abc\x82", 0, &wide);
		TestTrue(wcscmp(L"", wide.s) == 0);
		TestEqn(0, wide.buffer.length-1);
	}
	{ /* ascii_to_bcwstr should not accept utf8 */
		ascii_to_bcwstr("abc\xC3\xA9", 0, &wide);
		TestTrue(wcscmp(L"", wide.s) == 0);
		TestEqn(0, wide.buffer.length-1);
	}
#endif

	bcstr_close(&narrow);
	bcwstr_close(&wide);
}

void test_database()
{
	{ /* bcqry_runf_nextchr should return nul when end reached */
		Uint32 i = 0;
		TestEqn('a', bcqry_runf_nextchr(&i, safecast32u(strlen("ab")), "ab"));
		TestEqn('b', bcqry_runf_nextchr(&i, safecast32u(strlen("ab")), "ab"));
		TestEqn('\0', bcqry_runf_nextchr(&i, safecast32u(strlen("ab")), "ab"));
		TestEqn('\0', bcqry_runf_nextchr(&i, safecast32u(strlen("ab")), "ab"));
		TestEqn('\0', bcqry_runf_nextchr(&i, safecast32u(strlen("ab")), "ab"));
	}
}

void runtests()
{
	bcstr testdir = os_getthisprocessdir();
	bcstr_append(&testdir, pathsep, "tests");
	check_fatal(os_create_dir(testdir.s), "could not create test dir");
	check_fatal(os_removechildren(testdir.s).errortag == 0);

	test_dynamicarray();
	test_dynamicarraysort();
	test_basicarithmatic();
	test_basic();
	test_string();
	test_stringlist();
	test_getfileextension();
	test_logging(testdir.s);
	test_readconfigs(testdir.s);
	test_os_util(testdir.s);
	test_string_w();
	test_database();

	puts("Complete.");
	bcstr_close(&testdir);
	TestTrue(!g_log.suppress_dialogs);
}
