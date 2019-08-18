/*
tests.h

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef TESTS_H_INCLUDE
#define TESTS_H_INCLUDE

#include "../operations.h"

bool testintimpl(int lineno, unsigned long long n1, unsigned long long n2);
bool teststrimpl(int lineno, const char *s1, const char *s2);
bool testlistimpl(int lineno, const char *expected, const bstrlist *list);
bool testpslistimpl(int lineno, const char *expected, sv_pseudosplit *list);
void expect_err_with_message_impl(sv_result res, const char *msgcontains);
check_result tmpwritetextfile(
    const char *dir, const char *leaf, bstring fullpath, const char *contents);
void run_all_tests(void);

#define TestEqs(s1, s2)                         \
    do                                          \
    {                                           \
        if (!teststrimpl(__LINE__, (s1), (s2))) \
        {                                       \
            DEBUGBREAK();                       \
            exit(1);                            \
        }                                       \
    } while (0)

#define TestEqn(n1, n2)                                                        \
    do                                                                         \
    {                                                                          \
        if (!testintimpl(                                                      \
                __LINE__, (unsigned long long)(n1), (unsigned long long)(n2))) \
        {                                                                      \
            DEBUGBREAK();                                                      \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

#define TestEqList(s1, list)                       \
    do                                             \
    {                                              \
        if (!testlistimpl(__LINE__, (s1), (list))) \
        {                                          \
            DEBUGBREAK();                          \
            exit(1);                               \
        }                                          \
    } while (0)

#define TestEqPsList(s1, list)                       \
    do                                               \
    {                                                \
        if (!testpslistimpl(__LINE__, (s1), (list))) \
        {                                            \
            DEBUGBREAK();                            \
            exit(1);                                 \
        }                                            \
    } while (0)

#define TestEqFloat(expected, got) TestTrue(fabs((expected) - (got)) < 1e-6)

#define TestTrue(cond) TestEqn(1, (cond) != 0)

#define expect_err_with_message(expr, msgcontains)      \
    do                                                  \
    {                                                   \
        quiet_warnings(true);                           \
        sv_result r = (expr);                           \
        quiet_warnings(false);                          \
        expect_err_with_message_impl(r, (msgcontains)); \
    } while (0)

#ifdef _MSC_VER
#define UNREFERENCED_LABEL_OK_BEGIN() \
    __pragma(warning(push)) __pragma(warning(disable : 4102))

#define UNREFERENCED_LABEL_OK_END() __pragma(warning(pop))
#else
#define UNREFERENCED_LABEL_OK_BEGIN()
#define UNREFERENCED_LABEL_OK_END()
#endif

#define SV_BEGIN_TEST_SUITE(suitename)                                        \
    UNREFERENCED_LABEL_OK_BEGIN()                                             \
    void suitename(const char *tempdir)                                       \
    {                                                                         \
        const char *currentfailmsg = "unit test failed in suite " #suitename; \
        const char *currentpassmsg = "%d passed in " #suitename "\n";         \
        const char *currentcontext = "";                                      \
        extern uint32_t sleep_between_tries;                                  \
        uint32_t countrun = 0;                                                \
        sleep_between_tries = 1;                                              \
        sv_result currenterr = {};                                            \
        check(tests_cleardir(tempdir));

#define SV_END_TEST_SUITE()                              \
    currentcontext = "";                                 \
    cleanup:                                             \
    if (currentcontext && currentcontext[0])             \
    {                                                    \
        printf("context: %s\n", currentcontext);         \
    }                                                    \
    check_warn(currenterr, currentfailmsg, exit_on_err); \
    printf(currentpassmsg, countrun);                    \
    UNREFERENCED_LABEL_OK_END()                          \
    }

#define SV_TEST(tname)      \
    currentcontext = tname; \
    countrun++;

#define SV_TEST_() SV_TEST("")

#if __linux__
#define SV_TEST_WIN(s) if (0)
#define SV_TEST_WIN_() if (0)
#define SV_TEST_LIN(s) SV_TEST(s)
#define SV_TEST_LIN_() SV_TEST_()
#elif _WIN32
#define SV_TEST_WIN(s) SV_TEST(s)
#define SV_TEST_WIN_() SV_TEST_()
#define SV_TEST_LIN(s) if (0)
#define SV_TEST_LIN_() if (0)
#else
#error platform not yet supported
#endif

/* because this is test-only code, ok to leak small values */
#define SV_TEST_STR(v) bstring v = bstring_open()

