#include "EditState.h"
#include "../models/EditUtilities.h"
#include "../models/PsgFrameRetimer.h"

#include <limits>

using namespace std::literals;

namespace MoTool {

// ZoomViewState

namespace {
    // Every zoom stops at the limits rather than refusing to move
    te::TimeDuration clampTimePerPixel(te::TimeDuration t) {
        return jlimit(ZoomViewState::MinTimePerPixel, ZoomViewState::MaxTimePerPixel, t);
    }

    // Time per pixel scale for a wheel or keyboard zoom step; positive zooms in
    double zoomStepScale(double factor) {
        return std::pow(2.0, -factor * 5.0);
    }
}

ZoomViewState::ZoomViewState(te::Edit& e, ValueTree s)
  : edit(e), state(s)
{
    // Saved with the edit, but kept out of undo: scrolling and zooming are not edits
    viewStartTime.referTo(state, IDs::viewStartTime, nullptr, 0s);
    viewTimePerPixel.referTo(state, IDs::viewTimePerPixel, nullptr, 60s / (double) viewWidthPx.load());
    viewY.referTo(state, IDs::viewY, nullptr, 0);
    followPlayback.referTo(state, IDs::followPlayback, nullptr, true);
    edit.getTransport().state.addListener(this);
    edit.getTransport().addListener(this);
    state.addListener(this);
}

ZoomViewState::~ZoomViewState() {
    state.removeListener(this);
    edit.getTransport().removeListener(this);
    edit.getTransport().state.removeListener(this);
}

void ZoomViewState::addListener(Listener* l) {
    listeners.add(l);
}

void ZoomViewState::removeListener(Listener* l) {
    listeners.remove(l);
}

bool ZoomViewState::isZoomProperty(const juce::Identifier& id) {
    return id == IDs::viewStartTime || id == IDs::viewTimePerPixel;
}

te::TimeRange ZoomViewState::getRange() const {
    return { viewStartTime, getViewSpan() };
}

void ZoomViewState::setRange(te::TimeRange range) {
    continuousZoom_ = false;
    applyView(range.getStart(), range.getLength() / getViewWidthPx());
}

void ZoomViewState::applyView(te::TimePosition start, te::TimeDuration timePerPixel) {
    viewStartTime = start;
    viewTimePerPixel = timePerPixel;
    markAndUpdate(updateViewRange);
}

void ZoomViewState::setViewWidthPx(int w) noexcept {
    if (w <= 0)
        return;
    viewWidthPx.store(w);
    markAndUpdate(updateViewRange);
    handleUpdateNowIfNeeded();
}

te::TimePosition ZoomViewState::getStart() const noexcept {
    return viewStartTime;
}

void ZoomViewState::setStart(te::TimePosition start) {
    viewStartTime = start;
}

double ZoomViewState::getViewY() const {
    return viewY;
}

te::TimeDuration ZoomViewState::getViewSpan() const {
    return viewTimePerPixel.get() * getViewWidthPx();
}

te::TimeDuration ZoomViewState::getTimePerPixel() const {
    return viewTimePerPixel.get();
}

float ZoomViewState::timeToX(te::TimePosition time) const {
    return (float) ((time - viewStartTime) / viewTimePerPixel);
}

te::TimePosition ZoomViewState::xToTime(int x) const {
    return viewStartTime + viewTimePerPixel.get() * (double) x;
}

float ZoomViewState::durationToPixels(te::TimeDuration duration) const {
    return (float)(duration / viewTimePerPixel.get());
}

bool ZoomViewState::jumpToPosition(te::TimePosition pos) {
    if (!getRange().containsInclusive(pos)) {
        auto newViewX1 = jmax(te::TimePosition(), pos - getViewSpan() / 2.0);
        setStart(newViewX1);
        return true;
    }
    return false;
}

bool ZoomViewState::jumpToCurrentPosition() {
    return jumpToPosition(getPlayheadPosition());
}

te::TimePosition ZoomViewState::getPlayheadPosition() const {
    if (offlineFrames)
        return framePlayhead;
    auto& transport = edit.getTransport();
    if (!transport.isPlaying() && !transport.isRecording())
        return transport.getPosition();
    if (framePlayheadValid && areFramesRunning())
        return framePlayhead;
    return getHeardPosition();
}

te::TimePosition ZoomViewState::getHeardPosition() const {
    auto& transport = edit.getTransport();
    // Tracktion already takes the graph's own latency off the transport position, but the device
    // sounds each block only after its output latency (buffer, driver, Bluetooth...). After a jump,
    // wait at the new position until it is heard rather than stepping back before it.
    // ponytail: a loop wrap counts as a jump, so the playhead leaves the loop end one latency early;
    // map the wrap if that shows with long latencies.
    auto latency = te::TimeDuration::fromSeconds(edit.engine.getDeviceManager().getOutputLatencySeconds());
    return std::max(transport.getPosition() - latency, lastJumpPosition);
}

void ZoomViewState::reportHeardPosition() {
    const auto heard = getHeardPosition();
    playheadClock.report(heard.inSeconds(), Time::getMillisecondCounterHiRes(), heard > lastJumpPosition);
}

bool ZoomViewState::areFramesRunning() const {
    return Time::getMillisecondCounterHiRes() - lastFrameMs < framesStopAfterMs;
}

void ZoomViewState::advanceFrame(double timestampSec) {
    // Every frame, playing or not, so the frames' period and times are ready when playback starts
    displayFrames.add(timestampSec, Time::getMillisecondCounterHiRes());

    auto& transport = edit.getTransport();
    if (!transport.isPlaying() && !transport.isRecording())
        return;  // nothing moves by itself

    // ponytail: frameLead is a fixed one frame of display pipeline; tune it by eye against the sound,
    // or measure the presentation time per platform if that varies.
    // Where the playhead is by the time the frame is seen
    lastFrameMs = Time::getMillisecondCounterHiRes();
    auto pos = te::TimePosition::fromSeconds(
        playheadClock.positionAt(displayFrames.getFrameMs() + displayFrames.getPeriod() * frameLead * 1000.0));
    // In or before the loop, playback wraps at its end, but the wrap report comes later; past it, it plays on
    if (const auto loopEnd = transport.getLoopRange().getEnd(); transport.looping && lastTransportPosition < loopEnd)
        pos = std::min(pos, loopEnd);
    framePlayhead = pos;
    framePlayheadValid = true;

    handlePlaybackScrolling();
    markAndUpdate(updatePlayhead);
    handleUpdateNowIfNeeded();
}

void ZoomViewState::showOfflineFrame(te::TimePosition pos) {
    offlineFrames = true;
    framePlayhead = pos;
    followPlayhead(pos);
    markAndUpdate(updatePlayhead);
    handleUpdateNowIfNeeded();
}

void ZoomViewState::zoomHorizontally(double factor) {
    zoomToTimePerPixel(getTimePerPixel() * zoomStepScale(factor), true);  // a held key is one history step
}

void ZoomViewState::zoomToTimePerPixel(te::TimeDuration timePerPixel, bool continuous) {
    timePerPixel = clampTimePerPixel(timePerPixel);
    const auto newSpan = timePerPixel * getViewWidthPx();
    applyZoom(jmax(te::TimePosition(), getPlayheadPosition() - newSpan / 2.0), timePerPixel, continuous);
    handlePlaybackScrolling();
    handleUpdateNowIfNeeded();
}

void ZoomViewState::zoomToLevel(ZoomLevel level) {
    const auto pos = getPlayheadPosition();
    const auto& tempo = edit.tempoSequence.getTempoAt(pos);
    const auto ladder = Helpers::makeSnapLadder(edit, pos, TimecodeDisplayMode::barsBeatsFrames);
    const int beat = ladder.getBeatLevel();
    auto interval = ladder.getApproxInterval(beat, tempo, false);
    switch (level) {
        case ZoomLevel::frames:
            interval = ladder.getApproxInterval(0, tempo, false);
            break;
        case ZoomLevel::rows:
            interval = beat > 0 && ladder.getKind(beat - 1) == SnapLadder::LevelKind::subdivision
                           ? ladder.getApproxInterval(beat - 1, tempo, false)
                           : interval / 4.0;
            break;
        case ZoomLevel::beats:
            break;
        case ZoomLevel::bars:
            interval = ladder.getApproxInterval(ladder.getBarLevel(), tempo, false);
            break;
    }
    zoomToTimePerPixel(interval / 16.0);
}

void ZoomViewState::rememberZoom(ZoomSnapshot snapshot) {
    if (zoomHistory_.size() == maxZoomHistory)
        zoomHistory_.erase(zoomHistory_.begin());
    zoomHistory_.push_back(snapshot);
}

void ZoomViewState::applyZoom(te::TimePosition start, te::TimeDuration timePerPixel, bool continuous) {
    continuousZoom_ = continuousZoom_ && continuous;
    if (approximatelyEqual(start.inSeconds(), getStart().inSeconds())
        && approximatelyEqual(timePerPixel.inSeconds(), getTimePerPixel().inSeconds()))
        return;
    const auto now = Time::getMillisecondCounter();
    if (!continuousZoom_ || now - lastZoomGestureMs_ >= zoomGestureGapMs)
        rememberZoom({getStart(), getTimePerPixel()});
    continuousZoom_ = continuous;
    lastZoomGestureMs_ = now;
    applyView(start, timePerPixel);
}

void ZoomViewState::zoomToRange(te::TimeRange range) {
    continuousZoom_ = false;
    if (!range.isEmpty()) {
        applyZoom(jmax(te::TimePosition(), range.getStart()), range.getLength() / getViewWidthPx());
        handleUpdateNowIfNeeded();
    }
}

void ZoomViewState::zoomBack() {
    continuousZoom_ = false;
    if (!canZoomBack())
        return;
    const auto previous = zoomHistory_.back();
    zoomHistory_.pop_back();
    lastWheelScrollMs = Time::getMillisecondCounter();  // let the restored view stay visible during playback
    applyView(previous.start, previous.timePerPixel);
    handleUpdateNowIfNeeded();
}

void ZoomViewState::zoomAroundX(double factor, int anchorX) {
    zoomAroundXByScale(zoomStepScale(factor), anchorX);
}

void ZoomViewState::magnifyAroundX(float scale, int anchorX) {
    if (!std::isfinite(scale) || scale <= 0.0f)
        return;
    zoomAroundXByScale(1.0 / scale, anchorX);
}

void ZoomViewState::zoomAroundXByScale(double timePerPixelScale, int anchorX) {
    if (isFollowingPlayhead())
        anchorX = roundToInt(timeToX(getPlayheadPosition()));  // else following would jump the view back to it
    const auto anchorTime = xToTime(anchorX);
    const auto newTimePerPixel = clampTimePerPixel(getTimePerPixel() * timePerPixelScale);
    applyZoom(jmax(te::TimePosition(), anchorTime - newTimePerPixel * (double) anchorX), newTimePerPixel, true);
    handleUpdateNowIfNeeded();
}

void ZoomViewState::scrollBy(double pixels) {
    continuousZoom_ = false;
    setStart(jmax(te::TimePosition(), getStart() + getTimePerPixel() * pixels));
}

void ZoomViewState::scrollByWheel(const MouseWheelDetails& wheel) {
    lastWheelScrollMs = Time::getMillisecondCounter();
    scrollBy(-200.0 * (std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY : wheel.deltaX));
}

void ZoomViewState::beginDrag() {
    continuousZoom_ = false;
    dragZoomRecorded_ = false;
    userDraggingView = true;
    dragStartViewStart = getStart();
    dragStartTimePerPixel = getTimePerPixel();
}

void ZoomViewState::dragBy(int anchorX, int deltaX, int deltaY) {
    auto newTimePerPixel = clampTimePerPixel(dragStartTimePerPixel * std::pow(2.0, deltaY * 0.01));
    auto anchorTime = dragStartViewStart + dragStartTimePerPixel * (double) anchorX;
    auto newStart = jmax(te::TimePosition(), anchorTime - newTimePerPixel * (double) (anchorX + deltaX));
    if (!dragZoomRecorded_ && !approximatelyEqual(newTimePerPixel.inSeconds(), dragStartTimePerPixel.inSeconds())) {
        rememberZoom({dragStartViewStart, dragStartTimePerPixel});
        dragZoomRecorded_ = true;
    }
    applyView(newStart, newTimePerPixel);
}

void ZoomViewState::endDrag() {
    userDraggingView = false;
}

void ZoomViewState::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree == state && isZoomProperty(prop)) {
        markAndUpdate(updateViewRange);
    } else if (auto& tc = edit.getTransport(); tree == tc.state && prop == te::IDs::position) {
        if (!approximatelyEqual((double) tc.state[te::IDs::position], tc.getPosition().inSeconds())) {
            // because this callback could be called before tc.position is updated from state
            tc.position.forceUpdateOfCachedValue();
        }
        // DBG("ZoomViewState::valueTreePropertyChanged position: " << tc.getPosition().inSeconds()
            // << " dragging: " << (int) tc.isUserDragging()
            // << " playing: " << (int) tc.isPlaying());
        auto pos = tc.getPosition();
        const bool jumped = !tc.isPositionUpdatingFromPlayhead() || pos < lastTransportPosition;
        if (jumped) {
            lastJumpPosition = pos;
            framePlayheadValid = false;  // show the jump now, not the frame's old position
        }
        lastTransportPosition = pos;
        reportHeardPosition();
        // While frames run they move the playhead and the view; reports only feed the clock
        if (jumped || !areFramesRunning()) {
            handlePlaybackScrolling();
            markAndUpdate(updatePlayhead);
        }
    }
    handleUpdateNowIfNeeded();
}

