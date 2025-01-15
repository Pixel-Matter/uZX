#pragma once

#include <JuceHeader.h>
#include "../../nodes/Graph.h"

class MainHostWindow;

//==============================================================================
/**
    A panel that displays and edits a PluginGraph.
*/
class GraphEditorPanel final : public Component,
                               public ChangeListener,
                               private Timer
{
public:
    //==============================================================================
    GraphEditorPanel (MoTool::Nodes::Graph& graph);
    ~GraphEditorPanel() override;

    void createNewNode(const MoTool::Nodes::NodeType& nodeType, Point<int> position);

    void paint (Graphics&) override;
    void resized() override;

    void mouseDown (const MouseEvent&) override;
    void mouseUp   (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;

    void changeListenerCallback (ChangeBroadcaster*) override;

    //==============================================================================
    void updateComponents();

    //==============================================================================
    void showPopupMenu (Point<int> position);

    //==============================================================================
    void beginConnectorDrag (const std::shared_ptr<MoTool::Nodes::Pin> source,
                             const std::shared_ptr<MoTool::Nodes::Pin> dest,
                             const MouseEvent&);
    void dragConnector (const MouseEvent&);
    void endDraggingConnector (const MouseEvent&);

    //==============================================================================
    MoTool::Nodes::Graph& graph;

private:
    struct NodeComponent;
    struct ConnectorComponent;
    struct PinComponent;

    OwnedArray<NodeComponent> nodes;
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<PopupMenu> menu;

    NodeComponent* getComponentForNode(MoTool::Nodes::Node* node) const;
    ConnectorComponent* getComponentForConnection(const MoTool::Nodes::Connection* conn) const;
    PinComponent* findPinAt (Point<float>) const;

    //==============================================================================
    Point<int> originalTouchPos;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};


//==============================================================================
/**
    A panel that embeds a GraphEditorPanel with a midi keyboard at the bottom.

    It also manages the graph itself, and plays it.
*/
class GraphDocumentComponent final : public Component,
                                     public DragAndDropTarget,
                                     public DragAndDropContainer,
                                     private ChangeListener
{
public:
    GraphDocumentComponent();

    ~GraphDocumentComponent() override;

    //==============================================================================
    void createNewNode (const MoTool::Nodes::NodeType&, Point<int> position);
    void setDoublePrecision (bool doublePrecision);
    // bool closeAnyOpenPluginWindows();

    //==============================================================================
    std::unique_ptr<MoTool::Nodes::Graph> graph;

    void resized() override;
    void releaseGraph();

    //==============================================================================
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    //==============================================================================
    std::unique_ptr<GraphEditorPanel> graphPanel;
    // std::unique_ptr<MidiKeyboardComponent> keyboardComp;

    //==============================================================================
    void showSidePanel (bool isSettingsPanel);
    void hideLastSidePanel();

    BurgerMenuComponent burgerMenu;

private:
    //==============================================================================

    struct TooltipBar;
    // std::unique_ptr<TooltipBar> statusBar;

    class TitleBarComponent;
    // std::unique_ptr<TitleBarComponent> titleBarComponent;

    //==============================================================================
    // struct PluginListBoxModel;
    // std::unique_ptr<PluginListBoxModel> pluginListBoxModel;

    // ListBox pluginListBox;

    // SidePanel mobileSettingsSidePanel { "Settings", 300, true };
    // SidePanel pluginListSidePanel    { "Plugins", 250, false };
    // SidePanel* lastOpenedSidePanel = nullptr;

    //==============================================================================
    void changeListenerCallback (ChangeBroadcaster*) override;

    void init();
    void checkAvailableWidth();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphDocumentComponent)
};
