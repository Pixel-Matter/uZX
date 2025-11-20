# Ayumi Emulator - MoTool Modifications

This directory contains the Ayumi AY-3-8910/YM2149 emulator with enhancements for MoTool.

## Original Author

Peter Sovietov (original Ayumi implementation)

## MoTool Enhancements

### Flexible Output Mode System

The original Ayumi provided fixed stereo output. MoTool adds support for three output modes with proper filtering for each:

#### Output Modes

1. **AYUMI_MONO** - Single mono output (average of all 3 channels)
2. **AYUMI_STEREO** - Traditional stereo output with panning (default)
3. **AYUMI_THREE_CHANNEL** - Three separate unmixed outputs

Each mode processes outputs through the full signal chain:
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

All 3 outputs are statically allocated. The output mode determines how many are processed (1, 2, or 3).

### Performance

Benchmark results (GCC -O3, 441K samples):

| Mode          | Speed         | Real-time factor |
|---------------|---------------|------------------|
| MONO          | 6.3 MHz/s     | 143x             |
| STEREO        | 4.0 MHz/s     | 91x              |
| THREE_CHANNEL | 3.4 MHz/s     | 77x              |

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
ayumi_set_output_mode(&ay, AYUMI_THREE_CHANNEL);
```

**Important**: Set output mode during configuration, not during processing. Changing modes mid-processing can corrupt filter state.

#### Processing Audio

```c
// Mono mode
ayumi_process(&ay);
double mono = ayumi_get_output(&ay, 0);

// Stereo mode (default)
ayumi_process(&ay);
double left = ayumi_get_output(&ay, 0);
double right = ayumi_get_output(&ay, 1);

// Three-channel mode
ayumi_process(&ay);
double ch0 = ayumi_get_output(&ay, 0);
double ch1 = ayumi_get_output(&ay, 1);
double ch2 = ayumi_get_output(&ay, 2);
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

`test_ayumi.c` verifies all three output modes work correctly:
- Tests mono, stereo, and three-channel modes
- Validates output amplitude and channel separation
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

## C++ Wrapper (MoTool)

The C++ wrapper `AyumiEmulator` (in `src/plugins/uZX/aychip/`) provides:

```cpp
// Constructor with output mode
AyumiEmulator emulator(44100, 2000000, ChipType::AY, 3);  // 3-channel mode

// Or set mode later
emulator.setOutputMode(2);  // Stereo mode

// Process blocks
emulator.processBlockMono(mono, samples);
emulator.processBlock(left, right, samples);
emulator.processBlockUnmixed(ch0, ch1, ch2, samples);
```

## Modifications Summary

1. **Added flexible output mode system** with enum `ayumi_output_mode`
2. **Extracted interpolation logic** to `update_interpolator()` function
3. **Refactored ayumi_process()** to inline mixing per mode
4. **Added DC filtering** per output (not just stereo pair)
5. **Created test and benchmark utilities** for validation
6. **Optimized performance** through code specialization

## Compatibility

The refactored code maintains backward compatibility:
- Default mode is `AYUMI_STEREO`
- Original API functions unchanged
- Performance is equal or better than original for stereo mode
