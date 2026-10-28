#include "PsgClipTestHelpers.h"

#include <algorithm>

namespace MoTool::Tests {

class PsgPaintingBenchmark : public UnitTest
{
public:
    PsgPaintingBenchmark() : UnitTest("PsgPaintingBenchmark", "MoTool") {}

    void runTest() override {
        const auto path = SystemStats::getEnvironmentVariable("UZX_PSG_PAINT_BENCHMARK", {});
        if (path.isEmpty())
            return;
        beginTest("Measure cached paintNotes only; native offscreen bitmap, warm cache, no fixture setup");
        auto& engine = *te::Engine::getEngines()[0];
        struct Display { int width, height, scale; };
        const std::array<Display, 3> displays {{{1800, 720, 1}, {1800, 720, 2}, {2560, 1440, 1}}};
        Array<var> results;
        for (const auto display : displays) {
            for (int visibleFrames : {60, 240, 600}) {
                for (int mode = 0; mode < 3; ++mode) {
                    const bool envelope = mode == 1;
                    const bool tone = mode != 2;
                    for (bool noise : {false, true}) {
                        uZX::PsgData data;
                        data.options = {50.0, 1};
                        for (int i = 0; i < 2048; ++i) {
                            uZX::PsgRegsFrame frame;
                            frame.mask.fill(true);
                            frame.setMixer(0x3f);
                            frame.setNoisePeriod(16);
                            frame.setEnvelopePeriod(60);
                            frame.setEnvelopeShape(8);
                            for (size_t ch = 0; ch < 3; ++ch) {
                                frame.setToneOn(ch, tone || i == 0);
                                frame.setNoiseOn(ch, noise);
                                frame.setVolumeAndEnvMod(ch, 15, envelope);
                                frame.setTonePeriod(ch, static_cast<uint16_t>(320 + ch * 256
                                                                            + static_cast<size_t>(i % 12) * 8));
                            }
                            if (i == 0) {
                                frame.setTonePeriod(0, 8);
                                frame.setTonePeriod(1, 4095);
                            }
                            data.frames.push_back(frame);
                        }
                        auto edit = te::Edit::createSingleTrackEdit(engine);
                        const auto duration = te::TimeDuration::fromSeconds(data.getLengthSeconds());
                        auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                                    {{te::TimePosition(), duration}, {}}, "bench");
                        te::SelectionManager selection(engine);
                        EditViewState view(*edit, selection);
                        view.zoom.setViewWidthPx(display.width);
                        view.zoom.setRange({te::TimePosition::fromSeconds(512.0 / 50.0),
                                            te::TimeDuration::fromSeconds(visibleFrames / 50.0)});
                        TimelineGrid grid(view);
                        PsgClipComponent component(view, grid, clip);
                        const double pixelsPerFrame = double(display.width) / visibleFrames;
                        component.setSize(roundToInt(2048 * pixelsPerFrame), display.height);
                        Image image(Image::RGB, display.width * display.scale, display.height * display.scale, true);
                        Graphics graphics(image);
                        graphics.addTransform(AffineTransform::translation(-float(512 * pixelsPerFrame), 0)
                                                  .scaled(float(display.scale)));
                        std::vector<double> times;
                        for (int repeat = 0; repeat < 56; ++repeat) {
                            graphics.fillAll(Colors::Theme::background);
                            const auto start = Time::getHighResolutionTicks();
                            component.paintNotes(graphics);
                            const auto elapsed = Time::highResolutionTicksToSeconds(
                                Time::getHighResolutionTicks() - start);
                            if (repeat >= 5)
                                times.push_back(elapsed * 1000.0);
                        }
                        std::sort(times.begin(), times.end());
                        auto row = std::make_unique<DynamicObject>();
                        row->setProperty("width", display.width);
                        row->setProperty("height", display.height);
                        row->setProperty("scale", display.scale);
                        row->setProperty("visibleFrames", visibleFrames);
                        row->setProperty("pixelsPerFrame", pixelsPerFrame);
                        row->setProperty("envelope", envelope);
                        row->setProperty("tone", tone);
                        row->setProperty("noise", noise);
                        row->setProperty("medianMs", times[25]);
                        row->setProperty("p95Ms", times[48]);
                        results.add(var(row.release()));
                    }
                }
            }
        }
        auto report = std::make_unique<DynamicObject>();
        report->setProperty("build", JUCE_DEBUG ? "Debug" : "Release");
        report->setProperty("measuredAt", Time::getCurrentTime().toISO8601(true));
        report->setProperty("os", SystemStats::getOperatingSystemName());
        report->setProperty("cpu", SystemStats::getCpuModel());
        report->setProperty("samplesPerCase", 51);
        report->setProperty("method",
                            "paintNotes only; cached clip/range; NativeImageType bitmap; RGB requested; 5 warmups; clear excluded");
        report->setProperty("cases", results);
        const auto reportFile = File::getCurrentWorkingDirectory().getChildFile(path);
        expect(reportFile.replaceWithText(JSON::toString(var(report.release())), false, false, "\n"));
    }
};

static PsgPaintingBenchmark psgPaintingBenchmark;

}  // namespace MoTool::Tests
