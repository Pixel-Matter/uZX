#include <JuceHeader.h>
#include <memory>

#include "GraphEditorPanel.h"
#include "../../nodes/Graph.h"
#include "juce_core/system/juce_PlatformDefs.h"

using namespace juce;
using namespace MoTool::Nodes;


//==============================================================================
struct GraphEditorPanel::PinComponent final : public Component,
                                              public SettableTooltipClient
{
    PinComponent(GraphEditorPanel& p, std::shared_ptr<Pin> pinToUse)
        : panel(p)
        , graph(p.graph)
        , pin(std::move(pinToUse))
        , isInput (pin->isInput())
    {
        setPinTooltip();
        setSize(16, 16);
    }

    void setPinTooltip() {
        String tip = isInput ? "Main Input: " : "Main Output: ";
        setTooltip(tip + pin->getName());
    }

    void paint(Graphics& g) override {
        auto w = (float) getWidth();
        auto h = (float) getHeight();

        Path p;
        p.addEllipse(0, 0, w, h);
        // p.addEllipse(w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
        // p.addRectangle(w * 0.4f, isInput ? (0.5f * h) : 0.0f, w * 0.2f, h * 0.5f);

        auto colour = Colours::green;
        // auto colour = (pin.isMIDI() ? Colours::red : Colours::green);
        // g.setColour(colour.withRotatedHue ((float) busIdx / 5.0f));

        g.setColour(colour);
        g.fillPath(p);

        // put text in the centre of circle
        g.setColour(Colours::white);
        g.setFont(12.0f);
        g.drawText(pin->getName(), getLocalBounds(), Justification::centred);
    }

    void mouseDown(const MouseEvent& e) override {
        DBG("PinComponent::mouseDown");
        // TODO: make it work right
        // auto dummy = std::shared_ptr<Pin>{}; // empty pin
        // panel.beginConnectorDrag(isInput ? dummy : pin,
        //                          isInput ? pin : dummy,
        //                          e);
    }

    void mouseDrag(const MouseEvent& e) override {
        panel.dragConnector(e);
    }

    void mouseUp(const MouseEvent& e) override {
        panel.endDraggingConnector(e);
    }

    GraphEditorPanel& panel;
    Graph& graph;
    std::shared_ptr<Pin> pin;
    const bool isInput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinComponent)
};

