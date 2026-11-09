#include "PlayheadClock.h"

#include <algorithm>
#include <cmath>

namespace MoTool {

void PlayheadClock::report(double positionSec, double nowMs, bool isMoving) {
    if (!isMoving) {
        shownPos = positionSec;
        shownMs = nowMs;
    } else if (!moving) {
        shownMs = nowMs;  // starts running from where it held
    }
    reportPos = positionSec;
    reportMs = nowMs;
    moving = isMoving;
}

double PlayheadClock::positionAt(double nowMs) {
    const auto sinceReport = nowMs - reportMs;
    if (!moving || sinceReport > maxExtrapolationMs) {
        shownMs = nowMs;
        return shownPos;
    }

    const auto target = reportPos + std::max(0.0, sinceReport) / 1000.0;
    const auto predicted = shownPos + std::max(0.0, nowMs - shownMs) / 1000.0;
    const auto error = target - predicted;
    shownPos = std::abs(error) > snapSec ? target : predicted + error * catchUp;
    shownMs = nowMs;
    return shownPos;
}

void FramePeriod::add(double intervalSec) {
    recent[next++ % recent.size()] = intervalSec;
    auto sorted = recent;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    period = sorted[sorted.size() / 2];
}

void DisplayFrames::add(double timestampSec, double nowMs) {
    const auto interval = timestampSec - lastTimestampSec;
    lastTimestampSec = timestampSec;
    if (interval > 1.0 / 240.0 && interval < 1.0 / 20.0)
        period.add(interval);

    const auto onTime = frameMs + interval * 1000.0;
    frameMs = nowMs < onTime || nowMs - onTime > period.get() * 1000.0 ? nowMs : onTime;
}

}  // namespace MoTool
