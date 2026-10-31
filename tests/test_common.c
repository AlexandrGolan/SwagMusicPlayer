/*
 * SMP - Swag Music Player
 * Tests for the common runtime helpers.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/common.h"
#include "test.h"

static void test_status_strings(void)
{
    smp_test_begin("status strings are never NULL");
    SMP_CHECK(smp_status_str(SMP_OK) != NULL);
    SMP_CHECK_STR(smp_status_str(SMP_OK), "ok");
    SMP_CHECK(smp_status_str(SMP_ERR_IO) != NULL);
    SMP_CHECK(smp_status_str((SmpStatus)9999) != NULL);
    smp_test_end();
}

static void test_clock(void)
{
    uint64_t first;
    uint64_t second;

    smp_test_begin("monotonic clock moves forward");
    first = smp_now_ms();
    smp_sleep_ms(20);
    second = smp_now_ms();
    SMP_CHECK(second >= first);
    SMP_CHECK(second - first >= 15u);
    smp_test_end();
}

void suite_common(void)
{
    smp_test_suite("common");
    test_status_strings();
    test_clock();
}
