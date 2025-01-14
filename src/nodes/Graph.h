#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>


namespace MoTool::Nodes {

// TODO use plugin registry mechanism like from DemoRunner (make registy generic)
struct NodeType {
    std::string Name;
    // Could add info about default pins configuration

    bool canBeCreated = true;
    bool canBeDeleted = true;
};

class Node;

struct Point {
    double x, y;
};

struct Pin {
    std::string Name;
    bool IsInput;
    Node* Owner;  // Back reference to owning node
};

class Node {
public:
    Node(const NodeType& type)
        : Type(type)
    {}

    void setName(std::string name) {
        Name = std::move(name);
    }

    std::string getName() const {
        return Name;
    }

    Pin& addInputPin(const std::string& name) {
        InputPins.emplace_back(name, true, this);
        return InputPins.back();
    }

    Pin& addOutputPin(const std::string& name) {
        OutputPins.emplace_back(name, false, this);
        return OutputPins.back();
    }

    Pin& addPin(bool isInput, const std::string& name) {
        return isInput ? addInputPin(name) : addOutputPin(name);
    }

    Pin& getPin(bool isInput, int index) {
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
    std::vector<Pin> InputPins;
    std::vector<Pin> OutputPins;
    Point Position;
};

struct Connection {
    std::weak_ptr<Pin> Input;
    std::weak_ptr<Pin> Output;
};

inline static std::map<std::string, NodeType> BuiltinNodeTypes = {
    { "Source",    { "Source",    false, false }},
    { "Sink",      { "Sink",      false, false }},
    { "Transform", { "Transform", true,  true  }},
};

class Graph {
public:

    Graph() {
        addNode(BuiltinNodeTypes.at("Source"),    { 0.1, 0.1 })->setName("Source");
        addNode(BuiltinNodeTypes.at("Sink"),      { 0.3, 0.1 })->setName("Sink");
        addNode(BuiltinNodeTypes.at("Transform"), { 0.2, 0.2 })->setName("Transform");
    }

    Node* addNode(const NodeType& type, Point position) {
        auto node = std::make_shared<Node>(type);
        node->setPosition(position);
        Nodes.push_back(std::move(node));
        return Nodes.back().get();
    }

    bool canConnect(Pin* output, Pin* input) {
        if (!output || !input) return false;
        if (output->IsInput || !input->IsInput) return false;
        // Add more validation as needed
        return true;
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

    std::vector<std::shared_ptr<Node>>& getNodes() {
        return Nodes;
    }

    Connection* findConnection(const Connection* connection) {
        if (auto it = std::find_if(Connections.begin(), Connections.end(), [connection](const auto& c) { return c.get() == connection; });
            it != Connections.end()) {
            return it->get();
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
