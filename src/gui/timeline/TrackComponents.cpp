/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#include "TrackComponents.h"
#include "PsgClipComponent.h"
#include "../common/Components.h"
#include "../common/LookAndFeel.h"

namespace te = tracktion;
using namespace std::literals;

namespace MoTool {

//==============================================================================
TrackHeaderComponent::TrackHeaderComponent(EditViewState& evs, te::Track::Ptr t)
    : editViewState(evs)
    , track(t)
{
    ::Helpers::addAndMakeVisible(*this, { &trackName, &armButton, &muteButton, &soloButton, &inputButton });

    armButton.setColour(TextButton::buttonOnColourId, Colours::red);
    muteButton.setColour(TextButton::buttonOnColourId, Colours::red);
    soloButton.setColour(TextButton::buttonOnColourId, Colours::green);

    trackName.setText(t->getName(), dontSendNotification);

    if (auto at = dynamic_cast<te::AudioTrack*>(track.get())) {
        inputButton.onClick = [this, at] {
            PopupMenu m;

            if (EngineHelpers::trackHasInput(*at)) {
                bool ticked = EngineHelpers::isInputMonitoringEnabled(*at);
                m.addItem(1000, "Input Monitoring", true, ticked);
                m.addSeparator();
            }

            if (editViewState.showWaveDevices) {
                int id = 1;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                    {
                        bool ticked = instance->getTargets().getFirst() == at->itemID;
                        m.addItem (id++, instance->getInputDevice().getName(), true, ticked);
                    }
                }
            }

            if (editViewState.showMidiDevices) {
                m.addSeparator();

                int id = 100;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice) {
                        bool ticked = instance->getTargets().getFirst() == at->itemID;
                        m.addItem (id++, instance->getInputDevice().getName(), true, ticked);
                    }
                }
            }

            int res = m.show();

            if (res == 1000) {
                EngineHelpers::enableInputMonitoring (*at, ! EngineHelpers::isInputMonitoringEnabled (*at));
            } else if (res >= 100) {
                int id = 100;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice) {
                        if (id == res)
                            [[ maybe_unused ]] auto result = instance->setTarget (at->itemID, true, &at->edit.getUndoManager(), 0);

                        id++;
                    }
                }
            } else if (res >= 1) {
                int id = 1;
                for (auto instance : at->edit.getAllInputDevices()) {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice) {
                        if (id == res)
                            [[ maybe_unused ]] auto result = instance->setTarget (at->itemID, true, &at->edit.getUndoManager(), 0);

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

    track->state.addListener (this);
    inputsState = track->edit.state.getChildWithName (te::IDs::INPUTDEVICES);
    inputsState.addListener (this);

    valueTreePropertyChanged (track->state, te::IDs::mute);
    valueTreePropertyChanged (track->state, te::IDs::solo);
    valueTreePropertyChanged (inputsState, te::IDs::targetIndex);
}

TrackHeaderComponent::~TrackHeaderComponent() {
    track->state.removeListener(this);
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

void TrackHeaderComponent::paint(Graphics& g) {
    g.setColour(Colors::Theme::backgroundAlt);
    g.fillRect(getLocalBounds().withTrimmedRight(2));

    if (editViewState.selectionManager.isSelected(track.get())) {
        g.setColour(Colors::Theme::primary);
        g.drawRect(getLocalBounds().withTrimmedRight(-4), 2);
    }
}

void TrackHeaderComponent::mouseDown (const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackHeaderComponent::resized() {
    auto r = getLocalBounds().reduced(4);
    trackName.setBounds(r.removeFromTop(20));
    int w = 24;
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
TrackFooterComponent::TrackFooterComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs)
    , track (t)
{
    addAndMakeVisible (addButton);

    buildPlugins();

    track->state.addListener(this);

    addButton.onClick = [this] {
        // TODO implement showMenuAndCreatePlugin in UIBehaviour
        // if (auto plugin = AppFunctions::getCurrentUIBehaviour().showMenuAndCreatePlugin (te::Plugin::Type::effectPlugins, track->edit)) {
        if (auto plugin = showMenuAndCreatePlugin(track->edit)) {
            track->pluginList.insertPlugin(plugin, 0, &editViewState.selectionManager);
        }
    };
}

TrackFooterComponent::~TrackFooterComponent() {
    track->state.removeListener(this);
}

void TrackFooterComponent::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) {
    if (c.hasType(te::IDs::PLUGIN))
        markAndUpdate(updatePlugins);
}

void TrackFooterComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (c.hasType(te::IDs::PLUGIN))
        markAndUpdate(updatePlugins);
}

void TrackFooterComponent::valueTreeChildOrderChanged(juce::ValueTree&, int, int) {
    markAndUpdate(updatePlugins);
}

void TrackFooterComponent::paint(Graphics& g) {
    g.setColour(Colors::Theme::backgroundAlt);
    g.fillRect(getLocalBounds().withTrimmedLeft(2));

    if (editViewState.selectionManager.isSelected (track.get())) {
        g.setColour(Colors::Theme::primary);
        g.drawRect(getLocalBounds().withTrimmedLeft(-4), 2);
    }
}

void TrackFooterComponent::mouseDown (const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackFooterComponent::resized() {
    auto r = getLocalBounds().reduced (4);
    const int cx = 21;

    addButton.setBounds(r.removeFromLeft (cx).withSizeKeepingCentre (cx, cx));
    r.removeFromLeft(6);

    for (auto p : plugins) {
        p->setBounds(r.removeFromLeft (cx).withSizeKeepingCentre (cx, cx));
        r.removeFromLeft (2);
    }
}

void TrackFooterComponent::handleAsyncUpdate() {
    if (compareAndReset(updatePlugins))
        buildPlugins();
}

void TrackFooterComponent::buildPlugins() {
    plugins.clear();

    for (auto plugin : track->pluginList) {
        auto p = new PluginComponent(editViewState, plugin);
        addAndMakeVisible(p);
        plugins.add(p);
    }
    resized();
}

//==============================================================================
TrackComponent::TrackComponent(EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs)
    , track (t)
{
    track->state.addListener(this);
    track->edit.getTransport().addChangeListener(this);
    editViewState.selectionManager.addChangeListener(this);

    setBufferedToImage(true);
    setRepaintsOnMouseActivity(true);
    markAndUpdate(updateClips);
}

TrackComponent::~TrackComponent() {
    track->state.removeListener(this);
    track->edit.getTransport().removeChangeListener(this);
    editViewState.selectionManager.removeChangeListener(this);
}

void TrackComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);

    if (editViewState.selectionManager.isSelected(track.get())) {
        g.setColour(Colors::Theme::primary);

        auto rc = getLocalBounds();
        if (editViewState.showHeaders) rc = rc.withTrimmedLeft(-4);
        if (editViewState.showFooters) rc = rc.withTrimmedRight(-4);

        g.drawRect(rc, 2);
    }

    // TODO draw a grid
}

