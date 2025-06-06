# Tuning Systems Design Document for MoTool

## Introduction

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

All integer-ratio-based tuning system must be based on specific scale and tonic note,
so we should link this tuning system to specific scale with choosen tonic: MusicScale instance.

#### Equal Temperament (12-TET)

- Standard in modern Western music since ~1850
- Divides the octave into 12 equal semitones
- Each semitone = 2^(1/12) ≈ 1.05946
- Enables unlimited modulation between keys
- Enables full 12 semitones chromaticism
- All intervals except octaves are mathematically impure, dissonant
- Can not be exactly represented on AY-3-8910 chip due to discrete period values, escpecially at higher octaves

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

- **Familiar modern harmony** (equal temperament, ~95-99% accuracy)
- **Pure intervals** (just intonation, 100% accuracy on lower octaves, better accuracy than 12-TET on higher octaves)

## Requirements

### Functional Requirements

**FR1: Multiple Tuning System Support**

- Equal temperament (12-TET)
- Just intonation (5-limit, 7-limit)
- Historical temperaments (well-tempered, meantone)
- Custom user-defined systems

**FR2: Scale/Mode Support**

- Common Western scales, modal scales
  - major
  - minor
  - ionian
  - dorian
  - phrygian
  - lydian
  - mixolydian
  - aeolian
  - locrian
  - melodicMinor
  - harmonicMinor
- Ethnic scales (pentatonic variants, exotic scales)
- Experimental scales (whole tone, octatonic, etc)
- User-defined custom scales
- All these scales must be diatonic/12-semitone compatible, no microtonal scales

**FR3: MIDI Integration**

- Convert MIDI note numbers (int and double) to AY periods
- Preserve MIDI octave standards (0 to 11)
- Handle out-of-range notes gracefully
- Accuracy tracking for approximations
  - In the DAW Tuning Settings Page
  - In the piano roll editor (snap notes to actual positions based on actual frequency from periods)

**FR4: Project Integration**

- Global project tuning settings
- Per-clip tuning overrides
- PSG file auto-detection of scale, key and tuning
- Consistent UI across all components

**FR5: Real-time Preview**

- Audio playback using calculated frequencies
- Visual period tables and accuracy indicators
- Interactive scale testing and comparison

### Non-Functional Requirements

**NFR1: Performance**

- MIDI-to-period lookup: O(1) constant time
- Tuning system switching: < 100ms response
- Realtime switching of tuning systems with no overhead, because per-clip tuning overrides
- Memory usage: < 1MB for all tuning data

**NFR2: Accuracy**

- Just intonation: 100% mathematical precision
- Equal temperament: > 99% accuracy for periods 10-4000
- Frequency calculations: ±0.1 Hz precision

**NFR3: Compatibility**

- JUCE ValueTree serialization support
- Thread-safe for real-time audio processing
- Compatible with existing MoTool clip system

## Architecture

### Core Components

```cpp
namespace MoTool {

// Interval representation (supports both equal-tempered and just)
class Interval {
public:
    static Interval fromSemitones(double semitones);
    static Interval fromRatio(int numerator, int denominator);
    static Interval fromRatio(const juce::String& ratio); // "3:2", "5:4", etc.

    double toSemitones() const;
    double toRatio() const;
    bool isJustInterval() const { return isRational; }

private:
    double value;        // Semitones or ratio
    bool isRational;     // true = just interval, false = equal tempered
    int num, den;        // For rational intervals
};

// Scale/mode definition
class Scale {
public:
    Scale(const String& name, const std::vector<Interval>& intervals);
    Scale(const String& name, const std::vector<int>& dividers); // For example, {48, 45, 40, 36, 32, 30, 27}

    String getName() const { return scaleName; }
    const std::vector<Interval>& getIntervals() const { return intervals; }
    int getNumSteps() const { return intervals.size(); }

    // Generate frequency ratios for given tuning system
    std::vector<double> getFrequencyRatios(const TuningSystem& tuning) const;

private:
    String scaleName;
    std::vector<Interval> intervals;  // In semitones or ratio notation
};


// Base tuning system interface
class TuningSystem {
public:
    virtual ~TuningSystem() = default;

    virtual String getName() const = 0;
    virtual TuningType getType() const = 0;

    // Core conversion functions
    virtual int midiNoteToPeriod(int midiNote) const = 0;
    virtual double midiNoteToFrequency(int midiNote) const = 0;
    virtual int periodToMidiNote(int period) const = 0;

    // Accuracy and validation
    virtual bool isNoteSupported(int midiNote) const = 0;
    virtual double getAccuracy(int midiNote) const = 0;
    virtual bool isEnvelopeCompatible(int midiNote) const = 0;

    // Serialization
    virtual juce::ValueTree getState() const = 0;
    virtual void setState(const juce::ValueTree& state) = 0;
};

}
```

