#include <JuceHeader.h>

#include <algorithm>

#include "PsgClipTestHelpers.h"
#include "TimelineGrid.h"
#include "TuningPickerComponent.h"
#include "../common/LookAndFeel.h"
#include "../../formats/psg/PsgData.h"
#include "../../models/TimerTestBank.h"
#include "../../formats/taym/TaymFile.h"

namespace MoTool {

class PsgClipPaintingTest : public UnitTest
{
public:
    PsgClipPaintingTest() : UnitTest("PsgClipPainting", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto renderData = [&engine](uZX::PsgData data, int width = 100, int height = 300,
                                    bool legend = false) {
            return Tests::renderPsgClip(engine, std::move(data), width, height, {}, legend);
        };
        auto render = [&renderData](uint8_t mixer, uint8_t volumeA, uint8_t volumeB, uint8_t volumeC = 0,
                                    bool legend = false, int width = 100, int height = 300) {
            const uZX::PsgData data {
                {{{uZX::PsgRegType::Mixer, mixer}, {uZX::PsgRegType::NoisePeriod, 16},
                  {uZX::PsgRegType::VolumeA, volumeA}, {uZX::PsgRegType::VolumeB, volumeB},
                  {uZX::PsgRegType::VolumeC, volumeC}}},
                {50.0, 1}
            };
            return renderData(data, width, height, legend);
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

        beginTest("Viz mode's Always decorations draw the noise band at any zoom");
        {
            const uZX::PsgData noise {{{{uZX::PsgRegType::Mixer, 0x37}, {uZX::PsgRegType::NoisePeriod, 16},
                                        {uZX::PsgRegType::VolumeA, 15}}},
                                      {50.0, 1}};
            const auto inViz = [&](VizOptions::Decorations decorations) {
                return Tests::renderPsgClip(engine, noise, 5, 300, {}, false, {}, VizOptions{.decorations = decorations});
            };
            expect(isBlank(inViz(VizOptions::Decorations::automatic)), "Auto hides it below six pixels per frame");
            expect(! isBlank(inViz(VizOptions::Decorations::always)), "Always draws it");
        }

        auto withTones = [](int toneMask, int noiseMask, int envMask = 0) {
            uZX::PsgRegsFrame frame;
            frame.mask.fill(true);
            frame.setMixer(0x3f);
            frame.setNoisePeriod(16);
            frame.setEnvelopePeriod(32);
            frame.setEnvelopeShape(8);
            for (size_t ch = 0; ch < 3; ++ch) {
                frame.setTonePeriod(ch, 320);
                frame.setVolumeAndEnvMod(ch, 15, (envMask & (1 << ch)) != 0);
                frame.setToneOn(ch, (toneMask & (1 << ch)) != 0);
                frame.setNoiseOn(ch, (noiseMask & (1 << ch)) != 0);
            }
            return uZX::PsgData {{frame}, {50.0, 1}};
        };
        auto equalImages = [](const Image& a, const Image& b) {
            if (a.getBounds() != b.getBounds())
                return false;
            const Image::BitmapData pa(a, Image::BitmapData::readOnly);
            const Image::BitmapData pb(b, Image::BitmapData::readOnly);
            for (int y = 0; y < a.getHeight(); ++y)
                for (int x = 0; x < a.getWidth(); ++x)
                    if (pa.getPixelColour(x, y) != pb.getPixelColour(x, y))
                        return false;
            return true;
        };

        beginTest("Noise-only ownership uses scattered rectangles around the gray band center");
        const auto ownershipNoiseA = render(0x37, 15, 0, 0, false, 100, 600);
        int ownershipPixels = 0, minOwnershipX = 100, maxOwnershipX = 0;
        for (int y = 0; y < ownershipNoiseA.getHeight(); ++y) {
            for (int x = 0; x < ownershipNoiseA.getWidth(); ++x) {
                const auto color = ownershipNoiseA.getPixelAt(x, y);
                if (color.getFloatAlpha() <= 0.3f)
                    continue;
                ++ownershipPixels;
                minOwnershipX = jmin(minOwnershipX, x);
                maxOwnershipX = jmax(maxOwnershipX, x);
                expect(color.getRed() > color.getGreen() && color.getRed() > color.getBlue());
                expect(y >= 457 && y <= 466, "Ownership stipple surrounds the noise-period band center");
            }
        }
        expect(ownershipPixels > 16 && maxOwnershipX - minOwnershipX > 80,
               "Noise-only ownership is scattered across the frame, not a centered tick");

        beginTest("Noise-only stipple identifies every enabled owner without mixing channel colors");
        for (int mask = 1; mask < 8; ++mask) {
            const auto image = renderData(withTones(0, mask), 100, 600);
            int paintedOwners = 0;
            for (int y = 0; y < image.getHeight(); ++y) {
                for (int x = 0; x < image.getWidth(); ++x) {
                    const auto color = image.getPixelAt(x, y);
                    if (color.getFloatAlpha() <= 0.3f)
                        continue;
                    const int ch = color.getRed() > color.getGreen() && color.getRed() > color.getBlue() ? 0
                        : color.getGreen() > color.getBlue() ? 1 : 2;
                    paintedOwners |= 1 << ch;
                }
            }
            expectEquals(paintedOwners, mask);
        }
        auto unmappedTone = withTones(1, 1);
        unmappedTone.frames[0].setTonePeriod(0, 1);
        expect(equalImages(renderData(unmappedTone, 100, 600), renderData(withTones(0, 1), 100, 600)),
               "An omitted tone pitch still uses the band ownership fallback");

        beginTest("Noise pixels sit outside notes, within three quarters of a note height, and identify enabled channels");
        const auto plainNotes = renderData(withTones(7, 0));
        Rectangle<int> noteBounds;
        for (int y = 0; y < plainNotes.getHeight(); ++y)
            for (int x = 0; x < plainNotes.getWidth(); ++x)
                if (! plainNotes.getPixelAt(x, y).isTransparent())
                    noteBounds = noteBounds.getUnion({x, y, 1, 1});
        expect(! noteBounds.isEmpty());
        for (int mask = 1; mask < 8; ++mask) {
            const auto image = renderData(withTones(7, mask));
            std::array<int, 3> counts {};
            int above = 0, below = 0;
            for (int y = 0; y < image.getHeight(); ++y) {
                for (int x = 0; x < image.getWidth(); ++x) {
                    if (noteBounds.contains(x, y) || image.getPixelAt(x, y).getFloatAlpha() <= 0.3f)
                        continue;
                    const auto color = image.getPixelAt(x, y);
                    expect(y >= noteBounds.getY() - noteBounds.getHeight() * 3 / 4);
                    expect(y < noteBounds.getBottom() + noteBounds.getHeight() * 3 / 4);
                    above += y < noteBounds.getY();
                    below += y >= noteBounds.getBottom();
                    if (color.getFloatAlpha() > 0.3f) {
                        const int ch = color.getRed() > color.getGreen() && color.getRed() > color.getBlue() ? 0
                            : color.getGreen() > color.getBlue() ? 1 : 2;
                        ++counts[static_cast<size_t>(ch)];
                    }
                }
            }
            expect(above > 0 && below > 0, "Noise dots appear above and below the note silhouette");
            for (int ch = 0; ch < 3; ++ch)
                expect((counts[static_cast<size_t>(ch)] > 0) == ((mask & (1 << ch)) != 0));
            expect(equalImages(image, renderData(withTones(7, mask))), "Repaint must not move noise pixels");
            for (int y = noteBounds.getY(); y < noteBounds.getBottom(); ++y)
                for (int x = noteBounds.getX(); x < noteBounds.getRight(); ++x)
                    expect(image.getPixelAt(x, y) == plainNotes.getPixelAt(x, y), "Noise must not dirty notes");
        }

        beginTest("Noise stays within three quarters of a note height above and below the silhouette");
        const auto expanded = renderData(withTones(1, 1));
        int fringeTop = 300, fringeBottom = 0;
        for (int y = noteBounds.getY() - noteBounds.getHeight();
             y < noteBounds.getBottom() + noteBounds.getHeight(); ++y) {
            if (y >= noteBounds.getY() && y < noteBounds.getBottom())
                continue;
            for (int x = 0; x < expanded.getWidth(); ++x) {
                if (! expanded.getPixelAt(x, y).isTransparent()) {
                    fringeTop = jmin(fringeTop, y);
                    fringeBottom = jmax(fringeBottom, y + 1);
                }
            }
        }
        expect(fringeTop >= noteBounds.getY() - noteBounds.getHeight() * 3 / 4 && fringeTop < noteBounds.getY());
        expect(fringeBottom <= noteBounds.getBottom() + noteBounds.getHeight() * 3 / 4
               && fringeBottom > noteBounds.getBottom());

        beginTest("Noise rectangles remain whole at frame edges and spread limits");
        for (int width : {6, 7, 11, 13}) {
            const auto image = renderData(withTones(1, 1), width);
            for (int y = noteBounds.getY() - noteBounds.getHeight();
                 y < noteBounds.getBottom() + noteBounds.getHeight(); ++y) {
                if (y >= noteBounds.getY() && y < noteBounds.getBottom())
                    continue;
                for (int x = 0; x < width; ++x) {
                    const auto color = image.getPixelAt(x, y);
                    if (color.isTransparent())
                        continue;
                    bool whole = false;
                    for (int top : {y - 1, y}) {
                        for (int left : {x - 1, x}) {
                            if (left < 0 || left + 1 >= width || top < 0 || top + 1 >= image.getHeight())
                                continue;
                            whole = whole || (image.getPixelAt(left, top) == color
                                && image.getPixelAt(left + 1, top) == color
                                && image.getPixelAt(left, top + 1) == color
                                && image.getPixelAt(left + 1, top + 1) == color);
                        }
                    }
                    expect(whole, "Every noise pixel belongs to a full 2x2 rectangle");
                }
            }
        }

        beginTest("Partial repaint preserves noise rectangles that cross the damage boundary");
        const Rectangle<int> noiseDamage {1, noteBounds.getY() - noteBounds.getHeight(), 10, noteBounds.getHeight()};
        const auto repainted = Tests::renderPsgClip(engine, withTones(1, 1), 100, 300, {}, false, noiseDamage);
        expect(equalImages(expanded.getClippedImage(noiseDamage), repainted.getClippedImage(noiseDamage)));

        beginTest("Noise fringe follows envelope-pattern LOD");
        expect(equalImages(renderData(withTones(7, 7), 5), renderData(withTones(7, 0), 5)));

        beginTest("Noise on a channel without a silhouette does not decorate another channel's note");
        const auto unrelatedNoise = renderData(withTones(1, 2));
        for (int y = noteBounds.getY() - noteBounds.getHeight();
             y < noteBounds.getBottom() + noteBounds.getHeight(); ++y)
            for (int x = 0; x < unrelatedNoise.getWidth(); ++x)
                if (! noteBounds.contains(x, y))
                    expect(unrelatedNoise.getPixelAt(x, y).getFloatAlpha() <= 0.3f);

        const auto hasEnvelopeFill = [](const Image& image, int y, Colour fill = Colors::PSG::Env) {
            for (int x = 0; x < image.getWidth(); ++x)
                if (image.getPixelAt(x, y) == fill)
                    return true;
            return false;
        };

        beginTest("Noise fringe stops before a neighboring envelope note");
        auto neighbors = withTones(1, 0, 2);
        neighbors.frames[0].setTonePeriod(0, 512);
        neighbors.frames[0].setEnvelopePeriod(34);
        const auto plainNeighbors = renderData(neighbors);
        neighbors.frames[0].setNoiseOn(0, true);
        const auto noisyNeighbors = renderData(neighbors);
        int envelopeRows = 0;
        for (int y = 0; y < plainNeighbors.getHeight(); ++y) {
            if (! hasEnvelopeFill(plainNeighbors, y, Colors::PSG::B))  // B plays the envelope alone
                continue;
            ++envelopeRows;
            for (int x = 0; x < plainNeighbors.getWidth(); ++x)
                expect(noisyNeighbors.getPixelAt(x, y) == plainNeighbors.getPixelAt(x, y));
        }
        expect(envelopeRows > 0);

        beginTest("Culling offscreen noise columns preserves the visible pixel pattern");
        auto twoFrames = withTones(7, 7);
        twoFrames.frames.push_back(twoFrames.frames.front());
        const auto whole = Tests::renderPsgClip(engine, twoFrames, 200, 300);
        const auto cropped = Tests::renderPsgClip(engine, twoFrames, 100, 300, {1, 2});
        expect(equalImages(whole.getClippedImage({100, 0, 100, 300}), cropped));

        beginTest("Noise uses rectangular marks large enough to span two logical pixels");
        const auto rectangles = renderData(withTones(1, 1), 6);
        bool hasTwoByTwo = false;
        for (int y = 0; y < rectangles.getHeight() - 1; ++y) {
            if (y >= noteBounds.getY() && y < noteBounds.getBottom())
                continue;
            for (int x = 0; x < rectangles.getWidth() - 1; ++x) {
                const auto color = rectangles.getPixelAt(x, y);
                if (color.getFloatAlpha() > 0.3f && color == rectangles.getPixelAt(x + 1, y)
                    && color == rectangles.getPixelAt(x, y + 1) && color == rectangles.getPixelAt(x + 1, y + 1))
                    hasTwoByTwo = true;
            }
        }
        expect(hasTwoByTwo);

        beginTest("A channel with a visible tone does not also put noise around its envelope pitch");
        auto dual = withTones(2, 2, 2);
        const auto dualImage = renderData(dual);
        const auto noNoiseDual = renderData(withTones(2, 0, 2));
        int envTop = 300, envBottom = 0;
        for (int y = 150; y < 300; ++y) {
            if (hasEnvelopeFill(noNoiseDual, y)) {
                envTop = jmin(envTop, y);
                envBottom = y + 1;
            }
        }
        expect(envBottom > envTop);
        for (int y = envTop - 10; y < envBottom + 10; ++y) {
            if (y >= envTop && y < envBottom)
                continue;
            for (int x = 0; x < 100; ++x)
                expect(dualImage.getPixelAt(x, y).getFloatAlpha() <= 0.3f,
                       "Only the faint gray band remains outside the envelope silhouette");
        }

        beginTest("Shared envelope noise retains only owners without visible tones");
        const auto sharedNoise = renderData(withTones(1, 3, 3));
        int greenFringe = 0;
        for (int y = envTop - 10; y < envBottom + 10; ++y) {
            if (y >= envTop && y < envBottom)
                continue;
            for (int x = 0; x < 100; ++x) {
                const auto color = sharedNoise.getPixelAt(x, y);
                if (color.getFloatAlpha() > 0.3f) {
                    expect(color.getGreen() > color.getRed() && color.getGreen() > color.getBlue());
                    ++greenFringe;
                }
            }
        }
        expect(greenFringe > 0);

        beginTest("Low envelope period draws an owner-outlined envelope-colored bottom arrow");
        auto lowEnvelope = withTones(2, 0, 2);
        lowEnvelope.frames[0].setEnvelopePeriod(256);
        const auto arrow = renderData(lowEnvelope);
        bool hasArrowColor = false, hasArrowOutline = false;
        for (int y = 280; y < 300; ++y) {
            for (int x = 0; x < 100; ++x) {
                const auto color = arrow.getPixelAt(x, y);
                hasArrowColor = hasArrowColor || color == Colors::PSG::Env;
                hasArrowOutline = hasArrowOutline || color == Colors::PSG::B;
            }
        }
        expect(hasArrowColor && hasArrowOutline);

        beginTest("Partial repaint does not move the low-envelope arrow");
        const Rectangle<int> dirtyRegion {0, 280, 20, 20};
        const auto partialArrow = Tests::renderPsgClip(engine, lowEnvelope, 100, 300, {}, false, dirtyRegion);
        expect(equalImages(arrow.getClippedImage(dirtyRegion), partialArrow.getClippedImage(dirtyRegion)));

        beginTest("Low-envelope arrow stays legible without any visible tone pitch");
        auto lowEnvelopeOnly = lowEnvelope;
        lowEnvelopeOnly.frames[0].setToneOn(1, false);
        const auto lowEnvelopeOnlyImage = renderData(lowEnvelopeOnly);
        Rectangle<int> lowEnvelopeBounds;
        for (int y = 280; y < 300; ++y)
            for (int x = 0; x < 100; ++x)
                if (! lowEnvelopeOnlyImage.getPixelAt(x, y).isTransparent())
                    lowEnvelopeBounds = lowEnvelopeBounds.getUnion({x, y, 1, 1});
        expect(lowEnvelopeBounds.getHeight() >= 8, "Keep the arrow distinct from a pitched note line");

        lowEnvelope.frames[0].setEnvMod(1, false);
        const auto noEnvelope = renderData(lowEnvelope);
        for (int y = 280; y < 300; ++y)
            for (int x = 0; x < 100; ++x)
                expect(noEnvelope.getPixelAt(x, y).isTransparent());

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

        beginTest("An envelope several channels play is envelope-colored, not a tint of theirs");
        const auto envelopeB = renderData({
            {{{uZX::PsgRegType::Mixer, 0x3f}, {uZX::PsgRegType::VolumeB, 16},
              {uZX::PsgRegType::EnvelopePeriodFine, 100}, {uZX::PsgRegType::EnvelopeShape, 8}}},
            {50.0, 1}
        });
        const auto envelopeAC = renderData({
            {{{uZX::PsgRegType::Mixer, 0x3f}, {uZX::PsgRegType::VolumeA, 16}, {uZX::PsgRegType::VolumeC, 16},
              {uZX::PsgRegType::EnvelopePeriodFine, 100}, {uZX::PsgRegType::EnvelopeShape, 8}}},
            {50.0, 1}
        });
        bool hasFill = false, hasHatching = false, hasChannelFill = false;
        for (int y = 0; y < envelopeAC.getHeight(); ++y) {
            for (int x = 0; x < envelopeAC.getWidth(); ++x) {
                const auto color = envelopeAC.getPixelAt(x, y);
                hasFill = hasFill || color == Colors::PSG::Env;
                hasHatching = hasHatching || color.getGreen() > Colors::PSG::Env.getGreen() + 16;
                hasChannelFill = hasChannelFill || envelopeB.getPixelAt(x, y) == Colors::PSG::B;
            }
        }
        expect(hasFill, "The envelope note is filled with the envelope color");
        expect(hasHatching, "The envelope note keeps its brighter hatching");
        expect(hasChannelFill, "An envelope B plays alone, with no tone of its own, is B's sound: in B's colour");

        beginTest("Legend shows A/B/C colors and a padded filled envelope swatch");
        const auto header = render(0x3f, 0, 0, 0, true);
        const std::array<Colour, 3> colors {Colors::PSG::A, Colors::PSG::B, Colors::PSG::C};
        for (int ch = 0; ch < 3; ++ch) {
            const int x = 3 + ch * 13;
            expect(header.getPixelAt(x, 14) == colors[static_cast<size_t>(ch)]);
            expect(header.getPixelAt(x, 15).isTransparent());
        }
        expect(header.getPixelAt(42, 15).isTransparent(), "E keeps bottom padding");
        bool hasLegendFill = false, hasEnvelopeHatching = false;
        for (int y = 4; y < 14; ++y) {
            for (int x = 43; x < 53; ++x) {
                const auto color = header.getPixelAt(x, y);
                expect(! color.isTransparent(), "E is filled");
                hasLegendFill = hasLegendFill || color == Colors::PSG::Env;
                hasEnvelopeHatching = hasEnvelopeHatching
                    || color.getGreen() > Colors::PSG::Env.getGreen() + 16;
            }
        }
        expect(hasLegendFill, "E is filled with the envelope pitch color");
        expect(hasEnvelopeHatching, "E keeps the brighter envelope hatching");
        beginTest("N legend uses the band color with vertical fading and bottom padding");
        const auto noiseCenter = header.getPixelAt(55, 9);
        expect(noiseCenter.getFloatAlpha() > 0.7f);
        expect(header.getPixelAt(55, 3).getAlpha() < noiseCenter.getAlpha());
        expect(header.getPixelAt(55, 14).getAlpha() < noiseCenter.getAlpha());
        expect(header.getPixelAt(55, 15).isTransparent());
        expect(std::abs(int(noiseCenter.getRed()) - int(Colors::PSG::Mix.getRed())) <= 2);
        expect(std::abs(int(noiseCenter.getGreen()) - int(Colors::PSG::Mix.getGreen())) <= 2);
        expect(std::abs(int(noiseCenter.getBlue()) - int(Colors::PSG::Mix.getBlue())) <= 2);
        expect(header.getPixelAt(68, 8).isTransparent(), "The legend ends with N");

        beginTest("Frame hover shows the full state as a right-aligned table, changes bright");
        using T = PsgParamType;
        const PsgParamFrameData frame {std::vector<std::tuple<T, uint16_t, bool>> {
            {T::TonePeriodA, 254, true}, {T::VolumeA, 15, false}, {T::ToneIsOnA, 1, false},
            {T::TonePeriodB, 1000, false}, {T::VolumeB, 8, true},
            {T::TonePeriodC, 4095, false}, {T::EnvelopeIsOnC, 1, true},
            {T::EnvelopeShape, 0xE, true}, {T::EnvelopePeriod, 120, false}, {T::NoisePeriod, 5, false}}};
        const auto marked = [] (const std::vector<std::pair<String, Colour>>& runs) {
            String text;  // changed (opaque) runs in [brackets]
            for (const auto& [run, colour] : runs)
                text << (colour.isOpaque() ? "[" + run + "]" : run);
            return text;
        };
        const auto runs = formatPsgFrame(frame, 1773400.0, false);
        expectEquals(marked(runs), String("[A    ]  [ 254]  [436.4Hz]  15  T--\n"
                                          "[B    ]  1000  110.8Hz  [ 8]  ---\n"
                                          "[C    ]  4095   27.1Hz   0  --[E]\n"
                                          "[Env  ]   120   57.7Hz  shape [E]\n"
                                          "Noise     5"));
        expectEquals(marked(formatPsgFrame(frame, 1773400.0, true)),
                     String("[A    ]  [ 0FE]  [436.4Hz]  F  T--\n"
                            "[B    ]   3E8  110.8Hz  [8]  ---\n"
                            "[C    ]   FFF   27.1Hz  0  --[E]\n"
                            "[Env  ]  0078   57.7Hz  shape [E]\n"
                            "Noise    05"));
        expect(runs[0].second == Colors::PSG::A, "Changed rows use the legend colour");
        const auto unchangedB = std::find_if(runs.begin(), runs.end(), [] (const auto& run) { return run.first == "1000"; });
        expect(unchangedB != runs.end() && unchangedB->second == Colors::PSG::B.withMultipliedAlpha(0.5f),
               "Unchanged values use the half-alpha legend colour");

        beginTest("With a tuning, the frame hover names each pitch's nearest note and its cents off it");
        {
            const auto grid = makeEqualTemperamentGrid(440.0, 1773400.0);  // A4 at period 252
            expect(getNearestNote(grid, 252.0) == std::pair<String, int> {"A4", 0});
            expect(getNearestNote(grid, 126.0) == std::pair<String, int> {"A5", 0});
            expect(getNearestNote(grid, 254.0) == std::pair<String, int> {"A4", -14}, "a longer period is flat");
            expect(getNearestNote(grid, 250.0) == std::pair<String, int> {"A4", 14}, "a shorter one sharp");
            expect(getNearestNote(grid, 8.0).first == "A9", "past the table's top, by octaves: " + getNearestNote(grid, 8.0).first);
            expect(getNearestNote({}, 252.0).first.isEmpty(), "no tuning, no name");

            expectEquals(marked(formatPsgFrame(frame, 1773400.0, false, grid)),
                         String::fromUTF8("[A    ]  [ 254]  [436.4Hz]  [A4  -14\xC2\xA2]  15  T--\n"
                                          "[B    ]  1000  110.8Hz  A2  +14\xC2\xA2  [ 8]  ---\n"
                                          "[C    ]  4095   27.1Hz  A0  -28\xC2\xA2   0  --[E]\n"
                                          "[Env  ]   120   57.7Hz  A#1 -16\xC2\xA2  shape [E]\n"
                                          "Noise     5"));
        }

        beginTest("The frame hover opens with where the frame is, in each of the timeline's timecodes");
        {
            // Frame 75 of a 50 Hz clip at the start of an edit at 120 bpm: a second and a half in, the fourth beat
            auto edit = te::Edit::createSingleTrackEdit(engine);
            uZX::PsgData data {std::vector<uZX::PsgRegsFrame>(100), {50.0, 1}};
            data.frames[0] = {{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::VolumeA, 15}, {uZX::PsgRegType::TonePeriodFineA, 252}};
            data.frames[99] = {{uZX::PsgRegType::VolumeA, 0}};
            auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                          {{te::TimePosition(), te::TimeDuration::fromSeconds(2.0)}, {}});
            const auto runs = describePsgClipFrame(*clip, te::TimePosition::fromSeconds(1.505), false);
            String header;
            for (const auto& [text, colour] : runs) {
                if (text == "\n")
                    break;
                header << (colour == Colors::Theme::textPrimary ? "[" + text + "]" : text);  // values bright, names dimmed
            }
            expectEquals(header, String("Time   [00:01:25]   Frame [75]   Bar [1.4.00]"));
            expect(describePsgClipFrame(*clip, te::TimePosition::fromSeconds(-1.0), false).empty(), "before the clip's first frame");
        }

        beginTest("The frame hover stays when a modifier key sends a move to where the mouse already is");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            uZX::PsgData data {std::vector<uZX::PsgRegsFrame>(100), {50.0, 1}};
            data.frames[0] = {{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::VolumeA, 15}, {uZX::PsgRegType::TonePeriodFineA, 252}};
            auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                          {{te::TimePosition(), te::TimeDuration::fromSeconds(2.0)}, {}});
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(400);
            view.zoom.setRange({te::TimePosition(), te::TimeDuration::fromSeconds(2.0)});
            TimelineGrid grid(view);
            Component top;
            top.setSize(400, 200);
            PsgClipComponent component(view, grid, clip);
            top.addAndMakeVisible(component);
            component.setBounds(0, 0, 400, 200);
            const auto moveTo = [&] (Point<float> pos) {
                const auto now = Time::getCurrentTime();
                component.mouseMove({Desktop::getInstance().getMainMouseSource(), pos, ModifierKeys {},
                                     MouseInputSource::defaultPressure, MouseInputSource::defaultOrientation,
                                     MouseInputSource::defaultRotation, MouseInputSource::defaultTiltX,
                                     MouseInputSource::defaultTiltY, &component, &component, now, pos, now, 0, false});
            };
            moveTo({100.0f, 50.0f});
            component.timerCallback();  // the pointer rested
            expectEquals(top.getNumChildComponents(), 2, "the hover opens");
            moveTo({100.0f, 50.0f});  // as JUCE sends for a modifier key
            expectEquals(top.getNumChildComponents(), 2, "a move to where the mouse is keeps it");
            moveTo({120.0f, 50.0f});
            expectEquals(top.getNumChildComponents(), 1, "a move elsewhere hides it");
        }