//==============================================================================
struct GraphEditorPanel::NodeComponent final : public Component,
                                            //    public Timer,
                                               private AudioProcessorParameter::Listener,
                                               private AsyncUpdater {
    NodeComponent(GraphEditorPanel& p, std::shared_ptr<Node> node_)
        : panel(p)
        , graph(p.graph)
        , node(std::move(node_))
    {
        shadow.setShadowProperties(DropShadow(Colours::black.withAlpha(0.5f), 3, { 0, 1 }));
        setComponentEffect(&shadow);
        setSize(150, 60);
    }

    NodeComponent(const NodeComponent&) = delete;
    NodeComponent& operator=(const NodeComponent&) = delete;

    ~NodeComponent() override {}

    void mouseDown(const MouseEvent& e) override {
        originalPos = localPointToGlobal(juce::Point<int>());

        toFront(true);

        if (e.mods.isPopupMenu())
            showPopupMenu();
    }

    void mouseDrag(const MouseEvent& e) override {
        if (!e.mods.isPopupMenu()) {
            auto pos = originalPos + e.getOffsetFromDragStart();

            if (getParentComponent() != nullptr)
                pos = getParentComponent()->getLocalPoint (nullptr, pos);

            pos += getLocalBounds().getCentre();

            graph.setNodePosition(node.get(),
                                  { pos.x / (double) getParentWidth(),
                                    pos.y / (double) getParentHeight() });

            panel.updateComponents();
        }
    }

    void mouseUp(const MouseEvent& e) override {
        if (e.mouseWasDraggedSinceMouseDown()) {
            // graph.setChangedFlag(true);
        } else if (e.getNumberOfClicks() == 2) {
            // Open node window
            // if (auto f = graph.graph.getNodeForId (pluginID))
            //     if (auto* w = graph.getOrCreateWindowFor (f, PluginWindow::Type::normal))
            //         w->toFront (true);
        }
    }

    bool hitTest(int x, int y) override {
        for (auto* child : getChildren())
            if (child->getBounds().contains(x, y))
                return true;

        return x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize;
    }

    void paint(Graphics& g) override {
        auto boxArea = getLocalBounds().reduced(4, pinSize);
        auto boxColour = findColour(TextEditor::backgroundColourId);

        g.setColour(boxColour);
        g.fillRect(boxArea.toFloat());

        g.setColour(findColour(TextEditor::textColourId));
        g.setFont(font);
        g.drawFittedText(getName(), boxArea, Justification::centred, 2);
    }

    void resized() override {
        int index = 0;
        auto totalSpacesIns = static_cast<float>(inputPins.size());
        for (auto* pin : inputPins) {
            auto indexPos = static_cast<float>(index);
            pin->setBounds(proportionOfWidth((1.0f + indexPos) / (totalSpacesIns + 1.0f)) - pinSize / 2, 0, pinSize, pinSize);
            ++index;
        }
        auto totalSpacesOuts = static_cast<float>(outputPins.size());
        index = 0;
        for (auto* pin : outputPins) {
            auto totalSpaces = static_cast<float>(totalSpacesOuts);
            auto indexPos = static_cast<float>(index);

            pin->setBounds(proportionOfWidth((1.0f + indexPos) / (totalSpaces + 1.0f)) - pinSize / 2,
                           getHeight() - pinSize,
                           pinSize, pinSize);
            ++index;
        }
    }

    PinComponent* getComponentForPin(const Pin& pin, bool isInput) const {
        auto& pins = isInput ? inputPins : outputPins;
        for (auto* p : pins)
            if (p->pin.get() == &pin)
                return p;

        return nullptr;
    }

    juce::Point<float> getPinPos(const Pin& pin, bool isInput) const {
        if (auto pinComp = getComponentForPin(pin, isInput)) {
            return getPosition().toFloat() + pinComp->getBounds().getCentre().toFloat();
        }
        return {};
    }

    juce::Point<float> getPinPos(const PinComponent& pin) const {
        return getPosition().toFloat() + pin.getBounds().getCentre().toFloat();
    }

    juce::Point<float> getPinPos(int index, bool isInput) const {
        auto* pin = isInput ? inputPins[index] : outputPins[index];
        return getPinPos(*pin);
    }

    void update() {
        int w = 100;
        int h = 60;

        int numIns = node->getNumInputs();
        int numOuts = node->getNumOutputs();
        w = jmax(w, (jmax(numIns, numOuts) + 1) * 20);

        const auto textWidth = GlyphArrangement::getStringWidthInt(font, node->getName());
        w = jmax(w, 16 + jmin(textWidth, 300));
        if (textWidth > 300) {
            h = 100;
        }

        setSize(w, h);
        setName(node->getName());

        {
            auto p = graph.getNodePosition(node.get());
            setCentreRelative((float) p.x, (float) p.y);
        }

        if (numIns != numInputs || numOuts != numOutputs) {
            numInputs = numIns;
            numOutputs = numOuts;

            inputPins.clear();
            outputPins.clear();

            for (int i = 0; i < node->getNumInputs(); ++i) {
                addAndMakeVisible(inputPins.add(new PinComponent(panel, node->getPin(true, i))));
            }

            for (int i = 0; i < node->getNumOutputs(); ++i)
                addAndMakeVisible(outputPins.add(new PinComponent(panel, node->getPin(false, i))));

            resized();
        }
    }

    void showPopupMenu() {
        menu.reset(new PopupMenu);
        menu->addItem("Delete this node", [this] {
            graph.removeNode(std::move(node).get());
            panel.updateComponents();
        });
        menu->addItem("Disconnect all pins", [this] {
            graph.disconnectNode(node.get());
            panel.updateComponents();
        });
        menu->showMenuAsync({});
    }

    void parameterValueChanged(int, float) override {
        // Parameter changes might come from the audio thread or elsewhere, but
        // we can only call repaint from the message thread.
        triggerAsyncUpdate();
    }

    void parameterGestureChanged(int, bool) override  {}

    void handleAsyncUpdate() override {
        repaint();
    }

    GraphEditorPanel& panel;
    Graph& graph;
    std::shared_ptr<Node> node;
    OwnedArray<PinComponent> inputPins;
    OwnedArray<PinComponent> outputPins;
    int numInputs = 0, numOutputs = 0;
    int pinSize = 16;
    juce::Point<int> originalPos;
    Font font = FontOptions { 13.0f, Font::bold };
    DropShadowEffect shadow;
    std::unique_ptr<PopupMenu> menu;
};


