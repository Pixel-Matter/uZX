/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

#include "EditState.h"

namespace MoTool {

//==============================================================================
class ClipComponent : public Component {
public:
    ClipComponent(EditViewState&, te::Clip::Ptr);

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;

    te::Clip& getClip() { return *clip; }

protected:
    EditViewState& editViewState;
    te::Clip::Ptr clip;
};

//==============================================================================
class AudioClipComponent : public ClipComponent {
public:
    AudioClipComponent(EditViewState&, te::Clip::Ptr);

    te::WaveAudioClip* getWaveAudioClip() {
        return dynamic_cast<te::WaveAudioClip*>(clip.get());
    }

    void paint(Graphics& g) override;

private:
    void updateThumbnail();
    void drawWaveform(Graphics& g, te::AudioClipBase& c, te::SmartThumbnail& thumb, Colour colour);
    void drawChannels(Graphics& g, te::SmartThumbnail& thumb, Rectangle<int> area,
                      te::TimeRange time, bool useLeft, bool useRight,
                      float leftGain, float rightGain);

    std::unique_ptr<te::SmartThumbnail> thumbnail;
};

//==============================================================================
class MidiClipComponent : public ClipComponent {
public:
    MidiClipComponent(EditViewState&, te::Clip::Ptr);

    te::MidiClip* getMidiClip() {
        return dynamic_cast<te::MidiClip*>(clip.get());
    }

    void paint(Graphics& g) override;
};

//==============================================================================
class RecordingClipComponent : public Component,
                               private Timer {
public:
    RecordingClipComponent (te::Track::Ptr t, EditViewState&);

    void paint(Graphics& g) override;

private:
    void timerCallback() override;
    void updatePosition();
    void initialiseThumbnailAndPunchTime();
    void drawThumbnail(Graphics& g, Colour waveformColour) const;
    bool getBoundsAndTime(juce::Rectangle<int>& bounds, tracktion::TimeRange& times) const;

    te::Track::Ptr track;
    EditViewState& editViewState;

    te::RecordingThumbnailManager::Thumbnail::Ptr thumbnail;
    te::TimePosition punchInTime = -1.0s;
};

//==============================================================================
class TrackHeaderComponent : public Component,
                             private te::ValueTreeAllEventListener {
public:
    TrackHeaderComponent(EditViewState&, te::Track::Ptr);
    ~TrackHeaderComponent() override;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void resized() override;

private:
    void valueTreeChanged() override {}
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    EditViewState& editViewState;
    te::Track::Ptr track;

    ValueTree inputsState;
    Label trackName;
    TextButton armButton {"A"}, muteButton {"M"}, soloButton {"S"}, inputButton {"I"};
};

//==============================================================================
class PluginComponent : public TextButton {
public:
    PluginComponent (EditViewState&, te::Plugin::Ptr);
    ~PluginComponent() override;

    using TextButton::clicked;
    void clicked(const ModifierKeys& modifiers) override;

private:
    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
};

//==============================================================================
class TrackFooterComponent : public Component,
                             private FlaggedAsyncUpdater,
                             private te::ValueTreeAllEventListener {
public:
    TrackFooterComponent(EditViewState&, te::Track::Ptr);
    ~TrackFooterComponent() override;

    void paint(Graphics&) override;
    void mouseDown(const MouseEvent&) override;
    void resized() override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override;

    void handleAsyncUpdate() override;

    void buildPlugins();

    EditViewState& editViewState;
    te::Track::Ptr track;

    TextButton addButton {"+"};
    OwnedArray<PluginComponent> plugins;

    bool updatePlugins = false;
};

//==============================================================================
class TrackComponent : public Component,
                       private te::ValueTreeAllEventListener,
                       private FlaggedAsyncUpdater,
                       private ChangeListener {
public:
    TrackComponent(EditViewState&, te::Track::Ptr);
    ~TrackComponent() override;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void resized() override;

private:
    void changeListenerCallback(ChangeBroadcaster*) override;

    void valueTreeChanged() override {}

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override;

    void handleAsyncUpdate() override;

    void buildClips();
    void buildRecordClips();

    EditViewState& editViewState;
    te::Track::Ptr track;

    OwnedArray<ClipComponent> clips;
    std::unique_ptr<RecordingClipComponent> recordingClip;

    bool updateClips = false, updatePositions = false, updateRecordClips = false;
};

//==============================================================================
class PlayheadComponent : public Component,
                          private Timer {
public:
    PlayheadComponent(te::Edit&, EditViewState&);

    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseDrag(const MouseEvent&) override;
    void mouseDown(const MouseEvent&) override;
    void mouseUp(const MouseEvent&) override;

private:
    void timerCallback() override;

    te::Edit& edit;
    EditViewState& editViewState;

    int xPosition = 0;
    bool firstTimer = true;
};

}  // namespace MoTool
