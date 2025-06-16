#include <JuceHeader.h>
#include "Scales.h"
#include <string_view>

using namespace MoTool;

class ScaleTest : public juce::UnitTest {
public:
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
            expectEquals(static_cast<int>(majorScale.getCategory()), static_cast<int>(Scale::ScaleCategory::DiatonicModes));

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
            expectEquals(static_cast<int>(harmonicMinorScale.getCategory()), static_cast<int>(Scale::ScaleCategory::MinorVariations));

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
            auto singleOctave = majorScale.getIntervals(1);
            auto originalIntervals = majorScale.getIntervals();

            expectEquals(static_cast<int>(singleOctave.size()), static_cast<int>(originalIntervals.size()));
            for (size_t i = 0; i < originalIntervals.size(); ++i) {
                expectEquals(singleOctave[i], originalIntervals[i]);
            }
        }

        beginTest("Scale octave extension - Two octaves");
        {
            Scale majorScale({0, 2, 4, 5, 7, 9, 11});
            auto twoOctaves = majorScale.getIntervals(2);

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
                         static_cast<int>(Scale::ScaleCategory::DiatonicModes));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::HarmonicMinor)),
                         static_cast<int>(Scale::ScaleCategory::MinorVariations));
            expectEquals(static_cast<int>(Scale::getCategoryForType(Scale::ScaleType::DoubleHarmonic)),
                         static_cast<int>(Scale::ScaleCategory::ExoticWorld));
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
            expectEquals(Scale::getNameForType(Scale::ScaleType::UserDefined), String("User Defined"));
        }

        beginTest("Scale name functions - Short names");
        {
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::IonianOrMajor), String("Maj"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::DoubleHarmonic), String("DHrm"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::HarmonicMinor), String("HMin"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::PhrygianDominant), String("PhrDom"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::MinorPentatonic), String("mPent"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::BebopMajor), String("BebMaj"));
            expectEquals(Scale::getShortNameForType(Scale::ScaleType::UserDefined), String("Custom"));
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

            expectEquals(static_cast<int>(intervals.size()), 7);
            expectEquals(intervals[0], 0);  // Root
            expectEquals(intervals[1], 2);  // Whole step
            expectEquals(intervals[2], 4);  // Whole step
            expectEquals(intervals[3], 5);  // Half step
            expectEquals(intervals[4], 7);  // Whole step
            expectEquals(intervals[5], 9);  // Whole step
            expectEquals(intervals[6], 11); // Whole step
        }

        beginTest("constexpr string_view getters - Category names");
        {
            auto diatonicName = Scale::getNameForCategoryView(Scale::ScaleCategory::DiatonicModes);
            auto minorName = Scale::getNameForCategoryView(Scale::ScaleCategory::MinorVariations);
            auto pentatonicName = Scale::getNameForCategoryView(Scale::ScaleCategory::Pentatonic);
            
            expect(diatonicName == "Diatonic Modes");
            expect(minorName == "Minor Variations");
            expect(pentatonicName == "Pentatonic");
            
            // Verify consistency with juce::String versions
            expect(Scale::getNameForCategory(Scale::ScaleCategory::DiatonicModes) == String(diatonicName.data(), diatonicName.size()));
            expect(Scale::getNameForCategory(Scale::ScaleCategory::MinorVariations) == String(minorName.data(), minorName.size()));
            expect(Scale::getNameForCategory(Scale::ScaleCategory::Pentatonic) == String(pentatonicName.data(), pentatonicName.size()));
        }

        beginTest("constexpr string_view getters - Category short names");
        {
            auto diatonicShort = Scale::getShortNameForCategoryView(Scale::ScaleCategory::DiatonicModes);
            auto minorShort = Scale::getShortNameForCategoryView(Scale::ScaleCategory::MinorVariations);
            auto userShort = Scale::getShortNameForCategoryView(Scale::ScaleCategory::UserDefined);
            
            expect(diatonicShort == "Diatonic");
            expect(minorShort == "Minor");
            expect(userShort == "Custom");
            
            // Verify consistency with juce::String versions
            expect(Scale::getShortNameForCategory(Scale::ScaleCategory::DiatonicModes) == String(diatonicShort.data(), diatonicShort.size()));
            expect(Scale::getShortNameForCategory(Scale::ScaleCategory::MinorVariations) == String(minorShort.data(), minorShort.size()));
            expect(Scale::getShortNameForCategory(Scale::ScaleCategory::UserDefined) == String(userShort.data(), userShort.size()));
        }

        beginTest("constexpr string_view getters - Scale type names");
        {
            auto majorName = Scale::getNameForTypeView(Scale::ScaleType::IonianOrMajor);
            auto harmonicMinorName = Scale::getNameForTypeView(Scale::ScaleType::HarmonicMinor);
            auto bluesName = Scale::getNameForTypeView(Scale::ScaleType::BluesScale);
            
            expect(majorName == "Major (Ionian)");
            expect(harmonicMinorName == "Harmonic Minor");
            expect(bluesName == "Blues Scale");
            
            // Verify consistency with juce::String versions
            expect(Scale::getNameForType(Scale::ScaleType::IonianOrMajor) == String(majorName.data(), majorName.size()));
            expect(Scale::getNameForType(Scale::ScaleType::HarmonicMinor) == String(harmonicMinorName.data(), harmonicMinorName.size()));
            expect(Scale::getNameForType(Scale::ScaleType::BluesScale) == String(bluesName.data(), bluesName.size()));
        }

        beginTest("constexpr string_view getters - Scale type short names");
        {
            auto majorShort = Scale::getShortNameForTypeView(Scale::ScaleType::IonianOrMajor);
            auto harmonicMinorShort = Scale::getShortNameForTypeView(Scale::ScaleType::HarmonicMinor);
            auto bluesShort = Scale::getShortNameForTypeView(Scale::ScaleType::BluesScale);
            auto userShort = Scale::getShortNameForTypeView(Scale::ScaleType::UserDefined);
            
            expect(majorShort == "Maj");
            expect(harmonicMinorShort == "HMin");
            expect(bluesShort == "Blues");
            expect(userShort == "Custom");
            
            // Verify consistency with juce::String versions
            expect(Scale::getShortNameForType(Scale::ScaleType::IonianOrMajor) == String(majorShort.data(), majorShort.size()));
            expect(Scale::getShortNameForType(Scale::ScaleType::HarmonicMinor) == String(harmonicMinorShort.data(), harmonicMinorShort.size()));
            expect(Scale::getShortNameForType(Scale::ScaleType::BluesScale) == String(bluesShort.data(), bluesShort.size()));
            expect(Scale::getShortNameForType(Scale::ScaleType::UserDefined) == String(userShort.data(), userShort.size()));
        }
    }
};

static ScaleTest scaleTest;