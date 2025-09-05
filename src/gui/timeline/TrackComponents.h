#pragma once

#include <JuceHeader.h>

#include "ClipComponents.h"
#include "PluginComponent.h"
#include "TimelineGrid.h"

#include "../../controllers/EditState.h"

#include <common/Utilities.h>  // from Tracktion, for FlaggedAsyncUpdater

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
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;

    EditViewState& editViewState;
    te::Track::Ptr track;

    ValueTree inputsState;
    Label trackName;
    TextButton armButton {"R"}, muteButton {"M"}, soloButton {"S"}, inputButton {"I"};
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
class TrackBodyComponent : public Component,
                       private ValueTree::Listener,
                       private FlaggedAsyncUpdater,
                       private ZoomViewState::Listener,
                       private ChangeListener {
public:
    TrackBodyComponent(EditViewState&, TimelineGrid& g, te::Track::Ptr);
    ~TrackBodyComponent() override;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void resized() override;

private:
    void changeListenerCallback(ChangeBroadcaster*) override;

    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void valueTreeChildOrderChanged(ValueTree&, int, int) override;
    void zoomChanged() override;

    void handleAsyncUpdate() override;

    void buildClips();
    void buildRecordClips();

    EditViewState& editViewState;
    te::Track::Ptr track;
    TimelineGrid& grid;

    OwnedArray<ClipComponent> clips;
    std::unique_ptr<RecordingClipComponent> recordingClip;

    bool updateClips = false, updatePositions = false, updateRecordClips = false, updateSelection = false, updateZoom = false;
};


//==============================================================================
// Its function is to only resize the track row as a whole
// and to sync track height with a view state
//==============================================================================
class TrackRowComponent : public Component,
                          private TrackViewState::Listener {
public:
    TrackRowComponent(EditViewState&, TimelineGrid& g, te::Track::Ptr);
    ~TrackRowComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void resized() override;

    void trackViewStateChanged() override;

    int getTrackHeight() const noexcept;

    TrackHeaderComponent header;
    TrackBodyComponent body;

private:
    EditViewState& editViewState;
    te::Track::Ptr track;
    TrackViewState trackViewState;
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

//==============================================================================
class TracksContainerComponent : public Component,
                                 private FlaggedAsyncUpdater,
                                 private ChangeListener,
                                 private ComponentListener,
                                 private ValueTree::Listener,
                                 private ZoomViewState::Listener
{
public:
    TracksContainerComponent(te::Edit& e, EditViewState& evs, TimelineGrid& g);

    ~TracksContainerComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void resized() override;
    void paint(Graphics& g) override;
    int getIdealHeight() const;

private:
    te::Edit& edit;
    EditViewState& editViewState;
    OwnedArray<TrackRowComponent> trackRows;
    TrackHeaderOverlayComponent trackHeaderOverlay {editViewState};
    bool updateTracks = false, updateZoom = false;
    Rectangle<int> gridRect;
    TimelineGrid& grid;

    void valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& i) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) override;
    void valueTreeChildOrderChanged(juce::ValueTree& v, int a, int b) override;

    void zoomChanged() override;
    void changeListenerCallback(ChangeBroadcaster*) override;
    void componentMovedOrResized(Component& /*component*/, bool /*wasMoved*/, bool /*wasResized*/) override;

    void handleAsyncUpdate() override;
    void buildTracks();
};


}  // namespace MoTool
