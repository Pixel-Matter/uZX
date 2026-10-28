#include "PsgClipTestHelpers.h"

#include <bit>
#include <cmath>
#include <set>

namespace MoTool::Tests {
namespace {

constexpr std::array<uint16_t, 4> pitchPeriods {256, 384, 576, 864};

String channels(int mask) {
    String result;
    for (int ch = 0; ch < 3; ++ch)
        if ((mask & (1 << ch)) != 0)
            result += String::charToString(static_cast<juce_wchar>('A' + ch));
    return result.isEmpty() ? "-" : result;
}

struct GuideSample
{
    String title;
    String relation;
    uZX::PsgRegsFrame frame;
    int width = 192;
    int height = 168;
};

GuideSample makeSample(int tone, int noise, int env, const std::vector<int>& partition, String title = {}) {
    GuideSample sample;
    sample.frame.mask.fill(true);
    sample.frame.setMixer(0x3f);
    sample.frame.setNoisePeriod(16);
    sample.frame.setEnvelopeShape(8);
    String sources = channels(tone);
    if (tone == 0)
        sources.clear();
    if (env != 0)
        sources += "E";
    jassert(static_cast<size_t>(sources.length()) == partition.size());
    size_t source = 0;
    for (int ch = 0; ch < 3; ++ch) {
        const bool hasTone = (tone & (1 << ch)) != 0;
        sample.frame.setToneOn(static_cast<size_t>(ch), hasTone);
        sample.frame.setNoiseOn(static_cast<size_t>(ch), (noise & (1 << ch)) != 0);
        sample.frame.setVolumeAndEnvMod(static_cast<size_t>(ch), 15, (env & (1 << ch)) != 0);
        sample.frame.setTonePeriod(static_cast<size_t>(ch),
                                   hasTone ? pitchPeriods[static_cast<size_t>(partition[source++])] : 256);
    }
    sample.frame.setEnvelopePeriod(env != 0 ? pitchPeriods[static_cast<size_t>(partition[source])] / 16 : 16);

    String key;
    StringArray groups;
    for (size_t i = 0; i < partition.size(); ++i) {
        key += String(partition[i]);
        while (groups.size() <= partition[i])
            groups.add({});
        auto& group = groups.getReference(partition[i]);
        if (group.isNotEmpty())
            group += "=";
        group += sources.substring(static_cast<int>(i), static_cast<int>(i + 1));
    }
    groups.removeEmptyStrings();
    sample.relation = groups.isEmpty() ? "No pitch sources" : groups.joinIntoString(" | ");
    sample.title = title.isNotEmpty() ? title : "T" + String(tone) + " N" + String(noise) + " E" + String(env)
        + " P" + (key.isEmpty() ? "-" : key);
    return sample;
}

String flags(const uZX::PsgRegsFrame& frame) {
    int tone = 0, noise = 0, env = 0;
    for (size_t ch = 0; ch < 3; ++ch) {
        tone |= frame.getToneOn(ch) ? 1 << ch : 0;
        noise |= frame.getNoiseOn(ch) ? 1 << ch : 0;
        env |= frame.getEnvMod(ch) ? 1 << ch : 0;
    }
    return "T:" + channels(tone) + "   N:" + channels(noise) + "   E:" + channels(env);
}

Image renderSample(te::Engine& engine, const GuideSample& sample) {
    // Off-screen reference frame fixes the pitch scale; only the sample frame is painted.
    auto anchor = makeSample(3, 0, 0, {0, 3}).frame;
    uZX::PsgData data {{anchor, sample.frame}, {50.0, 1}};
    return renderPsgClip(engine, std::move(data), sample.width, sample.height, {1, 2});
}

bool hasInk(const Image& image) {
    const Image::BitmapData pixels(image, Image::BitmapData::readOnly);
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (! pixels.getPixelColour(x, y).isTransparent())
                return true;
    return false;
}

}  // namespace

class PsgVisualGuideTest : public UnitTest
{
public:
    PsgVisualGuideTest() : UnitTest("PsgVisualGuide", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        const auto destination = SystemStats::getEnvironmentVariable("UZX_PSG_GUIDE_OUTPUT", {});
        const auto sheetFilter = SystemStats::getEnvironmentVariable("UZX_PSG_GUIDE_SHEET", {});
        const bool exporting = destination.isNotEmpty();
        const bool fullExport = exporting && sheetFilter.isEmpty();
        int writtenSheets = 0;
        const auto output = exporting ? File::getCurrentWorkingDirectory().getChildFile(destination) : File();
        if (exporting) {
            beginTest("Guide output directories are writable");
            expect(output.createDirectory().wasOk(), "Cannot create guide output directory");
            if (fullExport)
                expect(output.getChildFile("atlas").createDirectory().wasOk(), "Cannot create atlas directory");
        }

        auto writeSheet = [&](const std::vector<GuideSample>& samples, const String& path,
                              const String& title, int columns) {
            if (! exporting || samples.empty() || (sheetFilter.isNotEmpty() && sheetFilter != path))
                return;
            ++writtenSheets;
            constexpr int tileWidth = 224, tileHeight = 240, headingHeight = 64;
            const int rows = (static_cast<int>(samples.size()) + columns - 1) / columns;
            Image sheet(Image::RGB, columns * tileWidth, headingHeight + rows * tileHeight, true);
            Graphics g(sheet);
            g.fillAll(Colors::Theme::background);
            g.setColour(Colors::Theme::textPrimary);
            g.setFont(16.0f);
            g.drawFittedText(title, 12, 6, sheet.getWidth() - 24, 38, Justification::topLeft, 2);
            g.setFont(11.0f);
            g.setColour(Colors::Theme::textSecondary);
            g.drawText("Actual renderer | T: tone, N: noise, E: envelope users",
                       12, 44, sheet.getWidth() - 24, 16, Justification::left);
            for (size_t i = 0; i < samples.size(); ++i) {
                const auto& sample = samples[i];
                const int x = static_cast<int>(i) % columns * tileWidth;
                const int y = headingHeight + static_cast<int>(i) / columns * tileHeight;
                g.setColour(Colors::Theme::textPrimary);
                g.setFont(13.0f);
                g.drawFittedText(sample.title, x + 12, y + 4, 200, 20, Justification::left, 1);
                g.setColour(Colors::Theme::textSecondary);
                g.setFont(11.0f);
                g.drawText(flags(sample.frame), x + 12, y + 25, 200, 14, Justification::left);
                g.drawText(sample.relation, x + 12, y + 40, 200, 14, Justification::left);
                const Rectangle<int> plot(x + 16, y + 60, 192, 168);
                g.setColour(Colors::Theme::backgroundAlt);
                g.fillRect(plot);
                const auto image = renderSample(engine, sample);
                g.drawImageAt(image, plot.getCentreX() - image.getWidth() / 2,
                              plot.getCentreY() - image.getHeight() / 2);
            }
            MemoryOutputStream encoded;
            expect(PNGImageFormat().writeImageToStream(sheet, encoded), "PNG encoding failed: " + path);
            expect(output.getChildFile(path).replaceWithData(encoded.getData(), encoded.getDataSize()),
                   "PNG write failed: " + path);
        };

        beginTest("All channel flags and canonical pitch-equality partitions are covered");
        int caseCount = 0, page = 0;
        std::set<String> identifiers;
        std::vector<GuideSample> pageSamples;
        Array<var> records;
        String atlasIndex = "# Complete PSG visualization atlas\n\n"
            "Current renderer; 512 flag combinations and 2,192 pitch-equivalence states.\n\n"
            "Channel masks: 0 none, 1 A, 2 B, 3 AB, 4 C, 5 AC, 6 BC, 7 ABC.\n\n"
            "P digits assign equal pitches to the enabled tone sources (A, B, C) followed by shared E.\n"
            "For example, T7 N0 E7 P0010 means A=B=E, with C at a different pitch.\n\n"
            "Full flags, periods, expected pitch glyphs, and page/cell locations "
            "are in [states.json](../states.json).\n\n";
        auto flushPage = [&] {
            if (pageSamples.empty())
                return;
            const auto filename = "page-" + String(++page).paddedLeft('0', 2) + ".png";
            writeSheet(pageSamples, "atlas/" + filename, "All states | page " + String(page), 8);
            atlasIndex += "- [Page " + String(page) + "](" + filename + "): "
                + pageSamples.front().title + " through " + pageSamples.back().title + "\n";
            pageSamples.clear();
        };

        for (int tone = 0; tone < 8; ++tone) {
            for (int noise = 0; noise < 8; ++noise) {
                for (int env = 0; env < 8; ++env) {
                    const int sourceCount = std::popcount(static_cast<unsigned>(tone)) + (env != 0);
                    std::vector<int> partition(static_cast<size_t>(sourceCount));
                    auto enumerate = [&](auto&& self, int position, int largestGroup) -> void {
                        if (position < sourceCount) {
                            for (int group = 0; group <= largestGroup + 1; ++group) {
                                partition[static_cast<size_t>(position)] = group;
                                self(self, position + 1, jmax(largestGroup, group));
                            }
                            return;
                        }
                        const auto sample = makeSample(tone, noise, env, partition);
                        expect(identifiers.insert(sample.title).second, "Duplicate state: " + sample.title);
                        const PsgParamFrameData data(sample.frame);
                        std::vector<PsgFrameNote> notes;
                        visitFrameNotes(data, [&](const auto& note) { notes.push_back(note); });
                        expectEquals(static_cast<int>(notes.size()), sourceCount, sample.title);
                        for (size_t i = 0; i < notes.size(); ++i) {
                            expectEquals(static_cast<int>(notes[i].isEnvelopePitch ? notes[i].channelMask & env
                                                                               : notes[i].channelMask & tone),
                                         static_cast<int>(notes[i].channelMask));
                            if (notes[i].isEnvelopePitch)
                                expectEquals(static_cast<int>(notes[i].channelMask), env);
                            for (size_t j = 0; j < i; ++j)
                                expect((std::abs(notes[i].pitch - notes[j].pitch) < 1.0e-6f)
                                           == (partition[i] == partition[j]),
                                       "Pitch-equality mismatch: " + sample.title);
                        }
                        ++caseCount;
                        if (! fullExport)
                            return;
                        const auto image = renderSample(engine, sample);
                        expect(hasInk(image) == (sourceCount != 0 || noise != 0),
                               "Visible sample disagrees with flags: " + sample.title);
                        auto record = std::make_unique<DynamicObject>();
                        record->setProperty("id", sample.title);
                        record->setProperty("toneMask", tone);
                        record->setProperty("noiseMask", noise);
                        record->setProperty("envelopeMask", env);
                        record->setProperty("pitchGroups", sample.relation);
                        record->setProperty("page", page + 1);
                        record->setProperty("cell", static_cast<int>(pageSamples.size()) + 1);
                        Array<var> tonePeriods;
                        for (size_t ch = 0; ch < 3; ++ch)
                            tonePeriods.add(sample.frame.getTonePeriod(ch));
                        record->setProperty("tonePeriods", tonePeriods);
                        record->setProperty("envelopePeriod", sample.frame.getEnvelopePeriod());
                        Array<var> glyphs;
                        for (const auto& note : notes) {
                            auto glyph = std::make_unique<DynamicObject>();
                            glyph->setProperty("kind", note.isEnvelopePitch ? "envelope" : "tone");
                            glyph->setProperty("channelMask", note.channelMask);
                            glyph->setProperty("normalizedPitch", note.pitch);
                            glyphs.add(var(glyph.release()));
                        }
                        record->setProperty("glyphs", glyphs);
                        records.add(var(record.release()));
                        pageSamples.push_back(sample);
                        if (pageSamples.size() == 64)
                            flushPage();
                    };
                    enumerate(enumerate, 0, -1);
                }
            }
        }
        flushPage();
        expectEquals(caseCount, 2192);
        expectEquals(static_cast<int>(identifiers.size()), 2192);

        beginTest("Reference images describe flags, shared ownership, coincidence, shapes, and LOD");
        std::vector<GuideSample> samples;
        for (int bits = 0; bits < 8; ++bits) {
            const bool tone = (bits & 1) != 0, noise = (bits & 2) != 0, env = (bits & 4) != 0;
            std::vector<int> different;
            if (tone)
                different.push_back(0);
            if (env)
                different.push_back(1);
            const auto title = "B: T" + String(int(tone)) + " N" + String(int(noise)) + " E" + String(int(env));
            samples.push_back(makeSample(tone ? 2 : 0, noise ? 2 : 0, env ? 2 : 0, different, title));
            if (tone && env)
                samples.push_back(makeSample(2, noise ? 2 : 0, 2, {1, 1}, "B: tone = envelope"));
        }
        expectEquals(static_cast<int>(samples.size()), 10);
        writeSheet(samples, "flags.png", "Single channel | all flags and tone/envelope pitch matches", 2);

        samples.clear();
        for (int mask = 1; mask < 8; ++mask) {
            samples.push_back(makeSample(0, 0, mask, {0}, "Envelope: " + channels(mask)));
            samples.push_back(makeSample(0, mask, 0, {}, "Noise: " + channels(mask)));
            samples.push_back(makeSample(mask, mask, 0,
                                         std::vector<int>(static_cast<size_t>(std::popcount(unsigned(mask))), 0),
                                         "Tone + noise: " + channels(mask)));
        }
        writeSheet(samples, "ownership.png", "Shared envelope, noise only, and noisy tones | all channel groups", 3);

        samples.clear();
        std::vector<int> allFour(4);
        auto polyphony = [&](auto&& self, int pos, int largest) -> void {
            if (pos == 4) {
                samples.push_back(makeSample(7, 0, 7, allFour));
                return;
            }
            for (int group = 0; group <= largest + 1; ++group) {
                allFour[static_cast<size_t>(pos)] = group;
                self(self, pos + 1, jmax(largest, group));
            }
        };
        polyphony(polyphony, 0, -1);
        expectEquals(static_cast<int>(samples.size()), 15);
        writeSheet(samples, "pitch-relations.png", "A, B, C, shared E | all 15 pitch-equality groupings", 3);

        samples.clear();
        for (const bool samePitch : {false, true}) {
            for (int mode = 0; mode < 4; ++mode) {
                auto sample = makeSample(2, 2, 2, samePitch ? std::vector<int>{0, 0} : std::vector<int>{0, 1});
                sample.width = mode == 1 ? 5 : mode == 2 ? 6 : 192;
                sample.height = mode == 3 ? 24 : 168;
                sample.title = String(samePitch ? "Same" : "Different") + " pitch | " + String(sample.width)
                    + " x " + String(sample.height);
                samples.push_back(sample);
            }
        }
        writeSheet(samples, "lod.png", "Level of detail | native-size samples, shared pitch scale", 4);

        samples.clear();
        for (int shape = 0; shape < 16; ++shape) {
            auto sample = makeSample(2, 0, 2, {0, 1}, "Envelope shape " + String::toHexString(shape).toUpperCase());
            sample.frame.setEnvelopeShape(static_cast<uint8_t>(shape));
            samples.push_back(sample);
        }
        writeSheet(samples, "shapes.png", "Envelope shapes | existing hatch directions and zigzags", 4);

        samples.clear();
        for (int volume : {0, 1, 8, 15}) {
            auto sample = makeSample(2, 2, 0, {0}, "Fixed volume " + String(volume));
            sample.frame.setVolume(1, static_cast<uint8_t>(volume));
            samples.push_back(sample);
        }
        auto envZero = makeSample(2, 2, 2, {0, 1}, "Env on, fixed volume 0");
        envZero.frame.setVolume(1, 0);
        samples.push_back(envZero);
        for (int period : {0, 1, 255, 256}) {
            auto sample = makeSample(0, 0, 2, {0}, "Envelope period " + String(period));
            sample.frame.setEnvelopePeriod(static_cast<uint16_t>(period));
            sample.relation = period == 0 || period == 256 ? "E outside display window" : "E within display window";
            samples.push_back(sample);
        }
        for (int period : {7, 8}) {
            auto sample = makeSample(2, 0, 0, {0}, "Tone period " + String(period));
            sample.frame.setTonePeriod(1, static_cast<uint16_t>(period));
            samples.push_back(sample);
        }
        for (int period : {0, 1, 8, 16, 31}) {
            auto sample = makeSample(0, 2, 0, {}, "Noise period " + String(period));
            sample.frame.setNoisePeriod(static_cast<uint8_t>(period));
            samples.push_back(sample);
        }
        writeSheet(samples, "limits.png", "Volume and display limits | flags do not always imply a visible mark", 4);

        samples.clear();
        for (int mask = 1; mask < 8; ++mask) {
            for (int mode = 0; mode < 3; ++mode) {
                const int tones = mode == 1 ? mask : 0;
                const auto sourceCount = std::popcount(static_cast<unsigned>(tones)) + 1;
                auto sample = makeSample(tones, mode == 0 ? 0 : mask, mask,
                                         std::vector<int>(static_cast<size_t>(sourceCount), 0),
                                         String(mode == 0 ? "Low E: " : mode == 1 ? "Tone + low E: " : "Noise + low E: ")
                                             + channels(mask));
                sample.frame.setEnvelopePeriod(256);
                sample.relation = "E below display window";
                samples.push_back(sample);
            }
        }
        writeSheet(samples, "low-envelope.png", "Below-range envelope | arrows preserve state and channel ownership", 3);

        samples.clear();
        samples.push_back(makeSample(1, 1, 0, {0}, "Noise around A tone"));
        samples.push_back(makeSample(2, 2, 2, {0, 1}, "B tone + E: noise on tone only"));
        for (int mask = 1; mask < 8; ++mask)
            samples.push_back(makeSample(0, mask, 0, {}, "Noise only: " + channels(mask)));
        writeSheet(samples, "noise-detail.png", "Noise detail | 0.75 note height, whole rectangles, channel ownership", 3);

        if (exporting && ! fullExport)
            expectEquals(writtenSheets, 1, "Unknown guide sheet: " + sheetFilter);

        if (fullExport) {
            const uZX::PsgData legendData {{makeSample(0, 0, 0, {}).frame}, {50.0, 1}};
            Image legend(Image::RGB, 192, 24, true);
            Graphics legendGraphics(legend);
            legendGraphics.fillAll(Colors::Theme::background);
            legendGraphics.drawImageAt(renderPsgClip(engine, legendData, 192, 24, {}, true), 0, 0);
            MemoryOutputStream encodedLegend;
            expect(PNGImageFormat().writeImageToStream(legend, encodedLegend));
            expect(output.getChildFile("legend.png").replaceWithData(encodedLegend.getData(),
                                                                     encodedLegend.getDataSize()));
            auto manifest = std::make_unique<DynamicObject>();
            manifest->setProperty("description",
                                  "Actual PSG renderer; flags and pitch equivalence, not an audio simulation");
            manifest->setProperty("flagCombinations", 512);
            manifest->setProperty("caseCount", caseCount);
            manifest->setProperty("pageCount", page);
            const auto sourceRoot = File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory();
            auto hashes = std::make_unique<DynamicObject>();
            for (const auto* path : {"gui/timeline/PsgClipComponent.cpp", "gui/common/LookAndFeel.h",
                                     "models/PsgParameter.h", "gui/timeline/PsgClipTestHelpers.h",
                                     "gui/timeline/PsgVisualGuide.test.cpp"}) {
                const auto source = sourceRoot.getChildFile(path);
                expect(source.existsAsFile(), "Cannot fingerprint source: " + source.getFullPathName());
                hashes->setProperty(path, String::toHexString(source.loadFileAsString().hashCode64()));
            }
            manifest->setProperty("sourceHashes", var(hashes.release()));
            manifest->setProperty("cases", records);
            expect(output.getChildFile("states.json").replaceWithText(
                JSON::toString(var(manifest.release())), false, false, "\n"));
            expect(output.getChildFile("atlas/README.md").replaceWithText(atlasIndex, false, false, "\n"));
            logMessage("PSG visual guide written to " + output.getFullPathName());
        }
    }
};

static PsgVisualGuideTest psgVisualGuideTest;

}  // namespace MoTool::Tests