void TrackComponent::mouseDown(const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackComponent::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &editViewState.selectionManager) {
        // TODO repaint only on its track or clips selection/deselection
        markAndUpdate(updateSelection);
    }
    markAndUpdate(updateRecordClips);
}

void TrackComponent::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) {
    if (te::Clip::isClipState(v)) {
        if (i == te::IDs::start || i == te::IDs::length) {
            markAndUpdate(updatePositions);
        }
    }
}

void TrackComponent::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) {
    if (te::Clip::isClipState(c))
        markAndUpdate(updateClips);
}

void TrackComponent::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) {
    if (te::Clip::isClipState(c))
        markAndUpdate(updateClips);
}

void TrackComponent::valueTreeChildOrderChanged(juce::ValueTree& v, int a, int b) {
    if (te::Clip::isClipState(v.getChild(a)))
        markAndUpdate(updatePositions);
    else if (te::Clip::isClipState(v.getChild(b)))
        markAndUpdate(updatePositions);
}

void TrackComponent::handleAsyncUpdate() {
    if (compareAndReset(updateClips))
        buildClips();
    if (compareAndReset(updatePositions))
        resized();
    if (compareAndReset(updateRecordClips))
        buildRecordClips();
    if (compareAndReset(updateSelection))
        repaint();
}

void TrackComponent::resized() {
    for (auto cc : clips) {
        auto& c = cc->getClip();
        auto pos = c.getPosition();
        int x1 = editViewState.zoom.timeToX(pos.getStart(), getWidth());
        int x2 = editViewState.zoom.timeToX(pos.getEnd(), getWidth());

        cc->setBounds(x1, 0, x2 - x1, getHeight());
    }
}

void TrackComponent::buildClips() {
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

void TrackComponent::buildRecordClips() {
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
TrackRowComponent::TrackRowComponent(EditViewState& evs, te::Track::Ptr t)
    : header (evs, t)
    , body (evs, t)
    , footer (evs, t)
    , editViewState (evs)
    , track (t)
    , viewState(t->state, &editViewState.edit.getUndoManager())
    , resizer(this, &viewState.getConstrainer(), ResizableEdgeComponent::Edge::bottomEdge)
{
    addAndMakeVisible(header);
    addAndMakeVisible(body);
    addAndMakeVisible(footer);
    addAndMakeVisible(resizer);

    viewState.addListener (this);
}

TrackRowComponent::~TrackRowComponent() {
    viewState.removeListener(this);
}

int TrackRowComponent::getTrackHeight() const noexcept {
    return viewState.getHeight();
}

void TrackRowComponent::trackViewStateChanged() {
    // triggerAsyncUpdate();
    resized();
}

void TrackRowComponent::mouseDown(const MouseEvent&) {
    editViewState.selectionManager.selectOnly(track.get());
}

void TrackRowComponent::resized() {
    viewState.setTrackHeight(getHeight());

    const int headerWidth = editViewState.showHeaders ? editViewState.headersWidth : 0;
    const int footerWidth = editViewState.showFooters ? 100 : 0;
    auto r = getLocalBounds();
    resizer.setBounds(r.removeFromBottom(2));

    header.setBounds(r.removeFromLeft(headerWidth));
    footer.setBounds(r.removeFromRight(footerWidth));
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
    setSize(editViewState.showHeaders ? editViewState.headersWidth : 0, getHeight());
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
        setSize(editViewState.showHeaders ? editViewState.headersWidth : 0, getHeight());
        resized();
    }
}


}  // namespace MoTool
