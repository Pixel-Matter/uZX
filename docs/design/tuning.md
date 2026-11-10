# Tuning systems

Background on tuning for the AY chip, then how µZX models, detects and applies tunings. For the user-facing side see **µZX Tuning** and the **Tuning table and key** paragraph in the [user guide](../user-guide.md); for the just-intonation argument in full, [Playing music on AY chip like on a digital flute](just-intonation-on-ay.md).

## Background

### Tuning Systems in Music

A **tuning system** defines the mathematical relationships between musical pitches. The practical purpose is to translate
MIDI note numbers C-1..G10 (A4 is 440Hz) to frequency based on mathematical relationships between musical pitches.
And for our case tuning system must translate note numbers to chip register values for hardware clock dividers (periods) as an end result.
In legacy tracker software this is implemented as lookup tables with fixed period values, note number to value.

Throughout history, different cultures and periods have used various approaches:

#### Just Intonation

- Based on simple integer ratios derived from the harmonic series
- Pure intervals: octave (2:1), perfect fifth (3:2), major third (5:4)
- Multiple variants: 5-limit, 7-limit, 11-limit systems
- Limited modulation capability, may contain "wolf" intervals
- Used in Renaissance and Baroque periods, still relevant for specialized applications

#### Historical Temperaments

- **Pythagorean**: Based on perfect fifths (3:2), gives pure fifths but impure thirds
- **Meantone**: Compromises to get better thirds, commonly used in Renaissance
- **Well-Tempered**: Various unequal divisions allowing all 24 keys (Bach era)

Every integer-ratio tuning is built on a tonic, so a just intonation is always a pair: the system and its tonic.

#### Equal Temperament (12-TET)

- Standard in modern Western music since ~1850
- Divides the octave into 12 equal semitones
- Each semitone = 2^(1/12) ≈ 1.05946
- Enables unlimited modulation between keys
- Enables full 12 semitones chromaticism
- All intervals except octaves are mathematically impure, dissonant
- Can not be exactly represented on AY-3-8910 chip due to discrete period values, especially at higher octaves

#### Conclusion on tuning systems

Just intonation tunings could provide much better accuracy for real hardware chiptune than equal temperament,
but they are not suitable for modulation if we are not switching tunings on the fly.

### Musical Modes and Scales

**Modes** are specific patterns of intervals within a tuning system:

**Diatonic Modes** (from major scale):

- Ionian (Major): T-T-S-T-T-T-S
- Dorian: T-S-T-T-T-S-T
- Phrygian: S-T-T-T-S-T-T
- Lydian: T-T-T-S-T-T-S
- Mixolydian: T-T-S-T-T-S-T
- Aeolian (Natural Minor): T-S-T-T-S-T-T
- Locrian: S-T-T-S-T-T-T

**Non-Diatonic Scales**:

- Pentatonic: 5-note scales (major, minor variants)
- Chromatic: All 12 semitones
- Blues: Pentatonic + "blue notes"
- Exotic: Hungarian minor, Arabic maqams, etc.

### Relevance to AY-3-8910

The AY chip uses **discrete integer period dividers** (1-4095), making it naturally suited
to just intonation where intervals are simple ratios. Equal temperament requires approximation,
leading to slight detuning. For demoscene productions, this creates a choice between:

- **Familiar modern harmony** (equal temperament, approximated)
- **Pure intervals** (just intonation, exact in the lower octaves and closer than 12-TET in the higher ones)

## In the code

Everything is under `src/models/tuning/`; each class has an adjacent `*.test.cpp`.

### Two layers: reference tuning and chip tuning

- **`ReferenceTuningSystem`** (`TemperamentSystem.h`) maps MIDI notes to frequencies, with an A4 and a tonic. Implementations: `EqualTemperamentTuning`, and `RationalTuning` with its ratio sets `JustIntonation5Limit`, `JustIntonation5LimitT45_64` and `PythagoreanTuning`. Ratios are exact fractions (`Ratios.h`); scales and tonics are in `Scales.h`.
- **`TuningSystem`** (`TuningSystemBase.h`) maps MIDI notes to chip periods and back, for tone and for envelope (`PeriodMode`), at a chip clock. It owns a reference tuning, used to name notes and measure how far a period is off. Implementations:
  - `AutoTuning` computes each period from the reference frequency and the clock.
  - `TuningTable` is a fixed period list, as trackers use; it needs no clock to play, only to say what it sounds like.

`TuningRegistry.h` lists the built-in tunings (`BuiltinTuningType`): the four computed presets (12TET, two 5-limit just intonations, Pythagorean) and the tracker tables (ProTracker #0–#3, Ivan Roschin's #4 and its fixed version, the PT 3.3 variants of #0 and #2, Sound Tracker). `makeBuiltinTuning` builds any of them.

### Where a tuning is used

- **MIDI → PSG:** `NotesToPsgMapper` holds a `TuningSystem` and asks it for the period of every note (a `double`, so slides and pitch bend work).
- **µZX Tuning:** `TuningViewModel` builds a tuning from `TuningOptions` (table, system, tonic, scale, clock, A4) and feeds the grid, the preview player and CSV export.
- **PSG clips:** the Notes view places periods on the pitch axis through the clip's tuning, `PsgClip::getTuning()`.

### Detection on PSG clips

A PSG file carries periods, not a tuning, so the clip's tuning and key are detected (`TuningDetector.h`).

1. `PitchHistogram` counts frames per audible period, by the same rules the Notes view paints. Envelope pitches are kept apart: a tracker rounds them to tone / 16, so they match a grid only within half an envelope step.
2. Every tracker table is scored by the share of note-frames that land exactly on its periods. Tables sharing over 90% of their periods count as one candidate.
3. Each computed system is **fitted**: 12TET over A4, the just intonations over tonic and A4 (`TuningFit`). A just intonation has to beat 12TET, and a fit has to beat the best table, by more than `TuningDetection::tieGap` (5%).
4. Periods fix pitch only relative to the clock, so a fit is reported at the ZX Spectrum clock unless the Pentagon clock brings its A4 to 440 Hz. The Atari ST and Amstrad clocks are never guessed; the user picks them, and `atClock()` re-expresses the fit.
5. The key is the best of the 24 major/minor keys by Krumhansl-Kessler profile correlation over the pitch classes; `keyTie` marks a call too close to make.

Clips that play together, such as the chips of a Turbo Sound set, are detected as one tune (`detectTuningTogether`). The result is cached on the clip; the user's choice (`chooseTuningTable`, `chooseFit`) overrides it and is saved with the edit, `clearTuningChoice` returns to detection.

### Not there yet

Open tasks are under **Tuning System** in [TODO.md](../TODO.md): an edit stores a built-in tuning by name only, so custom tables (for example a Vortex module's own) cannot be saved; there is no edit-wide tuning; envelope-period snapping is not done.
