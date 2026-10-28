#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../formats/psg/PsgData.h"

namespace MoTool {

class PsgClipPaintingTest : public UnitTest
{
public:
    PsgClipPaintingTest() : UnitTest("PsgClipPainting", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto renderData = [&engine](uZX::PsgData data, int width = 100, int height = 300,
                                    bool header = false) {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                        {{te::TimePosition(), te::TimeDuration::fromSeconds(0.02)}, {}}, "noise");
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(width);
            view.zoom.setRange(clip->getEditTimeRange());
            TimelineGrid grid(view);
            PsgClipComponent component(view, grid, clip);
            component.setSize(width, height);
            Image image(Image::ARGB, width, height, true);
            Graphics g(image);
            if (header)
                component.paintHeader(g);
            else
                component.paintNotes(g);
            return image;
        };
        auto render = [&renderData](uint8_t mixer, uint8_t volumeA, uint8_t volumeB, uint8_t volumeC = 0,
                                    bool header = false, int width = 100, int height = 300) {
            const uZX::PsgData data {
                {{{uZX::PsgRegType::Mixer, mixer}, {uZX::PsgRegType::NoisePeriod, 16},
                  {uZX::PsgRegType::VolumeA, volumeA}, {uZX::PsgRegType::VolumeB, volumeB},
                  {uZX::PsgRegType::VolumeC, volumeC}}},
                {50.0, 1}
            };
            return renderData(data, width, height, header);
        };
        auto isBlank = [](const Image& image) {
            for (int y = 0; y < image.getHeight(); ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                    if (! image.getPixelAt(x, y).isTransparent())
                        return false;
            return true;
        };

        beginTest("Disabled and silent noise channels draw no band");
        expect(isBlank(render(0x3f, 15, 15)), "Nonzero volume must not draw disabled noise");
        expect(isBlank(render(0x37, 0, 0)), "Enabled but silent noise must not draw a band");

        beginTest("Only noise-enabled channels contribute color and opacity");
        const auto noiseA = render(0x37, 15, 0);
        const auto noiseAWithB = render(0x37, 15, 15);
        expect(! isBlank(noiseA), "Audible noise must draw a band");
        bool equal = true;
        for (int y = 0; y < noiseA.getHeight(); ++y)
            for (int x = 0; x < noiseA.getWidth(); ++x)
                equal = equal && noiseA.getPixelAt(x, y) == noiseAWithB.getPixelAt(x, y);
        expect(equal, "Channel B without noise must not change channel A's noise band");

        beginTest("Envelope-driven noise remains visible with zero fixed volume");
        expect(! isBlank(render(0x37, 16, 0)));

        beginTest("Noise bands follow envelope-pattern level of detail");
        expect(isBlank(render(0x37, 15, 0, 0, false, 5)), "Hide noise below six pixels per frame");
        expect(! isBlank(render(0x37, 15, 0, 0, false, 6)), "Show noise at six pixels per frame");
        expect(isBlank(render(0x37, 15, 0, 0, false, 100, 100)), "Hide noise when note rows are too short");

        struct Dot { Rectangle<int> bounds; Colour color; };
        auto findDots = [](const Image& image) {
            std::vector<Dot> dots;
            auto remaining = image.createCopy();
            for (int y = 0; y < image.getHeight(); ++y) {
                for (int x = 0; x < image.getWidth(); ++x) {
                    if (remaining.getPixelAt(x, y).getFloatAlpha() <= 0.3f)
                        continue;
                    Dot dot {{x, y, 1, 1}, image.getPixelAt(x, y)};
                    std::vector<Point<int>> pixels {{x, y}};
                    remaining.setPixelAt(x, y, Colours::transparentBlack);
                    for (size_t i = 0; i < pixels.size(); ++i) {
                        const auto point = pixels[i];
                        dot.bounds = dot.bounds.getUnion({point.x, point.y, 1, 1});
                        const auto color = image.getPixelAt(point.x, point.y);
                        if (color.getAlpha() > dot.color.getAlpha())
                            dot.color = color;
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                const auto next = point.translated(dx, dy);
                                if (remaining.getBounds().contains(next)
                                    && remaining.getPixelAt(next.x, next.y).getFloatAlpha() > 0.3f) {
                                    pixels.push_back(next);
                                    remaining.setPixelAt(next.x, next.y, Colours::transparentBlack);
                                }
                            }
                        }
                    }
                    dots.push_back(dot);
                }
            }
            return dots;
        };

