#include "EditState.h"
#include "../models/EditUtilities.h"

using namespace std::literals;

namespace MoTool {

// ZoomViewState

ZoomViewState::ZoomViewState(te::Edit& e, ValueTree& st)
  : edit(e)
  , state(st)
{
    state = edit.state.getOrCreateChildWithName(IDs::ZOOMVIEWSTATE, nullptr);
    auto um = &edit.getUndoManager();
    viewX1.referTo(state, IDs::viewX1, um, 0s);
    // viewX2.referTo(state, IDs::viewX2, um, 60s);
    viewSpan.referTo(state, IDs::viewSpan, um, 60s);
    viewY.referTo(state, IDs::viewY, um, 0);
    edit.getTransport().state.addListener(this);
    state.addListener(this);
}

ZoomViewState::~ZoomViewState() {
    state.removeListener(this);
    edit.getTransport().state.removeListener(this);
}

void ZoomViewState::addListener(Listener* l) {
    listeners.add(l);
}

void ZoomViewState::removeListener(Listener* l) {
    listeners.remove(l);
}

bool ZoomViewState::isZoomProperty(const juce::Identifier& id) {
    return id == IDs::viewX1 || id == IDs::viewSpan;
}

te::TimeRange ZoomViewState::getRange() const {
    return { viewX1, viewSpan };
}

void ZoomViewState::setRange(te::TimeRange range) {
    // DBG("ZoomViewState::setRange, range: " << range.getStart().inSeconds() << " - " << range.getEnd().inSeconds());
    viewX1 = range.getStart();
    viewSpan = range.getLength();
    // DBG("ZoomViewState::setRange, calling markAndUpdate(updateZoom)");
    markAndUpdate(updateZoom);
}

void ZoomViewState::setStart(te::TimePosition start) {
    setRange({ start, start + getViewSpan() });
}

double ZoomViewState::getViewY() const {
    return viewY;
}

te::TimeDuration ZoomViewState::getViewSpan() const {
    return viewSpan;
}

int ZoomViewState::timeToX(te::TimePosition time, int width) const {
    return roundToInt(((time - viewX1) * width) / getViewSpan());
}

te::TimePosition ZoomViewState::xToTime(int x, int width) const {
    return toPosition(getViewSpan() * (double(x) / width)) + toDuration(viewX1.get());
}

float ZoomViewState::durationToPixels(te::TimeDuration duration, int width) const {
    return (float)(duration * width / getViewSpan());
}

bool ZoomViewState::jumpToPosition(te::TimePosition pos) {
    // DBG("ZoomViewState::scrollToPosition pos: " << pos.inSeconds());
    if (!getRange().containsInclusive(pos)) {
        auto newViewX1 = jmax(te::TimePosition(), pos - getViewSpan() / 2.0);
        setStart(newViewX1);
        return true;
    }
    return false;
}

bool ZoomViewState::jumpToCurrentPosition() {
    auto pos = edit.getTransport().getPosition();
    // DBG("ZoomViewState::jumpToCurrentPosition pos: " << pos.inSeconds());
    return jumpToPosition(pos);
}

void ZoomViewState::zoomHorizontally(double factor) {
    // DBG("ZoomViewState::zoomHorizontally, factor: " << factor);
    double scaleFactor = std::pow(2.0, -factor * 5.0);
    auto pos = edit.getTransport().getPosition();
    auto range = getViewSpan();
    auto newHalfRange = range * scaleFactor / 2.0;
    if (newHalfRange > 0.5s && newHalfRange < 600s) {
        setRange({
            jmax(te::TimePosition(), pos - newHalfRange),
            newHalfRange * 2.0
        });
        // DBG("ZoomViewState::zoomHorizontally zoomChanged, new range: " << viewX1->inSeconds() << " - " << viewX2->inSeconds());
        handlePlaybackScrolling();
        markAndUpdate(updateZoom);
    }
}

void ZoomViewState::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree == state) {
        if (isZoomProperty(prop)) {
            DBG("ZoomViewState::valueTreePropertyChanged zoomChanged, range: " << getRange().getStart() << " - " << getRange().getEnd());
            markAndUpdate(updateZoom);
        }
    } else if (auto& tc = edit.getTransport(); tree == tc.state && prop == te::IDs::position) {
        if (!approximatelyEqual((double) tc.state[te::IDs::position], tc.getPosition().inSeconds())) {
            // because this callback could be called before tc.position is updated from state
            tc.position.forceUpdateOfCachedValue();
        }
        handlePlaybackScrolling();
        markAndUpdate(updatePos);
    }
    handleUpdateNowIfNeeded();
}

