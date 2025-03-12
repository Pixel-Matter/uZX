#pragma once

#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

enum class TimecodeTypeExt {
    millisecs       = (int) te::TimecodeType::millisecs,
    barsBeats       = (int) te::TimecodeType::barsBeats,
    fps24           = (int) te::TimecodeType::fps24,
    fps25           = (int) te::TimecodeType::fps25,
    fps30           = (int) te::TimecodeType::fps30,
    fps50           = 100,
    fps60,
    barsBeatsFps24,
    barsBeatsFps25,
    barsBeatsFps30,
    barsBeatsFps50,
    barsBeatsFps60
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

    inline int getFPS() const {
        if (typeExt <= TimecodeTypeExt::fps30) {
            return te::TimecodeDisplayFormat::getFPS();
        }
        int frameRate = 25; // Default
        switch (typeExt) {
            case TimecodeTypeExt::fps50: frameRate = 50; break;
            case TimecodeTypeExt::fps60: frameRate = 60; break;
            case TimecodeTypeExt::barsBeatsFps24: frameRate = 24; break;
            case TimecodeTypeExt::barsBeatsFps25: frameRate = 25; break;
            case TimecodeTypeExt::barsBeatsFps30: frameRate = 30; break;
            case TimecodeTypeExt::barsBeatsFps50: frameRate = 50; break;
            case TimecodeTypeExt::barsBeatsFps60: frameRate = 60; break;
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
        if (v == "seconds")        return MoTool::TimecodeTypeExt::millisecs;
        if (v == "beats")          return MoTool::TimecodeTypeExt::barsBeats;
        if (v == "fps24")          return MoTool::TimecodeTypeExt::fps24;
        if (v == "fps25")          return MoTool::TimecodeTypeExt::fps25;
        if (v == "fps30")          return MoTool::TimecodeTypeExt::fps30;
        if (v == "fps50")          return MoTool::TimecodeTypeExt::fps50;
        if (v == "fps60")          return MoTool::TimecodeTypeExt::fps60;
        if (v == "barsBeatsFps24") return MoTool::TimecodeTypeExt::barsBeatsFps24;
        if (v == "barsBeatsFps25") return MoTool::TimecodeTypeExt::barsBeatsFps25;
        if (v == "barsBeatsFps30") return MoTool::TimecodeTypeExt::barsBeatsFps30;
        if (v == "barsBeatsFps50") return MoTool::TimecodeTypeExt::barsBeatsFps50;
        if (v == "barsBeatsFps60") return MoTool::TimecodeTypeExt::barsBeatsFps60;
        return MoTool::TimecodeTypeExt::millisecs;
    }

    static var toVar(MoTool::TimecodeDisplayFormatExt t) {
        if (t.typeExt == MoTool::TimecodeTypeExt::millisecs)      return "seconds";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeats)      return "beats";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps24)          return "fps24";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps25)          return "fps25";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps30)          return "fps30";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps50)          return "fps50";
        if (t.typeExt == MoTool::TimecodeTypeExt::fps60)          return "fps60";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps24) return "barsBeatsFps24";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps25) return "barsBeatsFps25";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps30) return "barsBeatsFps30";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps50) return "barsBeatsFps50";
        if (t.typeExt == MoTool::TimecodeTypeExt::barsBeatsFps60) return "barsBeatsFps60";
        return "seconds";
    }
};

}
