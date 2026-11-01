#include <JuceHeader.h>

#include <algorithm>

#include "PsgClipTestHelpers.h"
#include "TimelineGrid.h"
#include "TuningPickerComponent.h"
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
            return Tests::renderPsgClip(engine, std::move(data), width, height, {}, header);
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

        beginTest("Noise fringe stops before a neighboring hollow envelope note");
        auto neighbors = withTones(1, 0, 2);
        neighbors.frames[0].setTonePeriod(0, 512);
        neighbors.frames[0].setEnvelopePeriod(34);
        const auto plainNeighbors = renderData(neighbors);
        neighbors.frames[0].setNoiseOn(0, true);
        const auto noisyNeighbors = renderData(neighbors);
        int envelopeRows = 0;
        for (int y = 0; y < plainNeighbors.getHeight(); ++y) {
            if (plainNeighbors.getPixelAt(0, y) != Colors::PSG::B)
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
            if (noNoiseDual.getPixelAt(0, y) == Colors::PSG::B) {
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

        beginTest("Envelope-only channel B retains a green outline around envelope-colored hatching");
        const uZX::PsgData envelopeOnlyData {
            {{{uZX::PsgRegType::Mixer, 0x3f}, {uZX::PsgRegType::VolumeB, 16},
              {uZX::PsgRegType::EnvelopePeriodFine, 100}, {uZX::PsgRegType::EnvelopeShape, 8}}},
            {50.0, 1}
        };
        const auto envelopeOnlyImage = renderData(envelopeOnlyData);
        bool hasOutline = false, hasHatching = false;
        for (int y = 0; y < envelopeOnlyImage.getHeight(); ++y) {
            const auto edge = envelopeOnlyImage.getPixelAt(0, y);
            if (! edge.isTransparent()) {
                expect(edge == Colors::PSG::B, "The envelope note outline identifies channel B");
                hasOutline = true;
            }
            for (int x = 1; x < envelopeOnlyImage.getWidth() - 1; ++x)
                hasHatching = hasHatching || envelopeOnlyImage.getPixelAt(x, y) == Colors::PSG::Env;
        }
        expect(hasOutline, "Tone-off must not hide envelope channel ownership");
        expect(hasHatching, "Envelope hatching keeps the envelope color");

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
        beginTest("N legend uses the band color with vertical fading and bottom padding");
        const auto noiseCenter = header.getPixelAt(55, 9);
        expect(noiseCenter.getFloatAlpha() > 0.7f);
        expect(header.getPixelAt(55, 3).getAlpha() < noiseCenter.getAlpha());
        expect(header.getPixelAt(55, 14).getAlpha() < noiseCenter.getAlpha());
        expect(header.getPixelAt(55, 15).isTransparent());
        expect(std::abs(int(noiseCenter.getRed()) - int(Colors::PSG::Mix.getRed())) <= 2);
        expect(std::abs(int(noiseCenter.getGreen()) - int(Colors::PSG::Mix.getGreen())) <= 2);
        expect(std::abs(int(noiseCenter.getBlue()) - int(Colors::PSG::Mix.getBlue())) <= 2);
        expect(header.getPixelAt(68, 8).isTransparent(), "Padding follows N before the clip name");

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

            // Lane lines are faint (alpha <= 0.16, 0.23 over the tonic band); names are
            // drawn at 0.45 or more, so a brighter pixel near the left edge is a name.
            bool nameNearLeftEdge = false;
            for (int y = 0; y < image.getHeight() && ! nameNearLeftEdge; ++y)
                for (int x = 0; x < 45 && ! nameNearLeftEdge; ++x)
                    nameNearLeftEdge = image.getPixelAt(x, y).getFloatAlpha() > 0.35f;
            expect(nameNearLeftEdge, "note names must be drawn at the clip's own left edge");
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
        expect(detail(5.0f) == std::pair{LaneDetail::All, LaneDetail::Tonic});
        expect(detail(0.5f) == std::pair{LaneDetail::Tonic, LaneDetail::None});
        expect(detail(0.1f) == std::pair{LaneDetail::None, LaneDetail::None});
    }
};

static PsgNoteLanesTest psgNoteLanesTest;

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