bool ZoomViewState::isFollowingPlayhead() const {
    if (offlineFrames)
        return true;  // as in playback, which lets clips scroll their cached content
    auto& transport = edit.getTransport();
    return (followPlayback.get() || followForced) && (transport.isPlaying() || transport.isRecording()) && !userDraggingView
        && Time::getMillisecondCounter() - lastWheelScrollMs >= wheelFollowPauseMs;
}

void ZoomViewState::handlePlaybackScrolling() {
    auto& transport = edit.getTransport();
    if (transport.isPlaying() || transport.isRecording()) {
        if (isFollowingPlayhead())  // else the user holds or has just scrolled the view
            followPlayhead(getPlayheadPosition());
    } else if (!transport.isUserDragging() && !userDraggingView) {
        // assuming autojump to current position when stopped
        // DBG("ZoomViewState::handlePlaybackScrolling, jumping to pos (not dragging, not playing)");
        jumpToCurrentPosition();
    } else {
        // DBG("ZoomViewState::handlePlaybackScrolling, skipping jump (user is dragging)");
    }
}

void ZoomViewState::followPlayhead(te::TimePosition pos) {
    const auto leftRange = getViewSpan() / 3.0;
    if (pos < viewStartTime || pos > viewStartTime + leftRange) {
        // On the timeline's whole pixels, so what is drawn keeps its place within a pixel and
        // clips can blit their cached content (PsgClipComponent) in line with the ruler and the playhead
        const auto timePerPixel = getTimePerPixel();
        auto newX1 = te::TimePosition() + timePerPixel * std::floor(jmax(te::TimePosition(), pos - leftRange)
                                                                         .inSeconds() / timePerPixel.inSeconds());
        if (newX1 != viewStartTime)
            setStart(newX1);
    }
}

