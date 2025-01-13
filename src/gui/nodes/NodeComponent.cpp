#include "NodeComponent.h"

#include <memory>

#include "NodeComponent.h"

using namespace juce;

PinComponent::PinComponent(bool isInput)
    : IsInputPin_(isInput)
{
    setSize(Size_, Size_);
}

void PinComponent::paint(Graphics& g) {
    g.setColour(Colours::white);
    g.fillEllipse(getLocalBounds().toFloat());
    g.setColour(Colours::darkgrey);
    g.drawEllipse(getLocalBounds().toFloat(), 1.0f);
}

bool PinComponent::isInput() const { return IsInputPin_; }

void PinComponent::mouseDown(const MouseEvent& e) {
    if (auto* editor = findParentComponentOfClass<NodeEditorComponent>()) {
        auto editorEvent = e.getEventRelativeTo(editor);
        DBG("PinComponent::mouseDown: " << editorEvent.position.toString() << " editor: " << editor->getBounds().toString());
        editor->handlePinMouseDown(this, editorEvent);
    }
}

void PinComponent::mouseDrag(const MouseEvent& e) {
    if (auto* editor = findParentComponentOfClass<NodeEditorComponent>()) {
        auto editorEvent = e.getEventRelativeTo(editor);
        editor->handlePinMouseDrag(this, editorEvent);
    }
}

void PinComponent::mouseUp(const MouseEvent& e) {
    if (auto* editor = findParentComponentOfClass<NodeEditorComponent>()) {
        auto editorEvent = e.getEventRelativeTo(editor);
        editor->handlePinMouseUp(this, editorEvent);
    }
}

/**
    ConnectorComponent
 */
void ConnectorComponent::setStartAndEnd(PinComponent* source, PinComponent* dest) {
    if (SourcePin_ != source || DestPin_ != dest) {
        SourcePin_ = source;
        DestPin_ = dest;
        resizeToFit();
        repaint();
    }
}

void ConnectorComponent::paint(Graphics& g) {
    if (SourcePin_ == nullptr || DestPin_ == nullptr)
        return;

    auto start = getPosition(SourcePin_);
    auto end = getPosition(DestPin_);

    Path path;
    path.startNewSubPath(start);

    auto midX = (start.getX() + end.getX()) * 0.5f;
    path.cubicTo(midX, start.getY(),
                midX, end.getY(),
                end.getX(), end.getY());

    g.setColour(Colours::white);
    g.strokePath(path, PathStrokeType(2.0f));
}

Point<float> ConnectorComponent::getPosition(PinComponent* pin) {
    if (pin != nullptr) {
        auto pos = getLocalPoint(pin, pin->getBounds().getCentre().toFloat());
        return pos;
    }
    return {};
}

void ConnectorComponent::resizeToFit() {
    if (SourcePin_ != nullptr && DestPin_ != nullptr) {
        auto p1 = SourcePin_->getBounds().getCentre();
        auto p2 = DestPin_->getBounds().getCentre();
        auto newBounds = Rectangle<int>::leftTopRightBottom(
            jmin(p1.x, p2.x) - 50,
            jmin(p1.y, p2.y) - 50,
            jmax(p1.x, p2.x) + 50,
            jmax(p1.y, p2.y) + 50);

        setBounds(newBounds);
    }
}

NodeComponent::NodeComponent(const String& name, bool hasInput, bool hasOutput)
    : NodeName(name), HasInput(hasInput), HasOutput(hasOutput) {
    if (hasInput) {
        InputPin_ = std::make_unique<PinComponent>(true);
        auto bounds = getLocalBounds();
        InputPin_->setCentrePosition(bounds.getX(), bounds.getCentreY());
        addAndMakeVisible(InputPin_.get());
    }
    if (hasOutput) {
        OutputPin_ = std::make_unique<PinComponent>(false);
        auto bounds = getLocalBounds();
        OutputPin_->setCentrePosition(bounds.getRight(), bounds.getCentreY());
        addAndMakeVisible(OutputPin_.get());
    }
}

void NodeComponent::paint(Graphics& g) {
    Path p;
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    float cornerSize = 8.0f;
    p.addRoundedRectangle(bounds, cornerSize);

    g.setColour(Colours::darkgrey);
    g.fillPath(p);

    g.setColour(Colours::black);
    g.strokePath(p, PathStrokeType(1.0f));

    g.setColour(Colours::white);
    g.setFont(16.0f);
    g.drawText(NodeName, bounds.reduced(5), Justification::centred);
}

void NodeComponent::resized() {
    auto bounds = getLocalBounds();
    if (InputPin_)
        InputPin_->setBounds(bounds.getX(), bounds.getCentreY() - 8, 16, 16);
    if (OutputPin_)
        OutputPin_->setBounds(bounds.getRight() - 16, bounds.getCentreY() - 8, 16, 16);
}

void NodeComponent::mouseDown(const MouseEvent& e) {
    Dragger_.startDraggingComponent(this, e);
}

void NodeComponent::mouseDrag(const MouseEvent& e) {
    Dragger_.dragComponent(this, e, nullptr);
}

