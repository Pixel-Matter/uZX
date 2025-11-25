#include "TrackComponents.h"
#include "PsgClipComponent.h"

#include "../common/LookAndFeel.h"
#include "../../controllers/App.h"

namespace te = tracktion;
using namespace std::literals;
using namespace juce;

namespace MoTool {

//==============================================================================
TrackHeaderComponent::TrackHeaderComponent(EditViewState& evs, te::Track::Ptr t)
    : editViewState(evs)
    , track(t)
{
    ::Helpers::addAndMakeVisible(*this, { &trackName, &armButton, &muteButton, &soloButton, &inputButton });

    armButton.setColour( TextButton::buttonOnColourId, Colors::Theme::muted);
    muteButton.setColour(TextButton::buttonOnColourId, Colors::Theme::muted);
    soloButton.setColour(TextButton::buttonOnColourId, Colors::Theme::soloed);

    // TODO move to L&f for Label font
    trackName.setFont(trackName.getFont().withPointHeight(12.0f).withExtraKerningFactor(0.03f));

    trackName.setText(t->getName(), dontSendNotification);
    trackName.setEditable(false, true, true);
    trackName.onTextChange = [this] {
        if (trackName.getText() != track->getName()) {
            track->setName(trackName.getText());
        }
    };
    trackName.setTooltip("Double-click to rename track");
    trackName.addMouseListener(this, false);

    // TODO move all the logic to TrackViewModel
    if (auto at = dynamic_cast<te::AudioTrack*>(track.get())) {
        inputButton.onClick = [this, at] {
            PopupMenu m;

            if (EngineHelpers::trackHasInput(*at)) {
                // Add monitor mode submenu
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getTargets().contains(at->itemID)) {
                        auto currentMode = instance->getInputDevice().getMonitorMode();
                        m.addItem(1000, "Monitoring off", true, currentMode == te::InputDevice::MonitorMode::off);
                        m.addItem(1001, "Monitoring auto", true, currentMode == te::InputDevice::MonitorMode::automatic);
                        m.addItem(1002, "Monitoring on", true, currentMode == te::InputDevice::MonitorMode::on);

                        m.addSeparator();
                        break;
                    }
                }
            }

            if (editViewState.showWaveDevices) {
                int id = 1;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice) {
                        bool ticked = instance->getTargets().contains(at->itemID);
                        m.addItem(id++, instance->getInputDevice().getName(), true, ticked);
                    }
                }
            }

            if (editViewState.showMidiDevices) {
                m.addSeparator();

                int id = 100;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().isMidi()) {
                        bool ticked = instance->getTargets().contains(at->itemID);
                        m.addItem(id++, instance->getInputDevice().getName(), true, ticked);
                    }
                }
            }

            int res = m.show();

            if (res >= 1000) {  // Handle monitor mode selection
                te::InputDevice::MonitorMode newMode;
                switch (res) {
                    case 1000: newMode = te::InputDevice::MonitorMode::off; break;
                    case 1001: newMode = te::InputDevice::MonitorMode::automatic; break;
                    case 1002: newMode = te::InputDevice::MonitorMode::on; break;
                    default: newMode = te::InputDevice::MonitorMode::automatic; break;
                }

                // Set monitor mode for all input devices assigned to this track
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getTargets().contains(at->itemID)) {
                        instance->getInputDevice().setMonitorMode(newMode);
                    }
                }
            } else if (res >= 100) {  // midi devices
                int id = 100;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().isMidi()) {
                        if (id == res) {
                            // Toggle: if already assigned, remove it; otherwise, set it
                            if (instance->getTargets().contains(at->itemID)) {
                                [[ maybe_unused ]] auto result = instance->removeTarget(at->itemID, &at->edit.getUndoManager());
                            } else {  // set it
                                // Remove any existing MIDI device assignments except all_midi_in
                                for (auto existingInstance : at->edit.getAllInputDevices()) {
                                    if (existingInstance->getInputDevice().isMidi() &&
                                        existingInstance->getTargets().contains(at->itemID) &&
                                        (
                                            existingInstance->getInputDevice().getDeviceID() == "all_midi_in" ||
                                            instance->getInputDevice().getDeviceID() == "all_midi_in"
                                        ))
                                    {
                                        [[ maybe_unused ]] auto result = existingInstance->removeTarget(at->itemID, &at->edit.getUndoManager());
                                    }
                                }
                                [[ maybe_unused ]] auto result = instance->setTarget(at->itemID, false, &at->edit.getUndoManager(), 0);
                            }
                        }
                        id++;
                    }
                }
            } else if (res >= 1) {  // wave devices
                int id = 1;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice) {
                        if (id == res) {
                            // Toggle: if already assigned, remove it; otherwise, set it
                            if (instance->getTargets().contains(at->itemID)) {
                                [[ maybe_unused ]] auto result = instance->removeTarget(at->itemID, &at->edit.getUndoManager());
                            } else {
                                [[ maybe_unused ]] auto result = instance->setTarget(at->itemID, false, &at->edit.getUndoManager(), 0);
                            }
                        }
                        id++;
                    }
                }
            }
        };
        armButton.onClick = [this, at] {
            EngineHelpers::armTrack(*at, ! EngineHelpers::isTrackArmed(*at));
            armButton.setToggleState(EngineHelpers::isTrackArmed(*at), dontSendNotification);
        };
        muteButton.onClick = [at] { at->setMute(!at->isMuted(false)); };
        soloButton.onClick = [at] { at->setSolo(!at->isSolo(false)); };

        armButton.setToggleState(EngineHelpers::isTrackArmed(*at), dontSendNotification);
    } else {
        armButton.setVisible(false);
        muteButton.setVisible(false);
        soloButton.setVisible(false);
    }

    track->state.addListener(this);
    inputsState = track->edit.state.getChildWithName(te::IDs::INPUTDEVICES);
    inputsState.addListener(this);

    valueTreePropertyChanged(track->state, te::IDs::mute);
    valueTreePropertyChanged(track->state, te::IDs::solo);
    valueTreePropertyChanged(inputsState, te::IDs::targetIndex);
}

