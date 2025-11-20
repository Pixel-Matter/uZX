/* Ayumi Output Modes Test
 *
 * Tests all three output modes to verify correct functionality.
 * Compile: gcc -Wall test_ayumi.c ayumi.c -lm -o test_ayumi
 * Run: ./test_ayumi
 */

#include <stdio.h>
#include <math.h>
#include "ayumi.h"

#define TEST_SAMPLES 1000

static double compute_mean(const double* samples, int count) {
    double sum = 0.0;
    int i;
    for (i = 0; i < count; i++) {
        sum += fabs(samples[i]);
    }
    return sum / count;
}

static int test_stereo_mode(void) {
    struct ayumi ay;
    double left_samples[TEST_SAMPLES];
    double right_samples[TEST_SAMPLES];
    double left_mean, right_mean;
    int i;

    printf("Testing STEREO mode:\n");

    ayumi_configure(&ay, 0, 2000000, 44100);
    ayumi_set_output_mode(&ay, AYUMI_STEREO);

    // Channel 0: Left (pan=0.0)
    ayumi_set_pan(&ay, 0, 0.0, 0);
    ayumi_set_tone(&ay, 0, 1000);
    ayumi_set_mixer(&ay, 0, 0, 1, 0);
    ayumi_set_volume(&ay, 0, 15);

    // Channel 1: Right (pan=1.0)
    ayumi_set_pan(&ay, 1, 1.0, 0);
    ayumi_set_tone(&ay, 1, 800);
    ayumi_set_mixer(&ay, 1, 0, 1, 0);
    ayumi_set_volume(&ay, 1, 10);

    // Channel 2: Center (pan=0.5)
    ayumi_set_pan(&ay, 2, 0.5, 0);
    ayumi_set_tone(&ay, 2, 600);
    ayumi_set_mixer(&ay, 2, 0, 1, 0);
    ayumi_set_volume(&ay, 2, 5);

    for (i = 0; i < TEST_SAMPLES; i++) {
        ayumi_process(&ay);
        left_samples[i] = ayumi_get_output(&ay, 0);
        right_samples[i] = ayumi_get_output(&ay, 1);
    }

    left_mean = compute_mean(left_samples, TEST_SAMPLES);
    right_mean = compute_mean(right_samples, TEST_SAMPLES);

    printf("  Left output mean:  %.6f\n", left_mean);
    printf("  Right output mean: %.6f\n", right_mean);

    if (left_mean > 0.1 && right_mean > 0.05) {
        printf("  ✓ STEREO mode working correctly\n\n");
        return 1;
    } else {
        printf("  ✗ STEREO mode FAILED\n\n");
        return 0;
    }
}

static int test_three_channel_mode(void) {
    struct ayumi ay;
    double ch0_samples[TEST_SAMPLES];
    double ch1_samples[TEST_SAMPLES];
    double ch2_samples[TEST_SAMPLES];
    double ch0_mean, ch1_mean, ch2_mean;
    int i;

    printf("Testing THREE_CHANNEL mode:\n");

    ayumi_configure(&ay, 0, 2000000, 44100);
    ayumi_set_output_mode(&ay, AYUMI_THREE_CHANNEL);

    ayumi_set_tone(&ay, 0, 1000);
    ayumi_set_mixer(&ay, 0, 0, 1, 0);
    ayumi_set_volume(&ay, 0, 15);

    ayumi_set_tone(&ay, 1, 800);
    ayumi_set_mixer(&ay, 1, 0, 1, 0);
    ayumi_set_volume(&ay, 1, 10);

    ayumi_set_tone(&ay, 2, 600);
    ayumi_set_mixer(&ay, 2, 0, 1, 0);
    ayumi_set_volume(&ay, 2, 5);

    for (i = 0; i < TEST_SAMPLES; i++) {
        ayumi_process(&ay);
        ch0_samples[i] = ayumi_get_output(&ay, 0);
        ch1_samples[i] = ayumi_get_output(&ay, 1);
        ch2_samples[i] = ayumi_get_output(&ay, 2);
    }

    ch0_mean = compute_mean(ch0_samples, TEST_SAMPLES);
    ch1_mean = compute_mean(ch1_samples, TEST_SAMPLES);
    ch2_mean = compute_mean(ch2_samples, TEST_SAMPLES);

    printf("  Channel 0 mean: %.6f\n", ch0_mean);
    printf("  Channel 1 mean: %.6f\n", ch1_mean);
    printf("  Channel 2 mean: %.6f\n", ch2_mean);

    if (ch0_mean > ch1_mean && ch1_mean > ch2_mean) {
        printf("  ✓ Unmixed channels correctly reflect volume settings (15 > 10 > 5)\n\n");
        return 1;
    } else {
        printf("  ✗ THREE_CHANNEL mode FAILED\n\n");
        return 0;
    }
}

static int test_mono_mode(void) {
    struct ayumi ay;
    double mono_samples[TEST_SAMPLES];
    double mono_mean;
    int i;

    printf("Testing MONO mode:\n");

    ayumi_configure(&ay, 0, 2000000, 44100);
    ayumi_set_output_mode(&ay, AYUMI_MONO);

    ayumi_set_tone(&ay, 0, 1000);
    ayumi_set_mixer(&ay, 0, 0, 1, 0);
    ayumi_set_volume(&ay, 0, 15);

    for (i = 0; i < TEST_SAMPLES; i++) {
        ayumi_process(&ay);
        mono_samples[i] = ayumi_get_output(&ay, 0);
    }

    mono_mean = compute_mean(mono_samples, TEST_SAMPLES);

    printf("  Mono output mean: %.6f\n", mono_mean);

    if (mono_mean > 0.1) {
        printf("  ✓ MONO mode working correctly\n\n");
        return 1;
    } else {
        printf("  ✗ MONO mode FAILED\n\n");
        return 0;
    }
}

int main(void) {
    int tests_passed = 0;
    int tests_total = 3;

    printf("Ayumi Output Modes Test\n");
    printf("=======================\n\n");

    if (test_mono_mode()) tests_passed++;
    if (test_stereo_mode()) tests_passed++;
    if (test_three_channel_mode()) tests_passed++;

    printf("Results: %d/%d tests passed\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}
