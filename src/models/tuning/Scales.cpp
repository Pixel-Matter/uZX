#include "Scales.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include <cstddef>
#include <string_view>

namespace MoTool {

// Constexpr function to get scale intervals
constexpr std::vector<int> getScaleIntervals(Scale::ScaleType scaleType) {
    using ScaleType = Scale::ScaleType;
    switch (scaleType) {
        // Diatonic Modes                         1     2     3  4     5     6     7
        case ScaleType::IonianOrMajor:    return {0,    2,    4, 5,    7,    9,    11};
        //                                        1     2  b3    4     5     6  b7
        case ScaleType::Dorian:           return {0,    2, 3,    5,    7,    9, 10   };
        //                                        1  b2    b3    4     5  b6    b7
        case ScaleType::Phrygian:         return {0, 1,    3,    5,    7, 8,    10   };
        //                                        1     2     3    #4  5     6     7
        case ScaleType::Lydian:           return {0,    2,    4,    6, 7,    9,    11};
        //                                        1     2     3  4     5     6  b7
        case ScaleType::Mixolydian:       return {0,    2,    4, 5,    7,    9, 10   };
        //                                        1     2  b3    4     5  b6    b7
        case ScaleType::AeolianOrMinor:   return {0,    2, 3,    5,    7, 8,    10   };
        //                                        1  b2    b3    4  b5    b6    b7
        case ScaleType::Locrian:          return {0, 1,    3,    5, 6,    8,    10   };

        // Minor Variations
        case ScaleType::HarmonicMinor:    return {0,    2, 3,    5,    7, 8,       11};
        case ScaleType::MelodicMinor:     return {0,    2, 3,    5,    7,    9,    11};
        case ScaleType::NeapolitanMinor:  return {0, 1,    3,    5,    7, 8,       11};
        case ScaleType::NeapolitanMajor:  return {0, 1,    3,    5,    7,    9,    11};
        //                                        1     2  b3      #4  5 b6        7
        case ScaleType::HungarianMinor:   return {0,    2, 3,       6, 7, 8,       11};
        //                                        1       #2  3    #4  5     6  b7
        case ScaleType::HungarianMajor:   return {0,       3, 4,    6, 7,    9, 10   };

        // Harmonic Minor Modes
        case ScaleType::LocrianNatural6:  return {0, 1,    3,    5, 6,       9, 10    };
        case ScaleType::IonianSharp5:     return {0,    2,    4, 5,       8, 9,     11};
        case ScaleType::UkrainianDorian:  return {0,    2, 3,       6, 7,    9, 10    };
        case ScaleType::PhrygianDominant: return {0, 1,       4, 5,    7, 8,    10    };
        case ScaleType::LydianSharp2:     return {0,       3, 4,    6, 7,    9,     11};
        case ScaleType::AlteredDiminished:return {0, 1,    3, 4,    6,    8, 9        };

        // Melodic Minor Modes
        case ScaleType::DorianFlat2:      return {0, 1,    3,    5,    7,    9, 10   };
        case ScaleType::LydianAugmented:  return {0,    2,    4,    6,    8, 9,    11};
        case ScaleType::LydianDominant:   return {0,    2,    4,    6, 7,    9, 10   };
        case ScaleType::MixolydianFlat6:  return {0,    2,    4, 5,    7, 8,    10   };
        case ScaleType::HalfDiminished:   return {0,    2, 3,    5, 6,    8,    10   };
        case ScaleType::AlteredScale:     return {0, 1,    3, 4,    6,    8,    10   };

        // Rules of assigning degrees for pentatonic and blues scales:
        // - Can not be two gaps in a sequence
        // - if the step is more than whole tone, it is a gap in degree sequence
        // - If there is pure quint, it is 5
        //   - Else if there is pure quart, it is 4
        //   - Else it is not a real pentatonic
        // - If there is a 3 half-tone steps sequence, it is N-1, bN, N
        // or simply
        // All b-s (with omitted degrees)

        // Pentatonic                             1     2     3 (4)    5     6    (7)
        case ScaleType::MajorPentatonic:  return {0,    2,    4,       7,    9        };
        //                                        1    (2) b3    4     5    (6) b7
        case ScaleType::MinorPentatonic:  return {0,       3,    5,    7,       10    };
        //                                        1     2  b3   (4)    5  b6      (7)
        case ScaleType::Hirajoshi:        return {0,    2, 3,          7, 8           };
        //                                        1  b2      (3) 4  b5   (6)    b7
        case ScaleType::Iwato:            return {0, 1,          5, 6,          10    };
        //                                        1  b2      (3) 4     5  b6      (7)
        case ScaleType::In:               return {0, 1,          5,    7, 8           };
        //                                        1     2    (3) 4     5     6    (7)
        case ScaleType::Yo:               return {0,    2,       5,    7,    9        };
        //                                        1  b2      (3) 4     5    (6) b7
        case ScaleType::Insen:            return {0, 1,          5,    7,       10    };
        // //                                        1     2     3 (4)    5     6     (7)    !!! Or not all b-s?
        // case ScaleType::Chinese:          return {0,    2,    4,       7,    9        };

        // Blues                                  1    (2) b3    4  b5 5    (6) b7        !!! Pentatonic with added note or all b-s
        case ScaleType::BluesScale:       return {0,       3,    5, 6, 7,       10    };
        //                                        1     2  b3 3        5     6     (7)    !!! Pentatonic with added note  or all b-s
        case ScaleType::MajorBlues:       return {0,    2, 3, 4,       7,    9        };

        // Symmetrical 6                          1     2     3    #4    #5     #6  (7)   !!! same logic as for 7-degree scales or all b-s
        case ScaleType::WholeTone:        return {0,    2,    4,    6,    8,    10    };
        //                                        1    (2) b3 b4       5  b6        7     !!! Pentatonic logic bit for 6 degrees or all b-s
        case ScaleType::Hexatonic:        return {0,       3, 4,       7, 8,        11};
        // Symmetrical 8                          1  b2    b3 3    b5  5     6  b7        !!! All b-s?
        case ScaleType::DimHalfWhole:     return {0, 1,    3, 4,    6, 7,    9, 10    };
        //                                        1     2  b3    4  b5    b6 6       7    !!! All b-s?
        case ScaleType::DimWholeHalf:     return {0,    2, 3,    5, 6,    8, 9,     11};
        //                                        1   b2 2  b3 3  4  b5 5  b6 6  b7  7    !!! All b-s?
        case ScaleType::Chromatic:        return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

        // Exotic/World
        case ScaleType::Persian:          return {0, 1,       4, 5, 6,    8,        11};
        case ScaleType::Arabic:           return {0, 1,       4, 5, 7,    8,        11};
        case ScaleType::Gypsy:            return {0,    2, 3,       6, 7, 8,        11};
        case ScaleType::Enigmatic:        return {0, 1,       4,    6,    8,    10, 11};
        case ScaleType::DoubleHarmonic:   return {0, 1,       4, 5,    7, 8,        11};
        case ScaleType::Prometheus:       return {0,    2,    4,    6,       9, 10    };
        case ScaleType::Tritone:          return {0, 1,       4,    6, 7,       10    };

        // Bebop
        case ScaleType::BebopMajor:       return {0, 2, 4, 5, 7, 8, 9, 11};
        case ScaleType::BebopDominant:    return {0, 2, 4, 5, 7, 9, 10, 11};
        case ScaleType::BebopMinor:       return {0, 2, 3, 5, 7, 8, 9, 10};

        // Default fallback
        case ScaleType::User:
        default:                          return {};
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
            return Scale::ScaleCategory::Diatonic;

        // Minor Variations
        case Scale::ScaleType::HarmonicMinor:
        case Scale::ScaleType::MelodicMinor:
        case Scale::ScaleType::NeapolitanMinor:
        case Scale::ScaleType::NeapolitanMajor:
        case Scale::ScaleType::HungarianMinor:
        case Scale::ScaleType::HungarianMajor:
            return Scale::ScaleCategory::Minor;

        // Harmonic Minor Modes
        case Scale::ScaleType::LocrianNatural6:
        case Scale::ScaleType::IonianSharp5:
        case Scale::ScaleType::UkrainianDorian:
        case Scale::ScaleType::PhrygianDominant:
        case Scale::ScaleType::LydianSharp2:
        case Scale::ScaleType::AlteredDiminished:
            return Scale::ScaleCategory::Harmonic;

        // Melodic Minor Modes
        case Scale::ScaleType::DorianFlat2:
        case Scale::ScaleType::LydianAugmented:
        case Scale::ScaleType::LydianDominant:
        case Scale::ScaleType::MixolydianFlat6:
        case Scale::ScaleType::HalfDiminished:
        case Scale::ScaleType::AlteredScale:
            return Scale::ScaleCategory::Melodic;

        // Pentatonic
        case Scale::ScaleType::MajorPentatonic:
        case Scale::ScaleType::MinorPentatonic:
        case Scale::ScaleType::Hirajoshi:
        case Scale::ScaleType::Iwato:
        case Scale::ScaleType::In:
        case Scale::ScaleType::Yo:
        case Scale::ScaleType::Insen:
        // case Scale::ScaleType::Chinese:
            return Scale::ScaleCategory::Pentatonic;

        // Blues
        case Scale::ScaleType::BluesScale:
        case Scale::ScaleType::MajorBlues:
            return Scale::ScaleCategory::Blues;

        // Symmetrical
        case Scale::ScaleType::WholeTone:
        case Scale::ScaleType::Hexatonic:
        case Scale::ScaleType::DimHalfWhole:
        case Scale::ScaleType::DimWholeHalf:
        case Scale::ScaleType::Chromatic:
            return Scale::ScaleCategory::Symmetrical;

        // Exotic/World
        case Scale::ScaleType::Persian:
        case Scale::ScaleType::Arabic:
        case Scale::ScaleType::Gypsy:
        case Scale::ScaleType::Enigmatic:
        case Scale::ScaleType::DoubleHarmonic:
        case Scale::ScaleType::Prometheus:
        case Scale::ScaleType::Tritone:
            return Scale::ScaleCategory::Exotic;

        // Bebop
        case Scale::ScaleType::BebopMajor:
        case Scale::ScaleType::BebopDominant:
        case Scale::ScaleType::BebopMinor:
            return Scale::ScaleCategory::Bebop;

        // User Defined
        case Scale::ScaleType::User:
        default:
            return Scale::ScaleCategory::User;
    }
}

Scale::Scale(ScaleType t)
    : type(t)
    , intervals(getScaleIntervals(t))
{
    jassert(type != ScaleType::User && "UserDefined scale type should not be used here");
}

Scale::Scale(std::initializer_list<int> ivals)
    : type(ScaleType::User)
    , intervals(ivals)
{
}

String Scale::Degree::accidentalSymbols() const {
    return String::fromUTF8(accidentalSymbolsView().data());
}


String Scale::Degree::toString() const {
    return accidentalSymbols() + String(degree);
}

// Scale::Degree Scale::Degree::fromSemitones(int semitones, int degreeNum) noexcept {
//     // Normalize semitones to the range [0, 11]
//     // degreeNum must be 1-based (1 for root, 2 for second, etc.)

//     static constexpr std::array<int, 7> majorSemitones {0, 2, 4, 5, 7, 9, 11}; // Major scale intervals
//     // FIXME this is wrong
//     jassert(degreeNum >= 1 && degreeNum <= 8 && "Degree number must be between 1 and 8");
//     semitones = (semitones % 12 + 12) % 12;
//     return Scale::Degree(degreeNum, static_cast<Accidental>(semitones - degreeNum));
// }

// int Scale::Degree::toSemitones() const noexcept {
//     return (degree - 1) + static_cast<int>(accidental);
// }

Scale::Scale(std::initializer_list<Steps> intervalSteps)
    : type(ScaleType::User)
{
    // Convert Steps enum to semitone intervals
    intervals.reserve(intervalSteps.size());
    int currentInterval = 0;
    for (auto step : intervalSteps) {
        intervals.push_back(currentInterval);
        switch (step) {
            case Steps::Whole:     currentInterval += 2; break;
            case Steps::Half:      currentInterval += 1; break;
            case Steps::WholeHalf: currentInterval += 3; break;
        }
    }
}

// Static methods
Scale::ScaleCategory Scale::getCategoryForType(ScaleType scaleType) {
    return getScaleCategory(scaleType);
}

std::vector<Scale::ScaleCategory> Scale::getAllScaleCategories() {
    return {
        ScaleCategory::Diatonic,
        ScaleCategory::Minor,
        ScaleCategory::Harmonic,
        ScaleCategory::Melodic,
        ScaleCategory::Pentatonic,
        ScaleCategory::Blues,
        ScaleCategory::Symmetrical,
        ScaleCategory::Exotic,
        ScaleCategory::Bebop,
        ScaleCategory::User
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
        ScaleType::MajorPentatonic, ScaleType::MinorPentatonic,
        ScaleType::Hirajoshi, ScaleType::Iwato, ScaleType::In, ScaleType::Yo, ScaleType::Insen,
        // ScaleType::ChineseScale,

        // Blues
        ScaleType::BluesScale, ScaleType::MajorBlues,

        // Symmetrical
        ScaleType::WholeTone, ScaleType::DimHalfWhole, ScaleType::DimWholeHalf,
        ScaleType::Chromatic, ScaleType::Hexatonic,

        // Exotic/World
        ScaleType::Persian, ScaleType::Arabic, ScaleType::Gypsy, ScaleType::Enigmatic,
        ScaleType::DoubleHarmonic, ScaleType::Prometheus, ScaleType::Tritone,

        // Bebop
        ScaleType::BebopMajor, ScaleType::BebopDominant, ScaleType::BebopMinor,

        // User Defined
        ScaleType::User
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
    auto view = category.getLongLabel();
    return juce::String(view.data(), view.size());
}


juce::String Scale::getShortNameForCategory(ScaleCategory category) {
    auto view = category.getLabel();
    return juce::String(view.data(), view.size());
}


juce::String Scale::getNameForType(ScaleType scaleType) {
    auto view = scaleType.getLongLabel();
    return juce::String::fromUTF8(view.data(), static_cast<int>(view.size()));
}


juce::String Scale::getShortNameForType(ScaleType scaleType) {
    auto view = scaleType.getShortLabel();
    return juce::String::fromUTF8(view.data(), static_cast<int>(view.size()));
}

Scale::ScaleType Scale::getTypeFromName(juce::String name) {
    for (const auto& scaleType : getAllScaleTypes()) {
        if (getNameForType(scaleType) == name) {
            return scaleType;
        }
    }
    return ScaleType::User;
}

// Instance methods
juce::String Scale::getName() const {
    return getNameForType(type);
}

juce::String Scale::getShortName() const {
    return getShortNameForType(type);
}

bool Scale::isIntervalInScale(int semitone) const {
    semitone = (semitone % 12 + 12) % 12; // Normalize to [0, 11]
    return std::find(intervals.begin(), intervals.end(), semitone) != intervals.end();
}

const std::vector<int>& Scale::getIntervals() const {
    return intervals;
}

std::vector<int> Scale::getIntervalsForOctaves(int octaves) const {
    if (octaves <= 1) {
        return intervals;
    }

    // Extend across multiple octaves
    std::vector<int> result = intervals;
    size_t originalSize = intervals.size();

    for (int octave = 1; octave < octaves; ++octave) {
        for (size_t i = 0; i < originalSize; ++i) {
            result.push_back(intervals.at(i) + octave * 12);
        }
    }

    return result;
}

std::vector<Scale::Degree> Scale::getAllFlatsDegrees() const {
    std::vector<Degree> result;
    result.reserve(intervals.size());
    // map to chromatic degrees
    for (const auto& interval : intervals) {
        result.emplace_back(chromaticDegrees.at((size_t) interval));
    }
    return result;
}

std::vector<Scale::Degree> Scale::getDegrees() const {
    std::vector<Degree> degrees;
    degrees.reserve(intervals.size());
    if (intervals.size() == 7) {
        for (size_t i = 0; i < 7; i++) {
            degrees.emplace_back(
                static_cast<int>(i + 1), // Degree number (1-based)
                static_cast<Accidental>(intervals.at(i) - majorPattern.at(i)) // Calculate accidental
            );
        }
        return degrees;
    }
    return getAllFlatsDegrees();
}

std::array<Scale::Degree, 12> Scale::getChromaticDegrees() const {
    const auto degrees = getDegrees();
    // merge with chromatic degrees
    std::array<Degree, 12> chromaticDegreesArray = chromaticDegrees;
    for (size_t i = 0; i < intervals.size(); ++i) {
        chromaticDegreesArray.at(static_cast<size_t>(intervals[i])) = degrees.at(i);
    }
    return chromaticDegreesArray;
}

} // namespace MoTool