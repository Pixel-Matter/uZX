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
    viewStartTime.referTo(state, IDs::viewStartTime, um, 0s);
    viewTimePerPixel.referTo(state, IDs::viewTimePerPixel, um, 60s / (double) viewWidthPx.load());

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

inline int ZoomViewState::getViewWidthPx() const noexcept {
    return viewWidthPx.load();
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
    auto pos = edit.getTransport().getPosition();
    return jumpToPosition(pos);
}

void ZoomViewState::zoomHorizontally(double factor) {
    double scaleFactor = std::pow(2.0, -factor * 5.0);
    auto pos = edit.getTransport().getPosition();
    auto span = getViewSpan();
    auto newRange = span * scaleFactor;

    if (auto newTimePerPixel = newRange / getViewWidthPx(); newTimePerPixel > 0.0005s && newTimePerPixel < 2s) {
        setRange({
            jmax(te::TimePosition(), pos - newRange / 2.0),
            newRange
        });
        handlePlaybackScrolling();
        markAndUpdate(updateZoom);
    }
    handleUpdateNowIfNeeded();
}

void ZoomViewState::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree == state && isZoomProperty(prop)) {
        markAndUpdate(updateZoom);
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
        if (pos < viewStartTime || pos > viewStartTime + leftRange) {
            auto newX1 = jmax(te::TimePosition(), pos - leftRange);
            if (newX1 != viewStartTime) {
                setStart(newX1);
            }
        }
    } else {
        // assuming autojump to current position when stopped
        jumpToCurrentPosition();
    }
}

void ZoomViewState::handleAsyncUpdate() {
    if (compareAndReset(updatePos)) {
        listeners.call(&Listener::zoomOrPosChanged);
    }
    if (compareAndReset(updateZoom)) {
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