//==============================================================================
struct GraphEditorPanel::ConnectorComponent final : public Component,
                                                    public SettableTooltipClient
{
    explicit ConnectorComponent(GraphEditorPanel& p)
        : panel(p)
        , graph(p.graph)
    {
        // setAlwaysOnTop(true);
    }

    ConnectorComponent(GraphEditorPanel& p, const Connection& conn)
        : panel(p)
        , graph(p.graph)
        , connection(conn)
    {
        // setAlwaysOnTop(true);
    }

    void setInput(std::shared_ptr<Pin> newSource) {
        if (connection.Source.lock().get() != newSource.get()) {
            connection.Source = newSource;
            update();
        }
    }

    void setOutput(std::shared_ptr<Pin> newDest) {
        if (connection.Destination.lock().get() != newDest.get()) {
            connection.Destination = newDest;
            update();
        }
    }

    void dragStart(juce::Point<float> pos) {
        lastInputPos = pos;
        resizeToFit();
    }

    void dragEnd(juce::Point<float> pos) {
        lastOutputPos = pos;
        resizeToFit();
    }

    void update() {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        if (lastInputPos != p1 || lastOutputPos != p2)
            resizeToFit();
    }

    void resizeToFit() {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        auto newBounds = Rectangle<float>(p1, p2).expanded(4.0f).getSmallestIntegerContainer();

        if (newBounds != getBounds()) {
            setBounds(newBounds);
        } else {
            resized();
        }

        repaint();
    }

    void getPoints(juce::Point<float>& p1, juce::Point<float>& p2) const {
        p1 = lastInputPos;
        p2 = lastOutputPos;

        if (auto srcPin = connection.Source.lock())
            if (auto* srcNode = panel.getComponentForNode(srcPin->getOwner())) {
                p1 = srcNode->getPinPos(*srcPin.get(), false);
            }

        if (auto destPin = connection.Destination.lock())
            if (auto* destNode = panel.getComponentForNode(destPin->getOwner())) {
                p2 = destNode->getPinPos(*destPin.get(), true);
            }
    }

    void paint(Graphics& g) override {
        g.setColour(Colours::green);
        // fill rect
        g.fillPath(linePath);
    }

    bool hitTest(int x, int y) override {
        auto pos = juce::Point<int>(x, y).toFloat();

        if (hitPath.contains(pos)) {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (pos, distanceFromStart, distanceFromEnd);

            // avoid clicking the connector when over a pin
            return distanceFromStart > 7.0 && distanceFromEnd > 7.0;
        }

        return false;
    }

    void mouseDown(const MouseEvent&) override {
        dragging = false;
    }

    void mouseDrag(const MouseEvent& e) override {
        if (dragging) {
            panel.dragConnector(e);
        } else if (e.mouseWasDraggedSinceMouseDown()) {
            dragging = true;
            DBG("ConnectorComponent::mouseDrag");

            graph.removeConnection(connection);

            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds(getPosition().toFloat() + e.position, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);

            auto dummy = std::shared_ptr<Pin>{};
            panel.beginConnectorDrag(isNearerSource ? dummy : connection.Source.lock(),
                                     isNearerSource ? connection.Destination.lock() : dummy,
                                     e);
        }
    }

    void mouseUp(const MouseEvent& e) override {
        if (dragging)
            panel.endDraggingConnector (e);
    }

    void resized() override {
        juce::Point<float> p1, p2;
        getPoints(p1, p2);

        lastInputPos = p1;
        lastOutputPos = p2;

        p1 -= getPosition().toFloat();
        p2 -= getPosition().toFloat();

        linePath.clear();
        linePath.startNewSubPath(p1);
        linePath.cubicTo(p1.x, p1.y + (p2.y - p1.y) * 0.33f,
                         p2.x, p1.y + (p2.y - p1.y) * 0.66f,
                         p2.x, p2.y);

        PathStrokeType wideStroke(8.0f);
        wideStroke.createStrokedPath(hitPath, linePath);

        PathStrokeType stroke(2.5f);
        stroke.createStrokedPath(linePath, linePath);

        auto arrowW = 5.0f;
        auto arrowL = 4.0f;

        Path arrow;
        arrow.addTriangle(-arrowL, arrowW,
                          -arrowL, -arrowW,
                          arrowL, 0.0f);

        arrow.applyTransform(AffineTransform()
                             .rotated(MathConstants<float>::halfPi - (float) atan2(p2.x - p1.x, p2.y - p1.y))
                             .translated((p1 + p2) * 0.5f));

        linePath.addPath(arrow);
        linePath.setUsingNonZeroWinding(true);
    }

    void getDistancesFromEnds(juce::Point<float> p, double& distanceFromStart, double& distanceFromEnd) const {
        juce::Point<float> p1, p2;
        getPoints (p1, p2);

        distanceFromStart = p1.getDistanceFrom (p);
        distanceFromEnd   = p2.getDistanceFrom (p);
    }

    GraphEditorPanel& panel;
    Graph& graph;
    Connection connection = {};
    juce::Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
};


