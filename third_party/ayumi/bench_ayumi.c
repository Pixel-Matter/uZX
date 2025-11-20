/* Ayumi Performance Benchmark
 *
 * Benchmarks the three output modes to measure CPU performance.
 * Compile: gcc -O3 -Wall bench_ayumi.c ayumi.c -lm -o bench_ayumi
 * Run: ./bench_ayumi
 */

#include <stdio.h>
#include <time.h>
#include "ayumi.h"

#define SAMPLES (44100 * 10)  // 10 seconds of audio at 44.1kHz

static double benchmark_mode(struct ayumi* ay, enum ayumi_output_mode mode, const char* name) {
    clock_t start, end;
    int i;
    double cpu_time;

    ayumi_set_output_mode(ay, mode);

    start = clock();
    for (i = 0; i < SAMPLES; i++) {
        ayumi_process(ay);
    }
    end = clock();

    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("%-18s: %.3f sec  (%.1f MHz samples/sec)\n",
           name, cpu_time, SAMPLES / cpu_time / 1000000.0);

    return cpu_time;
}

static void setup_chip(struct ayumi* ay) {
    int i;

    // Configure chip
    ayumi_configure(ay, 0, 2000000, 44100);

    // Set pan positions
    ayumi_set_pan(ay, 0, 0.0, 0);  // Left
    ayumi_set_pan(ay, 1, 1.0, 0);  // Right
    ayumi_set_pan(ay, 2, 0.5, 0);  // Center

    // Enable all channels with tone
    for (i = 0; i < TONE_CHANNELS; i++) {
        ayumi_set_tone(ay, i, 1000);
        ayumi_set_mixer(ay, i, 0, 1, 0);  // Tone on, noise off, envelope off
        ayumi_set_volume(ay, i, 15);
    }
}

int main(void) {
    struct ayumi ay;
    double mono_time, stereo_time, three_ch_time;

    printf("Ayumi Performance Benchmark\n");
    printf("============================\n");
    printf("Processing %d samples (10 sec @ 44.1kHz)\n", SAMPLES);
    printf("Compiler optimizations: ");
#ifdef __OPTIMIZE__
    printf("enabled (-O");
#if __OPTIMIZE_SIZE__
    printf("s");
#else
    printf("%d", __OPTIMIZE__);
#endif
    printf(")\n");
#else
    printf("disabled\n");
#endif
    printf("\n");

    setup_chip(&ay);

    mono_time = benchmark_mode(&ay, AYUMI_MONO, "MONO mode");

    setup_chip(&ay);
    stereo_time = benchmark_mode(&ay, AYUMI_STEREO, "STEREO mode");

    setup_chip(&ay);
    three_ch_time = benchmark_mode(&ay, AYUMI_THREE_CHANNEL, "THREE_CHANNEL mode");

    printf("\n");
    printf("Relative performance:\n");
    printf("  MONO vs STEREO:        %.2fx faster\n", stereo_time / mono_time);
    printf("  MONO vs THREE_CHANNEL: %.2fx faster\n", three_ch_time / mono_time);
    printf("  STEREO vs THREE_CHANNEL: %.2fx faster\n", three_ch_time / stereo_time);

    printf("\n");
    printf("Real-time capability @ 44.1kHz:\n");
    printf("  MONO:          %.1fx real-time\n", 10.0 / mono_time);
    printf("  STEREO:        %.1fx real-time\n", 10.0 / stereo_time);
    printf("  THREE_CHANNEL: %.1fx real-time\n", 10.0 / three_ch_time);

    return 0;
}