### Concrete Implementations

```cpp
// Equal temperament implementation
class EqualTemperamentTuning : public TuningSystem {
public:
    EqualTemperamentTuning(double a4Frequency = 440.0);

    int midiNoteToPeriod(int midiNote) const override;
    double midiNoteToFrequency(int midiNote) const override;
    // ... other methods

private:
    double a4Freq;
    static constexpr double AY_CLOCK_HZ = 1773400.0;
    mutable std::array<int, 128> periodCache;
    mutable std::array<double, 128> accuracyCache;
};

// Just intonation implementation
class JustIntonationTuning : public TuningSystem {
public:
    JustIntonationTuning(const Scale& baseScale, int rootNote = 60);

    int midiNoteToPeriod(int midiNote) const override;
    // ... other methods

private:
    Scale baseScale;
    int rootMidiNote;
    std::map<int, int> periodLookup;  // Pre-computed for all valid notes
};

// User-defined custom tuning
class CustomTuning : public TuningSystem {
public:
    CustomTuning(const String& name);

    void setPeriodForNote(int midiNote, int period);
    void setFrequencyForNote(int midiNote, double frequency);

private:
    String customName;
    std::map<int, int> customPeriods;
};
```

### Manager and Factory

```cpp
class TuningManager : public juce::ValueTree::Listener {
public:
    TuningManager();
    ~TuningManager();

    // Tuning system management
    void registerTuning(std::unique_ptr<TuningSystem> tuning);
    TuningSystem* getTuning(const String& name) const;

    // Scale management
    void registerScale(const Scale& scale);
    const Scale* getScale(const String& name) const;
    std::vector<String> getAvailableScales() const;

    // Presets and serialization
    juce::ValueTree getState() const;
    void setState(const juce::ValueTree& state);
    void loadPresets();
    void savePresets();

    // Change notification
    std::function<void()> onTuningChanged;

private:
    std::map<String, std::unique_ptr<TuningSystem>> tunings;
    std::map<String, Scale> scales;
    juce::ValueTree state;

    void initializeDefaultTunings();
    void initializeDefaultScales();
};
```

## Implementation Strategy

### Phase 1: Core Infrastructure

**1.1 Base Classes (Week 1)**
```cpp
// Implement base interfaces
- TuningSystem abstract class
- Interval class with ratio/semitone support
- Scale class with interval storage
- TuningManager singleton
```

**1.2 Equal Temperament (Week 2)**
```cpp
// First concrete implementation for immediate testing
- EqualTemperamentTuning class
- MIDI note → period conversion with accuracy tracking
- Period → frequency calculations using AY clock
- Unit tests for conversion accuracy
```

**1.3 Just Intonation (Week 3)**
```cpp
// Core just intonation implementation
- JustIntonationTuning class
- Pre-computed period tables for common scales
- Support for 5-limit and 7-limit systems
- Scale-aware note mapping
```

### Phase 2: Integration

**2.1 ValueTree Integration (Week 4)**
```cpp
// Serialization and state management
- ValueTree serialization for all tuning data
- Project-level tuning settings storage
- Clip-level tuning override support
- Undo/redo support for tuning changes
```

**2.2 MidiClip Integration (Week 5)**
```cpp
// Integrate with existing MIDI system
class MidiClip : public tracktion::MidiClip {
    void setTuningOverride(const String& tuningName);
    TuningSystem* getEffectiveTuning() const;

private:
    juce::CachedValue<String> tuningOverride;
};

// Update MIDI → AY conversion in PsgMidiConverter
class PsgMidiConverter {
    void setTuningSystem(TuningSystem* tuning);
    // Update all conversion methods to use tuning system
};
```

**2.3 UI Components (Week 6)**
```cpp
// User interface components
class TuningSelector : public juce::ComboBox {
    void populateFromManager(TuningManager& manager);
    void setCurrentTuning(const String& name);
};

class ScaleSelector : public juce::ComboBox {
    void populateFromManager(TuningManager& manager);
};

class TuningPreviewComponent : public juce::Component {
    void playScale(const Scale& scale, const TuningSystem& tuning);
    void showPeriodTable();
};
```

### Phase 3: Advanced Features

**3.1 Auto-Detection (Week 7)**
```cpp
// PSG file analysis
class TuningDetector {
    TuningSystem* detectFromPsgData(const PsgData& data);
    double calculateFitness(const PsgData& data, const TuningSystem& tuning);
};
```

