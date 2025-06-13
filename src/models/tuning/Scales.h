#pragma once

#include <JuceHeader.h>
#include <initializer_list>

namespace MoTool {

// Scale/mode definition
//==============================================================================
class Scale {
public:

    enum class ScaleCategory {
        DiatonicModes = 0,
        MinorVariations,
        HarmonicMinorModes,
        MelodicMinorModes,
        Pentatonic,
        Blues,
        Symmetrical,
        ExoticWorld,
        Bebop,
        UserDefined
    };

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
    static juce::String getNameForCategory(ScaleCategory category);
    static juce::String getShortNameForCategory(ScaleCategory category);
    static juce::String getNameForType(ScaleType type);
    static juce::String getShortNameForType(ScaleType type);
    static ScaleType getTypeFromName(juce::String name);

    std::vector<int> getIntervals(int octaves = 1) const;

private:
    ScaleType type;
    std::vector<int> customIntervals;  // For user-defined scales from initializer_list<int>
};

}