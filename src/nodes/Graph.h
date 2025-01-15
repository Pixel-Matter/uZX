#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>


namespace MoTool::Nodes {

// TODO use plugin registry mechanism like from DemoRunner (make registy generic)

struct PinType {
    template <typename T>
    static PinType of() {
        return PinType{ typeid(T) };
    }

    bool operator==(const PinType& other) const {
        return TypeInfo == other.TypeInfo;
    }

    bool operator!=(const PinType& other) const {
        return !(*this == other);
    }

    std::string getName() const {
        return TypeInfo.name();
    }

    bool isFor(const std::type_info& type) const {
        return TypeInfo == type;
    }

    template <typename T>
    bool isFor() const {
        return isFor(typeid(T));
    }

    const std::type_info& TypeInfo;
};

struct NodeType {
    std::string Name;
    std::vector<PinType> InputPins;
    std::vector<PinType> OutputPins;
    bool CanBeCreated = true;
    bool CanBeDeleted = true;
};

class Node;

struct Point {
    double x, y;
};

class Pin {
public:
    Pin(std::string name, bool isInput, const PinType& type, Node* owner)
        : Name(std::move(name))
        , IsInput(isInput)
        , Type(type)
        , Owner(owner)
    {}

    bool canRecieveConnectionFrom(const Pin& output) const {
        if (!IsInput || output.IsInput) return false;
        // TODO add compatibility checks, for example int -> float, double -> float, etc.
        return Type == output.Type;
    }

    std::string getName() const {
        return Name;
    }

    bool isInput() const {
        return IsInput;
    }

    const PinType& getType() const {
        return Type;
    }

    Node* getOwner() const {
        return Owner;
    }

private:
    std::string Name;
    bool IsInput;
    PinType Type;
    Node* Owner;  // Back reference to owning node
};

class Node {
public:
    Node(const NodeType& type)
        : Type(type)
        , Name(type.Name)
    {
        for (const auto& pinType : type.InputPins) {
            addPin(true, pinType.getName(), pinType);
        }
        for (const auto& pinType : type.OutputPins) {
            addPin(false, pinType.getName(), pinType);
        }
    }

    void setName(std::string name) {
        Name = std::move(name);
    }

    std::string getName() const {
        return Name;
    }

    std::shared_ptr<Pin> addInputPin(const std::string& name, const PinType& type) {
        InputPins.push_back(std::make_shared<Pin>(name, true, type, this));
        return InputPins.back();
    }

    std::shared_ptr<Pin> addOutputPin(const std::string& name, const PinType& type) {
        OutputPins.push_back(std::make_shared<Pin>(name, false, type, this));
        return OutputPins.back();
    }

    std::shared_ptr<Pin> addPin(bool isInput, const std::string& name, const PinType& type) {
        return isInput ? addInputPin(name, type) : addOutputPin(name, type);
    }

    std::shared_ptr<Pin> getPin(bool isInput, int index) {
        return isInput ? InputPins.at(static_cast<size_t>(index)) : OutputPins.at(static_cast<size_t>(index));
    }

    int getNumInputs() const {
        return static_cast<int>(InputPins.size());
    }

    int getNumOutputs() const {
        return static_cast<int>(OutputPins.size());
    }

    Point getPosition() const {
        return Position;
    }

    void setPosition(Point position) {
        Position = position;
    }

    const NodeType& getType() const {
        return Type;
    }

private:
    const NodeType& Type;
    std::string Name;
    std::vector<std::shared_ptr<Pin>> InputPins;
    std::vector<std::shared_ptr<Pin>> OutputPins;
    Point Position;
};


struct Connection {
    bool operator==(const Connection& other) const {
        return Source.lock() == other.Source.lock() && Destination.lock() == other.Destination.lock();
    }

    bool operator!=(const Connection& other) const {
        return !(*this == other);
    }