void ZoomViewState::startVideo() {
    lastJumpPosition = lastTransportPosition = edit.getTransport().getPosition();
    framePlayheadValid = false;
    reportHeardPosition();
}

void ZoomViewState::stopVideo() {
    framePlayheadValid = false;
    lastFrameMs = -1.0e9;  // stopped, the transport moves the playhead again
    // the playhead goes back to the transport position, which does not change on stop
    markAndUpdate(updatePlayhead);
}

void ZoomViewState::handleAsyncUpdate() {
    if (compareAndReset(updatePlayhead)) {
        // DBG("ZoomViewState::handleAsyncUpdate, dispatching playheadMoved, pos: "
            // << edit.getTransport().getPosition().inSeconds());
        listeners.call(&ZoomViewState::Listener::playheadMoved);
    }
    if (compareAndReset(updateViewRange)) {
        listeners.call(&ZoomViewState::Listener::viewRangeChanged);
    }
}

//==============================================================================
// VizOptions

namespace {
    constexpr auto vizLayoutSetting = "vizLayout";
    constexpr auto vizColoursSetting = "vizColours";
    constexpr auto vizScopesSetting = "vizScopes";
    constexpr auto vizDecorationsSetting = "vizDecorations";

    template <typename Enum>
    Enum loadVizEnum(PropertiesFile& p, const char* name, Enum fallback, const StringArray& names) {
        const int value = p.getIntValue(name, (int) fallback);
        return isPositiveAndBelow(value, names.size()) ? (Enum) value : fallback;
    }
}

