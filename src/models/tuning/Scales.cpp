#include "Scales.h"

namespace MoTool {

// Constexpr function to get scale intervals
constexpr std::vector<int> getScaleIntervals(Scale::ScaleType scaleType) {
    switch (scaleType) {
        // Diatonic Modes
        case Scale::ScaleType::IonianOrMajor:    return {0, 2, 4, 5, 7, 9, 11};
        case Scale::ScaleType::Dorian:           return {0, 2, 3, 5, 7, 9, 10};
        case Scale::ScaleType::Phrygian:         return {0, 1, 3, 5, 7, 8, 10};
        case Scale::ScaleType::Lydian:           return {0, 2, 4, 6, 7, 9, 11};
        case Scale::ScaleType::Mixolydian:       return {0, 2, 4, 5, 7, 9, 10};
        case Scale::ScaleType::AeolianOrMinor:   return {0, 2, 3, 5, 7, 8, 10};
        case Scale::ScaleType::Locrian:          return {0, 1, 3, 5, 6, 8, 10};

        // Minor Variations
        case Scale::ScaleType::HarmonicMinor:    return {0, 2, 3, 5, 7, 8, 11};
        case Scale::ScaleType::MelodicMinor:     return {0, 2, 3, 5, 7, 9, 11};
        case Scale::ScaleType::NeapolitanMinor:  return {0, 1, 3, 5, 7, 8, 11};
        case Scale::ScaleType::NeapolitanMajor:  return {0, 1, 3, 5, 7, 9, 11};
        case Scale::ScaleType::HungarianMinor:   return {0, 2, 3, 6, 7, 8, 11};
        case Scale::ScaleType::HungarianMajor:   return {0, 3, 4, 6, 7, 9, 10};

        // Harmonic Minor Modes
        case Scale::ScaleType::LocrianNatural6:  return {0, 1, 3, 5, 6, 9, 10};
        case Scale::ScaleType::IonianSharp5:     return {0, 2, 4, 5, 8, 9, 11};
        case Scale::ScaleType::UkrainianDorian:  return {0, 2, 3, 6, 7, 9, 10};
        case Scale::ScaleType::PhrygianDominant: return {0, 1, 4, 5, 7, 8, 10};
        case Scale::ScaleType::LydianSharp2:     return {0, 3, 4, 6, 7, 9, 11};
        case Scale::ScaleType::AlteredDiminished: return {0, 1, 3, 4, 6, 8, 9};

        // Melodic Minor Modes
        case Scale::ScaleType::DorianFlat2:      return {0, 1, 3, 5, 7, 9, 10};
        case Scale::ScaleType::LydianAugmented:  return {0, 2, 4, 6, 8, 9, 11};
        case Scale::ScaleType::LydianDominant:   return {0, 2, 4, 6, 7, 9, 10};
        case Scale::ScaleType::MixolydianFlat6:  return {0, 2, 4, 5, 7, 8, 10};
        case Scale::ScaleType::HalfDiminished:   return {0, 2, 3, 5, 6, 8, 10};
        case Scale::ScaleType::AlteredScale:     return {0, 1, 3, 4, 6, 8, 10};

        // Pentatonic
        case Scale::ScaleType::MajorPentatonic:  return {0, 2, 4, 7, 9};
        case Scale::ScaleType::MinorPentatonic:  return {0, 3, 5, 7, 10};
        case Scale::ScaleType::JapaneseHirajoshi: return {0, 2, 3, 7, 8};
        case Scale::ScaleType::JapaneseIn:       return {0, 1, 5, 7, 8};
        case Scale::ScaleType::ChineseScale:     return {0, 4, 6, 7, 11};

        // Blues
        case Scale::ScaleType::BluesScale:       return {0, 3, 5, 6, 7, 10};
        case Scale::ScaleType::MajorBlues:       return {0, 2, 3, 4, 7, 9};

        // Symmetrical
        case Scale::ScaleType::WholeTone:        return {0, 2, 4, 6, 8, 10};
        case Scale::ScaleType::DiminishedHalfWhole: return {0, 1, 3, 4, 6, 7, 9, 10};
        case Scale::ScaleType::DiminishedWholeHalf: return {0, 2, 3, 5, 6, 8, 9, 11};
        case Scale::ScaleType::Chromatic:        return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        case Scale::ScaleType::Augmented:        return {0, 3, 4, 7, 8, 11};

        // Exotic/World
        case Scale::ScaleType::Persian:          return {0, 1, 4, 5, 6, 8, 11};
        case Scale::ScaleType::Arabic:           return {0, 1, 4, 5, 7, 8, 11};
        case Scale::ScaleType::Gypsy:            return {0, 2, 3, 6, 7, 8, 11};
        case Scale::ScaleType::Enigmatic:        return {0, 1, 4, 6, 8, 10, 11};
        case Scale::ScaleType::DoubleHarmonic:   return {0, 1, 4, 5, 7, 8, 11};
        case Scale::ScaleType::Prometheus:       return {0, 2, 4, 6, 9, 10};
        case Scale::ScaleType::Tritone:          return {0, 1, 4, 6, 7, 10};

        // Bebop
        case Scale::ScaleType::BebopMajor:       return {0, 2, 4, 5, 7, 8, 9, 11};
        case Scale::ScaleType::BebopDominant:    return {0, 2, 4, 5, 7, 9, 10, 11};
        case Scale::ScaleType::BebopMinor:       return {0, 2, 3, 5, 7, 8, 9, 10};

        // Default fallback
        case Scale::ScaleType::UserDefined:
        default:                                 return {0, 2, 4, 5, 7, 9, 11}; // Major scale fallback
    }
}

// Constexpr function to get scale category
constexpr Scale::ScaleCategory getScaleCategory(Scale::ScaleType scaleType) {
    switch (scaleType) {
        // Diatonic Modes
        case Scale::ScaleType::IonianOrMajor:
        case Scale::ScaleType::Dorian:
        case Scale::ScaleType::Phrygian:
        case Scale::ScaleType::Lydian:
        case Scale::ScaleType::Mixolydian:
        case Scale::ScaleType::AeolianOrMinor:
        case Scale::ScaleType::Locrian:
            return Scale::ScaleCategory::DiatonicModes;

        // Minor Variations
        case Scale::ScaleType::HarmonicMinor:
        case Scale::ScaleType::MelodicMinor:
        case Scale::ScaleType::NeapolitanMinor:
        case Scale::ScaleType::NeapolitanMajor:
        case Scale::ScaleType::HungarianMinor:
        case Scale::ScaleType::HungarianMajor:
            return Scale::ScaleCategory::MinorVariations;

        // Harmonic Minor Modes
        case Scale::ScaleType::LocrianNatural6:
        case Scale::ScaleType::IonianSharp5:
        case Scale::ScaleType::UkrainianDorian:
        case Scale::ScaleType::PhrygianDominant:
        case Scale::ScaleType::LydianSharp2:
        case Scale::ScaleType::AlteredDiminished:
            return Scale::ScaleCategory::HarmonicMinorModes;

        // Melodic Minor Modes
        case Scale::ScaleType::DorianFlat2:
        case Scale::ScaleType::LydianAugmented:
        case Scale::ScaleType::LydianDominant:
        case Scale::ScaleType::MixolydianFlat6:
        case Scale::ScaleType::HalfDiminished:
        case Scale::ScaleType::AlteredScale:
            return Scale::ScaleCategory::MelodicMinorModes;

        // Pentatonic
        case Scale::ScaleType::MajorPentatonic:
        case Scale::ScaleType::MinorPentatonic:
        case Scale::ScaleType::JapaneseHirajoshi:
        case Scale::ScaleType::JapaneseIn:
        case Scale::ScaleType::ChineseScale:
            return Scale::ScaleCategory::Pentatonic;

        // Blues
        case Scale::ScaleType::BluesScale:
        case Scale::ScaleType::MajorBlues:
            return Scale::ScaleCategory::Blues;

        // Symmetrical
        case Scale::ScaleType::WholeTone:
        case Scale::ScaleType::DiminishedHalfWhole:
        case Scale::ScaleType::DiminishedWholeHalf:
        case Scale::ScaleType::Chromatic:
        case Scale::ScaleType::Augmented:
            return Scale::ScaleCategory::Symmetrical;

        // Exotic/World
        case Scale::ScaleType::Persian:
        case Scale::ScaleType::Arabic:
        case Scale::ScaleType::Gypsy:
        case Scale::ScaleType::Enigmatic:
        case Scale::ScaleType::DoubleHarmonic:
        case Scale::ScaleType::Prometheus:
        case Scale::ScaleType::Tritone:
            return Scale::ScaleCategory::ExoticWorld;

        // Bebop
        case Scale::ScaleType::BebopMajor:
        case Scale::ScaleType::BebopDominant:
        case Scale::ScaleType::BebopMinor:
            return Scale::ScaleCategory::Bebop;

        // User Defined
        case Scale::ScaleType::UserDefined:
        default:
            return Scale::ScaleCategory::UserDefined;
    }
}

// Constructors
Scale::Scale(ScaleType scaleType) : type(scaleType) {
    // Intervals are retrieved via constexpr function getScaleIntervals()
}

Scale::Scale(std::initializer_list<Steps> intervalSteps) : type(ScaleType::UserDefined) {
    // Convert Steps enum to semitone intervals
    customIntervals.reserve(intervalSteps.size());
    int currentInterval = 0;
    for (auto step : intervalSteps) {
        customIntervals.push_back(currentInterval);
        switch (step) {
            case Steps::Whole:     currentInterval += 2; break;
            case Steps::Half:      currentInterval += 1; break;
            case Steps::WholeHalf: currentInterval += 3; break;
        }
    }
}

Scale::Scale(std::initializer_list<int> intervals) : type(ScaleType::UserDefined) {
    customIntervals = std::vector<int>(intervals);
}

// Static methods
Scale::ScaleCategory Scale::getCategoryForType(ScaleType scaleType) {
    return getScaleCategory(scaleType);
}

std::vector<Scale::ScaleCategory> Scale::getAllScaleCategories() {
    return {
        ScaleCategory::DiatonicModes,
        ScaleCategory::MinorVariations,
        ScaleCategory::HarmonicMinorModes,
        ScaleCategory::MelodicMinorModes,
        ScaleCategory::Pentatonic,
        ScaleCategory::Blues,
        ScaleCategory::Symmetrical,
        ScaleCategory::ExoticWorld,
        ScaleCategory::Bebop,
        ScaleCategory::UserDefined
    };
}

std::vector<Scale::ScaleType> Scale::getAllScaleTypesForCategory(ScaleCategory category) {
    std::vector<ScaleType> result;

    // Iterate through all scale types and check their category
    const std::vector<ScaleType> allTypes = getAllScaleTypes();
    for (const auto& scaleType : allTypes) {
        if (getScaleCategory(scaleType) == category) {
            result.push_back(scaleType);
        }
    }
    return result;
}

std::vector<Scale::ScaleType> Scale::getAllScaleTypes() {
    return {
        // Diatonic Modes
        ScaleType::IonianOrMajor, ScaleType::Dorian, ScaleType::Phrygian, ScaleType::Lydian,
        ScaleType::Mixolydian, ScaleType::AeolianOrMinor, ScaleType::Locrian,

        // Minor Variations
        ScaleType::HarmonicMinor, ScaleType::MelodicMinor, ScaleType::NeapolitanMinor,
        ScaleType::NeapolitanMajor, ScaleType::HungarianMinor, ScaleType::HungarianMajor,

        // Harmonic Minor Modes
        ScaleType::LocrianNatural6, ScaleType::IonianSharp5, ScaleType::UkrainianDorian,
        ScaleType::PhrygianDominant, ScaleType::LydianSharp2, ScaleType::AlteredDiminished,

        // Melodic Minor Modes
        ScaleType::DorianFlat2, ScaleType::LydianAugmented, ScaleType::LydianDominant,
        ScaleType::MixolydianFlat6, ScaleType::HalfDiminished, ScaleType::AlteredScale,

        // Pentatonic
        ScaleType::MajorPentatonic, ScaleType::MinorPentatonic, ScaleType::JapaneseHirajoshi,
        ScaleType::JapaneseIn, ScaleType::ChineseScale,

        // Blues
        ScaleType::BluesScale, ScaleType::MajorBlues,

        // Symmetrical
        ScaleType::WholeTone, ScaleType::DiminishedHalfWhole, ScaleType::DiminishedWholeHalf,
        ScaleType::Chromatic, ScaleType::Augmented,

        // Exotic/World
        ScaleType::Persian, ScaleType::Arabic, ScaleType::Gypsy, ScaleType::Enigmatic,
        ScaleType::DoubleHarmonic, ScaleType::Prometheus, ScaleType::Tritone,

        // Bebop
        ScaleType::BebopMajor, ScaleType::BebopDominant, ScaleType::BebopMinor,

        // User Defined
        ScaleType::UserDefined
    };
}

std::vector<juce::String> Scale::getScaleStrings() {
    std::vector<juce::String> result;
    for (const auto& scaleType : getAllScaleTypes()) {
        result.push_back(getNameForType(scaleType));
    }
    return result;
}

juce::String Scale::getNameForCategory(ScaleCategory category) {
    switch (category) {
        case ScaleCategory::DiatonicModes:      return "Diatonic Modes";
        case ScaleCategory::MinorVariations:    return "Minor Variations";
        case ScaleCategory::HarmonicMinorModes: return "Harmonic Minor Modes";
        case ScaleCategory::MelodicMinorModes:  return "Melodic Minor Modes";
        case ScaleCategory::Pentatonic:         return "Pentatonic";
        case ScaleCategory::Blues:              return "Blues";
        case ScaleCategory::Symmetrical:        return "Symmetrical";
        case ScaleCategory::ExoticWorld:        return "Exotic/World";
        case ScaleCategory::Bebop:              return "Bebop";
        case ScaleCategory::UserDefined:        return "User Defined";
        default:                                return "Unknown";
    }
}

juce::String Scale::getShortNameForCategory(ScaleCategory category) {
    switch (category) {
        case ScaleCategory::DiatonicModes:      return "Diatonic";
        case ScaleCategory::MinorVariations:    return "Minor";
        case ScaleCategory::HarmonicMinorModes: return "Harmonic";
        case ScaleCategory::MelodicMinorModes:  return "Melodic";
        case ScaleCategory::Pentatonic:         return "Pentatonic";
        case ScaleCategory::Blues:              return "Blues";
        case ScaleCategory::Symmetrical:        return "Symmetrical";
        case ScaleCategory::ExoticWorld:        return "Exotic";
        case ScaleCategory::Bebop:              return "Bebop";
        case ScaleCategory::UserDefined:        return "Custom";
        default:                                return "Unknown";
    }
}

juce::String Scale::getNameForType(ScaleType scaleType) {
    switch (scaleType) {
        // Diatonic Modes
        case ScaleType::IonianOrMajor:       return "Major (Ionian)";
        case ScaleType::Dorian:              return "Dorian";
        case ScaleType::Phrygian:            return "Phrygian";
        case ScaleType::Lydian:              return "Lydian";
        case ScaleType::Mixolydian:          return "Mixolydian";
        case ScaleType::AeolianOrMinor:      return "Minor (Aeolian)";
        case ScaleType::Locrian:             return "Locrian";

        // Minor Variations
        case ScaleType::HarmonicMinor:       return "Harmonic Minor";
        case ScaleType::MelodicMinor:        return "Melodic Minor";
        case ScaleType::NeapolitanMinor:     return "Neapolitan Minor";
        case ScaleType::NeapolitanMajor:     return "Neapolitan Major";
        case ScaleType::HungarianMinor:      return "Hungarian Minor";
        case ScaleType::HungarianMajor:      return "Hungarian Major";

        // Harmonic Minor Modes
        case ScaleType::LocrianNatural6:     return "Locrian ♮6";
        case ScaleType::IonianSharp5:        return "Ionian ♯5";
        case ScaleType::UkrainianDorian:     return "Ukrainian Dorian";
        case ScaleType::PhrygianDominant:    return "Phrygian Dominant";
        case ScaleType::LydianSharp2:        return "Lydian ♯2";
        case ScaleType::AlteredDiminished:   return "Altered Diminished";

        // Melodic Minor Modes
        case ScaleType::DorianFlat2:         return "Dorian ♭2";
        case ScaleType::LydianAugmented:     return "Lydian Augmented";
        case ScaleType::LydianDominant:      return "Lydian Dominant";
        case ScaleType::MixolydianFlat6:     return "Mixolydian ♭6";
        case ScaleType::HalfDiminished:      return "Half Diminished";
        case ScaleType::AlteredScale:        return "Altered Scale";

        // Pentatonic
        case ScaleType::MajorPentatonic:     return "Major Pentatonic";
        case ScaleType::MinorPentatonic:     return "Minor Pentatonic";
        case ScaleType::JapaneseHirajoshi:   return "Japanese Hirajoshi";
        case ScaleType::JapaneseIn:          return "Japanese In";
        case ScaleType::ChineseScale:        return "Chinese Scale";

        // Blues
        case ScaleType::BluesScale:          return "Blues Scale";
        case ScaleType::MajorBlues:          return "Major Blues";

        // Symmetrical
        case ScaleType::WholeTone:           return "Whole Tone";
        case ScaleType::DiminishedHalfWhole: return "Diminished (H-W)";
        case ScaleType::DiminishedWholeHalf: return "Diminished (W-H)";
        case ScaleType::Chromatic:           return "Chromatic";
        case ScaleType::Augmented:           return "Augmented";

        // Exotic/World
        case ScaleType::Persian:             return "Persian";
        case ScaleType::Arabic:              return "Arabic";
        case ScaleType::Gypsy:               return "Gypsy";
        case ScaleType::Enigmatic:           return "Enigmatic";
        case ScaleType::DoubleHarmonic:      return "Double Harmonic";
        case ScaleType::Prometheus:          return "Prometheus";
        case ScaleType::Tritone:             return "Tritone";

        // Bebop
        case ScaleType::BebopMajor:          return "Bebop Major";
        case ScaleType::BebopDominant:       return "Bebop Dominant";
        case ScaleType::BebopMinor:          return "Bebop Minor";

        case ScaleType::UserDefined:         return "User Defined";
        default:                             return "Unknown Scale";
    }
}

juce::String Scale::getShortNameForType(ScaleType scaleType) {
    switch (scaleType) {
        // Diatonic Modes
        case ScaleType::IonianOrMajor:    return "Maj";
        case ScaleType::Dorian:           return "Dor";
        case ScaleType::Phrygian:         return "Phr";
        case ScaleType::Lydian:           return "Lyd";
        case ScaleType::Mixolydian:       return "Mix";
        case ScaleType::AeolianOrMinor:   return "Min";
        case ScaleType::Locrian:          return "Loc";

        // Minor Variations
        case ScaleType::HarmonicMinor:    return "HMin";
        case ScaleType::MelodicMinor:     return "MMin";
        case ScaleType::NeapolitanMinor:  return "NMin";
        case ScaleType::NeapolitanMajor:  return "NMaj";
        case ScaleType::HungarianMinor:   return "HuMin";
        case ScaleType::HungarianMajor:   return "HuMaj";

        // Harmonic Minor Modes
        case ScaleType::LocrianNatural6:  return "Loc♮6";
        case ScaleType::IonianSharp5:     return "Ion♯5";
        case ScaleType::UkrainianDorian:  return "UkrDor";
        case ScaleType::PhrygianDominant: return "PhrDom";
        case ScaleType::LydianSharp2:     return "Lyd♯2";
        case ScaleType::AlteredDiminished: return "AltDim";

        // Melodic Minor Modes
        case ScaleType::DorianFlat2:      return "Dor♭2";
        case ScaleType::LydianAugmented:  return "LydAug";
        case ScaleType::LydianDominant:   return "LydDom";
        case ScaleType::MixolydianFlat6:  return "Mix♭6";
        case ScaleType::HalfDiminished:   return "HalfDim";
        case ScaleType::AlteredScale:     return "Alt";

        // Pentatonic
        case ScaleType::MajorPentatonic:  return "MPent";
        case ScaleType::MinorPentatonic:  return "mPent";
        case ScaleType::JapaneseHirajoshi: return "Hirajo";
        case ScaleType::JapaneseIn:       return "In";
        case ScaleType::ChineseScale:     return "Chinese";

        // Blues
        case ScaleType::BluesScale:       return "Blues";
        case ScaleType::MajorBlues:       return "MajBlu";

        // Symmetrical
        case ScaleType::WholeTone:        return "WholeTn";
        case ScaleType::DiminishedHalfWhole: return "Dim(H-W)";
        case ScaleType::DiminishedWholeHalf: return "Dim(W-H)";
        case ScaleType::Chromatic:        return "Chrom";
        case ScaleType::Augmented:        return "Aug";

        // Exotic/World
        case ScaleType::Persian:          return "Persian";
        case ScaleType::Arabic:           return "Arabic";
        case ScaleType::Gypsy:            return "Gypsy";
        case ScaleType::Enigmatic:        return "Enigma";
        case ScaleType::DoubleHarmonic:   return "DHrm";
        case ScaleType::Prometheus:       return "Prometh";
        case ScaleType::Tritone:          return "Tritone";

        // Bebop
        case ScaleType::BebopMajor:       return "BebMaj";
        case ScaleType::BebopDominant:    return "BebDom";
        case ScaleType::BebopMinor:       return "BebMin";

        case ScaleType::UserDefined:      return "Custom";
        default:                          return "Unknown";
    }
}

Scale::ScaleType Scale::getTypeFromName(juce::String name) {
    for (const auto& scaleType : getAllScaleTypes()) {
        if (getNameForType(scaleType) == name) {
            return scaleType;
        }
    }
    return ScaleType::UserDefined;
}

// Instance methods
juce::String Scale::getName() const {
    return getNameForType(type);
}

juce::String Scale::getShortName() const {
    return getShortNameForType(type);
}

std::vector<int> Scale::getIntervals(int octaves) const {
    std::vector<int> intervals;

    if (type == ScaleType::UserDefined && !customIntervals.empty()) {
        // Use custom intervals for user-defined scales
        intervals = customIntervals;
    } else {
        // Use constexpr function to get predefined intervals
        intervals = getScaleIntervals(type);
    }

    if (octaves <= 1) {
        return intervals;
    }

    // Extend across multiple octaves
    std::vector<int> result = intervals;
    size_t originalSize = intervals.size();

    for (int octave = 1; octave < octaves; ++octave) {
        for (size_t i = 0; i < originalSize; ++i) {
            result.push_back(intervals[i] + octave * 12);
        }
    }

    return result;
}

} // namespace MoTool