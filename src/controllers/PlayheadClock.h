#pragma once

#include <array>
#include <cstddef>

namespace MoTool {

//==============================================================================
/**
    A steady playhead for display frames, from the transport's irregular position reports.

    The transport reports where playback is about 45 times a second, each report rounded down
    to the start of an audio block, while frames come at the display rate. Between reports the
    clock runs on at playback speed and leans a little towards the latest report each frame,
    so every frame steps by about the same amount. A report that is not moving (playback
    waiting at a jump until it is heard) moves the clock there at once and holds it.

    Times are milliseconds of Time::getMillisecondCounterHiRes(), positions are seconds.
*/
class PlayheadClock {
public:
    /** Where playback is at nowMs, and whether it is advancing. */
    void report(double positionSec, double nowMs, bool isMoving);

    /** The position to show at nowMs. Call with nowMs not going backwards. */
    double positionAt(double nowMs);

    /** Reports older than this stop the clock rather than letting it run on alone. */
    static constexpr double maxExtrapolationMs = 150.0;
    /** Further from the reports than this is not jitter: the clock jumps to them. */
    static constexpr double snapSec = 0.25;
    /** The share of the distance to the reports taken each frame. */
    static constexpr double catchUp = 0.1;

private:
    double reportPos = 0.0, reportMs = 0.0;
    double shownPos = 0.0, shownMs = 0.0;
    bool moving = false;
};

/**
    The display's frame period, from the intervals between its vblanks: the median of the last
    few, so a late frame (a vblank the message thread missed) does not count as a slower display,
    while a display with another refresh rate takes over within three frames.
*/
class FramePeriod {
public:
    /** Takes the interval since the previous vblank, in seconds. */
    void add(double intervalSec);

    /** The period in seconds: a 60 Hz frame until intervals come in. */
    double get() const noexcept { return period; }

private:
    std::array<double, 5> recent {1.0 / 60.0, 1.0 / 60.0, 1.0 / 60.0, 1.0 / 60.0, 1.0 / 60.0};
    size_t next = 0;
    double period = 1.0 / 60.0;
};

}  // namespace MoTool
