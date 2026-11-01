/*
 * SMP - Swag Music Player
 * Test runner.
 *
 * SPDX-License-Identifier: MIT
 */
#include "test.h"

int main(void)
{
    suite_common();
    suite_config();
    suite_str();
    suite_fs();
    suite_pcm();
    suite_spectrum();
    suite_tags();
    suite_media();
    suite_playlist();
    suite_frame();
    suite_cover();
    suite_player();

    printf("\n%d checks, %d failures\n", smp_test_checks, smp_test_failures);
    return smp_test_failures == 0 ? 0 : 1;
}