        beginTest("Frame hover lists the timers running at the frame: what each drives, its pitch, duty and values");
        {
            juce::ValueTree clipState {"PSGCLIP"};
            ClipTimers timers {clipState};
            TaymMods faster {};
            faster.command = TAYM_CMD_MODULATE;
            faster.base_timer_value = 100;
            faster.timer_lane_ref = TAYM_TLAN_UNCHANGED;
            auto otherLane = faster;  // R8 from lane 1 (3, 0)
            otherLane.base_timer_value = 0;
            otherLane.first_action = 2;
            otherLane.action_count = 1;
            timers.set(Tests::makeTestTaym(), 0, {
                {10, 0, Tests::timerStart(222, 0)},   // R8 from lane 0 (15, 9): 3552 cycles an expiry
                {12, 1, Tests::timerStart(222, 4)},   // R13, always 0x0C
                {20, 0, faster},
                {25, 0, otherLane},
            }, nullptr);

            expectEquals(marked(formatTimerFrame(timers, 9, 1773400.0, false)), String(), "no timer runs yet");
            expectEquals(marked(formatTimerFrame(timers, 10, 1773400.0, false)),
                         String("[T1   ] [start]  [249.6Hz]  [duty 50%]\n"
                                "      [interval          222]\n"
                                "      [A vol     lane 0   15    9]"));
            expectEquals(marked(formatTimerFrame(timers, 15, 1773400.0, false)),
                         String("T1           249.6Hz  duty 50%\n"
                                "      interval          222\n"
                                "      A vol     lane 0   15    9\n"
                                "T2           499.3Hz\n"
                                "      interval   222\n"
                                "      env shape   12"));
            expectEquals(marked(formatTimerFrame(timers, 20, 1773400.0, true)),
                         String("[T1   ] [mod  ]  [554.2Hz]  [duty 50%]\n"
                                "      [interval          0064]\n"
                                "      A vol     lane 0    0F    09\n"
                                "T2           499.3Hz\n"
                                "      interval   00DE\n"
                                "      env shape    0C"));
            // A modulate's rows are bright where it changed them: here its base, then its volume's lane
            expectEquals(marked(formatTimerFrame(timers, 25, 1773400.0, false)),
                         String("[T1   ] [mod  ]  [554.2Hz]  [duty 50%]\n"
                                "      interval          100\n"
                                "      [A vol     lane 1    3    0]\n"
                                "T2           499.3Hz\n"
                                "      interval   222\n"
                                "      env shape   12"));
            const auto runs = formatTimerFrame(timers, 15, 1773400.0, false);
            expect(runs.front().second == Colors::PSG::A.withMultipliedAlpha(0.5f), "a timer on A's volume is in A's colour");
            expect(runs.back().second == Colors::PSG::Env.withMultipliedAlpha(0.5f), "one on the envelope's shape in the envelope's");
            // With the clip's tuning, the timer's pitch is named too: a repeat of 3552 cycles is a tone of period 222,
            // and the grid's B4 is 224
            const auto grid = makeEqualTemperamentGrid(440.0, 1773400.0);
            const auto named = marked(formatTimerFrame(timers, 15, 1773400.0, false, grid));
            expect(named.contains(String::fromUTF8("\nT2           499.3Hz  B4 +16\xC2\xA2\n")), named);

            // Under a chip's rows the pitch and the note stand in their columns: right of periods 4 wide, the
            // Hz right-aligned in 8, the note's name in 3 and its cents in 5
            const auto aligned = marked(formatTimerFrame(timers, 15, 1773400.0, false, grid, {4, 8, 3, 5}));
            expect(aligned.contains(String::fromUTF8("\nT2            499.3Hz  B4   +16\xC2\xA2\n")), aligned);
        }

