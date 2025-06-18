#pragma once

#include <JuceHeader.h>
#include "../../util/enumchoice.h"

#include <initializer_list>
#include <string_view>

namespace MoTool {

// Scale/mode definition
//==============================================================================
class Scale {
public:

    struct ScaleCategoryEnum {
        enum Enum {
            Diatonic = 0,
            Minor,
            Harmonic,
            Melodic,
            Pentatonic,
            Blues,
            Symmetrical,
            Exotic,
            Bebop,
            Custom
        };

        static inline constexpr std::string_view longLabels[] {
            "Diatonic Modes",
            "Minor Variations",
            "Harmonic Minor Modes",
            "Melodic Minor Modes",
            "Pentatonic",
            "Blues",
            "Symmetrical",
            "Exotic/World",
            "Bebop",
            "Custom"
        };
    };

    using ScaleCategory = Util::EnumChoice<ScaleCategoryEnum>;

    enum class ScaleType {
        // DiatonicModes
        IonianOrMajor = 0,  // I
        Dorian,             // II
        Phrygian,           // III
        Lydian,             // IV
        Mixolydian,         // V
        AeolianOrMinor,     // VI
        Locrian,            // VII

        // MinorVariations
        HarmonicMinor,
        MelodicMinor,
        NeapolitanMinor,
        NeapolitanMajor,
        HungarianMinor,
        HungarianMajor,

        // HarmonicMinorModes
        LocrianNatural6,
        IonianSharp5,
        UkrainianDorian,
        PhrygianDominant,
        LydianSharp2,
        AlteredDiminished,

        // MelodicMinorModes
        DorianFlat2,
        LydianAugmented,
        LydianDominant,
        MixolydianFlat6,
        HalfDiminished,
        AlteredScale,

        // Pentatonic
        MajorPentatonic,
        MinorPentatonic,
        JapaneseHirajoshi,
        JapaneseIn,
        ChineseScale,

        // Blues
        BluesScale,
        MajorBlues,

        // Symmetrical
        WholeTone,
        DiminishedHalfWhole,
        DiminishedWholeHalf,
        Chromatic,
        Augmented,

        // ExoticWorld
        Persian,
        Arabic,
        Gypsy,
        Enigmatic,
        DoubleHarmonic,
        Prometheus,
        Tritone,

        // Bebop
        BebopMajor,
        BebopDominant,
        BebopMinor,

        // UserDefined
        UserDefined
    };

    enum Steps {
        Whole = 0,
        Half,
        WholeHalf,
    };

    Scale(ScaleType type);
    Scale(std::initializer_list<Steps> intervalSteps);
    Scale(std::initializer_list<int> steps);  // for example 0, 2, 4, 5, 7, 9, 11 for a major scale

    static ScaleCategory getCategoryForType(ScaleType type);
    ScaleCategory getCategory() const { return getCategoryForType(type); }
    ScaleType getType() const { return type; }

    juce::String getName() const;
    juce::String getShortName() const;

    static std::vector<ScaleCategory> getAllScaleCategories();
    static std::vector<ScaleType> getAllScaleTypesForCategory(ScaleCategory category);
    static std::vector<ScaleType> getAllScaleTypes();

    static std::vector<juce::String> getScaleStrings();
    // juce::String getters for compatibility
    static juce::String getNameForCategory(ScaleCategory category);
    static juce::String getShortNameForCategory(ScaleCategory category);
    static juce::String getNameForType(ScaleType type);
    static juce::String getShortNameForType(ScaleType type);
    static ScaleType getTypeFromName(juce::String name);

    static constexpr std::string_view getNameForTypeView(ScaleType type);
    static constexpr std::string_view getShortNameForTypeView(ScaleType type);

    std::vector<int> getIntervals(int octaves = 1) const;

private:
    ScaleType type;
    std::vector<int> intervals;  // For all scales - both predefined and user-defined
};

// Implementation of constexpr functions that are too large for inline
constexpr std::string_view Scale::getNameForTypeView(ScaleType scaleType) {
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

constexpr std::string_view Scale::getShortNameForTypeView(ScaleType scaleType) {
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

}