VizOptions VizOptions::load(PropertiesFile& p) {
    VizOptions o;
    o.layout = loadVizEnum(p, vizLayoutSetting, o.layout, layoutNames);
    o.colours = loadVizEnum(p, vizColoursSetting, o.colours, colourNames);
    o.scopes = loadVizEnum(p, vizScopesSetting, o.scopes, scopesNames);
    o.decorations = loadVizEnum(p, vizDecorationsSetting, o.decorations, decorationsNames);
    return o;
}

void VizOptions::save(PropertiesFile& p) const {
    p.setValue(vizLayoutSetting, (int) layout);
    p.setValue(vizColoursSetting, (int) colours);
    p.setValue(vizScopesSetting, (int) scopes);
    p.setValue(vizDecorationsSetting, (int) decorations);
}

//==============================================================================
// EditViewState

namespace {
    ValueTree viewSettings(te::Edit& e, const Identifier& type, bool detached) {
        auto settings = e.state.getOrCreateChildWithName(type, nullptr);
        return detached ? settings.createCopy() : settings;
    }
}

void EditViewState::showAsPlayer(bool viz, VizOptions options) {
    showHeaders = false;
    showClipHeaders = false;
    vizMode = viz;
    zoom.setFollowForced(viz);  // Viz mode is for watching the tune play
    vizOptions = options;
    showFooters = ! viz || options.showsScopes();
}

