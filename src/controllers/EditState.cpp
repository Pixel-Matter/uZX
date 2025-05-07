#include "EditState.h"

using namespace std::literals;

namespace MoTool {

// ZoomViewState

ZoomViewState::ZoomViewState(te::Edit& e, ValueTree& st)
  : edit(e), state(st)
{
    state = edit.state.getOrCreateChildWithName(IDs::ZOOMVIEWSTATE, nullptr);
    auto um = &edit.getUndoManager();
    viewX1.referTo(state, IDs::viewX1, um, 0s);
    viewX2.referTo(state, IDs::viewX2, um, 60s);
    viewY.referTo(state, IDs::viewY, um, 0);
    edit.getTransport().addListener(this);
    edit.getTransport().addChangeListener(this);
}

ZoomViewState::~ZoomViewState()
{
    edit.getTransport().removeChangeListener(this);
    edit.getTransport().removeListener(this);
}

void ZoomViewState::addListener(Listener* l) { listeners.add(l); }
void ZoomViewState::removeListener(Listener* l) { listeners.remove(l); }

te::TimeRange ZoomViewState::getRange() const
{
    return { viewX1, viewX2 };
}

void ZoomViewState::setRange(te::TimeRange range)
{
    viewX1 = range.getStart();
    viewX2 = range.getEnd();
    listeners.call(&Listener::zoomChanged);
}

void ZoomViewState::setStart(te::TimePosition start)
{
    setRange({ start, start + viewLength() });
}

te::TimePosition ZoomViewState::getRangeStart() const
{
    return viewX1;
}

te::TimePosition ZoomViewState::getRangeEnd() const
{
    return viewX2;
}

double ZoomViewState::getViewY() const
{
    return viewY;
}

te::TimeDuration ZoomViewState::viewLength() const
{
    return viewX2 - viewX1;
}

te::TimePosition ZoomViewState::beatToTime(te::BeatPosition b) const
{
    auto& ts = edit.tempoSequence;
    return ts.toTime(b);
}

int ZoomViewState::timeToX(te::TimePosition time, int width) const
{
    return roundToInt(((time - viewX1) * width) / viewLength());
}

te::TimePosition ZoomViewState::xToTime(int x, int width) const
{
    return toPosition(viewLength() * (double(x) / width)) + toDuration(viewX1.get());
}

float ZoomViewState::durationToPixels(te::TimeDuration duration, int width) const
{
    return (float)(duration * width / viewLength());
}

float ZoomViewState::pixelsPerBeat(te::TimeDuration beatDur, int width) const
{
    return durationToPixels(beatDur, width);
}

float ZoomViewState::pixelsPerBeat(double beatDur, int width) const
{
    return durationToPixels(te::TimeDuration::fromSeconds(beatDur), width);
}

bool ZoomViewState::scrollToPosition(te::TimePosition pos)
{
    if (pos < viewX1 || pos > viewX2)
    {
        auto range = viewLength();
        auto newViewX1 = jmax(te::TimePosition(), pos - range / 2.0);
        setStart(newViewX1);
        return true;
    }
    return false;
}

bool ZoomViewState::scrollToCurrentPosition()
{
    auto pos = edit.getTransport().getPosition();
    return scrollToPosition(pos);
}

void ZoomViewState::zoomHorizontally(double factor)
{
    double scaleFactor = std::pow(2.0, -factor * 5.0);
    auto pos = edit.getTransport().getPosition();
    auto range = viewLength();
    auto newHalfRange = range * scaleFactor / 2.0;
    if (newHalfRange > 0.5s && newHalfRange < 600s)
    {
        viewX1 = jmax(te::TimePosition(), pos - newHalfRange);
        viewX2 = viewX1 + newHalfRange * 2.0;
        listeners.call(&Listener::zoomChanged);
    }
}

void ZoomViewState::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == &edit.getTransport())
    {
        if (edit.getTransport().isPlaying() || edit.getTransport().isRecording())
            startTimerHz(30);
        else
            stopTimer();
    }
}

void ZoomViewState::timerCallback()
{
    handlePlaybackScrolling();
}

void ZoomViewState::handlePlaybackScrolling()
{
    if (edit.getTransport().isPlaying() || edit.getTransport().isRecording())
    {
        auto pos = edit.getTransport().getPosition();
        auto range = getRange();
        auto leftRange = range.getLength() / 3.0;
        if (pos < viewX1 || pos > viewX1 + leftRange)
        {
            auto newX1 = jmax(te::TimePosition(), pos - leftRange);
            if (newX1 != viewX1)
                setRange({ newX1, range.getLength() });
        }
    }
}


// EditViewState

EditViewState::EditViewState(te::Edit& e, te::SelectionManager& s)
  : state(e.state.getOrCreateChildWithName(IDs::EDITVIEWSTATE, nullptr)),
    zoom(e, state), selectionManager(s), edit(e)
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
    showMidiDevices.referTo(state, IDs::showMidiDevices, um, false);
    showWaveDevices.referTo(state, IDs::showWaveDevices, um, true);
    headersWidth.referTo(state, IDs::headersWidth, nullptr, 110);
}

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