**3.2 Custom Tunings (Week 8)**
```cpp
// User-defined tuning systems
class CustomTuningEditor : public juce::Component {
    void editPeriodTable();
    void importFromScala(); // .scl file format support
    void exportToScala();
};
```

## Integration Points

### Edit Level Integration

```cpp
class Edit : public tracktion::Edit {
    TuningManager& getTuningManager() { return tuningManager; }

    void setGlobalTuning(const String& tuningName);
    TuningSystem* getGlobalTuning() const;

private:
    TuningManager tuningManager;
    juce::CachedValue<String> globalTuningName;
};
```

### Clip Level Integration

```cpp
// Update PsgClip to support tuning systems
class PsgClip : public MidiClip, public CustomClip {
public:
    void refreshFromTuningSystem();
    void convertMidiUsingTuning();

private:
    void onTuningChanged();
};
```

### UI Integration

```cpp
// Add tuning controls to main interface
class ProjectSettingsPanel : public juce::Component {
    TuningSelector* tuningSelector;
    ScaleSelector* scaleSelector;
    TuningPreviewComponent* previewComponent;
};

// Add tuning override to clip properties
class ClipPropertiesPanel : public juce::Component {
    TuningSelector* clipTuningOverride;
    juce::TextButton* useGlobalTuning;
};
```

## Data Structures and File Format

### Project Storage

```xml
<EDIT>
    <TUNING_SETTINGS>
        <GLOBAL_TUNING name="just_major_5limit"/>
        <AVAILABLE_TUNINGS>
            <TUNING name="equal_temperament_440" type="equal" a4="440.0"/>
            <TUNING name="just_major_5limit" type="just" scale="major_5limit" root="60"/>
            <TUNING name="custom_demo" type="custom" file="custom_tunings/demo.tun"/>
        </AVAILABLE_TUNINGS>
    </TUNING_SETTINGS>

    <TRACK id="track1">
        <CLIP type="PSG" tuning_override="equal_temperament_440">
            <!-- existing clip data -->
        </CLIP>
    </TRACK>
</EDIT>
```

### Scale Definitions

```cpp
// Built-in scale library
const std::vector<Scale> DEFAULT_SCALES = {
    {"major", {0, 2, 4, 5, 7, 9, 11, 12}},
    {"minor", {0, 2, 3, 5, 7, 8, 10, 12}},
    {"major_pentatonic", {0, 2, 4, 7, 9, 12}},
    {"minor_pentatonic", {0, 3, 5, 7, 10, 12}},
    {"dorian", {0, 2, 3, 5, 7, 9, 10, 12}},
    {"phrygian", {0, 1, 3, 5, 7, 8, 10, 12}},
    // ... more scales
};

const std::vector<Scale> JUST_SCALES = {
    {"major_5limit", {
        Interval::fromRatio("1:1"),
        Interval::fromRatio("9:8"),
        Interval::fromRatio("5:4"),
        Interval::fromRatio("4:3"),
        Interval::fromRatio("3:2"),
        Interval::fromRatio("5:3"),
        Interval::fromRatio("15:8"),
        Interval::fromRatio("2:1")
    }},
    // ... more just scales
};
```

## Testing Strategy

### Unit Tests

```cpp
class TuningSystemTests : public juce::UnitTest {
    void testEqualTemperamentAccuracy();
    void testJustIntonationPurity();
    void testMidiNoteConversion();
    void testPeriodValidation();
    void testSerializationRoundtrip();
};
```

### Integration Tests

```cpp
class TuningIntegrationTests : public juce::UnitTest {
    void testClipTuningOverride();
    void testPsgConversionAccuracy();
    void testUIStateConsistency();
    void testAutoDetection();
};
```

### Performance Tests

```cpp
void benchmarkConversionSpeed() {
    // Test MIDI → period conversion performance
    // Target: < 1μs per conversion
}

void benchmarkTuningSwitch() {
    // Test tuning system switching time
    // Target: < 100ms for UI responsiveness
}
```

## Future Considerations

### Live Performance Features

- MIDI controller mapping for tuning changes
- Real-time tuning modulation

### Extended Platform Support

- Other retro sound chips (SID, POKEY, etc.)
- Cross-platform tuning data exchange

## Conclusion

This design provides a flexible, extensible foundation for tuning systems in MoTool while maintaining compatibility with existing code. The phased implementation approach allows for incremental development and testing, ensuring stability while adding powerful new capabilities for demoscene production.

The system balances mathematical precision with practical usability, offering both familiar equal temperament and exotic just intonation options. This gives demoscene artists the choice between conventional harmony and the unique sonic possibilities that emerge from working directly with the AY chip's discrete period architecture.