//==============================================================================
GraphEditorPanel::GraphEditorPanel(Graph& g)
    : graph (g)
{
    // graph.addChangeListener (this);
    setOpaque (true);
}

GraphEditorPanel::~GraphEditorPanel() {
    // graph.removeChangeListener (this);
    draggingConnector = nullptr;
    nodes.clear();
    connectors.clear();
}

void GraphEditorPanel::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void GraphEditorPanel::mouseDown(const MouseEvent& e) {
    if (e.mods.isPopupMenu())
        showPopupMenu(e.position.toInt());
}

void GraphEditorPanel::mouseUp(const MouseEvent&) {
}

void GraphEditorPanel::mouseDrag(const MouseEvent& e) {
}

void GraphEditorPanel::createNewNode(const NodeType& nodeType, juce::Point<int> position) {
    auto pos = position.toDouble() / juce::Point<double> ((double) getWidth(), (double) getHeight());
    graph.addNode(nodeType, {pos.x, pos.y});
}

GraphEditorPanel::NodeComponent* GraphEditorPanel::getComponentForNode(Node* nodePtr) const {
    if (nodePtr == nullptr)
        return nullptr;
    for (auto* node : nodes)
       if (node->node.get() == nodePtr)
            return node;

    return nullptr;
}

GraphEditorPanel::ConnectorComponent* GraphEditorPanel::getComponentForConnection(const Connection* conn) const {
    if (conn == nullptr)
        return nullptr;
    for (auto* cc : connectors)
        if (cc->connection == *conn)
            return cc;

    return nullptr;
}

GraphEditorPanel::PinComponent* GraphEditorPanel::findPinAt(juce::Point<float> pos) const {
    for (auto* fc : nodes) {
        // NB: A Visual Studio optimiser error means we have to put this Component* in a local
        // variable before trying to cast it, or it gets mysteriously optimised away..
        auto* comp = fc->getComponentAt(pos.toInt() - fc->getPosition());

        if (auto* pin = dynamic_cast<PinComponent*> (comp))
            return pin;
    }

    return nullptr;
}

