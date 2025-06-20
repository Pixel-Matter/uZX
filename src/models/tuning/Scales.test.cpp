#include <JuceHeader.h>
#include "Scales.h"
#include <string_view>

using namespace MoTool;

class ScaleTest : public juce::UnitTest {
public:
    using Degree = Scale::Degree;
    using Alteration = Scale::Accidental;

    ScaleTest() : UnitTest("Scale", "MoTool") {}

    void runTest() override {
        beginTest("Scale construction from initializer list - Major scale");
        {
            Scale majorScale({0, 2, 4, 5, 7, 9, 11});
            auto intervals = majorScale.getIntervals();

            expectEquals(static_cast<int>(intervals.size()), 7);
            expectEquals(intervals[0], 0);  // Root
            expectEquals(intervals[1], 2);  // Major second
            expectEquals(intervals[2], 4);  // Major third
            expectEquals(intervals[3], 5);  // Perfect fourth
            expectEquals(intervals[4], 7);  // Perfect fifth
            expectEquals(intervals[5], 9);  // Major sixth
            expectEquals(intervals[6], 11); // Major seventh
        }

        beginTest("Scale construction from initializer list - Double Harmonic scale");
        {
            // Double Harmonic: [0, 1, 4, 5, 7, 8, 11]
            Scale doubleHarmonicScale({0, 1, 4, 5, 7, 8, 11});
            auto intervals = doubleHarmonicScale.getIntervals();

            expectEquals(static_cast<int>(intervals.size()), 7);
            expectEquals(intervals[0], 0);  // Root
            expectEquals(intervals[1], 1);  // Minor second
            expectEquals(intervals[2], 4);  // Major third
            expectEquals(intervals[3], 5);  // Perfect fourth
            expectEquals(intervals[4], 7);  // Perfect fifth
            expectEquals(intervals[5], 8);  // Minor sixth
            expectEquals(intervals[6], 11); // Major seventh
        }

        beginTest("Scale construction from initializer list - Minor Pentatonic");
        {
            // Minor Pentatonic: [0, 3, 5, 7, 10]
            Scale minorPentatonicScale({0, 3, 5, 7, 10});
            auto intervals = minorPentatonicScale.getIntervals();

            expectEquals(static_cast<int>(intervals.size()), 5);
            expectEquals(intervals[0], 0);  // Root
            expectEquals(intervals[1], 3);  // Minor third
            expectEquals(intervals[2], 5);  // Perfect fourth
            expectEquals(intervals[3], 7);  // Perfect fifth
            expectEquals(intervals[4], 10); // Minor seventh
        }

        beginTest("Scale construction from predefined types - Major scale");
        {
            Scale majorScale(Scale::ScaleType::IonianOrMajor);
            expectEquals(static_cast<int>(majorScale.getType()), static_cast<int>(Scale::ScaleType::IonianOrMajor));
            expectEquals(static_cast<int>(majorScale.getCategory()), static_cast<int>(Scale::ScaleCategory::Diatonic));

            // Check intervals for Major scale: [0, 2, 4, 5, 7, 9, 11]
            auto intervals = majorScale.getIntervals();
            expectEquals(static_cast<int>(intervals.size()), 7);
            expectEquals(intervals[0], 0);  // Root
            expectEquals(intervals[1], 2);  // Major second
            expectEquals(intervals[2], 4);  // Major third
            expectEquals(intervals[3], 5);  // Perfect fourth
            expectEquals(intervals[4], 7);  // Perfect fifth
            expectEquals(intervals[5], 9);  // Major sixth
            expectEquals(intervals[6], 11); // Major seventh
        }

        beginTest("Scale construction from predefined types - Harmonic minor");
        {
            Scale harmonicMinorScale(Scale::ScaleType::HarmonicMinor);
            expectEquals(static_cast<int>(harmonicMinorScale.getType()), static_cast<int>(Scale::ScaleType::HarmonicMinor));
            expectEquals(static_cast<int>(harmonicMinorScale.getCategory()), static_cast<int>(Scale::ScaleCategory::Minor));

            // Check intervals for Harmonic Minor scale: [0, 2, 3, 5, 7, 8, 11]
            auto intervals = harmonicMinorScale.getIntervals();
            expectEquals(static_cast<int>(intervals.size()), 7);
            expectEquals(intervals[0], 0);  // Root
            expectEquals(intervals[1], 2);  // Major second
            expectEquals(intervals[2], 3);  // Minor third
            expectEquals(intervals[3], 5);  // Perfect fourth
            expectEquals(intervals[4], 7);  // Perfect fifth
            expectEquals(intervals[5], 8);  // Minor sixth
            expectEquals(intervals[6], 11); // Major seventh
        }

        beginTest("Scale octave extension - Single octave");
        {
            Scale majorScale({0, 2, 4, 5, 7, 9, 11});
            auto singleOctave = majorScale.getIntervalsForOctaves(1);
            auto originalIntervals = majorScale.getIntervals();

            expectEquals(static_cast<int>(singleOctave.size()), static_cast<int>(originalIntervals.size()));
            for (size_t i = 0; i < originalIntervals.size(); ++i) {
                expectEquals(singleOctave[i], originalIntervals[i]);
            }
        }

        beginTest("Scale octave extension - Two octaves");
        {
            Scale majorScale({0, 2, 4, 5, 7, 9, 11});
            auto twoOctaves = majorScale.getIntervalsForOctaves(2);

            expectEquals(static_cast<int>(twoOctaves.size()), 14); // 7 original + 7 transposed

            // First octave should match original
            for (size_t i = 0; i < 7; ++i) {
                expectEquals(twoOctaves[i], majorScale.getIntervals()[i]);
            }

            // Second octave should be transposed by 12 semitones
            for (size_t i = 7; i < 14; ++i) {
                expectEquals(twoOctaves[i], majorScale.getIntervals()[i - 7] + 12);
            }
        }

        beginTest("Scale category mapping");
        {
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::IonianOrMajor)),
                         static_cast<int>(Scale::ScaleCategory::Diatonic));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::HarmonicMinor)),
                         static_cast<int>(Scale::ScaleCategory::Minor));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::DoubleHarmonic)),
                         static_cast<int>(Scale::ScaleCategory::Exotic));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::MajorPentatonic)),
                         static_cast<int>(Scale::ScaleCategory::Pentatonic));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::BluesScale)),
                         static_cast<int>(Scale::ScaleCategory::Blues));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::WholeTone)),
                         static_cast<int>(Scale::ScaleCategory::Symmetrical));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::BebopMajor)),
                         static_cast<int>(Scale::ScaleCategory::Bebop));
        }

        beginTest("Scale name functions - Full names");
        {
            expectEquals(Scale::getNameForType(Scale::ScaleType::IonianOrMajor), String("Major (Ionian)"));
            expectEquals(Scale::getNameForType(Scale::ScaleType::DoubleHarmonic), String("Double Harmonic"));
            expectEquals(Scale::getNameForType(Scale::ScaleType::HarmonicMinor), String("Harmonic Minor"));
            expectEquals(Scale::getNameForType(Scale::ScaleType::PhrygianDominant), String("Phrygian Dominant"));
            expectEquals(Scale::getNameForType(Scale::ScaleType::User), String("User Defined"));
        }

        beginTest("Scale name functions - Short names");
        {
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::IonianOrMajor), String("Maj"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::DoubleHarmonic), String("DHrm"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::HarmonicMinor), String("HMin"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::PhrygianDominant), String("PhrDom"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::MinorPentatonic), String("mPent"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::BebopMajor), String("BebMaj"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::User), String("User"));
        }

        beginTest("Scale instance name methods");
        {
            Scale majorScale(Scale::ScaleType::IonianOrMajor);
            expectEquals(majorScale.getName(), String("Major (Ionian)"));
            expectEquals(majorScale.getShortName(), String("Maj"));

            Scale doubleHarmonicScale(Scale::ScaleType::DoubleHarmonic);
            expectEquals(doubleHarmonicScale.getName(), String("Double Harmonic"));
            expectEquals(doubleHarmonicScale.getShortName(), String("DHrm"));
        }

        beginTest("Scale construction from Steps enum");
        {
            // Major scale pattern: W-W-H-W-W-W-H
            Scale majorFromSteps({Scale::Steps::Whole, Scale::Steps::Whole, Scale::Steps::Half,
                                  Scale::Steps::Whole, Scale::Steps::Whole, Scale::Steps::Whole, Scale::Steps::Half});
            auto intervals = majorFromSteps.getIntervals();

            expectEquals((int) intervals.size(), 7);
            expectEquals(intervals[0], 0);  // Unison
            expectEquals(intervals[1], 2);  // Whole step
            expectEquals(intervals[2], 4);  // Whole step
            expectEquals(intervals[3], 5);  // Half step
            expectEquals(intervals[4], 7);  // Whole step
            expectEquals(intervals[5], 9);  // Whole step
            expectEquals(intervals[6], 11); // Whole step
        }

        beginTest("Scale steps in the musical form Major");
        {
            Scale majorScale(Scale::ScaleType::IonianOrMajor);
            auto degrees = majorScale.getDegrees();
            expectEquals((int) degrees.size(), 7);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2}); // Major second
            expectEquals(degrees[2], Degree{3}); // Major third
            expectEquals(degrees[3], Degree{4}); // Perfect fourth
            expectEquals(degrees[4], Degree{5}); // Perfect fifth
            expectEquals(degrees[5], Degree{6}); // Major sixth
            expectEquals(degrees[6], Degree{7}); // Major seventh
        }

        beginTest("Scale steps in the musical form Harminic Minor");
        {
            Scale harmonicMinorScale(Scale::ScaleType::HarmonicMinor);
            auto degrees = harmonicMinorScale.getDegrees();
            expectEquals((int) degrees.size(), 7);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2}); // Major second
            expectEquals(degrees[2], Degree{3, Accidental::Flat}); // Minor third
            expectEquals(degrees[3], Degree{4}); // Perfect fourth
            expectEquals(degrees[4], Degree{5}); // Perfect fifth
            expectEquals(degrees[5], Degree{6, Accidental::Flat}); // Minor sixth
            expectEquals(degrees[6], Degree{7}); // Major seventh
        }

        beginTest("Scale degrees - Double Harmonic (exotic 7-degree scale)");
        {
            // Double Harmonic: [0, 1, 4, 5, 7, 8, 11] - semitones
            Scale doubleHarmonic(Scale::ScaleType::DoubleHarmonic);
            auto degrees = doubleHarmonic.getDegrees();
            expectEquals((int) degrees.size(), 7);
            expectEquals(degrees[0], Degree{1}); // Unison (0 semitones)
            expectEquals(degrees[1], Degree{2, Accidental::Flat}); // Minor second (1 semitone)
            expectEquals(degrees[2], Degree{3}); // Major third (4 semitones)
            expectEquals(degrees[3], Degree{4}); // Perfect fourth (5 semitones)
            expectEquals(degrees[4], Degree{5}); // Perfect fifth (7 semitones)
            expectEquals(degrees[5], Degree{6, Accidental::Flat}); // Minor sixth (8 semitones)
            expectEquals(degrees[6], Degree{7}); // Major seventh (11 semitones)
        }

        beginTest("Scale degrees - Phrygian Dominant (complex 7-degree scale)");
        {
            // Phrygian Dominant: [0, 1, 4, 5, 7, 8, 10] - semitones
            Scale phrygianDominant(Scale::ScaleType::PhrygianDominant);
            auto degrees = phrygianDominant.getDegrees();
            expectEquals((int) degrees.size(), 7);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2, Accidental::Flat}); // Minor second
            expectEquals(degrees[2], Degree{3}); // Major third
            expectEquals(degrees[3], Degree{4}); // Perfect fourth
            expectEquals(degrees[4], Degree{5}); // Perfect fifth
            expectEquals(degrees[5], Degree{6, Accidental::Flat}); // Minor sixth
            expectEquals(degrees[6], Degree{7, Accidental::Flat}); // Minor seventh
        }

        beginTest("Scale degree - Hungarian Major (complex 7-degree scale)");
        {
            // Hungarian Major: [0, 3, 4, 6, 7, 9, 10] - semitones
            Scale hungarianMajor(Scale::ScaleType::HungarianMajor);
            auto degrees = hungarianMajor.getDegrees();
            expectEquals((int) degrees.size(), 7);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2, Accidental::Sharp}); // Augmented second
            expectEquals(degrees[2], Degree{3}); // Major third
            expectEquals(degrees[3], Degree{4, Accidental::Sharp}); // Augumented fourth
            expectEquals(degrees[4], Degree{5}); // Perfect fifth
            expectEquals(degrees[5], Degree{6}); // Major sixth
            expectEquals(degrees[6], Degree{7, Accidental::Flat}); // Minor seventh

        }

        beginTest("Scale degrees - Major Pentatonic (5-degree scale)");
        {
            // Major Pentatonic: [0, 2, 4, 7, 9] - semitones
            Scale majorPentatonic(Scale::ScaleType::MajorPentatonic);
            auto degrees = majorPentatonic.getDegrees();
            expectEquals((int) degrees.size(), 5);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2}); // Major second
            expectEquals(degrees[2], Degree{3}); // Major third
            expectEquals(degrees[3], Degree{5}); // Perfect fifth
            expectEquals(degrees[4], Degree{6}); // Major sixth
        }

        beginTest("Scale degrees - Minor Pentatonic (5-degree scale)");
        {
            // Minor Pentatonic: [0, 3, 5, 7, 10] - semitones
            Scale minorPentatonic(Scale::ScaleType::MinorPentatonic);
            auto degrees = minorPentatonic.getDegrees();
            expectEquals((int) degrees.size(), 5);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{3, Alteration::Flat}); // Minor third
            expectEquals(degrees[2], Degree{4}); // Perfect fourth
            expectEquals(degrees[3], Degree{5}); // Perfect fifth
            expectEquals(degrees[4], Degree{7, Alteration::Flat}); // Minor seventh
        }

        beginTest("Scale degrees - Blues Scale (6-degree scale)");
        {
            // Blues Scale: [0, 3, 5, 6, 7, 10] - semitones
            Scale bluesScale(Scale::ScaleType::BluesScale);
            auto degrees = bluesScale.getDegrees();
            expectEquals((int) degrees.size(), 6);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{3, Alteration::Flat}); // Minor third
            expectEquals(degrees[2], Degree{4}); // Perfect fourth
            expectEquals(degrees[3], Degree{5, Alteration::Flat}); // Tritone (augmented fourth/diminished fifth)
            expectEquals(degrees[4], Degree{5}); // Perfect fifth
            expectEquals(degrees[5], Degree{7, Alteration::Flat}); // Minor seventh
        }

        beginTest("Scale degrees - Bebop Major (8-degree scale)");
        {
            // Bebop Major: [0, 2, 4, 5, 7, 8, 9, 11] - semitones
            Scale bebopMajor(Scale::ScaleType::BebopMajor);
            auto degrees = bebopMajor.getDegrees();
            expectEquals((int) degrees.size(), 8);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2}); // Major second
            expectEquals(degrees[2], Degree{3}); // Major third
            expectEquals(degrees[3], Degree{4}); // Perfect fourth
            expectEquals(degrees[4], Degree{5}); // Perfect fifth
            expectEquals(degrees[5], Degree{6, Alteration::Flat}); // Minor sixth (chromatic passing tone)
            expectEquals(degrees[6], Degree{6}); // Major sixth
            expectEquals(degrees[7], Degree{7}); // Major seventh
        }

        beginTest("Scale degrees - Whole Tone (6-degree scale)");
        {
            // Whole Tone: [0, 2, 4, 6, 8, 10] - semitones
            Scale wholeTone(Scale::ScaleType::WholeTone);
            auto degrees = wholeTone.getDegrees();
            expectEquals((int) degrees.size(), 6);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2}); // Major second
            expectEquals(degrees[2], Degree{3}); // Major third
            expectEquals(degrees[3], Degree{5, Alteration::Flat}); // Tritone
            expectEquals(degrees[4], Degree{6, Alteration::Flat}); // Augmented fifth/diminished sixth
            expectEquals(degrees[5], Degree{7, Alteration::Flat}); // Minor seventh
        }

        beginTest("Scale degrees - Custom scale from intervals");
        {
            // Custom scale: [0, 1, 3, 6, 8] - asymmetric 5-note scale
            Scale customScale({0, 1, 3, 6, 8});
            auto degrees = customScale.getDegrees();
            expectEquals((int) degrees.size(), 5);
            expectEquals(degrees[0], Degree{1}); // Unison
            expectEquals(degrees[1], Degree{2, Alteration::Flat}); // Minor second
            expectEquals(degrees[2], Degree{3, Alteration::Flat}); // Minor third
            expectEquals(degrees[3], Degree{5, Alteration::Flat}); // Tritone
            expectEquals(degrees[4], Degree{6, Alteration::Flat}); // Minor sixth
        }
    }
};

static ScaleTest scaleTest;