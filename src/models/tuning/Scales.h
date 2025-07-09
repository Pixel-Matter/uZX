#pragma once

#include <JuceHeader.h>
#include "../../util/enumchoice.h"

#include <initializer_list>
#include <string_view>

namespace MoTool {

// Scale/mode definition
//==============================================================================

enum Accidental {
    DoubleFlat = -2,
    Flat = -1,
    Natural = 0,
    Sharp = 1,
    DoubleSharp = 2
};

struct ScaleDegree {
    int degree;
    Accidental accidental;

    constexpr explicit ScaleDegree(int deg, Accidental alt = Accidental::Natural) noexcept
        : degree(deg), accidental(alt) {}

    constexpr ScaleDegree(int deg, int alt) noexcept
        : degree(deg), accidental(static_cast<Accidental>(alt)) {}

    inline constexpr bool operator==(const ScaleDegree& other) const noexcept {
        return degree == other.degree && accidental == other.accidental;
    }

    inline constexpr bool operator!=(const ScaleDegree& other) const noexcept {
        return !(*this == other);
    }

    inline constexpr std::string_view accidentalSymbolsView() const {
        switch(accidental) {
            case DoubleFlat:  return "♭♭";
            case Flat:        return "♭";
            case Natural:     return "";
            case Sharp:       return "♯";
            case DoubleSharp: return "♯♯";
            default:          return "?";
        }
    }
    String accidentalSymbols() const;
    String toString() const;
    operator String() const { return "Degree(" + String(degree) + ", " + String(accidental) + ")"; }

    // static Degree fromSemitones(int semitones, int degreeNum) noexcept;
    // int toSemitones() const noexcept;

};


class Scale {
public:
    using Degree = ScaleDegree;
    using Accidental = Accidental;

    struct TonicEnum {
        enum Enum {
            C = 0, CSharp, D, DSharp, E, F, FSharp, G, GSharp, A, ASharp, B
        };
        // static inline constexpr std::string_view labels[] {
        //     "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        // };
        static inline constexpr std::string_view longLabels[] {
            "C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"
        };
    };

    struct Tonic : public Util::EnumChoice<TonicEnum> {
        using Util::EnumChoice<TonicEnum>::EnumChoice;
        inline constexpr auto getName() const { return getLongLabel().data(); }
    };

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
            Hirajoshi,
            Iwato,
            In,
            Yo,
            Insen,
            // Chinese,

            // Blues
            BluesScale,
            MajorBlues,

            // Symmetrical
            WholeTone,
            DimHalfWhole,
            DimWholeHalf,
            Chromatic,
            Hexatonic,

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
            "Japanese Iwato",
            "Japanese In",
            "Japanese Yo",
            "Japanese Insen",
            // "Chinese Scale",

            // Blues
            "Blues Scale",
            "Major Blues",

            // Symmetrical
            "Whole Tone",
            "Hexatonic",
            "Diminished (H-W)",
            "Diminished (W-H)",
            "Chromatic",

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
            "MPent", "mPent",                                         // Pentatonic
            "Hirajo", "Iwato", "In", "Yo", "Insen",                   // Pentatonic Japanese
            // "Chinese",                                                // Pentatonic Chinese
            "Blues", "MajBlu",                                        // Blues
            "WholeTn", "Hex", "Dim(H-W)", "Dim(W-H)", "Chrom",        // Symmetrical
            "Persian", "Arabic", "Gypsy", "Enigma",                   // Exotic/World
            "DHrm", "Promet", "3Tone",                                // Exotic/World
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

    Scale(ScaleType t);
    explicit Scale(std::initializer_list<Steps> intervalSteps);
    explicit Scale(std::initializer_list<int> steps);  // for example 0, 2, 4, 5, 7, 9, 11 for a major scale

    static ScaleCategory getCategoryForType(ScaleType type);
    ScaleCategory getCategory() const { return getCategoryForType(type); }
    ScaleType getType() const { return type; }

    String getName() const;
    String getShortName() const;

    static std::vector<ScaleCategory> getAllScaleCategories();
    static std::vector<ScaleType> getAllScaleTypesForCategory(ScaleCategory category);
    static std::vector<ScaleType> getAllScaleTypes();

    static std::vector<String> getScaleStrings();
    static String getNameForCategory(ScaleCategory category);
    static String getShortNameForCategory(ScaleCategory category);
    static String getNameForType(ScaleType type);
    static String getShortNameForType(ScaleType type);
    static ScaleType getTypeFromName(String name);

    bool isIntervalInScale(int semitone) const;
    const std::vector<int>& getIntervals() const;
    std::vector<int> getIntervalsForOctaves(int octaves = 1) const;
    std::vector<Degree> getAllFlatsDegrees() const;
    std::vector<Degree> getDegrees() const;
    std::array<Degree, 12> getChromaticDegrees() const;

    // Key-related static methods
    static StringArray getAllKeyNames();
    static String getKeyName(Tonic key);

private:
    ScaleType type;
    std::vector<int> intervals;  // For all scales - both predefined and user-defined

    static inline constexpr std::array<int, 7> majorPattern {0, 2, 4, 5, 7, 9, 11}; // Major scale intervals
    static inline constexpr std::array<Degree, 12> chromaticDegrees {{
        Degree {1},  // Unison
        Degree {2, Accidental::Flat}, // Minor second
        Degree {2},  // Major second
        Degree {3, Accidental::Flat}, // Minor third
        Degree {3},  // Major third
        Degree {4},  // Perfect fourth
        Degree {5, Accidental::Flat}, // Diminished fifth
        Degree {5},  // Perfect fifth
        Degree {6, Accidental::Flat}, // Minor sixth
        Degree {6},  // Major sixth
        Degree {7, Accidental::Flat}, // Minor seventh
        Degree {7}   // Major seventh
    }};
};

}

namespace juce {

using namespace MoTool;
using namespace MoTool::Util;

template <>
struct juce::VariantConverter<Scale::Tonic> : public EnumVariantConverter<Scale::Tonic> {};

template <>
struct juce::VariantConverter<Scale::ScaleType> : public EnumVariantConverter<Scale::ScaleType> {};

}