# Ayumi Emulator - µZX Modifications

This directory contains the Ayumi AY-3-8910/YM2149 emulator with enhancements for µZX.

## Original Author

Peter Sovietov (original Ayumi implementation)

## µZX Enhancements

### Flexible Output Mode System

The original Ayumi provided fixed stereo output. µZX adds a mode with three separate filtered outputs, and raw channel levels in either mode:

#### Output Modes

1. **AYUMI_STEREO** - Traditional stereo output with panning (default)
2. **AYUMI_SEPARATE** - Three separate unmixed outputs, for mixing them elsewhere; no stereo

The mode's value is its number of outputs. Each output goes through the full signal chain:
- Cubic interpolation (upsampling from chip rate to sample rate)
- 192-tap FIR decimation (anti-aliasing filter)
- DC filtering (optional, per-output)

### Architecture

The refactored architecture uses a reusable `ayumi_output` structure:

```c
struct ayumi_output {
  struct interpolator interpolator;  // Cubic interpolation
  double fir[FIR_SIZE * 2];          // 192-tap FIR filter
  struct dc_filter dc;                // DC removal
  double value;                       // Final output
};
```

All 3 outputs are statically allocated. The output mode determines how many are processed (2 or 3).

#### Raw Channel Levels

After each `ayumi_process()`, `ay->raw[0..2]` holds each channel's DAC level 0..1 averaged over the chip ticks of that
output sample: no interpolation, FIR or DC filter, so a tone's edge takes at most one sample. They are for scopes. A
sample with no chip tick, at a low clock and a high sample rate, holds the current level.

### Performance

Benchmark results (`make bench`, Apple clang -O3, 441K samples):

| Mode          | Speed         | Real-time factor |
|---------------|---------------|------------------|
| STEREO        | 6.0 MHz/s     | 136x             |
| THREE_CHANNEL | 4.2 MHz/s     | 96x              |

All modes run comfortably faster than real-time even without SIMD optimizations.

### Memory Footprint

- **Single ayumi_output**: 11.3 KB (mostly DC filter: 8.2 KB)
- **Total ayumi struct**: ~34 KB (3 outputs)
- **Overhead vs original**: +11.3 KB (50% increase for 3rd output)

### API Usage

#### Setting Output Mode

```c
struct ayumi ay;
ayumi_configure(&ay, 0, 2000000, 44100);
ayumi_set_output_mode(&ay, AYUMI_SEPARATE);
```

Changing modes mid-processing clicks once: the outputs it brings in still hold their old filter state.

#### Processing Audio

```c
// Stereo mode (default)
ayumi_process(&ay);
double left, right;
ayumi_get_stereo_output(&ay, &left, &right);

// Three-channel mode
ayumi_process(&ay);
double ch0, ch1, ch2;
ayumi_get_separate_output(&ay, &ch0, &ch1, &ch2);

// Raw levels, in either mode
double a = ay.raw[0], b = ay.raw[1], c = ay.raw[2];
```

#### DC Filtering

```c
ayumi_process(&ay);
ayumi_remove_dc(&ay);  // Optional, filters active outputs only
```

## Testing and Benchmarking

### Build and Test

```bash
make test    # Compile and run functionality tests
make bench   # Compile and run performance benchmark
make clean   # Remove built binaries
```

### Test Utility

`test_ayumi.c` verifies both output modes and the raw levels:
- Tests stereo and three-channel modes
- Validates output amplitude and channel separation
- Checks raw levels reach the DAC's 0 and 1 with one-sample edges
- Returns 0 on success, 1 on failure

### Benchmark Utility

`bench_ayumi.c` measures performance of all modes:
- Processes 10 seconds of audio per mode
- Reports samples/sec and real-time factors
- Shows relative performance between modes

## Implementation Details

### Optimization Strategy

The code uses mode-specialized inline paths to maximize performance:

- **Switch on mode once** per `ayumi_process()` call (not per sample)
- **Fully inline** mixing logic for each mode
- **No function calls** in hot path (except `update_interpolator`, which inlines)
- **No dynamic branching** during sample processing

### Code Structure

- `ayumi.h` - Public API and structure definitions
- `ayumi.c` - Core emulator implementation
- `test_ayumi.c` - Functionality tests
- `bench_ayumi.c` - Performance benchmarks
- `Makefile` - Build configuration

## C++ Wrapper (µZX)

The C++ wrapper `AyumiEmulator` (in `src/plugins/uZX/aychip/`) sets the mode from the call, so a reset can't leave
it rendering fewer outputs than asked for:

```cpp
AyumiEmulator emulator(44100, 2000000, ChipType::AY);

// Process blocks; raw (3 pointers, optional) takes the unfiltered channel levels for scopes
emulator.processBlockStereo(left, right, samples, removeDC, raw);
emulator.processBlockSeparate(a, b, c, samples, removeDC, raw);
```

## Modifications Summary

1. **Added an output mode system** with enum `ayumi_output_mode`: stereo or three separate outputs
2. **Added raw channel levels** averaged per output sample, for scopes
3. **Extracted interpolation logic** to `update_interpolator()` function
4. **Refactored ayumi_process()** to inline mixing per mode
5. **Added DC filtering** per output (not just stereo pair)
6. **Created test and benchmark utilities** for validation
7. **Optimized performance** through code specialization

## Compatibility

The refactored code maintains backward compatibility:
- Default mode is `AYUMI_STEREO`
- Original API functions unchanged
- Performance is equal or better than original for stereo mode
