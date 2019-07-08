/*
tests_array_utils.c

GlacialBackup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GlacialBackup is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <math.h>
#include "tests.h"

SV_BEGIN_TEST_SUITE(tests_sv_array)
{
    SV_TEST("uint64 add one at a time") {
        TEST_OPENA(sv_array, a, sizeof32u(uint64_t), 0);
        sv_array_add64u(&a, 10);
        sv_array_add64u(&a, 20);
        sv_array_add64u(&a, 30);
        TestEqn(10, sv_array_at64u(&a, 0));
        TestEqn(20, sv_array_at64u(&a, 1));
        TestEqn(30, sv_array_at64u(&a, 2));
        TestEqn(3, a.length);
    }

    SV_TEST("add elements one at a time") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 0);
        int32_t n;
        n = 10;
        sv_array_append(&a, (byte *)&n, 1);
        n = 20;
        sv_array_append(&a, (byte *)&n, 1);
        n = 30;
        sv_array_append(&a, (byte *)&n, 1);
        TestEqn(10, *(int32_t *)sv_array_at(&a, 0));
        TestEqn(20, *(int32_t *)sv_array_at(&a, 1));
        TestEqn(30, *(int32_t *)sv_array_at(&a, 2));
        TestEqn(3, a.length);
    }

    SV_TEST("add elements two at a time") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 0);
        int32_t src[] = { 10, 20, 30 };
        sv_array_append(&a, (byte *)&src[0], 2);
        sv_array_append(&a, (byte *)&src[1], 2);
        TestEqn(10, *(const int32_t *)sv_array_atconst(&a, 0));
        TestEqn(20, *(const int32_t *)sv_array_atconst(&a, 1));
        TestEqn(20, *(const int32_t *)sv_array_atconst(&a, 2));
        TestEqn(30, *(const int32_t *)sv_array_atconst(&a, 3));
        TestEqn(4, a.length);
    }

    SV_TEST("skip allocation when initializing with no capacity") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 0);
        TestTrue(NULL == a.buffer);
        TestEqn(0, a.capacity);
        TestEqn(0, a.length);
    }

    SV_TEST("perform allocation when initializing with capacity") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        TestTrue(NULL != a.buffer);
        TestEqn(3, a.capacity);
        TestEqn(0, a.length);
    }

    SV_TEST("verify capacity, 1 item added") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 1;
        sv_array_append(&a, (byte *)&n, 1);
        TestEqn(3, a.capacity);
        TestEqn(1, a.length);
    }

    SV_TEST("verify capacity, 2 items added") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 1;
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        TestEqn(3, a.capacity);
        TestEqn(2, a.length);
    }

    SV_TEST("verify capacity, 3 items added") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 1;
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        TestEqn(3, a.capacity);
        TestEqn(3, a.length);
    }

    SV_TEST("verify capacity, 4 items added") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 1;
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        TestEqn(4, a.capacity);
        TestEqn(4, a.length);
    }

    SV_TEST("verify capacity, 5 items added") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 1;
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_append(&a, (byte *)&n, 1);
        TestEqn(8, a.capacity);
        TestEqn(5, a.length);
    }

    SV_TEST("uint64 add large values") {
        TEST_OPENA(sv_array, a, sizeof32u(uint64_t), 0);
        sv_array_add64u(&a, 10);
        sv_array_add64u(&a, 0xffffffff12341234ULL);
        sv_array_add64u(&a, 0x0000000156785678ULL);
        TestEqn(10, sv_array_at64u(&a, 0));
        TestEqn(0xffffffff12341234ULL, sv_array_at64u(&a, 1));
        TestEqn(0x0000000156785678ULL, sv_array_at64u(&a, 2));
        TestEqn(3, a.length);
    }

    SV_TEST("test truncate") {
        TEST_OPENA(sv_array, a, sizeof32u(uint64_t), 0);
        sv_array_add64u(&a, 10);
        sv_array_add64u(&a, 20);
        sv_array_add64u(&a, 30);
        sv_array_truncatelength(&a, 2);

        TestEqn(10, sv_array_at64u(&a, 0));
        TestEqn(20, sv_array_at64u(&a, 1));
        TestEqn(2, a.length);
        TestEqn(4, a.capacity);
    }

    SV_TEST("reserve capacity") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 10;
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_reserve(&a, 4000);
        TestEqn(4096, a.capacity);
        TestEqn(1, a.length);
        TestEqn(10, *(int32_t *)sv_array_at(&a, 0));
    }

    SV_TEST("add zeros, causing realloc") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 10;
        sv_array_append(&a, (byte *)&n, 1);
        sv_array_appendzeros(&a, 4000);
        TestEqn(4096, a.capacity);
        TestEqn(4001, a.length);
        TestEqn(10, *(int32_t *)sv_array_at(&a, 0));
        for (uint32_t i = 1; i < 4000; i++)
        {
            TestEqn(0, *(int32_t *)sv_array_at(&a, i));
        }
    }

    SV_TEST("add many items, causing realloc") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        for (int i = 0; i < 4000; i++)
        {
            sv_array_append(&a, (byte *)&i, 1);
        }

        for (uint32_t i = 0; i < 4000; i++)
        {
            TestEqn(i, *(int32_t *)sv_array_at(&a, i));
        }

        TestEqn(4096, a.capacity);
        TestEqn(4000, a.length);
    }

    SV_TEST("add from buffer, causing realloc") {
        TEST_OPENA(sv_array, a, sizeof32u(int32_t), 3);
        sv_array_truncatelength(&a, 0);
        int32_t n = 10;
        sv_array_append(&a, (byte *)&n, 1);
        int buffer[2000] = {};
        for (int i = 0; i < 2000; i++)
        {
            buffer[i] = i * 2;
        }

        sv_array_append(&a, (byte *)&buffer[0], countof(buffer));
        TestEqn(10, *(int32_t *)sv_array_at(&a, 0));
        for (uint32_t i = 0; i < 2000; i++)
        {
            TestEqn(i * 2, *(int32_t *)sv_array_at(&a, i + 1));
        }

        TestEqn(2048, a.capacity);
        TestEqn(2001, a.length);
    }
}
SV_END_TEST_SUITE()

SV_BEGIN_TEST_SUITE(tests_arithmetic)
{
    SV_TEST_() {
        TestEqn(2070, checkedmul32(45, 46));
    }

    SV_TEST_() {
        TestEqn(4294910784ULL, checkedmul32(123456, 34789));
    }

    SV_TEST_() {
        TestEqn(4294967295, checkedmul32(1, 4294967295));
    }

    SV_TEST_() {
        TestEqn(0, checkedmul32(0, 4294967295));
    }

    SV_TEST_() {
        TestEqn(690, checkedadd32(123, 567));
    }

    SV_TEST_() {
        TestEqn(4294967295, checkedadd32(2147483647, 2147483648));
    }

    SV_TEST_() {
        TestEqn(4294967295, checkedadd32(1, 4294967294));
    }

    SV_TEST_() {
        TestEqn(690, checkedadd32s(123, 567));
    }

    SV_TEST_() {
        TestEqn(2147483647, checkedadd32s(1073741824, 1073741823));
    }

    SV_TEST_() {
        TestEqn(2147483647, checkedadd32s(1, 2147483646));
    }

    SV_TEST_() {
        TestEqn(-112, checkedadd32s(-45, -67));
    }

    SV_TEST_() {
        TestEqn(INT_MIN, checkedadd32s(-1073741824, -1073741824));
    }

    SV_TEST_() {
        TestEqn(INT_MIN, checkedadd32s(-1, -2147483647));
    }

    SV_TEST_() {
        TestEqn(5, countof("abcd"));
    }

    SV_TEST_() {
        TestEqn(5, sizeof("abcd"));
    }

    SV_TEST_() {
        TestEqn(0x1111000011112222ULL, make_u64(0x11110000, 0x11112222));
    }

    SV_TEST_() {
        TestEqn(0x00000000FF345678ULL, make_u64(0x00000000, 0xFF345678));
    }

    SV_TEST_() {
        TestEqn(0xFF345678FF345678ULL, make_u64(0xFF345678, 0xFF345678));
    }

    SV_TEST_() {
        TestEqn(0xFF34567800000000ULL, make_u64(0xFF345678, 0x00000000));
    }

    SV_TEST_() {
        TestEqn(0x12341234ULL, upper32(0x1234123466667777ULL));
    }

    SV_TEST_() {
        TestEqn(0x66667777ULL, lower32(0x1234123466667777ULL));
    }

    SV_TEST_() {
        TestEqn(1, nearest_power_of_two(0));
    }

    SV_TEST_() {
        TestEqn(1, nearest_power_of_two(1));
    }

    SV_TEST_() {
        TestEqn(2, nearest_power_of_two(2));
    }

    SV_TEST_() {
        TestEqn(4, nearest_power_of_two(3));
    }

    SV_TEST_() {
        TestEqn(16, nearest_power_of_two(15));
    }

    SV_TEST_() {
        TestEqn(16, nearest_power_of_two(16));
    }
}
SV_END_TEST_SUITE()

check_result tests_2d_array_iter_cb(void *context,
    uint32_t x, uint32_t y, byte *val)
{
    uint32_t *got_iter = (uint32_t *)context;
    *got_iter += 1 + x + x + y + *val;
    return OK;
}

SV_BEGIN_TEST_SUITE(tests_2d_array)
{
    SV_TEST("store data in 1x1 array") {
        TEST_OPENA(sv_2darray, arr, sizeof32u(byte));
        *sv_2darray_get_expand(&arr, 0, 0) = 1;
        sv_array *subarr = (sv_array *)sv_array_at(&arr.arr, 0);
        TestEqn(1, arr.arr.length);
        TestEqn(1, subarr->length);
        TestEqn(1, *sv_array_at(subarr, 0));
        TestEqn(1, *sv_array_at(subarr, 0));
        TestEqn(1, *sv_2darray_at(&arr, 0, 0));
    }

    SV_TEST("store data in 8x4 array") {
        TEST_OPENA(sv_2darray, arr, sizeof32u(byte));
        *sv_2darray_get_expand(&arr, 7, 3) = 1;
        sv_array *subarr = (sv_array *)sv_array_at(&arr.arr, 7);
        TestEqn(8, arr.arr.length);
        TestEqn(4, subarr->length);
        TestEqn(0, *sv_array_at(subarr, 0));
        TestEqn(0, *sv_array_at(subarr, 1));
        TestEqn(0, *sv_array_at(subarr, 2));
        TestEqn(1, *sv_array_at(subarr, 3));
        TestTrue(sv_2darray_at(&arr, 7, 0) == sv_array_at(subarr, 0));
        TestTrue(sv_2darray_at(&arr, 7, 1) == sv_array_at(subarr, 1));
        TestTrue(sv_2darray_at(&arr, 7, 2) == sv_array_at(subarr, 2));
        TestTrue(sv_2darray_at(&arr, 7, 3) == sv_array_at(subarr, 3));
        TestEqn(0, ((sv_array *)sv_array_at(&arr.arr, 0))->length);
        TestEqn(0, ((sv_array *)sv_array_at(&arr.arr, 1))->length);
        TestEqn(0, ((sv_array *)sv_array_at(&arr.arr, 2))->length);
        TestEqn(0, ((sv_array *)sv_array_at(&arr.arr, 3))->length);
        TestEqn(1, *sv_2darray_at(&arr, 7, 3));
    }

    SV_TEST("set and reset data") {
        TEST_OPENA(sv_2darray, arr, sizeof32u(byte));
        *sv_2darray_get_expand(&arr, 4, 3) = 1;
        *sv_2darray_get_expand(&arr, 4, 3) = 2;
        *sv_2darray_get_expand(&arr, 4, 3) = 3;
        TestEqn(0, *sv_2darray_at(&arr, 4, 0));
        TestEqn(0, *sv_2darray_at(&arr, 4, 1));
        TestEqn(0, *sv_2darray_at(&arr, 4, 2));
        TestEqn(3, *sv_2darray_at(&arr, 4, 3));
    }

    SV_TEST("store and read data") {
        TEST_OPENA(sv_2darray, arr, sizeof32u(byte));
        uint32_t expected = 0, got = 0, got_iter = 0;
        for (uint32_t x = 0; x < 16; x++)
        {
            for (uint32_t y = 0; y < 32; y++)
            {
                byte b = (byte)(250 * sin(x * x * y));
                *sv_2darray_get_expand(&arr, x, y) = b;
                expected += 1 + x + x + y + b;
            }
        }

        for (uint32_t x = 0; x < 16; x++)
        {
            for (uint32_t y = 0; y < 32; y++)
            {
                got += 1 + x + x + y +
                    *sv_2darray_at(&arr, x, y);
            }
        }

        check(sv_2darray_foreach(&arr, &tests_2d_array_iter_cb, &got_iter));
        TestEqn(expected, got);
        TestEqn(expected, got_iter);
    }
}
SV_END_TEST_SUITE()
