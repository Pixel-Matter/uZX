#include "VideoExport.h"

#include <algorithm>

namespace MoTool::Video {

namespace {
juce::String key(const char* name) {
    return juce::String("renderVideo.") + name;
}

template <typename Enum>
Enum loadEnum(juce::PropertiesFile& p, const char* name, Enum fallback, Enum last) {
    const int value = p.getIntValue(key(name), (int) fallback);
    return value >= 0 && value <= (int) last ? (Enum) value : fallback;
}

juce::File loadFile(juce::PropertiesFile& p, const char* name) {
    const auto path = p.getValue(key(name));
    return juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File();
}

template <typename Values>
bool isOneOf(const Values& values, int value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

juce::String runForOutput(const juce::File& program, const juce::String& arg) {
    juce::ChildProcess process;
    if (!process.start(juce::StringArray{program.getFullPathName(), arg}, juce::ChildProcess::wantStdOut))
        return {};
    const auto output = process.readAllProcessOutput();
    return process.getExitCode() == 0 ? output : juce::String();
}
}  // namespace

//==============================================================================
juce::Array<int> Settings::scalesFor(int renderHeight) {
    constexpr std::array outputHeights{720, 1080, 1440, 2160, 4320};
    juce::Array<int> scales;
    for (auto height : outputHeights)
        if (renderHeight > 0 && height % renderHeight == 0)
            scales.add(height / renderHeight);
    return scales;
}

int Settings::nearestScale(int renderHeight, int outputHeight) {
    int best = 1;
    for (auto s : scalesFor(renderHeight))
        if (std::abs(renderHeight * s - outputHeight) < std::abs(renderHeight * best - outputHeight))
            best = s;
    return best;
}

void Settings::load(juce::PropertiesFile& p) {
    if (const int h = p.getIntValue(key("renderHeight")); isOneOf(renderHeights, h))
        renderHeight = h;
    scale = nearestScale(renderHeight, renderHeight * p.getIntValue(key("scale"), scale));
    if (const int f = p.getIntValue(key("fps")); isOneOf(frameRates, f))
        fps = f;
    codec = loadEnum(p, "codec", codec, Codec::av1);
    quality = loadEnum(p, "quality", quality, Quality::nearLossless);
    audio = loadEnum(p, "audio", audio, Audio::alac);
    zoom = loadEnum(p, "zoom", zoom, Zoom::bars);
    ffmpeg = loadFile(p, "ffmpeg");
    folder = loadFile(p, "folder");
}

void Settings::save(juce::PropertiesFile& p) const {
    p.setValue(key("renderHeight"), renderHeight);
    p.setValue(key("scale"), scale);
    p.setValue(key("fps"), fps);
    p.setValue(key("codec"), (int) codec);
    p.setValue(key("quality"), (int) quality);
    p.setValue(key("audio"), (int) audio);
    p.setValue(key("zoom"), (int) zoom);
    p.setValue(key("ffmpeg"), ffmpeg.getFullPathName());
    p.setValue(key("folder"), folder.getFullPathName());
}

//==============================================================================
juce::String encoderFor(Settings::Codec codec) {
    switch (codec) {
        case Settings::Codec::h264:
            return "libx264";
        case Settings::Codec::hevc:
            return "libx265";
        case Settings::Codec::av1:
            return "libsvtav1";
    }
    jassertfalse;
    return {};
}

juce::String encoderFor(Settings::Audio audio) {
    switch (audio) {
        case Settings::Audio::aac:
            return "aac";
        case Settings::Audio::opus:
            return "libopus";
        case Settings::Audio::alac:
            return "alac";
    }
    jassertfalse;
    return {};
}

juce::StringArray encodeArgs(const Settings& s, const juce::File& video) {
    // BT.709, which HD video is tagged as and players assume: ffmpeg's default BT.601 would shift the colours.
    // Converted at the render size, a ninth of the pixels of a 3x output; then nearest neighbour keeps the
    // drawn pixels square and sharp. The tags go on the frames, as encoders take them from there.
    juce::String filter = "scale=out_color_matrix=bt709:out_range=tv,format=yuv444p,";
    if (s.scale > 1)
        filter << "scale=iw*" << s.scale << ":ih*" << s.scale << ":flags=neighbor,";
    filter << "format=yuv420p,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=tv";

    juce::StringArray args{"-hide_banner",
                           "-loglevel",
                           "error",
                           "-y",
                           "-f",
                           "rawvideo",
                           "-pix_fmt",
                           "bgra",
                           "-s",
                           juce::String(Settings::widthFor(s.renderHeight)) + "x" + juce::String(s.renderHeight),
                           "-framerate",
                           juce::String(s.fps),
                           "-i",
                           "pipe:0",
                           "-vf",
                           filter,
                           "-c:v",
                           encoderFor(s.codec)};

    // Constant quality rather than a bitrate: still frames take few bits, busy ones what they need
    const auto q = (size_t) s.quality;
    switch (s.codec) {
        case Settings::Codec::h264:
            args.addArray(juce::StringArray{
                "-preset", "medium", "-tune", "animation", "-crf", juce::String(std::array{20, 16, 10}[q])});
            break;
        case Settings::Codec::hevc:
            args.addArray(juce::StringArray{"-preset",
                                            "medium",
                                            "-x265-params",
                                            "log-level=error",
                                            "-crf",
                                            juce::String(std::array{24, 20, 14}[q])});
            break;
        case Settings::Codec::av1:
            args.addArray(juce::StringArray{"-preset", "8", "-crf", juce::String(std::array{34, 28, 20}[q])});
            break;
    }

    args.addArray(juce::StringArray{"-an", video.getFullPathName()});
    return args;
}

juce::StringArray muxArgs(const Settings& s, const juce::File& video, const juce::File& wav, const juce::File& dest) {
    juce::StringArray args{"-hide_banner",
                           "-loglevel",
                           "error",
                           "-nostdin",
                           "-y",
                           "-i",
                           video.getFullPathName(),
                           "-i",
                           wav.getFullPathName(),
                           "-map",
                           "0:v:0",
                           "-map",
                           "1:a:0",
                           "-c:v",
                           "copy"};
    if (s.codec == Settings::Codec::hevc)
        args.addArray(juce::StringArray{"-tag:v", "hvc1"});  // QuickTime and Apple devices play HEVC only tagged so

    args.addArray(juce::StringArray{"-c:a", encoderFor(s.audio)});
    if (s.audio == Settings::Audio::aac)
        args.addArray(juce::StringArray{"-b:a", "384k"});  // YouTube's recommendation for stereo
    else if (s.audio == Settings::Audio::opus)
        args.addArray(juce::StringArray{"-b:a", "256k"});

    args.addArray(juce::StringArray{"-movflags", "+faststart", dest.getFullPathName()});
    return args;
}

juce::StringArray parseEncoders(const juce::String& output) {
    juce::StringArray names;
    for (const auto& line : juce::StringArray::fromLines(output)) {
        // " V....D libx264   libx264 H.264 ...": six flags, then the name; the legend has "=" there
        const auto tokens = juce::StringArray::fromTokens(line.trim(), false);
        if (tokens.size() >= 2 && tokens[0].length() == 6 && tokens[0].containsOnly("VASFXBD.") && tokens[1] != "=")
            names.add(tokens[1]);
    }
    return names;
}

//==============================================================================
juce::Optional<Ffmpeg> probeFfmpeg(const juce::File& program) {
    if (!program.existsAsFile())
        return juce::nullopt;

    const auto versionOutput = runForOutput(program, "-version");
    constexpr auto prefix = "ffmpeg version ";
    if (!versionOutput.startsWith(prefix))
        return juce::nullopt;

    return Ffmpeg{program,
                  versionOutput.fromFirstOccurrenceOf(prefix, false, false).upToFirstOccurrenceOf(" ", false, false),
                  parseEncoders(runForOutput(program, "-encoders"))};
}

juce::Optional<Ffmpeg> findFfmpeg(const juce::File& chosen) {
#if JUCE_WINDOWS
    const juce::String name = "ffmpeg.exe", pathSeparator = ";";
#else
    const juce::String name = "ffmpeg", pathSeparator = ":";
#endif

    juce::Array<juce::File> candidates;
    if (chosen != juce::File())
        candidates.add(chosen);
    for (const auto& dir :
         juce::StringArray::fromTokens(juce::SystemStats::getEnvironmentVariable("PATH", {}), pathSeparator, {}))
        if (juce::File::isAbsolutePath(dir))
            candidates.add(juce::File(dir).getChildFile(name));
#if !JUCE_WINDOWS
    for (auto dir : {"/opt/homebrew/bin", "/usr/local/bin", "/usr/bin"})
        candidates.add(juce::File(dir).getChildFile(name));
#endif

    for (const auto& candidate : candidates)
        if (auto found = probeFfmpeg(candidate))
            return found;
    return juce::nullopt;
}

}  // namespace MoTool::Video