TrackHeaderComponent::~TrackHeaderComponent() {
    track->state.removeListener(this);
    trackName.removeMouseListener(this);
}

void TrackHeaderComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (te::TrackList::isTrack(v)) {
        if (i == te::IDs::mute)
            muteButton.setToggleState((bool)v[i], dontSendNotification);
        else if (i == te::IDs::solo)
            soloButton.setToggleState((bool)v[i], dontSendNotification);
        else if (i == te::IDs::name)
            trackName.setText(v[i], dontSendNotification);
    } else if (v.hasType (te::IDs::INPUTDEVICES)
              || v.hasType (te::IDs::INPUTDEVICE)
              || v.hasType (te::IDs::INPUTDEVICEDESTINATION)) {
        if (auto at = dynamic_cast<te::AudioTrack*>(track.get())) {
            armButton.setEnabled (EngineHelpers::trackHasInput(*at));
            armButton.setToggleState (EngineHelpers::isTrackArmed(*at), dontSendNotification);
        }
    }
}

void TrackHeaderComponent::valueTreeChildAdded(ValueTree&, ValueTree& c) {
    if (c.hasType(te::IDs::INPUTDEVICEDESTINATION)) {
        if (auto at = dynamic_cast<te::AudioTrack*>(track.get())) {
            if (c.getProperty(te::IDs::targetID) == at->itemID) {
                armButton.setEnabled(EngineHelpers::trackHasInput(*at));
                armButton.setToggleState(EngineHelpers::isTrackArmed(*at), dontSendNotification);
            }
        }
    }
}

void TrackHeaderComponent::valueTreeChildRemoved(ValueTree&, ValueTree& c, int) {
    if (c.hasType(te::IDs::INPUTDEVICEDESTINATION)) {
        if (auto at = dynamic_cast<te::AudioTrack*>(track.get())) {
            if (c.getProperty(te::IDs::targetID) == at->itemID) {
                armButton.setEnabled(EngineHelpers::trackHasInput(*at));
                armButton.setToggleState(EngineHelpers::isTrackArmed(*at), dontSendNotification);
            }
        }
    }
}

void TrackHeaderComponent::paint(Graphics& g) {
    bool isSelected = editViewState.selectionManager.isSelected(track.get());
    auto bgColor = isSelected ? Colors::Theme::backgroundSel : Colors::Theme::backgroundAlt;

    g.setColour(bgColor);
    g.fillRect(getLocalBounds().withTrimmedRight(2));
}

