# PSG to MIDI Control Mapping Design

## Overview

This design outlines the mapping between AY/YM PSG chip controls and MIDI messages, optimized for both DAW editing and hardware controller usage.

## Core Mappings

### Tone Channels (A, B, C)

- **Base Frequency**: MIDI Note On/Off messages
  - Uses predefined tuning table mapping MIDI notes to PSG divider values
  - High resolution in lower registers, natural degradation in higher registers
- **Fine Tuning**: Pitch Bend (±2 semitones standard range)
  - For small adjustments around base frequency
- **Volume**: Note Velocity (0-127)
- **Channel Mixing**: CC 74
  - Controls combination of tone/noise/envelope bits
  - Eight possible states (000 to 111)
  - Maps naturally to KeyStep 37's first knob

### Noise & Envelope Control Channel

- **Envelope Frequency**: MIDI Notes + Pitch Bend on dedicated channel
- **Noise Frequency**: CC 71 (mapped to KeyStep 37's second knob)
  - Logarithmic mapping between CC (0-127) and PSG period values (1-31)
  - Equal musical intervals across the range
  - Each doubling of period (octave down) spaced evenly in CC values

## Implementation Notes

### Mixing Bits Control (CC 74)
Values map to PSG mixing states:
- 000: tone only
- 001: noise only
- 010: envelope only
- 011: noise + envelope
- 100: tone only (duplicate)
- 101: tone + noise
- 110: tone + envelope
- 111: tone + noise + envelope

### Noise Period Mapping (CC 71)
Logarithmic scale where:
- CC 127 → Period 1 (highest frequency)
- CC 0 → Period 31 (lowest frequency)
- Each octave change corresponds to consistent CC value steps

## Rationale
- Uses standard MIDI messages in conventional ways
- Easily editable in DAW automation lanes
- Maps efficiently to hardware controllers
- Preserves PSG's native resolution where possible
- Provides intuitive control over all PSG parameters