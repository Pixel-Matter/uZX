// NodeComponent.h
#pragma once

#include <JuceHeader.h>
#include <memory>

class PinComponent : public juce::Component {
public:
    PinComponent(bool isInput);
    void paint(Graphics& g) override;
    bool isInput() const;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

private:
    bool IsInputPin_;
    int Size_ = 8;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinComponent)
};

class ConnectorComponent : public Component {
public:
    ConnectorComponent() {}
    void setStartAndEnd(PinComponent* source, PinComponent* dest);
    void paint(Graphics& g) override;

private:
    Point<float> getPosition(PinComponent* pin);
    void resizeToFit();

    PinComponent* SourcePin_ = nullptr;
    PinComponent* DestPin_ = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectorComponent)
};

class NodeComponent : public Component, public DragAndDropTarget {
public:
    NodeComponent(const String& name, bool hasInput, bool hasOutput);
    void paint(Graphics& g) override;
    void resized() override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;

    bool isInterestedInDragSource(const SourceDetails& source) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;

    PinComponent* getInputPin();
    PinComponent* getOutputPin();

private:
    String NodeName;

protected:
    bool HasInput = false;
    bool HasOutput = true;

private:
    std::unique_ptr<PinComponent> InputPin_;
    std::unique_ptr<PinComponent> OutputPin_;
    ComponentDragger Dragger_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeComponent)
};

class NodeEditorComponent : public Component {
public:
    NodeEditorComponent();
    void resized() override;
    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    void createNode(const String& name, bool hasInput, bool hasOutput, Point<int> position);
    void handlePinMouseDown(PinComponent* pin, const MouseEvent& e);
    void handlePinMouseDrag(PinComponent* pin, const MouseEvent& e);
    void handlePinMouseUp(PinComponent* pin, const MouseEvent& e);

private:
    void showNodeMenu();
    void beginConnectorDrag(PinComponent* pin);
    bool canConnect(PinComponent* src, PinComponent* dst);
    PinComponent* findPinAt(Point<int> position);
    void createConnection(PinComponent* source, PinComponent* dest);

    struct ConnectionInfo {
        PinComponent* source;
        PinComponent* dest;
    };

    std::unique_ptr<ConnectorComponent> DraggingConnector_;
    PinComponent* DragSourcePin_ = nullptr;
    std::vector<std::unique_ptr<NodeComponent>> Nodes_;
    std::vector<std::unique_ptr<ConnectorComponent>> Connections_;
    std::vector<ConnectionInfo> ActiveConnections_;
    TextButton AddNodeButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeEditorComponent)
};