void TrackHeaderComponent::mouseDown (const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackHeaderComponent::resized() {
    auto r = getLocalBounds().reduced(4, 0);
    trackName.setBounds(r.removeFromTop(8 + roundToInt(trackName.getFont().getHeight())));
    int w = 20;
    r.setHeight(w);
    inputButton.setBounds(r.removeFromLeft(w));
    r.removeFromLeft(2);
    armButton.setBounds(r.removeFromLeft(w));
    r.removeFromLeft(2);
    muteButton.setBounds(r.removeFromLeft(w));
    r.removeFromLeft(2);
    soloButton.setBounds(r.removeFromLeft(w));
    r.removeFromLeft(2);
}

//==============================================================================
// TrackBodyComponent
//==============================================================================

//==============================================================================
TrackBodyComponent::TrackBodyComponent(EditViewState& evs, TimelineGrid& g, te::Track::Ptr t)
    : editViewState(evs)
    , track(t)
    , grid(g)
{
    track->state.addListener(this);
    editViewState.state.addListener(this);
    editViewState.zoom.addListener(this);

    track->edit.getTransport().addChangeListener(this);
    editViewState.selectionManager.addChangeListener(this);

    setBufferedToImage(true);
    setRepaintsOnMouseActivity(true);
    markAndUpdate(updateClips);
}

TrackBodyComponent::~TrackBodyComponent() {
    track->state.removeListener(this);
    track->edit.getTransport().removeChangeListener(this);
    editViewState.selectionManager.removeChangeListener(this);
    editViewState.state.removeListener(this);
    editViewState.zoom.removeListener(this);
}

void TrackBodyComponent::paint(Graphics& g) {
    bool isSelected = editViewState.selectionManager.isSelected(track.get());
    auto bgColor = isSelected ? Colors::Theme::backgroundSel : Colors::Theme::backgroundAlt;

    g.fillAll(bgColor);

    auto ticks = grid.getTicks();
    MoToolApp::getApp().getLookAndFeel().drawTimelineGrid(g, getLocalBounds(), ticks);
}

void TrackBodyComponent::mouseDown(const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackBodyComponent::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &editViewState.selectionManager) {
        // TODO repaint only on its own track or clips selection/deselection
        markAndUpdate(updateSelection);
    }
    markAndUpdate(updateRecordClips);
}

void TrackBodyComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (te::Clip::isClipState(v)) {
        if (i == te::IDs::start || i == te::IDs::length) {
            markAndUpdate(updatePositions);
        }
    } else if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::drawWaveforms) {
            repaint();
        }
    }
}

void TrackBodyComponent::zoomChanged() {
    markAndUpdate(updateZoom);
}

void TrackBodyComponent::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) {
    if (te::Clip::isClipState(c))
        markAndUpdate(updateClips);
}

void TrackBodyComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (te::Clip::isClipState(c))
        markAndUpdate(updateClips);
}

void TrackBodyComponent::valueTreeChildOrderChanged(juce::ValueTree& v, int a, int b) {
    if (te::Clip::isClipState(v.getChild(a)))
        markAndUpdate(updatePositions);
    else if (te::Clip::isClipState(v.getChild(b)))
        markAndUpdate(updatePositions);
}

void TrackBodyComponent::handleAsyncUpdate() {
    if (compareAndReset(updateClips))
        buildClips();
    if (compareAndReset(updatePositions))
        resized();
    if (compareAndReset(updateZoom)) {
        resized();
        repaint();
    }
    if (compareAndReset(updateRecordClips))
        buildRecordClips();
    if (compareAndReset(updateSelection))
        repaint();
}

void TrackBodyComponent::resized() {
    for (auto cc : clips) {
        auto& c = cc->getClip();
        auto pos = c.getPosition();
        int x1 = roundToInt(editViewState.zoom.timeToX(pos.getStart()));
        int x2 = roundToInt(editViewState.zoom.timeToX(pos.getEnd()));

        cc->setBounds(x1, 0, x2 - x1, getHeight());
    }
}