/* because this is test-only code, ok to leak small values */
#define TEST_OPEN_EX(type, varname, opener) type varname = opener

/* because this is test-only code, ok to leak small values */
#define TEST_OPEN(type, varname) type varname = CONCAT(type, _open)()

/* because this is test-only code, ok to leak small values */
#define TEST_OPENA(type, varname, ...) \
    type varname = CONCAT(type, _open)(__VA_ARGS__)

/* because this is test-only code, ok to leak small values */
#define TEST_OPEN2(type, varname1, varname2) \
    type varname1 = CONCAT(type, _open)();   \
    type varname2 = CONCAT(type, _open)()

/* because this is test-only code, ok to leak small values */
#define TEST_OPEN3(type, varname1, varname2, varname3) \
    type varname1 = CONCAT(type, _open)();             \
    type varname2 = CONCAT(type, _open)();             \
    type varname3 = CONCAT(type, _open)()

/* because this is test-only code, ok to leak small values */
#define TEST_OPEN4(type, varname1, varname2, varname3, varname4) \
    type varname1 = CONCAT(type, _open)();                       \
    type varname2 = CONCAT(type, _open)();                       \
    type varname3 = CONCAT(type, _open)();                       \
    type varname4 = CONCAT(type, _open)()

/* let tests access this internal function */
check_result svdb_runsql(svdb_db * self, const char *sql, int lensql, svdb_expectchanges confirm_changes);

typedef struct sv_test_hook
{
    os_lockedfilehandle filelocks[10];
    uint64_t setlastmodtimes[10];
    bstring filenames[10];
    bstring path_userfiles;
    bstring path_group;
    bstring path_readytoupload;
    bstring path_readytoremove;
    bstring path_restoreto;
    bstring path_untar;
    bstring path_tmp;
    bstrlist *allcontentrows;
    bstrlist *allfilelistrows;
    bstrlist *listexpectcontentrows;
    bstrlist *listexpectfilelistrows;
    const char *expectcontentrows;
    const char *expectfilerows;
    bool messwithfiles;
} sv_test_hook;

sv_test_hook sv_test_hook_open(const char *dir);
void sv_test_hook_close(sv_test_hook *self);
bstring tests_make_subdir(const char *parent, const char *leaf);
check_result tests_tar_list(
    ar_util *self, const char *tarpath, bstrlist *results);
check_result tests_check_tar_contents(
    ar_util *ar, const char *archive, const char *expected, bool check_order);
sv_result tests_cleardir(const char *tempdir);
sv_result tests_lockfile(
    bool lock, const char *path, os_lockedfilehandle *handle);

/*
[^:]+:[^:]+:SV_BEGIN_TEST_SUITE\(([^)]+)\)
to void \1(const char *tempdir);
*/
void tests_sv_array(const char *tempdir);
void tests_arithmetic(const char *tempdir);
void tests_2d_array(const char *tempdir);
void tests_open_db_connection(const char *tempdir);
void tests_path_handling(const char *tempdir);
void tests_aligned_malloc(const char *tempdir);
void tests_write_text_file(const char *tempdir);
void tests_file_operations(const char *tempdir);
void tests_bypattern(const char *tempdir);
void tests_file_locks(const char *tempdir);
void tests_recurse_corner_cases(const char *tempdir);
void tests_startprocess(const char *tempdir);
void tests_incrementing_filenames(const char *tempdir);
void tests_recurse_dir(const char *tempdir);
void tests_logging(const char *tempdir);
void tests_get_file_extension_category(const char *tempdir);
void tests_persist_user_configs(const char *tempdir);
void tests_find_groups(const char *tempdir);
void tests_c_string(const char *tempdir);
void tests_fnmatch(const char *tempdir);
void tests_string_bstr(const char *tempdir);
void tests_string_bstr_extensions(const char *tempdir);
void tests_string_conversions(const char *tempdir);
void tests_string_list(const char *tempdir);
void tests_string_pseudosplit(const char *tempdir);
void tests_widestrings(const char *tempdir);
void tests_tar(const char *tempdir);
void tests_xz(const char *tempdir);
void whole_tests_archive_filenames(const char *tempdir);
void tests_read_md5_string(const char *tempdir);
void tests_get_version(const char *tempdir);
void tests_hash_audio(const char *tempdir);
void tests_sync_cloud_standalone(const char *tempdir);
void whole_tests_db(const char *tempdir);
void whole_tests_operations(const char *tempdir);

#endif
