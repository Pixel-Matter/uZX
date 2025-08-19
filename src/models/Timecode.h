#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

enum class TimecodeTypeExt {
    millisecs       = (int) te::TimecodeType::millisecs,
    barsBeats       = (int) te::TimecodeType::barsBeats,
    fps24           = (int) te::TimecodeType::fps24,
    fps25           = (int) te::TimecodeType::fps25,
    fps30           = (int) te::TimecodeType::fps30,
    fps48           = 100,  // For non-fractional BPM
    fps48828,               // Pentagon (TODO round up to 50?)
    fps50,                  // ZX Spectrum/PAL
    fps60,                  // Atari ST/NTSC
    fps100,                 // Double PAL
    fps200,                 // Atari ST 200Hz
    // fpsCustom,              // Custom FPS
    barsBeatsFps24  = 200,
    barsBeatsFps25,
    barsBeatsFps30,
    barsBeatsFps48,
    barsBeatsFps48_828,
    barsBeatsFps50,
    barsBeatsFps60,
    barsBeatsFps100,
    barsBeatsFps200,
    // barsBeatsFpsCustom
};

struct TimecodeDisplayFormatExt : public te::TimecodeDisplayFormat {
    //==============================================================================
    TimecodeTypeExt typeExt;

    TimecodeDisplayFormatExt() noexcept
        : te::TimecodeDisplayFormat(te::TimecodeType::barsBeats)
        , typeExt(TimecodeTypeExt::barsBeatsFps50)
    {}

    TimecodeDisplayFormatExt(TimecodeTypeExt t) noexcept
        : te::TimecodeDisplayFormat(t <= TimecodeTypeExt::fps30 ? static_cast<te::TimecodeType>(t) : te::TimecodeType::barsBeats)
        , typeExt(t)
    {}

    //==============================================================================

    using te::TimecodeDisplayFormat::TimecodeDisplayFormat;

    inline float getFPS() const {
        float frameRate = 25; // Default
        switch (typeExt) {
            case TimecodeTypeExt::millisecs: [[fallthrough]];
            case TimecodeTypeExt::barsBeats: [[fallthrough]];
            case TimecodeTypeExt::fps24:     [[fallthrough]];
            case TimecodeTypeExt::fps25:     [[fallthrough]];
            case TimecodeTypeExt::fps30:
                frameRate = static_cast<float>(te::TimecodeDisplayFormat::getFPS());
                break;

            case TimecodeTypeExt::fps48:              frameRate = 48;      break;
            case TimecodeTypeExt::fps48828:          frameRate = 48.828f; break;
            case TimecodeTypeExt::fps50:              frameRate = 50;      break;
            case TimecodeTypeExt::fps60:              frameRate = 60;      break;
            case TimecodeTypeExt::fps100:             frameRate = 100;     break;
            case TimecodeTypeExt::fps200:             frameRate = 200;     break;
            case TimecodeTypeExt::barsBeatsFps24:     frameRate = 24;      break;
            case TimecodeTypeExt::barsBeatsFps25:     frameRate = 25;      break;
            case TimecodeTypeExt::barsBeatsFps30:     frameRate = 30;      break;
            case TimecodeTypeExt::barsBeatsFps48:     frameRate = 48;      break;
            case TimecodeTypeExt::barsBeatsFps48_828: frameRate = 48.828f; break;
            case TimecodeTypeExt::barsBeatsFps50:     frameRate = 50;      break;
            case TimecodeTypeExt::barsBeatsFps60:     frameRate = 60;      break;
            case TimecodeTypeExt::barsBeatsFps100:    frameRate = 100;     break;
            case TimecodeTypeExt::barsBeatsFps200:    frameRate = 200;     break;
            default: jassertfalse; break;
        }
        return frameRate;
    }