void TrackBodyComponent::buildClips() {
    clips.clear();

    if (auto ct = dynamic_cast<te::ClipTrack*>(track.get())) {
        for (auto c : ct->getClips()) {
            ClipComponent* cc = nullptr;

            if (dynamic_cast<te::WaveAudioClip*>(c))
                cc = new AudioClipComponent(editViewState, c);
            else if (dynamic_cast<PsgClip*>(c))  // must go before MidiClip
                cc = new PsgClipComponent(editViewState, c);
            else if (dynamic_cast<te::MidiClip*>(c))
                cc = new MidiClipComponent(editViewState, c);
            else
                cc = new ClipComponent(editViewState, c);

            clips.add(cc);
            addAndMakeVisible(cc);
        }
    }

    resized();
}

void TrackBodyComponent::buildRecordClips() {
    bool needed = false;

    if (track->edit.getTransport().isRecording()) {
        for (auto in : track->edit.getAllInputDevices()) {
            if (in->isRecordingActive() && track->itemID == in->getTargets().getFirst()) {
                needed = true;
                break;
            }
        }
    }

    if (needed)
    {
        recordingClip = std::make_unique<RecordingClipComponent> (track, editViewState);
        addAndMakeVisible (*recordingClip);
    }
    else
    {
        recordingClip = nullptr;
    }
}

//==============================================================================
TrackRowComponent::TrackRowComponent(EditViewState& evs, TimelineGrid& g, te::Track::Ptr t)
    : header(evs, t)
    , body(evs, g, t)
    , editViewState(evs)
    , track(t)
    , trackViewState(t->state, &editViewState.edit.getUndoManager())
    , resizer(this, &trackViewState.getConstrainer(), ResizableEdgeComponent::Edge::bottomEdge)
{
    addAndMakeVisible(header);
    addAndMakeVisible(body);
    addAndMakeVisible(resizer);

    trackViewState.addListener(this);
}

TrackRowComponent::~TrackRowComponent() {
    trackViewState.removeListener(this);
}

int TrackRowComponent::getTrackHeight() const noexcept {
    return trackViewState.getHeight();
}

void TrackRowComponent::trackViewStateChanged() {
    resized();
}

void TrackRowComponent::mouseDown(const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackRowComponent::resized() {
    trackViewState.setTrackHeight(getHeight());

    const int headerWidth = editViewState.getTrackHeaderWidth();
    auto r = getLocalBounds();
    resizer.setBounds(r.removeFromBottom(2));

    header.setBounds(r.removeFromLeft(headerWidth));
    body.setBounds(r);

    // do not remove
    body.resized();
}


//==============================================================================
TrackHeaderOverlayComponent::TrackHeaderOverlayComponent(EditViewState& evs)
    : editViewState (evs)
{
    setInterceptsMouseClicks(false, true);

    editViewState.state.addListener(this);
    setSize(editViewState.getTrackHeaderWidth(), getHeight());
    constrainer.setMinimumWidth(110);
    constrainer.setMaximumWidth(300);
    addAndMakeVisible(resizer);
}

TrackHeaderOverlayComponent::~TrackHeaderOverlayComponent() {
    editViewState.state.removeListener(this);
}

void TrackHeaderOverlayComponent::paint(Graphics&) {
}

void TrackHeaderOverlayComponent::resized() {
    editViewState.headersWidth = getWidth();
    auto bounds = getLocalBounds();
    resizer.setBounds(bounds.removeFromRight(2));
}

void TrackHeaderOverlayComponent::valueTreePropertyChanged(juce::ValueTree& s, const juce::Identifier& i) {
    if (i == IDs::headersWidth && s.hasType(IDs::EDITVIEWSTATE)) {
        setSize(editViewState.getTrackHeaderWidth(), getHeight());
        resized();
    }
}


//==============================================================================
TracksContainerComponent::TracksContainerComponent(te::Edit& e, EditViewState& evs, TimelineGrid& g)
    : edit(e)
    , editViewState(evs)
    , grid(g)
{
    edit.state.addListener(this);
    editViewState.selectionManager.addChangeListener(this);
    editViewState.state.addListener(this);
    editViewState.zoom.addListener(this);

    addAndMakeVisible(trackHeaderOverlay);
    trackHeaderOverlay.setAlwaysOnTop(true);
    trackHeaderOverlay.addComponentListener(this);

    markAndUpdate(updateTracks);
}

TracksContainerComponent::~TracksContainerComponent() {
    editViewState.zoom.removeListener(this);
    editViewState.state.removeListener(this);
    editViewState.selectionManager.removeChangeListener(this);
    edit.state.removeListener(this);
}

void TracksContainerComponent::mouseDown(const MouseEvent& e) {
    editViewState.selectionManager.deselectAll();

    auto x = e.x - gridRect.getX();
    if (x >= 0 && x < gridRect.getWidth()) {
        auto pos = editViewState.zoom.xToTime(x);
        edit.getTransport().setPosition(pos);
    }
}

int TracksContainerComponent::getIdealHeight() const {
    return std::accumulate (trackRows.begin(), trackRows.end(), 0, [] (auto acc, auto& track) {
        return acc + track->getTrackHeight();
    });
}

void TracksContainerComponent::resized() {
    // also get called on updated zoom
    const int trackGap = 0;
    const auto headerWidth = trackHeaderOverlay.getWidth();
    auto r = getLocalBounds();
    setSize(r.getWidth(), getHeight());
    r = getLocalBounds();

    trackHeaderOverlay.setBounds(r.withWidth(headerWidth));

    int y = roundToInt(editViewState.zoom.getViewY());

    for (auto t : trackRows) {
        t->setBounds(0, y, getWidth(), t->getTrackHeight());
        // do not remove t->resized(); because it triggers repainting on zoom/scroll change when bounds not change
        t->resized();
        y += t->getTrackHeight() + trackGap;
    }

    gridRect = Rectangle<int>(
        headerWidth, y,
        getWidth() - headerWidth,
        getHeight() - y
    );
}

void TracksContainerComponent::paint(Graphics& g) {
    // g.setColour(Colors::Theme::backgroundAlt);
    // g.fillRect(gridSpace);
    auto ticks = grid.getTicks();
    MoToolApp::getApp().getLookAndFeel().drawTimelineGrid(g, gridRect, ticks);
}

void TracksContainerComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (v.hasType(IDs::EDITVIEWSTATE)) {
        if (i == IDs::showHeaders) {
            markAndUpdate(updateZoom);
        } else if (i == IDs::drawWaveforms) {
            // TODO move to track body?
            repaint();
        }
    }
}

