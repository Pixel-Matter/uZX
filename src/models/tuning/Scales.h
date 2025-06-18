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
            User
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
            "User"
        };
    };

    using ScaleCategory = Util::EnumChoice<ScaleCategoryEnum>;

    struct ScaleTypeEnum {
        enum Enum {
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
            User
        };

        static inline constexpr std::string_view longLabels[] {
            "Major (Ionian)",
            "Dorian",
            "Phrygian",
            "Lydian",
            "Mixolydian",
            "Minor (Aeolian)",
            "Locrian",

            // Minor Variations
            "Harmonic Minor",
            "Melodic Minor",
            "Neapolitan Minor",
            "Neapolitan Major",
            "Hungarian Minor",
            "Hungarian Major",

            // Harmonic Minor Modes
            "Locrian ♮6",
            "Ionian ♯5",
            "Ukrainian Dorian",
            "Phrygian Dominant",
            "Lydian ♯2",
            "Altered Diminished",

            // Melodic Minor Modes
            "Dorian ♭2",
            "Lydian Augmented",
            "Lydian Dominant",
            "Mixolydian ♭6",
            "Half Diminished",
            "Altered Scale",

            // Pentatonic
            "Major Pentatonic",
            "Minor Pentatonic",
            "Japanese Hirajoshi",
            "Japanese In",
            "Chinese Scale",

            // Blues
            "Blues Scale",
            "Major Blues",

            // Symmetrical
            "Whole Tone",
            "Diminished (H-W)",
            "Diminished (W-H)",
            "Chromatic",
            "Augmented",

            // Exotic/World
            "Persian",
            "Arabic",
            "Gypsy",
            "Enigmatic",
            "Double Harmonic",
            "Prometheus",
            "Tritone",

            // Bebop
            "Bebop Major",
            "Bebop Dominant",
            "Bebop Minor",

            // User Defined
            "User Defined"
        };

        static inline constexpr std::string_view shortLabels[] {
            "Maj", "Dor", "Phr", "Lyd", "Mix", "Min", "Loc",          // Diatonic Modes
            "HMin", "MMin", "NMin", "NMaj", "HuMin", "HuMaj",         // Minor Variations
            "Loc♮6", "Ion♯5", "UkrDor", "PhrDom", "Lyd♯2", "AltDim",  // Harmonic Minor Modes
            "Dor♭2", "LydAug", "LydDom", "Mix♭6", "HalfDim", "Alt",   // Melodic Minor Modes
            "MPent", "mPent", "Hirajo", "In", "Chinese",              // Pentatonic
            "Blues", "MajBlu",                                        // Blues
            "WholeTn", "Dim(H-W)", "Dim(W-H)", "Chrom", "Aug",        // Symmetrical
            "Persian", "Arabic", "Gypsy", "Enigma",                   // Exotic/World
            "DHrm", "Prometheus", "Tritone",                          // Exotic/World
            "BebMaj", "BebDom", "BebMin",                             // Bebop
            "User"                                                    // User Defined
        };
    };

    using ScaleType = Util::EnumChoice<ScaleTypeEnum>;

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

    std::vector<int> getIntervals(int octaves = 1) const;

private:
    ScaleType type;
    std::vector<int> intervals;  // For all scales - both predefined and user-defined
};


}