    juce::String getString(const te::TempoSequence& tempo, const te::TimePosition time, bool isRelative) const {
        using namespace std::literals;

        // maximum value we can add without shoving a time over into the next 'slot'
        static constexpr te::TimeDuration nudge = te::TimeDuration::fromSeconds(0.05 / 96000.0);

        if (typeExt <= TimecodeTypeExt::fps30) {
            return te::TimecodeDisplayFormat::getString(tempo, time, isRelative);
        } if (typeExt >= TimecodeTypeExt::barsBeatsFps24 && typeExt <= TimecodeTypeExt::barsBeatsFps60) {
            te::tempo::BarsAndBeats barsBeats;
            int bars, beats;
            te::BeatDuration fraction;

            if (! isRelative) {
                barsBeats = tempo.toBarsAndBeats(time + nudge);
                bars = barsBeats.bars + 1;
                beats = barsBeats.getWholeBeats() + 1;
                fraction = barsBeats.getFractionalBeats();
            } else if (time < 0s) {
                barsBeats = tempo.toBarsAndBeats(time - nudge);
                bars = -barsBeats.bars - 1;
                beats = (tempo.getTimeSig(0)->numerator - 1) - barsBeats.getWholeBeats();
                fraction = te::BeatDuration::fromBeats(1.0) - barsBeats.getFractionalBeats();
            } else {
                barsBeats = tempo.toBarsAndBeats(time + nudge);
                bars = barsBeats.bars + 1;
                beats = barsBeats.getWholeBeats() + 1;
                fraction = barsBeats.getFractionalBeats();
            }
            // Calculate frames from fractional beats
            // This assumes a consistent tempo during the beat
            double beatsPerSecond = tempo.getBeatsPerSecondAt(time);
            double secondsPerBeat = 1.0 / beatsPerSecond;
            double fractionInSeconds = fraction.inBeats() * secondsPerBeat;
            int frameNumber = static_cast<int>(fractionInSeconds * getFPS());

            juce::String s = juce::String::formatted("%d|%d|%02d",
                                                    bars,
                                                    beats,
                                                    frameNumber);
            return time < 0s ? ("-" + s) : s;
        }
        jassertfalse;
        return "UNSUPPORTED TimecodeDisplayFormatExt";
    }


};

}  // namespace MoTool

namespace juce {

template<>
struct VariantConverter<MoTool::TimecodeDisplayFormatExt> {
    static MoTool::TimecodeDisplayFormatExt fromVar(const var& v) {
        if (v == "seconds")            return MoTool::TimecodeTypeExt::millisecs;
        if (v == "beats")              return MoTool::TimecodeTypeExt::barsBeats;
        if (v == "fps24")              return MoTool::TimecodeTypeExt::fps24;
        if (v == "fps25")              return MoTool::TimecodeTypeExt::fps25;
        if (v == "fps30")              return MoTool::TimecodeTypeExt::fps30;
        if (v == "fps48")              return MoTool::TimecodeTypeExt::fps48;
        if (v == "fps48_828")          return MoTool::TimecodeTypeExt::fps48828;
        if (v == "fps50")              return MoTool::TimecodeTypeExt::fps50;
        if (v == "fps60")              return MoTool::TimecodeTypeExt::fps60;
        if (v == "fps100")             return MoTool::TimecodeTypeExt::fps100;
        if (v == "fps200")             return MoTool::TimecodeTypeExt::fps200;
        if (v == "barsBeatsFps24")     return MoTool::TimecodeTypeExt::barsBeatsFps24;
        if (v == "barsBeatsFps25")     return MoTool::TimecodeTypeExt::barsBeatsFps25;
        if (v == "barsBeatsFps30")     return MoTool::TimecodeTypeExt::barsBeatsFps30;
        if (v == "barsBeatsFps48")     return MoTool::TimecodeTypeExt::barsBeatsFps48;
        if (v == "barsBeatsFps48_828") return MoTool::TimecodeTypeExt::barsBeatsFps48_828;
        if (v == "barsBeatsFps50")     return MoTool::TimecodeTypeExt::barsBeatsFps50;
        if (v == "barsBeatsFps60")     return MoTool::TimecodeTypeExt::barsBeatsFps60;
        if (v == "barsBeatsFps100")    return MoTool::TimecodeTypeExt::barsBeatsFps100;
        if (v == "barsBeatsFps200")    return MoTool::TimecodeTypeExt::barsBeatsFps200;
        return MoTool::TimecodeTypeExt::millisecs;
    }

    static var toVar(MoTool::TimecodeDisplayFormatExt t) {
        if (t.typeExt == MoTool::TimecodeTypeExt::millisecs)          return "seconds";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeats)          return "beats";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps24)              return "fps24";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps25)              return "fps25";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps30)              return "fps30";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps48)              return "fps48";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps48828)          return "fps48_828";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps50)              return "fps50";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps60)              return "fps60";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps100)             return "fps100";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps200)             return "fps200";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps24)     return "barsBeatsFps24";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps25)     return "barsBeatsFps25";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps30)     return "barsBeatsFps30";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps48)     return "barsBeatsFps48";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps48_828) return "barsBeatsFps48_828";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps50)     return "barsBeatsFps50";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps60)     return "barsBeatsFps60";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps100)    return "barsBeatsFps100";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps200)    return "barsBeatsFps200";
        return "seconds";
    }
};

}