void GraphEditorPanel::resized() {
    updateComponents();
}

void GraphEditorPanel::changeListenerCallback (ChangeBroadcaster*) {
    updateComponents();
}

void GraphEditorPanel::updateComponents() {
    if (!graph.checkGraph()) {
        DBG("GraphEditorPanel::updateComponents: Graph is invalid");
    }

    // 1. Check deleted nodes and connections
    for (int i = connectors.size(); --i >= 0;)
        if (!graph.findConnection(connectors.getUnchecked(i)->connection))
            connectors.remove(i);

    for (int i = nodes.size(); --i >= 0;)
        if (!graph.findNode(nodes[i]->node.get()))
            nodes.remove(i);

    // 2. Update nodes and connections
    for (auto* fc : nodes) {
        fc->update();
        fc->toFront(false);
    }

    for (auto* cc : connectors) {
        cc->update();
        cc->toBack();
    }

    // 3. Add new nodes and connections
    for (const auto& c : graph.getConnections()) {
        if (getComponentForConnection(c.get()) == nullptr) {
            auto* comp = connectors.add(new ConnectorComponent(*this, *c));
            addAndMakeVisible(comp);
            comp->update();
            comp->toBack();
        }
    }
    for (const auto& node : graph.getNodes()) {
        if (getComponentForNode(node.get()) == nullptr) {
            auto* comp = nodes.add(new NodeComponent(*this, node));
            addAndMakeVisible(comp);
            comp->update();
            comp->toFront(false);
        }
    }
}

void GraphEditorPanel::showPopupMenu(juce::Point<int> mousePos) {
    menu.reset(new PopupMenu);
    for (const auto& type : BuiltinNodeTypes) {
        menu->addItem(type.first, [this, type, mousePos] {
            createNewNode(type.second, mousePos);
            updateComponents();
        });
    }
    menu->showMenuAsync({});
}

void GraphEditorPanel::beginConnectorDrag(const std::shared_ptr<Pin> source,
                                          const std::shared_ptr<Pin> dest,
                                          const MouseEvent& e) {
    auto* c = dynamic_cast<ConnectorComponent*>(e.originalComponent);

    if (c != nullptr) {
        graph.removeConnection(c->connection);
        connectors.removeObject(c, false);
    }
    draggingConnector.reset(c);

    if (draggingConnector == nullptr) {
        draggingConnector.reset(new ConnectorComponent(*this));
    }

    draggingConnector->setInput(source);
    draggingConnector->setOutput(dest);

    addAndMakeVisible(draggingConnector.get());
    draggingConnector->toFront(false);

    DBG("Dragging connection " << draggingConnector->connection.toString());
    dragConnector(e);
}

void GraphEditorPanel::dragConnector(const MouseEvent& e) {
    auto e2 = e.getEventRelativeTo(this);

    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip({});

    auto pos = e2.position;
    auto connection = draggingConnector->connection;

    if (auto* pinComp = findPinAt(pos)){
        if (connection.Source.lock() == nullptr && !pinComp->isInput) {
            connection.Source = pinComp->pin;
            DBG("Connecting source to" << pinComp->pin->getName());
        } else if (connection.Destination.lock() == nullptr && pinComp->isInput) {
            connection.Destination = pinComp->pin;
            DBG("Connecting destination to " << pinComp->pin->getName());
        }
        DBG("Ponential connection " << connection.toString());

        if (graph.canConnect(connection)) {
            pos = (pinComp->getParentComponent()->getPosition() + pinComp->getBounds().getCentre()).toFloat();
            draggingConnector->setTooltip(pinComp->getTooltip());
        }
    }

    if (connection.Source.lock() == nullptr) {
        draggingConnector->dragStart(pos);
    } else {
        draggingConnector->dragEnd(pos);
    }
    DBG("Dragging connection " << draggingConnector->connection.toString());
}

