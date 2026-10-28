# PSG note painting benchmark snapshot

This report records the preceding renderer revision: half-note-height noise spread and a tick fallback for noise-only owners. The renderer has since switched to scattered ownership rectangles and uniform decoration opacity; these timings have not been refreshed for those changes. “Current” in the table refers to the later measured snapshot below.

Measured on Apple M3 Pro, macOS 26.7.1, **Debug**, using JUCE’s native offscreen CoreGraphics bitmap context (RGB requested; JUCE promotes it to ARGB on macOS). These are offscreen paint costs, not live application frame times. They do not measure window compositing or establish GPU costs.

The measured noise fringe used **2×2 logical-pixel rectangles**, spaced in 6-pixel columns and 3-pixel depth steps. At 2× backing scale a full mark covers 4×4 physical pixels. It uses a fixed repeating pattern, clips to free space around notes, and visits only horizontally visible columns. There is no random-number generation, explicit heap allocation, or gradient creation per noise rectangle. The shared gray band still uses a vertical gradient per noisy frame.

In the measured revision, noise was decorated once per channel: tone first, otherwise its visible envelope pitch, otherwise ownership ticks on the band. The baseline used smaller 1×1 marks and could decorate both tone and envelope for the same owner.

## Measurements

The table shows 240 visible PSG frames, all three channels with full volume, and noise period 16. The tone/envelope cases have noise on all three channels. Logical dimensions and backing scale are listed separately; 1800×720 at 2× paints a 3600×1440 backing image.

“Noise overhead” subtracts the median with noise disabled from the median with noise enabled. It includes the gray band and ownership marks. “Current total” includes all note painting in that case. All times are milliseconds.

| Logical viewport / scale | Sources | Earlier noise overhead | Current noise overhead | Current total median | Current total p95 |
| --- | --- | ---: | ---: | ---: | ---: |
| 1800×720 / 1× | Tone | 4.56 | 3.68 | 4.46 | 4.67 |
| 1800×720 / 1× | Tone + envelope | 5.60 | 4.05 | 7.71 | 10.13 |
| 1800×720 / 1× | Noise only | 2.90 | 3.49 | 3.71 | 5.94 |
| 1800×720 / 2× | Tone | 8.98 | 8.11 | 8.94 | 9.20 |
| 1800×720 / 2× | Tone + envelope | 10.23 | 8.30 | 12.38 | 13.44 |
| 1800×720 / 2× | Noise only | 7.31 | 7.81 | 8.02 | 8.25 |
| 2560×1440 / 1× | Tone | 10.35 | 8.14 | 8.98 | 9.48 |
| 2560×1440 / 1× | Tone + envelope | 13.48 | 9.41 | 13.37 | 13.76 |
| 2560×1440 / 1× | Noise only | 6.51 | 6.82 | 7.04 | 7.29 |

The larger, sparser rectangles reduce overhead in the tested tone cases. Removing duplicate noise decoration also helps tone-plus-envelope cases. The noise-only case now does extra ownership-tick painting; its cost increased. It is not a pure-band reference for isolating rectangle cost. These measurements do not establish the cost of individual rectangles, and differences between runs include normal timing variation.

At 600 frames in these viewports, the horizontal detail threshold hides noise bands and ownership details. The raw reports also include 60-frame and 600-frame views. They contain 54 cases each.

## Method and reproduction

The opt-in [benchmark](../src/gui/timeline/PsgPaintingBenchmark.test.cpp) constructs each fixture before timing, warms the cached pitch/range state for five calls, then records 51 calls to `paintNotes()`. It reports the median and nearest-rank p95. Image clearing, clip construction, and other application painting are excluded. Each fixture has 2048 source frames and starts its viewport at frame 512, exercising offscreen culling. A reference frame fixes a wide pitch range.

```sh
cmake --build build --target uZXTests --parallel 4
UZX_PSG_PAINT_BENCHMARK=/tmp/psg-paint.json build/src/uZXTests_artefacts/Debug/uZXTests PsgPaintingBenchmark
```

The benchmark does no work in ordinary test runs unless the environment variable is set.

Raw reports: [earlier](benchmarks/psg-paint-before.json), [current](benchmarks/psg-paint-after.json). The earlier report is from an uncommitted working-tree snapshot in this session, not a reproducible historical commit. The current report was measured at 2026-10-28T16:49:06.995+01:00. Renderer source SHA-256 at that measurement:

```text
d652a216680d63f19d47c012bb769354da6b7fd0081b6981a321b27abf2a796c  src/gui/timeline/PsgClipComponent.cpp
f6d1c7c2c1433bc292e143f4b2cbd30b51d9b76063e71cf582aba798b5e1d3c3  src/models/PsgParameter.h
```

The initial benchmark metadata called this “software raster”; inspecting JUCE’s `NativeImageType` confirmed the CoreGraphics bitmap backend. The archived JSON records that label correction and retains the original label. Timing samples are unchanged.

Live application rendering, Release builds, other machines, display drivers, and real-song frame pacing were not measured. This fixture uses envelope period 60, so its timing does not include below-range envelope arrows.