        beginTest("A timer is drawn as a note at the pitch of what it writes, from its start to its stop, in its channel's colour");
        {
            // Fifty silent frames, and a timer on A's volume from frame 10 to 30: 15 and 9 turn about, 7104
            // cycles a repeat, the pitch of a tone of period 444
            const uZX::PsgData silent {
                {{{uZX::PsgRegType::Mixer, 0x3f}}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                 {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                 {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {{uZX::PsgRegType::VolumeB, 0}}},
                {50.0, 1}
            };
            const auto withTimers = [&] (std::vector<TimerEvent> events, std::optional<VizOptions> viz = {}) {
                return Tests::renderPsgClip(engine, silent, 100, 300, {}, false, {}, viz,
                    [&] (PsgClip& clip) { clip.getTimers().set(Tests::makeTestTaym(), 0, events, nullptr); });
            };
            TaymMods stop {};
            stop.command = TAYM_CMD_STOP;

            expect(isBlank(renderData(silent)), "the frames alone draw nothing");
            // Two pixels a frame leave the decorations out, unless they are drawn always, as the envelope's are
            const auto image = withTimers({{10, 0, Tests::timerStart(222, 0)}, {30, 0, stop}},
                                          VizOptions {.decorations = VizOptions::Decorations::always});
            int left = image.getWidth(), right = -1, top = image.getHeight(), bottom = -1;
            bool channelHue = true, dimBody = false, brightFill = false;
            for (int y = 0; y < image.getHeight(); ++y) {
                for (int x = 0; x < image.getWidth(); ++x) {
                    const auto colour = image.getPixelAt(x, y);
                    if (colour.isTransparent())
                        continue;
                    left = jmin(left, x); right = jmax(right, x); top = jmin(top, y); bottom = jmax(bottom, y);
                    channelHue = channelHue && std::abs(colour.getHue() - Colors::PSG::A.getHue()) < 0.03f;
                    dimBody = dimBody || colour.getAlpha() < 160;
                    brightFill = brightFill || colour.getAlpha() > 200;
                }
            }
            expectEquals(left, 20, "from the start's frame: two pixels a frame");
            expectEquals(right, 59, "to the stop's");
            expect(channelHue, "in channel A's colour");
            expect(dimBody && brightFill, "a dim body between bright edges, and a bright fill");
            expect(bottom - top <= 30, "one note, with its start's tick: rows " + String(top) + " to " + String(bottom));

            // At a duty of 50% the bottom half of the note is filled, away from the tick; above the fill, the
            // bright top edge
            int noteRows = 0, brightRows = 0;
            bool fromBottom = true;
            for (int y = 0; y < image.getHeight(); ++y)
                if (const auto colour = image.getPixelAt(40, y); ! colour.isTransparent()) {
                    if (noteRows++ == 0)
                        continue;  // the top edge
                    const bool bright = colour.getAlpha() > 200;
                    fromBottom = fromBottom && (bright || brightRows == 0);  // no dim row under a bright one
                    brightRows += bright ? 1 : 0;
                }
            expect(noteRows >= 8, "tall enough for the fill: " + String(noteRows));
            expect(fromBottom, "the fill rises from the note's bottom");
            expect(std::abs(brightRows - noteRows / 2) <= 1,
                   "a fill of " + String(brightRows) + " rows in a note of " + String(noteRows));
            bool plain = true;
            const auto undecorated = withTimers({{10, 0, Tests::timerStart(222, 0)}, {30, 0, stop}});
            for (int y = 0; y < undecorated.getHeight(); ++y)
                for (int x = 0; x < undecorated.getWidth(); ++x)
                    if (const auto colour = undecorated.getPixelAt(x, y); ! colour.isTransparent())
                        plain = plain && colour.getAlpha() > 200;
            expect(plain, "without the decorations, a timer at volume 15 is filled as a tone at it");

            // A timer is as opaque as the loudest volume it writes, as a tone at it is
            const auto mostOpaque = [] (const Image& drawn) {
                uint8 most = 0;
                for (int y = 0; y < drawn.getHeight(); ++y)
                    for (int x = 0; x < drawn.getWidth(); ++x)
                        most = jmax(most, drawn.getPixelAt(x, y).getAlpha());
                return most;
            };
            const auto quiet = withTimers({{10, 0, Tests::timerStart(222, 2)}, {30, 0, stop}});  // R8 from lane 1: 3, 0
            expect(mostOpaque(quiet) > 0, "a quiet timer is drawn");
            expect(mostOpaque(quiet) < mostOpaque(image) / 2,
                   "at 3 fainter than at 15: " + String(mostOpaque(quiet)) + " and " + String(mostOpaque(image)));

            // Two timers in one clip: on A's volume, and twice as fast on B's
            const auto both = withTimers({{10, 0, Tests::timerStart(222, 0)}, {10, 1, Tests::timerStart(111, 3)}});
            const auto topOf = [&] (Colour channel) {
                for (int y = 0; y < both.getHeight(); ++y)
                    for (int x = 30; x < 50; ++x)
                        if (const auto colour = both.getPixelAt(x, y);
                            ! colour.isTransparent() && std::abs(colour.getHue() - channel.getHue()) < 0.03f)
                            return y;
                return -1;
            };
            const int slower = topOf(Colors::PSG::A), faster = topOf(Colors::PSG::B);
            expect(slower >= 0 && faster >= 0, "each in its channel's colour");
            // The clip's pitch range has every tone period over the height: an octave is a twelfth or so of it
            expect(faster < slower - 10, "a timer twice as fast is drawn higher: rows " + String(faster) + " and " + String(slower));

            const auto shape = withTimers({{10, 1, Tests::timerStart(222, 4)}});  // R13, never stopped
            bool envelopeHue = false, toTheEnd = false;
            for (int y = 0; y < shape.getHeight(); ++y)
                for (int x = 0; x < shape.getWidth(); ++x)
                    if (const auto colour = shape.getPixelAt(x, y); ! colour.isTransparent()) {
                        envelopeHue = envelopeHue || std::abs(colour.getHue() - Colors::PSG::Env.getHue()) < 0.03f;
                        toTheEnd = toTheEnd || x == shape.getWidth() - 1;
                    }
            expect(envelopeHue, "a timer on the envelope's shape is in the envelope's colour");

            // Each register a timer holds is decorated as a frame's notes show it, drawn always here
            const VizOptions always {.decorations = VizOptions::Decorations::always};
            const auto middleRow = [] (const Image& drawn, int x) {
                int top = -1, bottom = -1;
                for (int y = 0; y < drawn.getHeight(); ++y)
                    if (drawn.getPixelAt(x, y).getAlpha() > 0) {
                        top = top < 0 ? y : top;
                        bottom = y;
                    }
                return (top + bottom) / 2;
            };
            // A's tone period from lane 0 (15, 9): thin vertical lines every third pixel, in A's colour
            const auto periodNote = withTimers({{10, 0, Tests::timerStart(222, 6)}, {30, 0, stop}}, always);
            const int toneRow = middleRow(periodNote, 40);
            std::vector<uint8> row;
            for (int x = 24; x < 56; ++x)
                row.push_back(periodNote.getPixelAt(x, toneRow).getAlpha());
            bool everyThird = *std::max_element(row.begin(), row.end()) > *std::min_element(row.begin(), row.end());
            for (size_t i = 3; i < row.size(); ++i)
                everyThird = everyThird && row[i] == row[i - 3];
            expect(everyThird, "a tone period's lines, every third pixel");
            expect(std::abs(periodNote.getPixelAt(40, toneRow).getHue() - Colors::PSG::A.getHue()) < 0.03f, "in A's colour");

            // The noise period and the mixer belong to no channel: grey, the noise period hatched as a tone's
            const auto isGrey = [] (const Image& drawn) {
                bool grey = true, any = false;
                for (int y = 0; y < drawn.getHeight(); ++y)
                    for (int x = 0; x < drawn.getWidth(); ++x)
                        if (const auto colour = drawn.getPixelAt(x, y); colour.getAlpha() > 0) {
                            any = true;
                            grey = grey && colour.getSaturation() < 0.05f;
                        }
                return any && grey;
            };
            const auto noiseNote = withTimers({{10, 1, Tests::timerStart(222, 7)}, {30, 1, stop}}, always);
            expect(isGrey(noiseNote), "a timer on the noise period is grey");
            const int noiseRow = middleRow(noiseNote, 40);
            expect(noiseNote.getPixelAt(40, noiseRow) != noiseNote.getPixelAt(41, noiseRow)
                       || noiseNote.getPixelAt(41, noiseRow) != noiseNote.getPixelAt(42, noiseRow),
                   "and hatched");
            expect(isGrey(withTimers({{10, 1, Tests::timerStart(222, 5)}, {30, 1, stop}}, always)),
                   "a timer on the mixer is grey");

            // The envelope's shape C restarted: rising stripes, brighter than the note
            const auto restarts = withTimers({{10, 1, Tests::timerStart(222, 4)}, {30, 1, stop}}, always);
            const int restartsRow = middleRow(restarts, 40);
            int faintest = 255, strongest = 0;
            for (int x = 22; x < 58; ++x) {
                const int alpha = restarts.getPixelAt(x, restartsRow).getAlpha();
                faintest = jmin(faintest, alpha);
                strongest = jmax(strongest, alpha);
            }
            expect(strongest > faintest + 50, "the envelope's stripes on a timer that restarts it, across its middle");

            // Where B plays the envelope alone, the envelope and the timer that restarts it are B's sound
            auto onB = silent;
            onB.frames[0] = {{uZX::PsgRegType::Mixer, 0x3f}, {uZX::PsgRegType::VolumeB, 16},
                             {uZX::PsgRegType::EnvelopePeriodFine, 22}, {uZX::PsgRegType::EnvelopeShape, 12}};
            onB.frames.back() = {};  // B keeps the envelope to the end
            const auto shapeOnB = Tests::renderPsgClip(engine, onB, 100, 300, {}, false, {}, {}, [&] (PsgClip& clip) {
                clip.getTimers().set(Tests::makeTestTaym(), 0, {{10, 1, Tests::timerStart(222, 4)}}, nullptr);
            });
            int drawnAfterStart = 0;
            bool allB = true;
            for (int y = 0; y < shapeOnB.getHeight(); ++y)
                for (int x = 0; x < shapeOnB.getWidth(); ++x)
                    if (const auto colour = shapeOnB.getPixelAt(x, y); colour.getAlpha() > 100) {
                        allB = allB && std::abs(colour.getHue() - Colors::PSG::B.getHue()) < 0.03f;
                        drawnAfterStart += x >= 20 ? 1 : 0;
                    }
            expect(drawnAfterStart > 0 && allB, "the envelope's note and the timer's, both in B's colour");
            expect(toTheEnd, "a timer nothing stops runs to the clip's end");

            // A tone the frames leave at volume 0 sounds where a timer writes its channel's volume: it is drawn
            // there, and counted for the tuning
            auto tone = silent;
            tone.frames[0] = {{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::TonePeriodFineA, 200},
                              {uZX::PsgRegType::TonePeriodCoarseA, 0}, {uZX::PsgRegType::VolumeA, 0}};
            int counted = -1;
            const auto underTimer = Tests::renderPsgClip(engine, tone, 100, 300, {}, false, {}, {}, [&] (PsgClip& clip) {
                clip.getTimers().set(Tests::makeTestTaym(), 0, {{10, 0, Tests::timerStart(222, 0)}, {30, 0, stop}}, nullptr);
                const auto& histogram = clip.getPitchHistogram();
                counted = histogram.tone.contains(200) ? histogram.tone.at(200) : 0;
            });
            expectEquals(counted, 20, "the frames from the timer's start to its stop");
            const auto notesAt = [&] (int x) {
                int notes = 0;
                for (int y = 0; y < underTimer.getHeight(); ++y)
                    if (! underTimer.getPixelAt(x, y).isTransparent() && (y == 0 || underTimer.getPixelAt(x, y - 1).isTransparent()))
                        ++notes;
                return notes;
            };
            expectEquals(notesAt(10), 0, "before the timer the tone is silent");
            expectEquals(notesAt(40), 2, "under it, the tone and the timer's own note");
            expectEquals(notesAt(80), 0, "after its stop, silent again");

            // A tone at the timer's pitch shares its lane, as two tones do: the tone above, the timer below
            auto samePitch = silent;
            samePitch.frames[0] = {{uZX::PsgRegType::Mixer, 0x3d}, {uZX::PsgRegType::TonePeriodFineB, 444 & 0xFF},
                                   {uZX::PsgRegType::TonePeriodCoarseB, 444 >> 8}, {uZX::PsgRegType::VolumeB, 15}};
            const auto shared = Tests::renderPsgClip(engine, samePitch, 100, 300, {}, false, {}, {}, [&] (PsgClip& clip) {
                clip.getTimers().set(Tests::makeTestTaym(), 0, {{10, 0, Tests::timerStart(222, 0)}, {30, 0, stop}}, nullptr);
            });
            int toneTop = -1, toneBottom = -1, timerTop = -1, timerBottom = -1;
            for (int y = 0; y < shared.getHeight(); ++y) {
                const auto colour = shared.getPixelAt(40, y);
                if (colour.isTransparent())
                    continue;
                const bool isTone = std::abs(colour.getHue() - Colors::PSG::B.getHue()) < 0.03f;
                const bool isTimer = std::abs(colour.getHue() - Colors::PSG::A.getHue()) < 0.03f;
                expect(isTone || isTimer, "no row of one drawn over the other: row " + String(y));
                auto& first = isTone ? toneTop : timerTop;
                (isTone ? toneBottom : timerBottom) = y;
                if (first < 0)
                    first = y;
            }
            expect(toneTop >= 0 && timerTop >= 0, "both are drawn");
            expect(toneBottom < timerTop, "the tone's half above the timer's: rows " + String(toneTop) + "-" + String(toneBottom)
                                              + " and " + String(timerTop) + "-" + String(timerBottom));
        }

        // To look at the frame hover on a real tune, the fixtures' example:
        //     UZX_SNAPSHOT_FRAME_HOVER=<file.png> uZXTests PsgClipPainting
        // UZX_SNAPSHOT_FRAME=<n> picks the frame (3072: a timer on two registers); a second file, -hex, has hex values.
        if (const juce::File out(juce::SystemStats::getEnvironmentVariable("UZX_SNAPSHOT_FRAME_HOVER", {})); out != juce::File()) {
            juce::MemoryBlock bytes;
            juce::File(UZX_FIXTURES_DIR).getChildFile("taym/atarized.taym").loadFileAsData(bytes);
            if (const auto tune = uZX::readTaymModule(bytes)) {
                auto edit = te::Edit::createSingleTrackEdit(engine);
                auto data = tune->chips[0].psg;
                auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                              {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
                clip->getTimers().set(tune->chips[0].timers.bank, 0, tune->chips[0].timers.events, nullptr);
                const int frame = juce::SystemStats::getEnvironmentVariable("UZX_SNAPSHOT_FRAME", "3072").getIntValue();
                for (bool hex : {false, true}) {
                    const auto hover = createPsgFrameHover(describePsgClipFrame(*clip, te::TimePosition::fromSeconds((frame + 0.25) / 50.0), hex));
                    const auto file = hex ? out.getSiblingFile(out.getFileNameWithoutExtension() + "-hex.png") : out;
                    file.deleteFile();
                    juce::FileOutputStream stream(file);
                    juce::PNGImageFormat().writeImageToStream(hover->createComponentSnapshot(hover->getLocalBounds(), true, 2.0f), stream);
                }
            }
        }

        beginTest("A held note has no seams between frames at a fractional frame width");
        {
            const uZX::PsgRegsFrame tone {{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::VolumeA, 15},
                                          {uZX::PsgRegType::TonePeriodFineA, 200}};
            const auto image = renderData({{tone, tone, tone}, {50.0, 1}});  // 33⅓ px a frame
            int row = -1;
            for (int y = 0; y < image.getHeight() && row < 0; ++y)
                if (image.getPixelAt(1, y).isOpaque())
                    row = y;
            expect(row >= 0, "the note must be painted");
            for (int x = 0; row >= 0 && x < image.getWidth(); ++x)
                expect(image.getPixelAt(x, row) == image.getPixelAt(1, row), "seam at x = " + String(x));

            // Narrower than a pixel, a frame is not snapped to one: a lone note would round away
            const uZX::PsgRegsFrame silence {{uZX::PsgRegType::Mixer, 0x3f}, {uZX::PsgRegType::VolumeA, 0}};
            std::vector<uZX::PsgRegsFrame> frames(300, silence);
            frames[152] = tone;  // x 50.67..51: rounded, no pixel
            expect(! isBlank(renderData({frames, {50.0, 1}})), "a note a third of a pixel wide must still show");
        }

        beginTest("A note holds through frames that change nothing, also scrolled past its start");
        {
            const uZX::PsgRegsFrame tone {{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::VolumeA, 15},
                                          {uZX::PsgRegType::TonePeriodFineA, 200}};
            const uZX::PsgData held {{tone, {}, {}, {}}, {50.0, 1}};  // empty frames are not stored
            const auto image = renderData(held, 400);
            int row = -1;
            for (int y = 0; y < image.getHeight() && row < 0; ++y)
                if (image.getPixelAt(1, y).isOpaque())
                    row = y;
            expect(row >= 0, "the note must be painted");
            for (int x = 0; row >= 0 && x < image.getWidth(); x += 50)
                expect(image.getPixelAt(x, row) == image.getPixelAt(1, row), "gap at x = " + String(x));
            const auto scrolled = Tests::renderPsgClip(engine, held, 200, 300, {2, 4});
            expect(equalImages(image.getClippedImage({200, 0, 200, 300}), scrolled));
        }
    }
};

static PsgClipPaintingTest psgClipPaintingTest;

//==============================================================================
/** Lane geometry and level of detail of the Notes view's tuning lanes. */
class PsgNoteLanesTest : public UnitTest
{
public:
    PsgNoteLanesTest() : UnitTest("PsgNoteLanes", "MoTool") {}

    void runTest() override {
        const auto toneScale = PsgParamType{PsgParamType::TonePeriodA}.getScale();
        const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
        const auto lanes = makeNoteLanes(grid);

        beginTest("One lane per grid note, lowest first, boundaries between pitch centres");
        expectGreaterThan(static_cast<int>(lanes.size()), static_cast<int>(grid.size()));
        for (size_t i = 0; i < lanes.size(); ++i) {
            const auto& lane = lanes[i];
            for (const auto& [period, note] : grid)
                if (note == lane.note) {
                    const float centre = toneScale.valueToNormalized(period);
                    expect(lane.lo < centre && centre < lane.hi, "lane " + lane.label + " must contain its note");
                }
            if (i > 0) {
                expectGreaterThan(lane.note, lanes[i - 1].note);
                expectWithinAbsoluteError(lane.lo, lanes[i - 1].hi, 1e-6f);  // lanes tile the axis
            }
        }
        expectEquals(lanes.front().label, getMidiNoteName(lanes.front().note));

        beginTest("Lanes continue past the table to the ends of the tone range");
        {
            // Tables span C1 (24) to B8 (119); notes play up to period 8 and down to 4095.
            expect(lanes.back().hi >= toneScale.valueToNormalized(8), "lanes must reach the highest drawn note");
            expect(lanes.front().lo <= toneScale.valueToNormalized(4095), "lanes must reach the lowest tone");
            expectGreaterOrEqual(lanes.front().note, 0);

            // The extrapolated octave above B8 keeps the table's top-octave spacing.
            auto laneOf = [&](int note) { return std::find_if(lanes.begin(), lanes.end(), [=](auto& l) { return l.note == note; }); };
            const auto c8 = laneOf(108), c9 = laneOf(120), b8 = laneOf(119);
            expect(c8 != lanes.end() && c9 != lanes.end() && b8 != lanes.end());
            if (c9 != lanes.end() && b8 != lanes.end())
                expectWithinAbsoluteError(c9->lo, b8->hi, 1e-6f);
            const auto g9sharp = laneOf(128);  // past MIDI 127, so the label is composed
            expect(g9sharp != lanes.end() && g9sharp->label == "G#9");
        }
        expect(makeNoteLanes({{100, 60}}).empty(), "a single note has no neighbour to bound it");

        beginTest("Lane names stay inside a clip that starts after the view");
        {
            // A 10 s clip at 10..20 s in a 20 s view: the view's left edge is 500 px left
            // of the clip, and names drawn there would be outside the component.
            auto& engine = *te::Engine::getEngines()[0];
            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto& pt1 = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
            const int a3 = std::find_if(pt1.begin(), pt1.end(), [](auto& e) { return e.second == 57; })->first;
            uZX::PsgData data{{}, {50.0, 1}};
            data.frames.push_back({{uZX::PsgRegType::VolumeA, 12},
                                   {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(a3 & 0xff)},
                                   {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(a3 >> 8)}});
            for (int i = 0; i < 499; ++i)
                data.frames.push_back({});
            auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                          {{te::TimePosition::fromSeconds(10.0), te::TimeDuration::fromSeconds(10.0)}, {}},
                                          "late");
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(1000);
            view.zoom.setRange({te::TimePosition(), te::TimeDuration::fromSeconds(20.0)});
            TimelineGrid grid(view);
            PsgClipComponent component(view, grid, clip);
            component.setSize(500, 300);
            // Direct2D images (Windows) expose painted pixels only after the Graphics is destroyed.
            Image image(Image::ARGB, 500, 300, true);
            {
                Graphics g(image);
                component.paintNoteLanes(g);
            }

            // Lane lines and the tonic band are opaque greys; names are white at 0.45 or more, so a
            // white pixel that solid near the left edge is a name.
            bool nameNearLeftEdge = false;
            for (int y = 0; y < image.getHeight() && ! nameNearLeftEdge; ++y)
                for (int x = 0; x < 45 && ! nameNearLeftEdge; ++x) {
                    const auto c = image.getPixelAt(x, y);
                    nameNearLeftEdge = c.getFloatAlpha() > 0.35f && c.getBrightness() > 0.9f;
                }
            expect(nameNearLeftEdge, "note names must be drawn at the clip's own left edge");

            beginTest("Piano Keys lay a keyboard over the grid's left edge, the key's notes named on it");
            const bool wasPianoKeys = arePianoKeysShown(engine);
            setPianoKeysShown(engine, true);
            const auto paintKeys = [&](int height) {
                component.setSize(500, height);
                Image image(Image::ARGB, 500, height, true);
                {
                    Graphics g(image);
                    component.paintNoteLanes(g);
                }
                return image;
            };
            clip->chooseKey({Scale::Tonic(4), true});  // E minor: F# is its one black key, F is out
            const auto keys = paintKeys(300);
            clip->chooseKey({Scale::Tonic(6), true});  // F# minor, in a clip that names tonics only without keys
            const auto tonicKeys = paintKeys(100);  // lanes ~6.7 px: keys, but too short to name them all
            component.setSize(500, 300);
            setPianoKeysShown(engine, wasPianoKeys);

            // A row's key colour is what the black keys' width shows besides a name, which is
            // neither black nor white on either key. A key is named when light pixels are on it,
            // if black, or dark ones right of the black keys, if white; a run of same-coloured
            // rows is one key.
            enum { none = -1, black, white };
            struct KeysRead
            {
                int named[2] = {}, unnamed[2] = {};
                bool whiteNameLeftOfBlackKeys = false, blackNameAtLeftEnd = false;
            };
            const auto readKeys = [](const Image& image) {
                const auto colourAt = [&](int y) {
                    int dark = 0, light = 0;
                    for (int x = 0; x < 18; ++x) {
                        const auto c = image.getPixelAt(x, y);
                        dark += c.getFloatAlpha() > 0.8f && c.getBrightness() < 0.3f;
                        light += c.getFloatAlpha() > 0.8f && c.getBrightness() > 0.9f;
                    }
                    return dark > light ? black : light > dark ? white : none;
                };
                KeysRead read;
                for (int y = 0; y < image.getHeight();) {
                    const int colour = colourAt(y);
                    bool hasName = false;
                    for (; y < image.getHeight() && colourAt(y) == colour; ++y)
                        for (int x = 0; x < 32 && colour != none; ++x) {
                            const float brightness = image.getPixelAt(x, y).getBrightness();
                            if (colour == black) {
                                hasName = hasName || (x < 18 && brightness > 0.4f);
                                read.blackNameAtLeftEnd = read.blackNameAtLeftEnd || (x < 2 && brightness > 0.4f);
                            } else if (x < 18) {
                                read.whiteNameLeftOfBlackKeys = read.whiteNameLeftOfBlackKeys || brightness < 0.6f;
                            } else {
                                hasName = hasName || brightness < 0.6f;
                            }
                        }
                    if (colour != none)
                        ++(hasName ? read.named : read.unnamed)[colour];
                }
                return read;
            };

            int gridPixels = 0;
            for (int y = 0; y < keys.getHeight(); ++y)
                for (int x = 50; x < keys.getWidth(); ++x)
                    gridPixels += keys.getPixelAt(x, y).getFloatAlpha() > 0.0f;
            expect(gridPixels > 0, "the grid stays across the clip");
            const auto read = readKeys(keys);
            expect(read.named[white] > 0 && read.named[black] > 0, "the key's notes are named, F# on its black key");
            expect(read.unnamed[white] > 0 && read.unnamed[black] > 0, "notes out of the key, F and C#, stay unnamed");
            expect(! read.whiteNameLeftOfBlackKeys, "a white key's name sits right of the black keys");
            expect(read.blackNameAtLeftEnd, "a black key's name starts at the key's left end");

            const auto tonicRead = readKeys(tonicKeys);
            expect(tonicRead.unnamed[black] > 0 && tonicRead.unnamed[white] > 0, "the keys are drawn");
            expect(tonicRead.named[black] == 0 && tonicRead.named[white] == 0,
                   "over the keys, a clip too short to name every lane names none");
        }

        beginTest("Lane names stay on the same pixels while the view scrolls by fractions of a pixel");
        {
            auto& engine = *te::Engine::getEngines()[0];
            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto& pt1 = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
            const int a3 = std::find_if(pt1.begin(), pt1.end(), [](auto& e) { return e.second == 57; })->first;
            uZX::PsgData data{{}, {50.0, 1}};
            data.frames.push_back({{uZX::PsgRegType::VolumeA, 12},
                                   {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(a3 & 0xff)},
                                   {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(a3 >> 8)}});
            for (int i = 0; i < 2999; ++i)
                data.frames.push_back({});
            auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                          {{te::TimePosition(), te::TimeDuration::fromSeconds(60.0)}, {}}, "long");
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(1000);
            TimelineGrid grid(view);
            PsgClipComponent component(view, grid, clip);

            // Placed and drawn the way the track body does it, into the view's own pixels
            const auto renderAt = [&](double viewStartSec) {
                view.zoom.setRange({te::TimePosition::fromSeconds(viewStartSec), te::TimeDuration::fromSeconds(10.0)});
                const int x = roundToInt(view.zoom.timeToX(te::TimePosition()));
                component.setBounds(x, 0, roundToInt(view.zoom.timeToX(te::TimePosition::fromSeconds(60.0))) - x, 300);
                Image image(Image::ARGB, 1000, 300, true);
                {
                    Graphics g(image);
                    g.setOrigin(component.getPosition());
                    component.paintNoteLanes(g);
                }
                return image;
            };
            const auto before = renderAt(20.0), after = renderAt(20.0 + 0.0137);  // 1.37 px later
            int differing = 0;
            for (int y = 0; y < 300; ++y)
                for (int x = 0; x < 45; ++x)
                    differing += before.getPixelAt(x, y) != after.getPixelAt(x, y);
            expectEquals(differing, 0, "names pinned to the view's left edge must not shift or blur");
        }

        beginTest("Level of detail follows the lane height");
        auto detail = [](float height) { const auto d = getLaneDetail(height); return std::pair{d.lines, d.labels}; };
        expect(detail(12.0f) == std::pair{LaneDetail::All, LaneDetail::All});
        expect(detail(7.0f) == std::pair{LaneDetail::All, LaneDetail::Tonic});
        expect(detail(3.0f) == std::pair{LaneDetail::Tonic, LaneDetail::Tonic}, "too short for a line per lane");
        expect(detail(0.5f) == std::pair{LaneDetail::Tonic, LaneDetail::None});
        expect(detail(0.1f) == std::pair{LaneDetail::None, LaneDetail::None});
    }
};