void GraphEditorPanel::endDraggingConnector(const MouseEvent& e) {
    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip ({});

    auto e2 = e.getEventRelativeTo (this);
    auto connection = draggingConnector->connection;
    draggingConnector = nullptr;
    DBG("Dragging connection " << connection.toString());

    if (auto* pinComp = findPinAt(e2.position)) {
        DBG("Drag to pin found");
        DBG("Connection " << connection.toString());
        if (connection.Source.lock() == nullptr) {
            if (pinComp->isInput) {
                DBG("Source is empty but target pin is input, sorry");
                return;
            }
            connection.Source = pinComp->pin;
        } else {
            if (!pinComp->isInput) {
                DBG("Source is not empty but target pin is output, sorry");
                return;
            }
            connection.Destination = pinComp->pin;
        }
        DBG("Adding connection after drag to pin " << pinComp->pin->getName());
        DBG("Connection " << connection.toString());
        auto added = graph.addConnection(connection);
        if (!added) {
            DBG("Connection was not added, sorry");
        }
        updateComponents();
    }
}

void GraphEditorPanel::timerCallback() {
    // this should only be called on touch devices
    // jassert (isOnTouchDevice());

    // stopTimer();
    // showPopupMenu (originalTouchPos);
}

//==============================================================================
struct GraphDocumentComponent::TooltipBar final : public Component, private Timer {
    TooltipBar() {
        startTimer (100);
    }

