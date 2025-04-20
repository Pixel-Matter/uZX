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

#include "ClipComponents.h"
#include "../common/EditState.h"
#include "../common/Components.h"

namespace MoTool {

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

    bool updateClips = false, updatePositions = false, updateRecordClips = false, updateSelection = false;
};

//==============================================================================
// Its function is to only resize the track row as a whole
// and to sync track height with a view state
//==============================================================================
class TrackRowComponent : public Component {
public:
    TrackRowComponent(EditViewState&, te::Track::Ptr);
    ~TrackRowComponent() override;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void resized() override;

    TrackHeaderComponent header;
    TrackComponent body;
    TrackFooterComponent footer;

private:
    //  TODO trackViewState instead?
    EditViewState& editViewState;
    te::Track::Ptr track;
};


}  // namespace MoTool