void TracksContainerComponent::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) {
    if (te::TrackList::isTrack(c))
        markAndUpdate(updateTracks);
}

void TracksContainerComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (te::TrackList::isTrack(c))
        markAndUpdate(updateTracks);
}

void TracksContainerComponent::valueTreeChildOrderChanged(juce::ValueTree& v, int a, int b) {
    if (te::TrackList::isTrack(v.getChild(a)))
        markAndUpdate(updateTracks);
    else if (te::TrackList::isTrack(v.getChild(b)))
        markAndUpdate(updateTracks);
}

void TracksContainerComponent::zoomChanged() {
    markAndUpdate(updateZoom);
}

void TracksContainerComponent::changeListenerCallback(ChangeBroadcaster*) {
    // selectin changed
    repaint();
}

void TracksContainerComponent::componentMovedOrResized(Component& /*component*/, bool /*wasMoved*/, bool /*wasResized*/) {
    markAndUpdate(updateZoom);
}

void TracksContainerComponent::buildTracks() {
    for (auto tr : trackRows) {
        tr->removeComponentListener(this);
        removeChildComponent(tr);
    }
    trackRows.clear();

    for (auto t : getAllTracks(edit)) {
        TrackRowComponent* c = nullptr;
        bool show = true;
        if (t->isMasterTrack()) {
            show = editViewState.showMasterTrack;
        } else if (t->isTempoTrack()) {
            show = editViewState.showGlobalTrack;
        } else if (t->isMarkerTrack()) {
            show = editViewState.showMarkerTrack;
        } else if (t->isChordTrack()) {
            show = editViewState.showChordTrack;
        } else if (t->isArrangerTrack()) {
            show = editViewState.showArrangerTrack;
        }

        if (show) {
            c = new TrackRowComponent(editViewState, grid, t);
            trackRows.add(c);
            c->addComponentListener(this);
            addAndMakeVisible(c);
        }
    }
    markAndUpdate(updateZoom);
}

void TracksContainerComponent::handleAsyncUpdate() {
    if (compareAndReset(updateTracks)) {
        buildTracks();
    }
    if (compareAndReset(updateZoom)) {
        resized();
        repaint();
    }
}

}  // namespace MoTool
