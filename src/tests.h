#pragma once

bool test_dlg(bool cond, const char* format, ...)
{
	if (cond)
	{
		return true;
	}

	bcstr s = bcstr_init();
	va_list args;
	va_start(args, format);
	bcstr_addvfmt(&s, format, args);
	va_end(args);
	alertdialog(s.s);
	bcstr_close(&s);
	return false;
}

/* use macros in order to conveniently break into the debugger on the same line. */
#define TestEqs(s1, s2) do { \
	if (!test_dlg(s_equal((s1), (s2)), "expected %s but got %s.", \
		(s1), (s2))) { \
		DEBUGBREAK(); \
		exit(1); \
	} } while(0)

#define TestEqn(n1, n2) do { \
	if (!test_dlg((n1) == (n2), "expected %lld but got %lld.", \
		(Int64)(n1), (Int64)(n2))) { \
		DEBUGBREAK(); \
		exit(1); \
	} } while(0)

#define TestTrue(cond) do { \
	if (!test_dlg((cond), "testtrue failed \"%s\"", #cond)) { \
		DEBUGBREAK(); \
		exit(1); \
	} } while(0)

/* use var-args syntax for an elegant way to compare string arrays */
#define TestEqsarr(par_received, ...) do { \
	bc_strarray ar_expected = bc_strarray_initwith(__VA_ARGS__);  \
	bcstr srec = bc_strarray_tostring(par_received); \
	bcstr sexpt = bc_strarray_tostring(&ar_expected); \
	TestEqs(sexpt.s, srec.s); \
	bcstr_close(&srec); \
	bcstr_close(&sexpt); \
	bc_strarray_close(&ar_expected); \
	} while (0)

