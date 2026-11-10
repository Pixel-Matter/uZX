#include "VideoExport.h"

#include <JuceHeader.h>

namespace MoTool::Video {

class VideoExportTest : public juce::UnitTest
{
public:
    VideoExportTest() : UnitTest("VideoExport", "MoTool") {}

    void runTest() override {
        const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory);

        beginTest("Each render height scales by whole numbers to the standard outputs, and no others");
        {
            expect(Settings::scalesFor(720) == juce::Array<int>{1, 2, 3, 6});
            expect(Settings::scalesFor(1080) == juce::Array<int>{1, 2, 4});
            expect(Settings::scalesFor(1440) == juce::Array<int>{1, 3});
            expect(Settings::scalesFor(2160) == juce::Array<int>{1, 2});
            expectEquals(Settings::nearestScale(1080, 2160), 2, "an output this height offers");
            expectEquals(Settings::nearestScale(1080, 3240), 2, "a tie takes the smaller");
            expectEquals(Settings::nearestScale(1440, 2160), 1, "else the nearest");
        }

        beginTest("Every frame rate takes a whole number of samples: a block of sound per frame");
        for (auto fps : Settings::frameRates)
            expectEquals(Settings::sampleRate % fps, 0, juce::String(fps) + " fps");

        beginTest("Frames become BT.709 at the render size, then scale up with square pixels");
        {
            Settings s;
            s.renderHeight = 720;
            s.scale = 3;
            const auto args = encodeArgs(s, temp.getChildFile("v.mp4")).joinIntoString(" ");
            expect(args.contains("-pix_fmt bgra -s 1280x720 -framerate 60 -i pipe:0"), args);
            expect(args.contains("-vf "
                                 "scale=out_color_matrix=bt709:out_range=tv,format=yuv444p,scale=iw*3:ih*3:flags="
                                 "neighbor,format=yuv420p"),
                   args);
            expect(args.contains("setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709"), args);

            s.scale = 1;
            expect(!encodeArgs(s, temp.getChildFile("v.mp4")).joinIntoString(" ").contains("neighbor"));
        }

        beginTest("HEVC is tagged for Apple players; the sound is encoded as chosen");
        {
            Settings s;
            s.codec = Settings::Codec::hevc;
            s.audio = Settings::Audio::alac;
            const auto dest = temp.getChildFile("out.mp4");
            auto args = muxArgs(s, temp.getChildFile("v.mp4"), temp.getChildFile("a.wav"), dest);
            expect(args.joinIntoString(" ").contains("-c:v copy -tag:v hvc1 -c:a alac -movflags"));
            expectEquals(args[args.size() - 1], dest.getFullPathName());

            s.codec = Settings::Codec::h264;
            s.audio = Settings::Audio::aac;
            args = muxArgs(s, temp.getChildFile("v.mp4"), temp.getChildFile("a.wav"), dest);
            expect(!args.contains("hvc1"));
            expect(args.joinIntoString(" ").contains("-c:a aac -b:a 384k"));
        }

        beginTest("Encoders are read from ffmpeg's list, not from its legend");
        {
            const auto names = parseEncoders("Encoders:\n"
                                             " V..... = Video\n"
                                             " A..... = Audio\n"
                                             " ------\n"
                                             " V....D libx264              libx264 H.264 / AVC\n"
                                             " A....D aac                  AAC (Advanced Audio Coding)\n");
            expect(names == juce::StringArray{"libx264", "aac"}, names.joinIntoString(", "));
        }

        beginTest("Settings come back as saved; values no longer offered keep their defaults");
        {
            juce::TemporaryFile file{".settings"};
            juce::PropertiesFile props(file.getFile(), {});

            Settings saved;
            saved.renderHeight = 1080;
            saved.scale = 4;
            saved.fps = 50;
            saved.codec = Settings::Codec::av1;
            saved.quality = Settings::Quality::high;
            saved.audio = Settings::Audio::alac;
            saved.zoom = Settings::Zoom::beats;
            saved.ffmpeg = temp.getChildFile("ffmpeg");
            saved.save(props);

            Settings loaded;
            loaded.load(props);
            expectEquals(loaded.renderHeight, 1080);
            expectEquals(loaded.scale, 4);
            expectEquals(loaded.fps, 50);
            expect(loaded.codec == Settings::Codec::av1);
            expect(loaded.quality == Settings::Quality::high);
            expect(loaded.audio == Settings::Audio::alac);
            expect(loaded.zoom == Settings::Zoom::beats);
            expect(loaded.ffmpeg == saved.ffmpeg);

            props.setValue("renderVideo.fps", 59);
            props.setValue("renderVideo.codec", 7);
            props.setValue("renderVideo.scale", 3);  // not a scale of 1080
            Settings fallback;
            fallback.load(props);
            expectEquals(fallback.fps, Settings().fps);
            expect(fallback.codec == Settings().codec);
            expectEquals(fallback.scale, 2, "the scale nearest the saved output");
        }
    }
};

static VideoExportTest videoExportTest;

}  // namespace MoTool::Video