    void paint (Graphics& g) override {
        g.setFont (FontOptions ((float) getHeight() * 0.7f, Font::bold));
        g.setColour (Colours::black);
        g.drawFittedText (tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
    }

    void timerCallback() override {
        String newTip;

        if (auto* underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (auto* ttc = dynamic_cast<TooltipClient*> (underMouse))
                if (! (underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                    newTip = ttc->getTooltip();

        if (newTip != tip) {
            tip = newTip;
            repaint();
        }
    }

    String tip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
};

//==============================================================================
// class GraphDocumentComponent::TitleBarComponent final : public Component,
//                                                         private Button::Listener
// {
// public:
//     explicit TitleBarComponent (GraphDocumentComponent& graphDocumentComponent)
//         : owner (graphDocumentComponent)
//     {
//         static const unsigned char burgerMenuPathData[]
//             = { 110,109,0,0,128,64,0,0,32,65,108,0,0,224,65,0,0,32,65,98,254,212,232,65,0,0,32,65,0,0,240,65,252,
//                 169,17,65,0,0,240,65,0,0,0,65,98,0,0,240,65,8,172,220,64,254,212,232,65,0,0,192,64,0,0,224,65,0,0,
//                 192,64,108,0,0,128,64,0,0,192,64,98,16,88,57,64,0,0,192,64,0,0,0,64,8,172,220,64,0,0,0,64,0,0,0,65,
//                 98,0,0,0,64,252,169,17,65,16,88,57,64,0,0,32,65,0,0,128,64,0,0,32,65,99,109,0,0,224,65,0,0,96,65,108,
//                 0,0,128,64,0,0,96,65,98,16,88,57,64,0,0,96,65,0,0,0,64,4,86,110,65,0,0,0,64,0,0,128,65,98,0,0,0,64,
//                 254,212,136,65,16,88,57,64,0,0,144,65,0,0,128,64,0,0,144,65,108,0,0,224,65,0,0,144,65,98,254,212,232,
//                 65,0,0,144,65,0,0,240,65,254,212,136,65,0,0,240,65,0,0,128,65,98,0,0,240,65,4,86,110,65,254,212,232,
//                 65,0,0,96,65,0,0,224,65,0,0,96,65,99,109,0,0,224,65,0,0,176,65,108,0,0,128,64,0,0,176,65,98,16,88,57,
//                 64,0,0,176,65,0,0,0,64,2,43,183,65,0,0,0,64,0,0,192,65,98,0,0,0,64,254,212,200,65,16,88,57,64,0,0,208,
//                 65,0,0,128,64,0,0,208,65,108,0,0,224,65,0,0,208,65,98,254,212,232,65,0,0,208,65,0,0,240,65,254,212,
//                 200,65,0,0,240,65,0,0,192,65,98,0,0,240,65,2,43,183,65,254,212,232,65,0,0,176,65,0,0,224,65,0,0,176,
//                 65,99,101,0,0 };

//         static const unsigned char pluginListPathData[]
//             = { 110,109,193,202,222,64,80,50,21,64,108,0,0,48,65,0,0,0,0,108,160,154,112,65,80,50,21,64,108,0,0,48,65,80,
//                 50,149,64,108,193,202,222,64,80,50,21,64,99,109,0,0,192,64,251,220,127,64,108,160,154,32,65,165,135,202,
//                 64,108,160,154,32,65,250,220,47,65,108,0,0,192,64,102,144,10,65,108,0,0,192,64,251,220,127,64,99,109,0,0,
//                 128,65,251,220,127,64,108,0,0,128,65,103,144,10,65,108,96,101,63,65,251,220,47,65,108,96,101,63,65,166,135,
//                 202,64,108,0,0,128,65,251,220,127,64,99,109,96,101,79,65,148,76,69,65,108,0,0,136,65,0,0,32,65,108,80,
//                 77,168,65,148,76,69,65,108,0,0,136,65,40,153,106,65,108,96,101,79,65,148,76,69,65,99,109,0,0,64,65,63,247,
//                 95,65,108,80,77,128,65,233,161,130,65,108,80,77,128,65,125,238,167,65,108,0,0,64,65,51,72,149,65,108,0,0,64,
//                 65,63,247,95,65,99,109,0,0,176,65,63,247,95,65,108,0,0,176,65,51,72,149,65,108,176,178,143,65,125,238,167,65,
//                 108,176,178,143,65,233,161,130,65,108,0,0,176,65,63,247,95,65,99,109,12,86,118,63,148,76,69,65,108,0,0,160,
//                 64,0,0,32,65,108,159,154,16,65,148,76,69,65,108,0,0,160,64,40,153,106,65,108,12,86,118,63,148,76,69,65,99,
//                 109,0,0,0,0,63,247,95,65,108,62,53,129,64,233,161,130,65,108,62,53,129,64,125,238,167,65,108,0,0,0,0,51,
//                 72,149,65,108,0,0,0,0,63,247,95,65,99,109,0,0,32,65,63,247,95,65,108,0,0,32,65,51,72,149,65,108,193,202,190,
//                 64,125,238,167,65,108,193,202,190,64,233,161,130,65,108,0,0,32,65,63,247,95,65,99,101,0,0 };

//         {
//             Path p;
//             p.loadPathFromData (burgerMenuPathData, sizeof (burgerMenuPathData));
//             burgerButton.setShape (p, true, true, false);
//         }

//         {
//             Path p;
//             p.loadPathFromData (pluginListPathData, sizeof (pluginListPathData));
//             pluginButton.setShape (p, true, true, false);
//         }

//         burgerButton.addListener (this);
//         addAndMakeVisible (burgerButton);

//         pluginButton.addListener (this);
//         addAndMakeVisible (pluginButton);

//         titleLabel.setJustificationType (Justification::centredLeft);
//         addAndMakeVisible (titleLabel);

//         setOpaque (true);
//     }

// private:
//     void paint (Graphics& g) override {
//         auto titleBarBackgroundColour = getLookAndFeel().findColour (ResizableWindow::backgroundColourId).darker();

//         g.setColour (titleBarBackgroundColour);
//         g.fillRect (getLocalBounds());
//     }

//     void resized() override {
//         auto r = getLocalBounds();

//         burgerButton.setBounds(r.removeFromLeft (40).withSizeKeepingCentre (20, 20));
//         pluginButton.setBounds(r.removeFromRight (40).withSizeKeepingCentre (20, 20));

//         titleLabel.setFont (FontOptions (static_cast<float> (getHeight()) * 0.5f, Font::plain));
//         titleLabel.setBounds (r);
//     }

//     void buttonClicked (Button* b) override
//     {
//         owner.showSidePanel (b == &burgerButton);
//     }

//     GraphDocumentComponent& owner;

//     Label titleLabel {"titleLabel", "Plugin Host"};
//     ShapeButton burgerButton {"burgerButton", Colours::lightgrey, Colours::lightgrey, Colours::white};
//     ShapeButton pluginButton {"pluginButton", Colours::lightgrey, Colours::lightgrey, Colours::white};

//     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TitleBarComponent)
// };


//==============================================================================
GraphDocumentComponent::GraphDocumentComponent ()
    : graph (new Graph())
{
    init();
}

void GraphDocumentComponent::init() {
    graphPanel.reset(new GraphEditorPanel (*graph));
    addAndMakeVisible(graphPanel.get());
    graphPanel->updateComponents();
}

GraphDocumentComponent::~GraphDocumentComponent() {
    releaseGraph();
}

void GraphDocumentComponent::resized() {
    auto r = [this] {
        auto bounds = getLocalBounds();

        if (auto* display = Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
            return display->safeAreaInsets.subtractedFrom(bounds);

        return bounds;
    }();

    // const int statusHeight = 20;

    // statusBar->setBounds(r.removeFromBottom(statusHeight));
    graphPanel->setBounds(r);

    checkAvailableWidth();
}

void GraphDocumentComponent::createNewNode(const NodeType& type, juce::Point<int> pos) {
    graphPanel->createNewNode(type, pos);
}

void GraphDocumentComponent::releaseGraph() {
    if (graphPanel != nullptr) {
        graphPanel = nullptr;
    }

    // statusBar = nullptr;
    graph = nullptr;
}

bool GraphDocumentComponent::isInterestedInDragSource(const SourceDetails& details) {
    return ((dynamic_cast<ListBox*> (details.sourceComponent.get()) != nullptr)
            && details.description.toString().startsWith("NODE"));
}

void GraphDocumentComponent::itemDropped(const SourceDetails& details) {
    // don't allow items to be dropped behind the sidebar
    // if (pluginListSidePanel.getBounds().contains (details.localPosition))
    //     return;

    auto nodeTypeIndex = details.description.toString()
                                .fromFirstOccurrenceOf ("NODE: ", false, false)
                                .getIntValue();

    // must be a valid index!
    // jassert (isPositiveAndBelow (nodeTypeIndex, pluginList.getNumTypes()));

    // createNewNode({pluginList.getTypes()[nodeTypeIndex] },
    //                  details.localPosition);
}

void GraphDocumentComponent::showSidePanel(bool showSettingsPanel) {
    // pluginListSidePanel.showOrHide (true);
    // checkAvailableWidth();
    // lastOpenedSidePanel = showSettingsPanel ? &mobileSettingsSidePanel
    //                                         : &pluginListSidePanel;
}

void GraphDocumentComponent::hideLastSidePanel() {
    // if (lastOpenedSidePanel != nullptr)
    //     lastOpenedSidePanel->showOrHide (false);

    // if      (mobileSettingsSidePanel.isPanelShowing())    lastOpenedSidePanel = &mobileSettingsSidePanel;
    // else if (pluginListSidePanel.isPanelShowing())        lastOpenedSidePanel = &pluginListSidePanel;
    // else                                                  lastOpenedSidePanel = nullptr;
}

void GraphDocumentComponent::checkAvailableWidth() {
    // if (mobileSettingsSidePanel.isPanelShowing() && pluginListSidePanel.isPanelShowing()) {
    //     if (getWidth() - (mobileSettingsSidePanel.getWidth() + pluginListSidePanel.getWidth()) < 150)
    //         hideLastSidePanel();
    // }
}

void GraphDocumentComponent::changeListenerCallback (ChangeBroadcaster*) {
    // updateMidiOutput();
}
