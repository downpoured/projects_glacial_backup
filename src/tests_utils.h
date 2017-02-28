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

#ifndef TESTS_UTILS_H_INCLUDE
#define TESTS_UTILS_H_INCLUDE

#include "operations.h"

bool testintimpl(int lineno, unsigned long long n1, unsigned long long n2);
bool teststrimpl(int lineno, const char *s1, const char *s2);
bool testlistimpl(int lineno, const char *expected, const bstrlist *list);
void expect_err_with_message(sv_result res, const char *msgcontains);
check_result tmpwritetextfile(const char *dir, const char *leaf, bstring fullpath, const char *contents);
check_result run_utils_tests(void);

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

#define TestEqList(s1, list) do { \
	if (!testlistimpl(__LINE__, (s1), (list))) { \
		DEBUGBREAK(); \
		exit(1); \
	} } while(0)

#define TestTrue(cond) \
	TestEqn(1, (cond)!=0)

#endif
