#pragma once

#include <JuceHeader.h>

#include "ClipComponents.h"
#include "TimelineGrid.h"

#include "../../controllers/EditState.h"

#include <common/Utilities.h>  // from Tracktion, for FlaggedAsyncUpdater

namespace MoTool {

class TrackDevicesPanel;

//==============================================================================
/**
 * Component for track header containing track name and controls
 */
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
/**
 * Component for track body containing clips and timeline content
 */
class TrackBodyComponent : public Component,
                       private ValueTree::Listener,
                       private FlaggedAsyncUpdater,
                       private ZoomViewState::Listener,
                       private ChangeListener,
                       private TimelineGrid::Listener {
public:
    TrackBodyComponent(EditViewState&, TimelineGrid& g, te::Track::Ptr);
    ~TrackBodyComponent() override;

    void paint(Graphics& g) override;
    void paintOverChildren(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void resized() override;

private:
    void changeListenerCallback(ChangeBroadcaster*) override;

    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void valueTreeChildOrderChanged(ValueTree&, int, int) override;
    void viewRangeChanged() override;
    void gridChanged() override;

    void handleAsyncUpdate() override;

    void buildClips();
    void buildRecordClips();

    EditViewState& editViewState;
    te::Track::Ptr track;
    TimelineGrid& grid;

    OwnedArray<ClipComponent> clips;
    OwnedArray<ClipHeader> clipHeaders;
    std::unique_ptr<RecordingClipComponent> recordingClip;

    bool updateClips = false, updatePositions = false, updateRecordClips = false, updateSelection = false;
};


//==============================================================================
/**
 * Container component for track row, handling resizing and height synchronization
 * Its function is to only resize the track row as a whole
 * and to sync track height with a view state
 */
class TrackRowComponent : public Component,
                          private TrackViewState::Listener {
public:
    TrackRowComponent(EditViewState&, TimelineGrid& g, te::Track::Ptr);
    ~TrackRowComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void paint(Graphics& g) override;
    void resized() override;

    void trackViewStateChanged() override;

    int getTrackHeight() const noexcept;
    /** Height the row needs for its devices; 0 without them. */
    int getMinimumHeight() const;
    void setResizerVisible(bool visible);

    static constexpr int resizerHeight = 2;

    TrackHeaderComponent header;
    TrackBodyComponent body;
    /** The track's devices, fixed, when the view shows footers; in Viz mode its chip's scopes. */
    std::unique_ptr<Component> footer;
    /** Level with the clips, apart from them and from the window edge. */
    static inline const BorderSize<int> footerPadding {ClipComponent::ringWidth, 8, ClipComponent::ringWidth, 8};

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
/** The column at the right of the track rows when the view shows footers: the devices, or in
    Viz mode the scopes. Drag the gap at its left edge, before the devices or scopes, to resize it. */
class TrackFooterOverlayComponent : public Component, private te::ValueTreeAllEventListener {
public:
    TrackFooterOverlayComponent(EditViewState& evs);
    ~TrackFooterOverlayComponent() override;
    void paint(Graphics& g) override;
    void resized() override;
    void setMaximumWidth(int width) { constrainer.setMaximumWidth(width); }
    void valueTreeChanged() override {}
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

private:
    EditViewState& editViewState;
    ComponentBoundsConstrainer constrainer;
    ResizableEdgeComponent resizer {this, &constrainer, ResizableEdgeComponent::Edge::leftEdge};
};

//==============================================================================
/**
 * Container component that holds all track rows and manages track layout
 */
class TracksContainerComponent : public Component,
                                 private FlaggedAsyncUpdater,
                                 private ChangeListener,
                                 private ComponentListener,
                                 private ValueTree::Listener,
                                 private ZoomViewState::Listener,
                                 private TimelineGrid::Listener
{
public:
    TracksContainerComponent(te::Edit& e, EditViewState& evs, TimelineGrid& g);

    ~TracksContainerComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void resized() override;
    void paint(Graphics& g) override;
    int getIdealHeight() const;
    /** Height the rows need when they fit the view. */
    int getMinimumHeight() const;
    void setAutoFitTrackHeights(bool enabled);

    /** Shares `total` among rows in proportion to their current heights; a row that would get
        less than its minimum keeps the minimum and the others share the rest. */
    static Array<int> fitHeights(const Array<int>& current, const Array<int>& minimum, int total);

    std::function<void()> onRowsChanged;  // the rows were rebuilt, so their minimum height may differ

private:
    te::Edit& edit;
    EditViewState& editViewState;
    OwnedArray<TrackRowComponent> trackRows;
    TrackHeaderOverlayComponent trackHeaderOverlay {editViewState};
    TrackFooterOverlayComponent trackFooterOverlay {editViewState};
    bool updateTracks = false;
    bool needsRepaint = false;
    bool needsResize = false;
    bool autoFitTrackHeights_ = false;
    int lastFitHeight_ = 0;
    Rectangle<int> gridRect;
    TimelineGrid& grid;

    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& c) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& c, int) override;
    void valueTreeChildOrderChanged(juce::ValueTree& v, int a, int b) override;

    void viewRangeChanged() override;
    void gridChanged() override;
    void changeListenerCallback(ChangeBroadcaster*) override;
    void componentMovedOrResized(Component& /*component*/, bool /*wasMoved*/, bool /*wasResized*/) override;

    void handleAsyncUpdate() override;
    void buildTracks();
    /** The devices or scopes have a column of their own at the right, after a gap to drag. */
    bool hasFooterGap() const { return editViewState.showFooters && editViewState.showsNotes(); }
};


}  // namespace MoTool
