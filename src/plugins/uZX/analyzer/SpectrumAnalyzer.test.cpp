#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/main/PlayerDocument.h"
#include "../../../util/Helpers.h"
#include "../../../util/TestHelpers.h"
#include "SpectrumAnalyzerPlugin.h"
#include "SpectrumPanel.h"

namespace MoTool::uZX {

class SpectrumAnalyzerTest : public juce::UnitTest
{
public:
    SpectrumAnalyzerTest() : UnitTest("SpectrumAnalyzer", "MoTool") {}

    static std::vector<float> sine(float hz, float amplitude, double rate = 48000.0) {
        std::vector<float> samples(SpectrumBars::fftSize);
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = amplitude * std::sin(MathConstants<float>::twoPi * hz * (float) i / (float) rate);
        return samples;
    }

    void runTest() override {
        using Levels = SpectrumBars::Levels;
        constexpr float floor = SpectrumBars::floorDb;

        beginTest("28 bands a third of an octave apart, 31.5 Hz to 16 kHz");
        expectWithinAbsoluteError(SpectrumBars::bandCentre(0), 31.25f, 0.01f);
        expectWithinAbsoluteError(SpectrumBars::bandCentre(15), 1000.0f, 0.01f);
        expectWithinAbsoluteError(SpectrumBars::bandCentre(SpectrumBars::numBands - 1), 16000.0f, 0.1f);

        beginTest("A full-scale sine reads 0 dB in its band, and two octaves off is quiet");
        {
            SpectrumBars bars;
            const auto levels = bars.analyse(sine(1000.0f, 1.0f).data(), 48000.0);
            expectWithinAbsoluteError(levels[15], 0.0f, 1.0f);
            expectLessThan(levels[9], -40.0f);
            expectLessThan(levels[21], -40.0f);
        }

        beginTest("A 40 Hz sine lights its band most, though no bin's centre is in it");
        {
            SpectrumBars bars;
            const auto levels = bars.analyse(sine(40.0f, 0.5f).data(), 48000.0);
            expect(std::max_element(levels.begin(), levels.end()) == levels.begin() + 1);
            expectGreaterThan(levels[1], -20.0f);
        }

        beginTest("Silence, or a DC offset, reads at the floor");
        {
            SpectrumBars bars;
            std::vector<float> dc(SpectrumBars::fftSize, 0.25f);
            for (auto level : bars.analyse(dc.data(), 48000.0))
                expectEquals(level, floor);
        }

        beginTest("Bars jump up and fall at their rate; caps hold, then fall ever faster, never below the bars");
        {
            SpectrumBars bars;
            const SpectrumBars::Falloff falloff{150.0f, 0.5f, 120.0f};
            Levels loud, silent;
            loud.fill(0.0f);
            silent.fill(floor);
            expect(bars.advance(loud, 0.125f, falloff));
            expectEquals(bars.getBars()[0], 0.0f);
            expectEquals(bars.getPeaks()[0], 0.0f);

            for (int i = 1; i <= 4; ++i) {
                bars.advance(silent, 0.125f, falloff);
                expectEquals(bars.getBars()[0], jmax(floor, -18.75f * (float) i), "bar falls 150 dB/s");
                expectEquals(bars.getPeaks()[0], 0.0f, "cap holds half a second");
            }
            float last = 0.0f, lastStep = 0.0f;
            for (int i = 0; i < 4; ++i) {
                bars.advance(silent, 0.125f, falloff);
                const float peak = bars.getPeaks()[0];
                expectGreaterThan(last - peak, lastStep, "the cap falls faster each frame");
                expectGreaterOrEqual(peak, bars.getBars()[0]);
                lastStep = last - peak;
                last = peak;
            }
            for (int i = 0; i < 20; ++i)
                bars.advance(silent, 0.125f, falloff);
            expectEquals(bars.getPeaks()[0], floor);
            expect(!bars.advance(silent, 0.125f, falloff), "all down: nothing to repaint");
        }

        beginTest("The analyzer takes the channels' average, and passes them on");
        {
            auto edit = te::Edit::createSingleTrackEdit(*te::Engine::getEngines()[0]);
            Helpers::setMasterSpectrumAnalyzer(*edit, true);
            auto* analyzer = edit->getMasterPluginList().findFirstPluginOfType<SpectrumAnalyzerPlugin>();
            expect(analyzer != nullptr);
            juce::AudioBuffer<float> stereo(2, 64);
            stereo.clear();
            FloatVectorOperations::fill(stereo.getWritePointer(0), 1.0f, 64);
            analyzer->capture(stereo, 0, 64, 44100.0);
            float last = 0.0f;
            analyzer->getBuffer().copyTo(&last, 1);
            expectEquals(last, 0.5f);
            expectEquals(analyzer->getCaptureRate(), 44100.0);
        }

        beginTest("The Viz option puts one analyzer on the master, and takes it off");
        {
            auto edit = te::Edit::createSingleTrackEdit(*te::Engine::getEngines()[0]);
            const auto count = [&edit] {
                const auto plugins = edit->getMasterPluginList().getPlugins();
                return (int) std::count_if(plugins.begin(), plugins.end(), [](auto* p) {
                    return dynamic_cast<SpectrumAnalyzerPlugin*>(p) != nullptr;
                });
            };
            Helpers::setMasterSpectrumAnalyzer(*edit, true);
            Helpers::setMasterSpectrumAnalyzer(*edit, true);
            expectEquals(count(), 1);
            Helpers::setMasterSpectrumAnalyzer(*edit, false);
            expectEquals(count(), 0);
        }

        beginTest("In Viz mode the spectrum takes a panel at the left or along the bottom; a tone lights its band");
        {
            auto& engine = *te::Engine::getEngines()[0];
            auto edit = te::Edit::createSingleTrackEdit(engine);
            Helpers::ensureAYPluginOnTrack(*te::getAudioTracks(*edit)[0]);
            Helpers::setMasterSpectrumAnalyzer(*edit, true);
            const auto tone = sine(1000.0f, 1.0f);
            juce::AudioBuffer<float> stereo(2, SpectrumBars::fftSize);
            for (int c = 0; c < 2; ++c)
                stereo.copyFrom(c, 0, tone.data(), SpectrumBars::fftSize);
            edit->getMasterPluginList().findFirstPluginOfType<SpectrumAnalyzerPlugin>()->capture(
                stereo, 0, SpectrumBars::fftSize, 48000.0);

            for (auto where : {VizOptions::Spectrum::left, VizOptions::Spectrum::bottom}) {
                const bool atLeft = where == VizOptions::Spectrum::left;
                te::SelectionManager selection(engine);
                EditViewState view(*edit, selection, true);
                view.showAsPlayer(true, {.spectrum = where});
                PlayerDocumentComponent document(*edit, view);
                constexpr int width = 1400, height = 800;
                document.setSize(width, height);
                TestHelpers::flushMessageQueue();

                SpectrumPanel* panel = nullptr;
                EditComponent* notes = nullptr;
                for (auto* child : document.getChildren()) {
                    panel = panel != nullptr ? panel : dynamic_cast<SpectrumPanel*>(child);
                    notes = notes != nullptr ? notes : dynamic_cast<EditComponent*>(child);
                }
                expect(panel != nullptr && notes != nullptr, "a panel beside the notes");
                if (panel == nullptr || notes == nullptr)
                    return;
                if (atLeft) {
                    expect(panel->getBounds() == juce::Rectangle<int>(0, 0, view.vizSpectrumWidth.get(), height));
                    expectEquals(notes->getX(), panel->getRight(), "the notes after the panel");
                } else {
                    expect(panel->getBounds()
                               == juce::Rectangle<int>(
                                   0, height - view.vizSpectrumHeight.get(), width, view.vizSpectrumHeight.get()),
                           "across the whole width");
                    expectEquals(notes->getBottom(), panel->getY(), "the notes above the panel");
                }

                // Next to each band's base: the lowest at the bottom of the left panel, at the left of the bottom one
                panel->refresh(0.0);
                const auto image = panel->createComponentSnapshot(panel->getLocalBounds(), true, 1.0f);
                const auto lit = [&](int band) {
                    const int length = atLeft ? panel->getHeight() : panel->getWidth();
                    const int middle = length * (2 * band + 1) / (2 * SpectrumBars::numBands);
                    const auto pixel = atLeft ? image.getPixelAt(panel->getWidth() - SpectrumPanel::gap - 3,
                                                                 panel->getHeight() - middle)
                                              : image.getPixelAt(middle, panel->getHeight() - 3);
                    return pixel != Colors::Timeline::vizLane;
                };
                expect(lit(15), "1 kHz: a bar");
                expect(!lit(3), "50 Hz: quiet");

                // What dragging its edge does
                if (atLeft)
                    panel->setSize(300, panel->getHeight());
                else
                    panel->setBounds(panel->getBounds().withTop(height - 200));
                expect(atLeft ? view.vizSpectrumWidth == 300 : view.vizSpectrumHeight == 200, "the size is kept");
                expect(atLeft ? notes->getX() == 300 : notes->getBottom() == height - 200, "the notes follow");
            }
        }
    }
};

static SpectrumAnalyzerTest spectrumAnalyzerTest;

}  // namespace MoTool::uZX