EditViewState::EditViewState(te::Edit& e, te::SelectionManager& s, bool detached)
  : state(viewSettings(e, IDs::EDITVIEWSTATE, detached))
  , zoom(e, viewSettings(e, IDs::ZOOMVIEWSTATE, detached))
  , selectionManager(s), edit(e)
{
    preservePsgTimingOnTempoChange.referTo(state, IDs::preservePsgTimingOnTempoChange, nullptr, true);
    snapPlayheadToGrid.referTo(state, IDs::snapPlayheadToGrid, nullptr, true);
    headersWidth.referTo(state, IDs::headersWidth, nullptr, minHeadersWidth);
    vizScopesWidth.referTo(state, IDs::vizScopesWidth, nullptr, footerWidth);
    vizScopesShare.referTo(state, IDs::vizScopesShare, nullptr, 35);
    footersWidth.referTo(state, IDs::footersWidth, nullptr, footerWidth);
}

void EditViewState::setPreservePsgTimingOnTempoChange(bool shouldPreserve) {
    preservePsgTimingOnTempoChange = shouldPreserve;
}

bool EditViewState::shouldPreservePsgTimingOnTempoChange() const {
    return preservePsgTimingOnTempoChange.get();
}

void EditViewState::setSnapPlayheadToGrid(bool shouldSnap) {
    snapPlayheadToGrid = shouldSnap;
}

bool EditViewState::shouldSnapPlayheadToGrid() const {
    return snapPlayheadToGrid.get();
}

int EditViewState::getTrackHeaderWidth() const {
    // clamped: older Player builds saved 0
    return showHeaders ? jlimit(minHeadersWidth, maxHeadersWidth, headersWidth.get()) : 0;
}

//==============================================================================
// TrackViewState

TrackViewState::TrackViewState(ValueTree trackState)
  : state(ensure(trackState)), height(state, IDs::height, nullptr, 160)  // layout, not an edit
{
    constrainer.setMinimumHeight(56);
    constrainer.setMinimumOnscreenAmounts(0, 0, std::numeric_limits<int>::max(), 0);
    state.addListener(this);
}


TrackViewState::~TrackViewState()
{
    state.removeListener(this);
}

int TrackViewState::getHeight() const noexcept
{
    return height.get();
}

void TrackViewState::setTrackHeight(int h)
{
    height = h;
}

void TrackViewState::addListener(Listener* l)
{
    listeners.add(l);
}

void TrackViewState::removeListener(Listener* l)
{
    listeners.remove(l);
}

ComponentBoundsConstrainer& TrackViewState::getConstrainer()
{
    return constrainer;
}

ValueTree TrackViewState::ensure(ValueTree trackState)
{
    if (auto existing = trackState.getChildWithName(IDs::VIEWSTATE); existing.isValid())
        return existing;
    auto vt = ValueTree(IDs::VIEWSTATE);
    trackState.addChild(vt, -1, nullptr);
    return vt;
}

void TrackViewState::valueTreePropertyChanged(ValueTree& v, const Identifier& id)
{
    if (v == state && id == IDs::height)
        listeners.call(&Listener::trackViewStateChanged);
}

} // namespace MoTool