    std::weak_ptr<Pin> Source;
    std::weak_ptr<Pin> Destination;
};

inline static const std::map<std::string, NodeType> BuiltinNodeTypes = {
    { "Source",    { "Source",    {},                     {PinType::of<float>()},                       false, false }},
    { "Sink",      { "Sink",      {PinType::of<float>()}, {},                                           false, false }},
    { "Transform", { "Transform", {PinType::of<float>()}, {PinType::of<float>(), PinType::of<int>()},   true,  true  }},
};

class Graph {
public:

    Graph() {
        fillWithSimpleGraph();
    }

    void fillWithSimpleGraph() {
        addNode(BuiltinNodeTypes.at("Source"),    { 0.5, 0.1 })->setName("Source");
        addNode(BuiltinNodeTypes.at("Transform"), { 0.5, 0.3 })->setName("Transform");
        addNode(BuiltinNodeTypes.at("Sink"),      { 0.5, 0.5 })->setName("Sink");

        // add connections
        Connections.push_back(std::make_shared<Connection>(
            Nodes[0]->getPin(false, 0),
            Nodes[1]->getPin(true, 0)
        ));
        Connections.push_back(std::make_shared<Connection>(
            Nodes[1]->getPin(false, 0),
            Nodes[2]->getPin(true, 0)
        ));
    }

    Node* addNode(const NodeType& type, Point position) {
        auto node = std::make_shared<Node>(type);
        node->setPosition(position);
        Nodes.push_back(std::move(node));
        return Nodes.back().get();
    }

    Point getNodePosition(const Node* node) {
        if (auto found = findNode(node); found) {
            return found->getPosition();
        }
        return {};
    }

    void setNodePosition(const Node* node, Point position) {
        if (auto found = findNode(node); found) {
            found->setPosition(position);
        }
    }

    Node* findNode(const Node* node) {
        if (auto it = std::find_if(Nodes.begin(), Nodes.end(), [node](const auto& n) { return n.get() == node; });
            it != Nodes.end()) {
            return it->get();
        }
        return nullptr;
    }

    void removeNode(const Node* node) {
        if (auto it = std::find_if(Nodes.begin(), Nodes.end(), [node](const auto& n) { return n.get() == node; });
            it != Nodes.end()) {
            Nodes.erase(it);
        }
    }

    void disconnectNode(const Node* node) {
        for (auto& connection : Connections) {
            if (connection->Destination.lock()->getOwner() == node || connection->Source.lock()->getOwner() == node) {
                removeConnection(connection.get());
            }
        }
    }

    std::vector<std::shared_ptr<Node>>& getNodes() {
        return Nodes;
    }

    const std::vector<std::shared_ptr<Node>>& getNodes() const {
        return Nodes;
    }

    std::vector<std::shared_ptr<Connection>>& getConnections() {
        return Connections;
    }

    const std::vector<std::shared_ptr<Connection>>& getConnections() const {
        return Connections;
    }

    Connection* findConnection(const Connection* connection) {
        if (auto it = std::find_if(Connections.begin(), Connections.end(), [connection](const auto& c) { return c.get() == connection; });
            it != Connections.end()) {
            return it->get();
        }
        return nullptr;
    }

    bool canConnect(Pin* output, Pin* input) {
        if (!output || !input) return false;
        return output->canRecieveConnectionFrom(*input);
    }

    Connection* addConnection(std::shared_ptr<Pin> output, std::shared_ptr<Pin> input) {
        if (canConnect(output.get(), input.get())) {
            Connections.push_back(std::make_shared<Connection>(std::move(output), std::move(input)));
            return Connections.back().get();
        }
        return nullptr;
    }

    void removeConnection(const Connection* connection) {
        if (auto it = std::find_if(Connections.begin(), Connections.end(), [connection](const auto& c) { return c.get() == connection; });
            it != Connections.end()) {
            Connections.erase(it);
        }
    }

private:
    std::vector<std::shared_ptr<Node>> Nodes;
    std::vector<std::shared_ptr<Connection>> Connections;
};


}  // namespace MoTool::Nodes
