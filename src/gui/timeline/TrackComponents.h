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
                             private ValueTree::Listener {
public:
    TrackHeaderComponent(EditViewState&, te::Track::Ptr);
    ~TrackHeaderComponent() override;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void resized() override;

private:
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

    EditViewState& editViewState;
    te::Track::Ptr track;

    ValueTree inputsState;
    Label trackName;
    TextButton armButton {"A"}, muteButton {"M"}, soloButton {"S"}, inputButton {"I"};
};

//==============================================================================
class TrackFooterComponent : public Component,
                             private FlaggedAsyncUpdater,
                             private ValueTree::Listener {
public:
    TrackFooterComponent(EditViewState&, te::Track::Ptr);
    ~TrackFooterComponent() override;

    void paint(Graphics&) override;
    void mouseDown(const MouseEvent&) override;
    void resized() override;

private:
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void valueTreeChildOrderChanged(ValueTree&, int, int) override;

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
                       private ValueTree::Listener,
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

    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void valueTreeChildOrderChanged(ValueTree&, int, int) override;

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
class TrackRowComponent : public Component,
                          private TrackViewState::Listener
{
public:
    TrackRowComponent(EditViewState&, te::Track::Ptr);
    ~TrackRowComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void resized() override;

    void trackViewStateChanged() override;

    int getTrackHeight() const noexcept;

    TrackHeaderComponent header;
    TrackComponent body;
    TrackFooterComponent footer;

private:
    EditViewState& editViewState;
    te::Track::Ptr track;
    TrackViewState viewState;
    ResizableEdgeComponent resizer;
};


class TrackHeaderOverlayComponent : public Component, private te::ValueTreeAllEventListener {
public:
    TrackHeaderOverlayComponent(EditViewState& evs);
    ~TrackHeaderOverlayComponent() override;
    void paint(Graphics& g) override;
    void resized() override;
    void valueTreeChanged() override {}
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;


private:
    EditViewState& editViewState;
    ComponentBoundsConstrainer constrainer;
    ResizableEdgeComponent resizer {this, &constrainer, ResizableEdgeComponent::Edge::rightEdge};
};


}  // namespace MoTool
