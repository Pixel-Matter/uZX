#include "EditState.h"
#include "../models/EditUtilities.h"
#include "../models/PsgFrameRetimer.h"

#include <limits>

using namespace std::literals;

namespace MoTool {

// ZoomViewState

ZoomViewState::ZoomViewState(te::Edit& e)
  : edit(e)
{
    state = edit.state.getOrCreateChildWithName(IDs::ZOOMVIEWSTATE, nullptr);
    // Saved with the edit, but kept out of undo: scrolling and zooming are not edits
    viewStartTime.referTo(state, IDs::viewStartTime, nullptr, 0s);
    viewTimePerPixel.referTo(state, IDs::viewTimePerPixel, nullptr, 60s / (double) viewWidthPx.load());
    viewY.referTo(state, IDs::viewY, nullptr, 0);
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
    viewStartTime = range.getStart();
    viewTimePerPixel = range.getLength() / getViewWidthPx();
    // DBG("viewTimePerPixel == " << viewTimePerPixel);
    markAndUpdate(updateZoom);
}

void ZoomViewState::setViewWidthPx(int w) noexcept {
    if (w <= 0)
        return;
    viewWidthPx.store(w);
    markAndUpdate(updateZoom);
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
    auto& transport = edit.getTransport();
    if (!transport.isPlaying() && !transport.isRecording())
        return transport.getPosition();

    // Tracktion already takes the graph's own latency off the transport position, but the device
    // sounds each block only after its output latency (buffer, driver, Bluetooth...). After a jump,
    // wait at the new position until it is heard rather than stepping back before it.
    // ponytail: a loop wrap counts as a jump, so the playhead leaves the loop end one latency early;
    // map the wrap if that shows with long latencies.
    auto latency = te::TimeDuration::fromSeconds(edit.engine.getDeviceManager().getOutputLatencySeconds());
    return std::max(transport.getPosition() - latency, lastJumpPosition);
}

void ZoomViewState::zoomHorizontally(double factor) {
    double scaleFactor = std::pow(2.0, -factor * 5.0);
    auto pos = getPlayheadPosition();
    auto span = getViewSpan();
    auto newRange = span * scaleFactor;

    if (auto newTimePerPixel = newRange / getViewWidthPx();
        newTimePerPixel > MinTimePerPixel && newTimePerPixel < MaxTimePerPixel) {
        setRange({
            jmax(te::TimePosition(), pos - newRange / 2.0),
            newRange
        });
        handlePlaybackScrolling();
        markAndUpdate(updateZoom);
    }
    handleUpdateNowIfNeeded();
}

void ZoomViewState::zoomAroundX(double factor, int anchorX) {
    zoomAroundXByScale(std::pow(2.0, -factor * 5.0), anchorX);
}

void ZoomViewState::magnifyAroundX(float scale, int anchorX) {
    zoomAroundXByScale(1.0 / scale, anchorX);
}

void ZoomViewState::zoomAroundXByScale(double timePerPixelScale, int anchorX) {
    if (isFollowingPlayhead())
        anchorX = roundToInt(timeToX(getPlayheadPosition()));  // else following would jump the view back to it
    auto anchorTime = xToTime(anchorX);
    auto newTimePerPixel = getTimePerPixel() * timePerPixelScale;

    if (newTimePerPixel > MinTimePerPixel && newTimePerPixel < MaxTimePerPixel) {
        auto newStart = anchorTime - newTimePerPixel * (double) anchorX;
        newStart = jmax(te::TimePosition(), newStart);
        auto newSpan = newTimePerPixel * getViewWidthPx();
        setRange({newStart, newSpan});
        markAndUpdate(updateZoom);
    }
    handleUpdateNowIfNeeded();
}

void ZoomViewState::scrollBy(double pixels) {
    setStart(jmax(te::TimePosition(), getStart() + getTimePerPixel() * pixels));
}

void ZoomViewState::scrollByWheel(const MouseWheelDetails& wheel) {
    lastWheelScrollMs = Time::getMillisecondCounter();
    scrollBy(-200.0 * (std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY : wheel.deltaX));
}

void ZoomViewState::beginDrag() {
    userDraggingView = true;
    dragStartViewStart = getStart();
    dragStartTimePerPixel = getTimePerPixel();
}

void ZoomViewState::dragBy(int anchorX, int deltaX, int deltaY) {
    auto newTimePerPixel = jlimit(MinTimePerPixel, MaxTimePerPixel, dragStartTimePerPixel * std::pow(2.0, deltaY * 0.01));
    auto anchorTime = dragStartViewStart + dragStartTimePerPixel * (double) anchorX;
    auto newStart = jmax(te::TimePosition(), anchorTime - newTimePerPixel * (double) (anchorX + deltaX));
    setRange({newStart, newTimePerPixel * getViewWidthPx()});
}

void ZoomViewState::endDrag() {
    userDraggingView = false;
}

void ZoomViewState::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree == state && isZoomProperty(prop)) {
        markAndUpdate(updateZoom);
    } else if (auto& tc = edit.getTransport(); tree == tc.state && prop == te::IDs::position) {
        if (!approximatelyEqual((double) tc.state[te::IDs::position], tc.getPosition().inSeconds())) {
            // because this callback could be called before tc.position is updated from state
            tc.position.forceUpdateOfCachedValue();
        }
        // DBG("ZoomViewState::valueTreePropertyChanged position: " << tc.getPosition().inSeconds()
            // << " dragging: " << (int) tc.isUserDragging()
            // << " playing: " << (int) tc.isPlaying());
        auto pos = tc.getPosition();
        if (!tc.isPositionUpdatingFromPlayhead() || pos < lastTransportPosition)
            lastJumpPosition = pos;
        lastTransportPosition = pos;
        handlePlaybackScrolling();
        markAndUpdate(updatePos);
    }
    handleUpdateNowIfNeeded();
}

