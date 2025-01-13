// NodeComponent.h
#pragma once

#include <JuceHeader.h>

#include <memory>

using namespace juce;

class NodeComponent : public Component, public DragAndDropTarget
{
public:
    NodeComponent(const String& name)
        : nodeName(name)
    {
        setSize(120, 80);
    }

    void paint(Graphics& g) override {
        g.setImageResamplingQuality(Graphics::ResamplingQuality::highResamplingQuality);

        // Use Path for smoother rendering
        {
            Path p;
            auto bounds = getLocalBounds().toFloat().reduced(0.5f);
            float cornerSize = 8.0f;
            p.addRoundedRectangle(bounds, cornerSize);

            // Fill
            g.setColour(Colours::darkgrey);
            g.fillPath(p);

            // Border
            g.setColour(Colours::black);
            g.strokePath(p, PathStrokeType(1.0f));

            // Text
            g.setColour(Colours::white);
            g.setFont(16.0f);
            g.drawText(nodeName, bounds.reduced(5), Justification::centred);
        }

        // Draw ports
        auto bounds = getLocalBounds();
        if (hasInput) {
            drawPort(g, bounds.getX(), bounds.getCentreY(), true);
        }
        if (hasOutput) {
            drawPort(g, bounds.getRight(), bounds.getCentreY(), false);
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        dragger.startDraggingComponent(this, e);
    }

    void mouseDrag(const MouseEvent& e) override
    {
        dragger.dragComponent(this, e, nullptr);
    }

    bool isInterestedInDragSource(const SourceDetails& source) override { return true; }
    void itemDragEnter(const SourceDetails&) override {}
    void itemDragMove(const SourceDetails&) override {}
    void itemDragExit(const SourceDetails&) override {}
    void itemDropped(const SourceDetails&) override {}

protected:
    bool hasInput = false;
    bool hasOutput = true;

private:
    void drawPort(Graphics& g, int x, int y, bool isInput) {
        float portSize = 8.0f;
        Rectangle<float> port(x - portSize / 2, y - portSize / 2, portSize, portSize);
        g.setColour(Colours::lightgrey);
        g.fillEllipse(port);
        g.setColour(Colours::white);
        g.drawEllipse(port, 1.0f);
    }

    String nodeName;
    ComponentDragger dragger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeComponent)
};

// Source node (no input, one output)
class SourceNodeComponent : public NodeComponent
{
public:
    SourceNodeComponent()
        : NodeComponent("Source")
    {
        hasInput = false;
        hasOutput = true;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SourceNodeComponent)
};


// Sink node (one input, no output)
class SinkNodeComponent : public NodeComponent
{
public:
    SinkNodeComponent()
        : NodeComponent("Sink")
    {
        hasInput = true;
        hasOutput = false;
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SinkNodeComponent)
};


class NodeEditorComponent : public Component {
public:
    NodeEditorComponent() {
        setSize(800, 600);
        setWantsKeyboardFocus(true);

        auto source = std::make_unique<SourceNodeComponent>();
        auto sink = std::make_unique<SinkNodeComponent>();

        addAndMakeVisible(source.get());
        addAndMakeVisible(sink.get());

        source->setTopLeftPosition(100, 100);
        sink->setTopLeftPosition(300, 100);

        nodes.push_back(std::move(source));
        nodes.push_back(std::move(sink));

    }



    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

private:
    std::vector<std::unique_ptr<NodeComponent>> nodes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeEditorComponent)
};