void ZoomViewState::handlePlaybackScrolling() {
    if (edit.getTransport().isPlaying() || edit.getTransport().isRecording()) {
        // TODO Proposed future change:
        // get position from audible time in playback context
        // because there can be latency in audio output

        // te::TimePosition pos;
        // auto pbc = edit.getCurrentPlaybackContext();
        // if (pbc != nullptr) {
        //     pos = pbc->getAudibleTimelineTime();
        //     DBG("ZoomViewState::handlePlaybackScrolling, "
        //         << " pos from context: " << pos
        //         << " edit pos: " << edit.getTransport().getPosition()
        //     );
        // } else {
        //     pos = edit.getTransport().getPosition();
        // }
        auto pos = edit.getTransport().getPosition();
        // DBG("ZoomViewState::handlePlaybackScrolling, pos: " << pos.inSeconds());
        auto range = getRange();
        auto leftRange = range.getLength() / 3.0;
        if (pos < viewX1 || pos > viewX1 + leftRange) {
            auto newX1 = jmax(te::TimePosition(), pos - leftRange);
            if (newX1 != viewX1) {
                setRange({ newX1, range.getLength() });
            }
        }
    } else {
        // assuming autojump to current position when stopped
        jumpToCurrentPosition();
    }
}

void ZoomViewState::handleAsyncUpdate() {
    // DBG("ZoomViewState::handleAsyncUpdate, pos : " << edit.getTransport().getPosition().inSeconds());
    if (compareAndReset(updatePos)) {
        // DBG("ZoomViewState::handleAsyncUpdate, calling zoomOrPosChanged");
        listeners.call(&Listener::zoomOrPosChanged);
    }
    if (compareAndReset(updateZoom)) {
        // DBG("ZoomViewState::handleAsyncUpdate, calling zoomChanged");
        listeners.call(&Listener::zoomChanged);
    }
}

//==============================================================================
// EditViewState

EditViewState::EditViewState(te::Edit& e, te::SelectionManager& s)
  : state(e.state.getOrCreateChildWithName(IDs::EDITVIEWSTATE, nullptr))
  , zoom(e, state)
  , selectionManager(s), edit(e)
{
    auto um = &edit.getUndoManager();
    showMasterTrack.referTo(state, IDs::showMasterTrack, um, false);
    showGlobalTrack.referTo(state, IDs::showGlobalTrack, um, false);
    showMarkerTrack.referTo(state, IDs::showMarkerTrack, um, false);
    showChordTrack.referTo(state, IDs::showChordTrack, um, false);
    showArrangerTrack.referTo(state, IDs::showArranger, um, false);
    drawWaveforms.referTo(state, IDs::drawWaveforms, um, true);
    showHeaders.referTo(state, IDs::showHeaders, um, true);
    showFooters.referTo(state, IDs::showFooters, um, true);
    showMidiDevices.referTo(state, IDs::showMidiDevices, um, true);
    showWaveDevices.referTo(state, IDs::showWaveDevices, um, true);
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
    const double fps = Helpers::getEditTimecodeFormat(edit).getFPS();
    return fps * getBeatLengthFor(bpm).inSeconds();
}

double EditViewState::getCurrentFramesPerBeat() const {
    const auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    return getFramesPerBeatFor(ts.getBpm());
}

// for note lengths: whole (divider=1), half (divider=2), quarter (divider=4), eighth (divider=8), etc.
double EditViewState::getFramesPerNote(size_t divider) const {
    // Quarter note is always one beat
    jassert(divider > 0);
    return getCurrentFramesPerBeat() / ((double) divider / 4.0);
}

void EditViewState::setBeatLength(te::TimeDuration beatLen) {
    jassert(beatLen > 0s);
    auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    auto bpm = 240.0 / (beatLen.inSeconds() * ts.getMatchingTimeSig().denominator);
    bpm = jlimit(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, bpm);
    ts.setBpm(bpm);
}

void EditViewState::setFramesPerBeat(int fpb) {
    const double fps = Helpers::getEditTimecodeFormat(edit).getFPS();
    auto targetBeatLen = te::TimeDuration::fromSeconds((double)fpb / fps);
    setBeatLength(targetBeatLen);
}

double EditViewState::getBpmSnappedToFps(double bpm) const {
    int fpb = roundToInt(getFramesPerBeatFor(bpm));
    const double fps = Helpers::getEditTimecodeFormat(edit).getFPS();
    auto targetBeatLen = te::TimeDuration::fromSeconds((double)fpb / fps);
    return getBpmForBeatLength(targetBeatLen);
}

double EditViewState::setBpmSnappedToFps(double bpm) {
    auto snappedBpm = getBpmSnappedToFps(bpm);
    auto& ts = edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
    ts.setBpm(snappedBpm);
    return snappedBpm;
}



//==============================================================================
// TrackViewState

TrackViewState::TrackViewState(ValueTree trackState, UndoManager* undoManager)
  : state(ensure(trackState)), height(state, IDs::height, undoManager, 160)
{
    constrainer.setMinimumHeight(56);
    constrainer.setMaximumHeight(600);
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