bool ZoomViewState::isFollowingPlayhead() const {
    auto& transport = edit.getTransport();
    return (transport.isPlaying() || transport.isRecording()) && !userDraggingView
        && Time::getMillisecondCounter() - lastWheelScrollMs >= wheelFollowPauseMs;
}

void ZoomViewState::handlePlaybackScrolling() {
    auto& transport = edit.getTransport();
    if (transport.isPlaying() || transport.isRecording()) {
        if (!isFollowingPlayhead())
            return;  // the user holds or has just scrolled the view
        auto pos = getPlayheadPosition();
        // DBG("ZoomViewState::handlePlaybackScrolling, pos: " << pos.inSeconds());
        auto range = getRange();
        auto leftRange = range.getLength() / 3.0;
        if (pos < viewStartTime || pos > viewStartTime + leftRange) {
            auto newX1 = jmax(te::TimePosition(), pos - leftRange);
            if (newX1 != viewStartTime) {
                setStart(newX1);
            }
        }
    } else if (!transport.isUserDragging() && !userDraggingView) {
        // assuming autojump to current position when stopped
        // DBG("ZoomViewState::handlePlaybackScrolling, jumping to pos (not dragging, not playing)");
        jumpToCurrentPosition();
    } else {
        // DBG("ZoomViewState::handlePlaybackScrolling, skipping jump (user is dragging)");
    }
}

void ZoomViewState::startVideo() {
    lastJumpPosition = lastTransportPosition = edit.getTransport().getPosition();
}

void ZoomViewState::stopVideo() {
    // the playhead goes back to the transport position, which does not change on stop
    markAndUpdate(updatePos);
}

void ZoomViewState::handleAsyncUpdate() {
    if (compareAndReset(updatePos)) {
        // DBG("ZoomViewState::handleAsyncUpdate, dispatching zoomOrPosChanged, pos: "
            // << edit.getTransport().getPosition().inSeconds());
        listeners.call(&ZoomViewState::Listener::zoomOrPosChanged);
    }
    if (compareAndReset(updateZoom)) {
        listeners.call(&ZoomViewState::Listener::zoomChanged);
    }
}

//==============================================================================
// EditViewState

EditViewState::EditViewState(te::Edit& e, te::SelectionManager& s)
  : state(e.state.getOrCreateChildWithName(IDs::EDITVIEWSTATE, nullptr))
  , zoom(e)
  , selectionManager(s), edit(e)
{
    auto um = &edit.getUndoManager();
    showHeaders.referTo(state, IDs::showHeaders, um, true);
    preservePsgTimingOnTempoChange.referTo(state, IDs::preservePsgTimingOnTempoChange, um, true);
    snapPlayheadToGrid.referTo(state, IDs::snapPlayheadToGrid, um, true);
    headersWidth.referTo(state, IDs::headersWidth, nullptr, 110);
}

te::TimeDuration EditViewState::getBeatLengthFor(double bpm) const {
    const auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    const auto beatLen = te::TimeDuration::fromSeconds(240.0 / (bpm * ts.getMatchingTimeSig().denominator));
    return beatLen;
}

double EditViewState::getBpmForBeatLength(te::TimeDuration beatLen) const {
    const auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    const auto bpm = 240.0 / (beatLen.inSeconds() * ts.getMatchingTimeSig().denominator);
    return bpm;
}

double EditViewState::getFramesPerBeatFor(double bpm) const {
    const double fps = Helpers::getProjectFps(edit);
    return fps * getBeatLengthFor(bpm).inSeconds();
}

double EditViewState::getCurrentFramesPerBeat() const {
    const auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    return getFramesPerBeatFor(ts.getBpm());
}

void EditViewState::setBeatLength(te::TimeDuration beatLen) {
    jassert(beatLen > 0s);
    auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    auto bpm = 240.0 / (beatLen.inSeconds() * ts.getMatchingTimeSig().denominator);
    bpm = jlimit(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, bpm);

    PsgTiming::setTempoBpmRetimingFrames(edit, ts, bpm, shouldPreservePsgTimingOnTempoChange());
}

void EditViewState::setFramesPerBeat(int fpb) {
    const double fps = Helpers::getProjectFps(edit);
    auto targetBeatLen = te::TimeDuration::fromSeconds((double)fpb / fps);
    setBeatLength(targetBeatLen);
}

double EditViewState::getBpmSnappedToFps(double bpm) const {
    int fpb = roundToInt(getFramesPerBeatFor(bpm));
    const double fps = Helpers::getProjectFps(edit);
    auto targetBeatLen = te::TimeDuration::fromSeconds((double)fpb / fps);
    return getBpmForBeatLength(targetBeatLen);
}

double EditViewState::setBpmSnappedToFps(double bpm) {
    auto snappedBpm = getBpmSnappedToFps(bpm);
    auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());

    PsgTiming::setTempoBpmRetimingFrames(edit, ts, snappedBpm, shouldPreservePsgTimingOnTempoChange());

    return snappedBpm;
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
    return showHeaders ? headersWidth.get() : 0;
}

//==============================================================================
// TrackViewState

TrackViewState::TrackViewState(ValueTree trackState, UndoManager* undoManager)
  : state(ensure(trackState)), height(state, IDs::height, undoManager, 160)
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
