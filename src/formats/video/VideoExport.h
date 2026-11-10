#pragma once

#include <JuceHeader.h>

#include <array>

namespace MoTool::Video {

//==============================================================================
/**
    What a video is rendered as. Frames are drawn at the render size, a 16:9 picture renderHeight tall
    laid out like the app at that size, and ffmpeg scales them up by a whole number with nearest
    neighbour, so every drawn pixel becomes a sharp square block.
*/
struct Settings
{
    enum class Codec
    {
        h264,
        hevc,
        av1
    };
    enum class Quality
    {
        high,
        veryHigh,
        nearLossless
    };
    enum class Audio
    {
        aac,
        opus,
        alac
    };
    /** The timeline zoom of the frames: the view's own, or a View > Zoom level, in that menu's order. */
    enum class Zoom
    {
        asOnScreen,
        frames,
        rows,
        beats,
        bars
    };

    int renderHeight = 720;
    int scale = 3;
    int fps = 60;
    Codec codec = Codec::h264;
    Quality quality = Quality::veryHigh;
    Audio audio = Audio::aac;
    Zoom zoom = Zoom::asOnScreen;
    juce::File ffmpeg;  // as chosen by the user; empty: looked up
    juce::File folder;  // where the last video went

    static constexpr int sampleRate = 48000;  // every frame rate below takes a whole number of samples
    static constexpr int tailSeconds = 3;     // after the tune, for the last notes to ring out
    static constexpr std::array renderHeights{720, 1080, 1440, 2160};
    static constexpr std::array frameRates{60, 50, 48, 30, 25, 24};

    static int widthFor(int height) { return height * 16 / 9; }
    /** The whole scales of a render height that give a standard output: 720p, 1080p, 1440p, 4K or 8K. */
    static juce::Array<int> scalesFor(int renderHeight);
    /** The scale of renderHeight whose output is nearest to outputHeight; the smaller on a tie. */
    static int nearestScale(int renderHeight, int outputHeight);

    int samplesPerFrame() const { return sampleRate / fps; }
    int outputHeight() const { return renderHeight * scale; }

    /** Anything missing or no longer offered keeps its default. */
    void load(juce::PropertiesFile&);
    void save(juce::PropertiesFile&) const;
};

/** The ffmpeg encoder each choice needs. */
juce::String encoderFor(Settings::Codec);
juce::String encoderFor(Settings::Audio);

/** ffmpeg arguments, after the program: encode raw BGRA frames of the render size, read from standard
    input, into a video-only file at the output size. */
juce::StringArray encodeArgs(const Settings&, const juce::File& video);

/** ffmpeg arguments, after the program: the encoded video and the sound in one file. */
juce::StringArray muxArgs(const Settings&, const juce::File& video, const juce::File& wav, const juce::File& dest);

/** The encoder names in what `ffmpeg -encoders` prints. */
juce::StringArray parseEncoders(const juce::String& output);

//==============================================================================
/** An ffmpeg program, and what it can encode. */
struct Ffmpeg
{
    juce::File program;
    juce::String version;
    juce::StringArray encoders;

    bool has(const juce::String& encoder) const { return encoders.contains(encoder); }
};

/** Runs program to learn its version and encoders; nothing if it isn't a working ffmpeg. */
juce::Optional<Ffmpeg> probeFfmpeg(const juce::File& program);

/** The user's ffmpeg if it still works, else the first one on the PATH or in the usual install folders,
    which an app started from the Finder doesn't see on its PATH. */
juce::Optional<Ffmpeg> findFfmpeg(const juce::File& chosen);

}  // namespace MoTool::Video