void test_dynamicarray()
{
	Uint64 testBuffer[10] = {};

	{ /* we shouldn't allocate when told to start with 0 elements */
		bcarray arr = bcarray_init(sizeof32(Uint64), 0);
		TestEqn(NULL, arr.buffer);
		TestEqn(sizeof32(Uint64), arr.elementsize);
		TestEqn(0, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_close(&arr);
	}
	{ /* we should allocate */
		bcarray arr = bcarray_init(sizeof32(Uint64), 3);
		TestTrue(NULL != arr.buffer);
		TestEqn(sizeof32(Uint64), arr.elementsize);
		TestEqn(3, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_close(&arr);
	}
	{ /* capacity should remain at initial value until < length, and then double. */
		bcarray arr = bcarray_init(sizeof32(Uint64), 3);
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
	{ /* reserve space when empty.*/
		bcarray arr = bcarray_init(sizeof32(Uint64), 0);
		TestTrue(NULL == arr.buffer);
		TestEqn(0, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_reserve(&arr, 15);
		TestTrue(NULL != arr.buffer);
		TestEqn(16, arr.capacity);
		TestEqn(0, arr.length);
		bcarray_close(&arr);
	}
	{ /* reserve space when has content */
		bcarray arr = bcarray_init(sizeof32(Uint64), 0);
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
		bcarray arr = bcarray_init(sizeof32(byte), 0);
		bcarray_addzeros(&arr, 15);
		TestEqn(16, arr.capacity);
		TestEqn(15, arr.length);
		for (Uint32 i=0; i<arr.length; i++)
			TestEqn(0, *bcarray_at(&arr, i));

		bcarray_close(&arr);
	}
	{ /* test contents*/
		bcarray arr = bcarray_init(sizeof32(Uint64), 3);
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
		bcarray arr = bcarray_init(sizeof32(Uint64), 0);
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
		bcarray arr = bcarray_init(sizeof32(Uint64), 0);
		bcarray_add64u(&arr, 123);
		TestEqn(123, *bcarray_at64u(&arr, 0));
		bcarray_add64u(&arr, 456);
		TestEqn(456, *bcarray_at64u(&arr, 1));
		bcarray_close(&arr);
	}
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

	/* test error handling macro*/
	Result currenterr = {};
	g_log.suppress_dialogs = true;
	check_or_goto_cleanup_msg_tag(1234, false, "an integer is %d.", 3);

	/* this line shouldn't be hit*/
	TestTrue(false);

cleanup:
	/* the error handling macro will set the fields of currenterr*/
	g_log.suppress_dialogs = false;
	TestEqn(1234, currenterr.errortag);
	TestTrue(s_contains(currenterr.errormsg, "an integer is 3."));
	Result_close(&currenterr);
}

void test_string()
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

	{ /* test initempty creates a valid empty string*/
		bcstr str = bcstr_init();
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test init can create empty string*/
		bcstr str = bcstr_initwith("");
		bcstr_appendsz(&str, "");
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test init with ordinary string*/
		bcstr str = bcstr_initwith("abcde");
		TestEqs("abcde", str.s);
		TestEqn(5, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test init with many ordinary strings*/
		bcstr str = bcstr_initwith("aa", "B", "cc", "d");
		TestEqs("aaBccd", str.s);
		TestEqn(6, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test append with many strings, some empty strings*/
		bcstr str = bcstr_initwith(""); 
		bcstr_append(&str, "", "aa", "B");
		TestEqs("aaB", str.s);
		TestEqn(3, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test append with many strings, all empty strings*/
		bcstr str = bcstr_initwith("");
		bcstr_append(&str, "", "", "", "");
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_append(&str, "", "", "", "");
		TestEqs("", str.s);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test that clear() sets to empty string*/
		bcstr str = bcstr_initwith("abc");
		TestEqs("abc", str.s);
		bcstr_clear(&str);
		TestEqs("", str.s);
		TestEqn(1, str.buffer.length /*includes nul term*/);
		TestEqn(0, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test that assign() clears previous content*/
		bcstr str = bcstr_initwith("abc");
		TestEqs("abc", str.s);
		bcstr_assign(&str, "def");
		TestEqs("def", str.s);
		TestEqn(4, str.buffer.length /*includes nul term*/);
		TestEqn(3, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test addchar() */
		bcstr str = bcstr_initwith("abc");
		bcstr_addchar(&str, 'D');
		bcstr_addchar(&str, 'E');
		TestEqs("abcDE", str.s);
		TestEqn(5, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing a uint64 0 */
		bcstr str = bcstr_init();
		int count = bcstr_addfmt(&str, "%llu", 0ULL);
		TestEqs("0", str.s);
		TestEqn(count, bcstr_length(&str));
		TestEqn(1, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing a large uint64 */
		bcstr str = bcstr_init();
		int count = bcstr_addfmt(&str, "%llu", 12345678987654321ULL);
		TestEqs("12345678987654321", str.s);
		TestEqn(count, bcstr_length(&str));
		TestEqn(17, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test printing some formatted stuff */
		bcstr str = bcstr_init();
		int count = bcstr_addfmt(&str, "there is %d, %s, and hex %x.", 12, "abc", 0xf00);
		TestEqs("there is 12, abc, and hex f00.", str.s);
		TestEqn(count, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /* test that addfmt() appends correctly */
		bcstr str = bcstr_initwith("abc");
		int count = strlen32("abc") + bcstr_addfmt(&str, "def");
		count += bcstr_addfmt(&str, "ghi123456789123456789");
		TestEqs("abcdefghi123456789123456789", str.s);
		TestEqn(count, bcstr_length(&str));
		bcstr_close(&str);
	}
	{ /*wide strings should be able to append */
		bcwstr str = bcwstr_initwz(L"");
		TestTrue(wcscmp(L"", str.s)==0);
		bcwstr_appendwz(&str, L"abcd");
		TestTrue(wcscmp(L"abcd", str.s)==0);
		bcwstr_close(&str);
	}
	{ /*wide strings should be able to clear */
		bcwstr str = bcwstr_initwz(L"abc");
		TestTrue(wcscmp(L"abc", str.s)==0);
		bcwstr_clear(&str);
		TestTrue(wcscmp(L"", str.s)==0);
		bcwstr_close(&str);
	}
}

void test_stringutils()
{
	{ /* int from string */
		int n = 0;
		TestTrue(s_intfromstring("12", &n) && n == 12);
		TestTrue(!s_intfromstring("", &n) && n == 0);
		TestTrue(s_intfromstring("0", &n) && n == 0);
		TestTrue(!s_intfromstring("12a", &n) && n == 0);
		TestTrue(!s_intfromstring("abc", &n) && n == 0);
		TestTrue(!s_intfromstring(" ", &n) && n == 0);
	}
	{ /* simple strarray example */
		bc_strarray sarr = bc_strarray_initwith("a", "b", "c");
		bcstr str = bc_strarray_tostring(&sarr);
		TestEqs("length=3\nindex 0:'a'\nindex 1:'b'\nindex 2:'c'\n", str.s);
		bcstr_close(&str);
		bc_strarray_close(&sarr);
	}
	{ /* manually retrieve from strarray*/
		bc_strarray sarr = bc_strarray_init();
		bc_strarray_pushback(&sarr, "first");
		bc_strarray_pushback(&sarr, "");
		bc_strarray_pushback(&sarr, "third");

		bcstr received = bcstr_init();
		bc_strarray_at(&sarr, 0, &received);
		TestEqs("first", received.s);
		bc_strarray_at(&sarr, 1, &received);
		TestEqs("", received.s);
		bc_strarray_at(&sarr, 2, &received);
		TestEqs("third", received.s);

		bcstr_close(&received);
		bc_strarray_close(&sarr);
	}
	{ /* push and pop from array*/
		bc_strarray sarr = bc_strarray_init();
		TestEqn(0, bc_strarray_length(&sarr));
		bc_strarray_pushback(&sarr, "a");
		TestEqn(1, bc_strarray_length(&sarr));
		bc_strarray_pushback(&sarr, "b");
		TestEqn(2, bc_strarray_length(&sarr));

		bcstr pop1 = bc_strarray_pop(&sarr);
		TestEqs("b", pop1.s);
		TestEqn(1, bc_strarray_length(&sarr));
		bcstr pop2 = bc_strarray_pop(&sarr);
		TestEqs("a", pop2.s);
		TestEqn(0, bc_strarray_length(&sarr));

		bcstr_close(&pop1);
		bcstr_close(&pop2);
		bc_strarray_close(&sarr);
	}
	{ /*clear should remove all elments*/
		bc_strarray sarr = bc_strarray_init();
		bc_strarray_pushback(&sarr, "first");
		bc_strarray_pushback(&sarr, "second");
		TestEqn(2, bc_strarray_length(&sarr));
		bc_strarray_clear(&sarr);
		TestEqn(0, bc_strarray_length(&sarr));
		bc_strarray_close(&sarr);
	}
	{ /* strarray to string */
		bc_strarray sarr1 = bc_strarray_initwith("first", "", "third");
		bcstr str1 = bc_strarray_tostring(&sarr1);
		TestEqsarr(&sarr1, "first", "", "third");
		TestEqs("length=3\nindex 0:'first'\nindex 1:''\nindex 2:'third'\n", str1.s);

		bc_strarray sarr2 = bc_strarray_initwith("", "");
		bcstr str2 = bc_strarray_tostring(&sarr2);
		TestEqsarr(&sarr2, "", "");
		TestEqs("length=2\nindex 0:''\nindex 1:''\n", str2.s);

		bcstr_close(&str1);
		bcstr_close(&str2);
		bc_strarray_close(&sarr1);
		bc_strarray_close(&sarr2);
	}
	{ /* test sorting*/
		bc_strarray sarr = bc_strarray_initwith("apple2", "apple1", "pear", "peach");
		TestEqn(4, bc_strarray_length(&sarr));
		bc_strarray_sort(&sarr);
		TestEqsarr(&sarr, "apple1", "apple2", "peach", "pear");
		bc_strarray_close(&sarr);
	}
	{ /* test split common cases*/
		bc_strarray sarr = bc_strarray_init();
		bc_strarray_add_str_split(&sarr, "a|bb|c", '|');
		TestEqsarr(&sarr, "a", "bb", "c");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "a||b|c", '|');
		TestEqsarr(&sarr, "a", "", "b", "c");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "a||b|c", '|');
		TestEqsarr(&sarr, "a", "", "b", "c");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "a||b|c", '?');
		TestEqsarr(&sarr, "a||b|c");
		bc_strarray_clear(&sarr);

		bc_strarray_close(&sarr);
	}
	{ /* test split less common cases*/
		bc_strarray sarr = bc_strarray_init();
		bc_strarray_add_str_split(&sarr, "", '|');
		TestEqsarr(&sarr, "");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "|", '|');
		TestEqsarr(&sarr, "", "");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "||", '|');
		TestEqsarr(&sarr, "", "", "");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "a |b\r\n|c\n|", '|');
		TestEqsarr(&sarr, "a ", "b\r\n", "c\n", "");
		bc_strarray_clear(&sarr);

		bc_strarray_add_str_split(&sarr, "||a||b|c||", '|');
		TestEqsarr(&sarr, "", "", "a", "", "b", "c", "", "");
		bc_strarray_clear(&sarr);

		bc_strarray_close(&sarr);
	}
}

void runtests()
{
	test_basic();
	test_basicarithmatic();
	test_dynamicarray();
	test_string();
	test_stringutils();

	puts("Complete.");

#if _WIN32
	if (IsDebuggerPresent())
		system("PAUSE");
#endif

	TestTrue(!g_log.suppress_dialogs);
}