bool NodeComponent::isInterestedInDragSource(const SourceDetails& source) { return true; }
void NodeComponent::itemDragEnter(const SourceDetails&) {}
void NodeComponent::itemDragMove(const SourceDetails&) {}
void NodeComponent::itemDragExit(const SourceDetails&) {}
void NodeComponent::itemDropped(const SourceDetails&) {}

PinComponent* NodeComponent::getInputPin() { return InputPin_.get(); }
PinComponent* NodeComponent::getOutputPin() { return OutputPin_.get(); }

NodeEditorComponent::NodeEditorComponent() {
    setSize(800, 600);

    createNode("Source", false, true, { 100, 100 });
    createNode("Sink", true, false, { 300, 100 });

    addAndMakeVisible(AddNodeButton_);
    AddNodeButton_.setButtonText("Add Node");
    AddNodeButton_.onClick = [this]() { showNodeMenu(); };
}

void NodeEditorComponent::resized() {
    auto bounds = getLocalBounds();
    auto toolbarBounds = bounds.removeFromTop(40);
    AddNodeButton_.setBounds(toolbarBounds.reduced(5));
}

void NodeEditorComponent::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void NodeEditorComponent::mouseDown(const MouseEvent& e) {}

void NodeEditorComponent::mouseDrag(const MouseEvent& e) {
    if (DraggingConnector_ != nullptr) {
        auto pos = e.getPosition();
        auto* pin = findPinAt(pos);

        if (pin != nullptr && canConnect(DragSourcePin_, pin))
            DraggingConnector_->setStartAndEnd(DragSourcePin_, pin);
        else
            DraggingConnector_->setStartAndEnd(DragSourcePin_, nullptr);
    }
}

void NodeEditorComponent::mouseUp(const MouseEvent& e) {
    if (DraggingConnector_ != nullptr) {
        auto* pin = findPinAt(e.getPosition());
        if (pin != nullptr && canConnect(DragSourcePin_, pin))
            createConnection(DragSourcePin_, pin);

        DraggingConnector_ = nullptr;
        repaint();
    }
}

void NodeEditorComponent::createNode(const String& name, bool hasInput, bool hasOutput, Point<int> position) {
    auto node = std::make_unique<NodeComponent>(name, hasInput, hasOutput);
    node->setSize(120, 80);
    node->setTopLeftPosition(position);
    addAndMakeVisible(node.get());
    Nodes_.push_back(std::move(node));
}

void NodeEditorComponent::showNodeMenu() {
    PopupMenu menu;
    menu.addItem(1, "Source Node");
    menu.addItem(2, "Sink Node");
    menu.addItem(3, "Transform Node");

    menu.showMenuAsync(PopupMenu::Options(),
        [this](int result) {
            auto pos = getMouseXYRelative();
            std::unique_ptr<NodeComponent> node;
            if (result == 1) {
                createNode("Source", false, true, pos);
            } else if (result == 2) {
                createNode("Sink", true, false, pos);
            } else if (result == 3) {
                createNode("Transform", true, true, pos);
            } else {
                jassertfalse;
            }
        });
}

void NodeEditorComponent::beginConnectorDrag(PinComponent* pin) {
    DragSourcePin_ = pin;
    DraggingConnector_ = std::make_unique<ConnectorComponent>();
    addAndMakeVisible(DraggingConnector_.get());
}

void NodeEditorComponent::handlePinMouseDown(PinComponent* pin, const MouseEvent& e) {
    beginConnectorDrag(pin);
}

void NodeEditorComponent::handlePinMouseDrag(PinComponent* pin, const MouseEvent& e) {
   if (DraggingConnector_ != nullptr) {
       auto pos = e.getPosition();
       auto* targetPin = findPinAt(pos);

       if (targetPin != nullptr && canConnect(DragSourcePin_, targetPin)) {
           DraggingConnector_->setStartAndEnd(DragSourcePin_, targetPin);
       } else {
           DraggingConnector_->setStartAndEnd(DragSourcePin_, nullptr);
       }

       DraggingConnector_->repaint();
   }
}

void NodeEditorComponent::handlePinMouseUp(PinComponent* pin, const MouseEvent& e) {
    if (DraggingConnector_ != nullptr) {
        auto* targetPin = findPinAt(e.getPosition());
        if (targetPin != nullptr && canConnect(DragSourcePin_, targetPin)) {
            createConnection(DragSourcePin_, targetPin);
        }
        DraggingConnector_ = nullptr;
        repaint();
    }
}


bool NodeEditorComponent::canConnect(PinComponent* src, PinComponent* dst) {
    return src != nullptr && dst != nullptr && src->isInput() != dst->isInput();
}

PinComponent* NodeEditorComponent::findPinAt(Point<int> position) {
    for (auto* child : getChildren()) {
        auto* pin = dynamic_cast<PinComponent*>(child);
        if (pin != nullptr && pin->getBounds().contains(position))
            return pin;
    }
    return nullptr;
}

void NodeEditorComponent::createConnection(PinComponent* source, PinComponent* dest) {
    auto connection = std::make_unique<ConnectorComponent>();
    connection->setStartAndEnd(source, dest);
    addAndMakeVisible(connection.get());
    Connections_.push_back(std::move(connection));

    ActiveConnections_.push_back({ source, dest });
    repaint();
}