static PsgNoteLanesTest psgNoteLanesTest;

//==============================================================================
/** While the view follows playback, a PSG clip blits its content from cached tiles. They must
    draw what painting the view draws, as the view scrolls by whole pixels (and by a fraction,
    zooms or changes mode, which repaint the tiles). */
class PsgClipContentTilesTest : public UnitTest
{
public:
    PsgClipContentTilesTest() : UnitTest("PsgClipContentTiles", "MoTool") {}

    struct CachedClip : PsgClipComponent
    {
        using PsgClipComponent::PsgClipComponent;
        bool shouldCacheContent() const override { return true; }
    };

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        // Tones only: noise fringes and low-envelope arrows near the view's edges are allowed to differ
        uZX::PsgData data{{}, {50.0, 1}};
        for (int i = 0; i < 600; ++i) {
            const int a = 100 + (i * 37) % 900, b = 300 + (i * 11) % 500;
            data.frames.push_back({{uZX::PsgRegType::Mixer, 0x3c},
                                   {uZX::PsgRegType::VolumeA, static_cast<uint8_t>(6 + i % 10)},
                                   {uZX::PsgRegType::VolumeB, static_cast<uint8_t>(15 - i % 7)},
                                   {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(a & 0xff)},
                                   {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(a >> 8)},
                                   {uZX::PsgRegType::TonePeriodFineB, static_cast<uint8_t>(b & 0xff)},
                                   {uZX::PsgRegType::TonePeriodCoarseB, static_cast<uint8_t>(b >> 8)}});
        }
        auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                      {{te::TimePosition::fromSeconds(1.0), te::TimeDuration::fromSeconds(12.0)}, {}},
                                      "tiles");
        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);
        view.zoom.setViewWidthPx(1000);
        TimelineGrid grid(view);
        PsgClipComponent direct(view, grid, clip);
        CachedClip cached(view, grid, clip);
        const bool wereLanes = areNoteLanesShown(engine), wereKeys = arePianoKeysShown(engine);
        setNoteLanesShown(engine, true);
        setPianoKeysShown(engine, true);

        // Placed and drawn the way the track body does it, into the view's own pixels at Retina scale
        const auto render = [&](PsgClipComponent& component) {
            const auto range = clip->getEditTimeRange();
            const int x = roundToInt(view.zoom.timeToX(range.getStart()));
            component.setBounds(x, 0, roundToInt(view.zoom.timeToX(range.getEnd())) - x, 300);
            Image image(Image::ARGB, 2000, 600, true);
            {
                Graphics g(image);
                g.addTransform(AffineTransform::scale(2.0f));
                g.setOrigin(component.getPosition());
                g.reduceClipRegion(component.getLocalBounds());  // as the track body clips it
                component.paint(g);
            }
            return image;
        };
        const auto expectSameAt = [&](double startPx, double timePerPixelSec, const String& what) {
            const auto timePerPixel = te::TimeDuration::fromSeconds(timePerPixelSec);
            view.zoom.setRange({te::TimePosition() + timePerPixel * startPx, timePerPixel * 1000.0});
            const auto expected = render(direct), actual = render(cached);
            int worst = 0, differing = 0;
            Point<int> at;
            for (int y = 0; y < expected.getHeight(); ++y)
                for (int x = 0; x < expected.getWidth(); ++x) {
                    const auto e = expected.getPixelAt(x, y), a = actual.getPixelAt(x, y);
                    const int d = std::max({std::abs(e.getRed() - a.getRed()), std::abs(e.getGreen() - a.getGreen()),
                                            std::abs(e.getBlue() - a.getBlue()), std::abs(e.getAlpha() - a.getAlpha())});
                    differing += d > 2;
                    if (d > worst) {
                        worst = d;
                        at = {x, y};
                    }
                }
            // The tiles hold the content alone, composited over the clip's fill: rounding only. Direct2D
            // antialiases the same fractional note edge a few percent differently in a tile's bitmap than in
            // the view's (50 edge pixels off by up to 17 seen), so there a few edge pixels may differ that much.
           #if JUCE_WINDOWS
            constexpr int tolerance = 24, allowedPixels = 100;
           #else
            constexpr int tolerance = 2, allowedPixels = 0;
           #endif
            const bool same = worst <= 2 || (worst <= tolerance && differing <= allowedPixels);
            expect(same, what + ": " + String(differing) + " device pixels differ, up to " + String(worst)
                                   + " at " + at.toString() + " (clip at " + String(direct.getX() * 2) + "): "
                                   + expected.getPixelAt(at.x, at.y).toDisplayString(true) + " drawn, "
                                   + actual.getPixelAt(at.x, at.y).toDisplayString(true) + " cached");
        };

        beginTest("Tiles draw what the view draws while it scrolls by whole pixels");
        for (const double startPx : {100.0, 113.0, 126.0, 139.0, 400.0, 1500.0, 1513.0})
            expectSameAt(startPx, 0.004, "at " + String(startPx) + " px");

        beginTest("Tiles are repainted when the view moves by a fraction of a pixel or zooms");
        expectSameAt(1513.37, 0.004, "a fraction later");
        expectSameAt(1513.37 * 0.004 / 0.0031, 0.0031, "zoomed in");

        beginTest("Tiles are repainted in the Registers view");
        clip->state.setProperty(IDs::paintMode, 1, nullptr);
        expectSameAt(800.0, 0.0031, "registers");
        expectSameAt(813.0, 0.0031, "registers, scrolled");
        clip->state.setProperty(IDs::paintMode, 0, nullptr);

        beginTest("Lane names without keys: the lines under them, then the grid column stretched");
        setPianoKeysShown(engine, false);
        for (const double startPx : {100.0, 113.0, 900.0, 913.0})
            expectSameAt(startPx, 0.0031, "no keys, at " + String(startPx) + " px");

        setNoteLanesShown(engine, wereLanes);
        setPianoKeysShown(engine, wereKeys);
    }
};

static PsgClipContentTilesTest psgClipContentTilesTest;

//==============================================================================
/** The tuning picker must never keep a clip alive past its Edit: a clip's destructor
    reaches into its Edit, so a picker left open while the Player opened another file
    used to crash when it closed. */
class TuningPickerLifetimeTest : public UnitTest
{
public:
    TuningPickerLifetimeTest() : UnitTest("TuningPickerLifetime", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        beginTest("Deleting the Edit deletes the clip under an open picker");

        auto edit = te::Edit::createSingleTrackEdit(engine);
        uZX::PsgData data{{{{uZX::PsgRegType::VolumeA, 12}, {uZX::PsgRegType::TonePeriodFineA, 200}}}, {50.0, 1}};
        PsgClip* clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                          {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}}, "t").get();
        expect(clip != nullptr);
        if (clip == nullptr)
            return;
        const te::SafeSelectable<PsgClip> weak(clip);

        auto picker = std::make_unique<TuningPickerComponent>(*clip);
        edit.reset();
        expect(weak.get() == nullptr, "the picker must not keep the clip alive past its Edit");
        picker.reset();  // must not touch the deleted clip or Edit
    }
};

static TuningPickerLifetimeTest tuningPickerLifetimeTest;

}  // namespace MoTool
