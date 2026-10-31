/*
 * SMP - Swag Music Player
 * Test harness implementation.
 *
 * SPDX-License-Identifier: MIT
 */
#include "test.h"

int smp_test_failures = 0;
int smp_test_checks = 0;

static int suite_failures = 0;

void smp_test_suite(const char *name)
{
    suite_failures = 0;
    printf("\n[%s]\n", name);
}

void smp_test_begin(const char *name)
{
    suite_failures = smp_test_failures;
    printf("  %-46s", name);
    fflush(stdout);
}

void smp_test_end(void)
{
    if (smp_test_failures == suite_failures) {
        printf("ok\n");
    } else {
        printf("FAILED\n");
    }
    fflush(stdout);
}