        beginTest("Six fixed noise dots divide evenly among active channels");
        const auto referenceDots = findDots(noiseA);
        expectEquals(int(referenceDots.size()), 6);
        for (int mask = 1; mask < 8; ++mask) {
            const auto image = render(static_cast<uint8_t>(0x3f & ~(mask << 3)), 15, 15, 15);
            const auto dots = findDots(image);
            expectEquals(int(dots.size()), 6);
            std::array<int, 3> counts {};
            for (size_t i = 0; i < dots.size(); ++i) {
                const auto color = dots[i].color;
                const int channel = color.getRed() > color.getGreen() && color.getRed() > color.getBlue() ? 0
                    : color.getGreen() > color.getBlue() ? 1 : 2;
                ++counts[static_cast<size_t>(channel)];
                if (i < referenceDots.size())
                    expect(dots[i].bounds == referenceDots[i].bounds, "Routing must not move the dots");
            }
            const int activeCount = ((mask & 1) != 0) + ((mask & 2) != 0) + ((mask & 4) != 0);
            for (int ch = 0; ch < 3; ++ch)
                expectEquals(counts[static_cast<size_t>(ch)], (mask & (1 << ch)) != 0 ? 6 / activeCount : 0);
        }

        beginTest("Noise dots fade with channel level");
        const auto quietNoise = render(0x37, 1, 0);
        uint8_t loudAlpha = 0, quietAlpha = 0;
        for (int y = 0; y < noiseA.getHeight(); ++y) {
            for (int x = 0; x < noiseA.getWidth(); ++x) {
                loudAlpha = jmax(loudAlpha, noiseA.getPixelAt(x, y).getAlpha());
                quietAlpha = jmax(quietAlpha, quietNoise.getPixelAt(x, y).getAlpha());
            }
        }
        expect(quietAlpha < loudAlpha);

        beginTest("Envelope pitch has its own color while its tone retains the channel color");
        const uZX::PsgData envelopeData {
            {{{uZX::PsgRegType::Mixer, 0x3d}, {uZX::PsgRegType::VolumeB, 16},
              {uZX::PsgRegType::TonePeriodFineB, 64}, {uZX::PsgRegType::TonePeriodCoarseB, 1},
              {uZX::PsgRegType::EnvelopePeriodFine, 100}, {uZX::PsgRegType::EnvelopeShape, 8}}},
            {50.0, 1}
        };
        const auto envelopeImage = renderData(envelopeData);
        bool hasEnvelopeColor = false, hasChannelColor = false;
        for (int y = 0; y < envelopeImage.getHeight(); ++y) {
            for (int x = 0; x < envelopeImage.getWidth(); ++x) {
                const auto color = envelopeImage.getPixelAt(x, y);
                hasEnvelopeColor = hasEnvelopeColor || color == Colors::PSG::Env;
                hasChannelColor = hasChannelColor || color == Colors::PSG::B;
            }
        }
        expect(hasEnvelopeColor, "Envelope pitch uses the shared envelope color");
        expect(hasChannelColor, "Envelope-controlled tone keeps channel B's color");

        beginTest("Header shows A/B/C colors and a padded outlined envelope swatch");
        const auto header = render(0x3f, 0, 0, 0, true);
        const std::array<Colour, 3> colors {Colors::PSG::A, Colors::PSG::B, Colors::PSG::C};
        for (int ch = 0; ch < 3; ++ch) {
            const int x = 3 + ch * 13;
            expect(header.getPixelAt(x, 14) == colors[static_cast<size_t>(ch)]);
            expect(header.getPixelAt(x, 15).isTransparent());
        }
        expect(header.getPixelAt(42, 14) == Colors::PSG::Env, "E outline matches the envelope pitch color");
        expect(header.getPixelAt(42, 15).isTransparent(), "E keeps bottom padding");
        bool hasUnfilledInterior = false;
        bool hasEnvelopeHatching = false;
        for (int y = 4; y < 14; ++y) {
            for (int x = 43; x < 53; ++x) {
                const auto color = header.getPixelAt(x, y);
                hasUnfilledInterior = hasUnfilledInterior || color.isTransparent();
                hasEnvelopeHatching = hasEnvelopeHatching
                    || (color.getFloatAlpha() > 0.0f && color.getFloatAlpha() < 0.4f
                        && color.getRed() > color.getGreen() && color.getBlue() > color.getGreen());
            }
        }
        expect(hasUnfilledInterior, "E interior stays unfilled behind its label and hatching");
        expect(hasEnvelopeHatching, "E uses colored envelope hatching behind its label");
        expect(header.getPixelAt(55, 8).isTransparent(), "No combined-channel swatch after E");
    }
};

static PsgClipPaintingTest psgClipPaintingTest;

}  // namespace MoTool
