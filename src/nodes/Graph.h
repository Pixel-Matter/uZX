#pragma once

#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>
#include <map>
#include <iostream>


namespace MoTool::Nodes {

// TODO use plugin registry mechanism like from DemoRunner (make registy generic)

struct PinType {
    std::string getName() const {
        return TypeInfo.name();
    }

    template <typename T>
    static PinType of() {
        return PinType{ typeid(T) };
    }

    template <typename T>
    bool is() const {
        return is(typeid(T));
    }

    bool is(const std::type_info& type) const {
        return TypeInfo == type;
    }

    bool is_integer() const {
        return is<char>() || is<unsigned char>() || is<short>() || is<unsigned short>() ||
               is<int>() || is<unsigned int>() || is<long>() || is<unsigned long>();
    }

    bool is_floating() const {
        return is<float>() || is<double>();
    }

    bool is_numeric() const {
        return is_integer() || is_floating();
    }

    bool is_signed() const {
        return is<char>() || is<short>() || is<int>() || is<long>();
    }

    bool is_unsigned() const {
        return is<unsigned char>() || is<unsigned short>() || is<unsigned int>() || is<unsigned long>();
    }

    bool operator == (const PinType& other) const {
        return TypeInfo == other.TypeInfo;
    }

    bool operator != (const PinType& other) const {
        return !(*this == other);
    }

    bool operator < (const PinType& other) const {
        if (is_numeric() && other.is_floating()) return true;
        if (is<uint8_t>() && (
            is<uint16_t>() || is<uint32_t>() || is<uint64_t>() ||
            is<int16_t>() || is<int32_t>() || is<int64_t>()
        )) return true;
        if (is<uint16_t>() && (
            is<uint32_t>() || is<uint64_t>() || is<int32_t>() || is<int64_t>()
        )) return true;
        if (is<uint32_t>() && (
            is<uint64_t>() || is<int64_t>()
        )) return true;
        if (is<int8_t>() && (
            is<int16_t>() || is<int32_t>() || is<int64_t>()
        )) return true;
        if (is<int16_t>() && (
            is<int32_t>() || is<int64_t>()
        )) return true;
        if (is<int32_t>() && (
            is<int64_t>()
        )) return true;
        if (is<bool>() && other.is_numeric()) return true;
        return false;
    }

    bool operator <= (const PinType& other) const {
        return *this == other || *this < other;
    }

    bool operator > (const PinType& other) const {
        return !(*this <= other);
    }

    bool operator >= (const PinType& other) const {
        return !(*this < other);
    }

    bool isConvertableFrom(const PinType& from) const {
        return from <= *this;
    }

    const std::type_info& TypeInfo;
};

struct NodeType {
    NodeType(std::string name, std::vector<PinType> inputs, std::vector<PinType> outputs, bool canCreate, bool canDelete)
        : Name(std::move(name))
        , InputPins(std::move(inputs))
        , OutputPins(std::move(outputs))
        , CanBeCreated(canCreate)
        , CanBeDeleted(canDelete)
    {}

    NodeType(std::string name, std::vector<PinType> inputs, std::vector<PinType> outputs)
        : NodeType(std::move(name), std::move(inputs), std::move(outputs), true, true)
    {}

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
        return Type.isConvertableFrom(output.getType());
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
    Node(const std::shared_ptr<NodeType> type, std::string name, const std::vector<PinType>& inputs, const std::vector<PinType>& outputs)
        : Type_(type)
        , Name_(name)
    {
        for (const auto& pinType : inputs) {
            addPin(true, pinType.getName(), pinType);
        }
        for (const auto& pinType : outputs) {
            addPin(false, pinType.getName(), pinType);
        }
    }

    Node(std::string name, const std::vector<PinType>& inputs, const std::vector<PinType>& outputs)
        : Node(std::make_shared<NodeType>(name, inputs, outputs), name, inputs, outputs)
    {}

    Node(const std::shared_ptr<NodeType> type)
        : Node(type, type->Name, type->InputPins, type->OutputPins)
    {}

    Node(const NodeType& type)
        : Node(std::make_shared<NodeType>(type))
    {}

    Node* setName(std::string name) {
        Name_ = std::move(name);
        return this;
    }

    std::string getName() const {
        return Name_;
    }

    std::shared_ptr<Pin> addInputPin(const std::string& name, const PinType& type) {
        InputPins_.push_back(std::make_shared<Pin>(name, true, type, this));
        return InputPins_.back();
    }

    std::shared_ptr<Pin> addOutputPin(const std::string& name, const PinType& type) {
        OutputPins_.push_back(std::make_shared<Pin>(name, false, type, this));
        return OutputPins_.back();
    }

    std::shared_ptr<Pin> addPin(bool isInput, const std::string& name, const PinType& type) {
        return isInput ? addInputPin(name, type) : addOutputPin(name, type);
    }

    std::shared_ptr<Pin> getInput(int index) {
        return InputPins_.at(static_cast<size_t>(index));
    }

    std::shared_ptr<Pin> getOutput(int index) {
        return OutputPins_.at(static_cast<size_t>(index));
    }

    std::shared_ptr<Pin> getPin(bool isInput, int index) {
        return (isInput ? InputPins_ : OutputPins_).at(static_cast<size_t>(index));
    }

    int getNumInputs() const {
        return static_cast<int>(InputPins_.size());
    }

    int getNumOutputs() const {
        return static_cast<int>(OutputPins_.size());
    }

    Point getPosition() const {
        return Position_;
    }

    void setPosition(Point position) {
        Position_ = position;
    }

    const NodeType* getType() const {
        return Type_.get();
    }

private:
    const std::shared_ptr<NodeType> Type_;
    std::string Name_;
    std::vector<std::shared_ptr<Pin>> InputPins_;
    std::vector<std::shared_ptr<Pin>> OutputPins_;
    Point Position_;
};


struct Connection {
    bool operator==(const Connection& other) const {
        return Source.lock() == other.Source.lock() && Destination.lock() == other.Destination.lock();
    }

    bool operator!=(const Connection& other) const {
        return !(*this == other);
    }

    std::string toString() const {
        std::stringstream buf;
        buf << (Source.expired() ? "null" : Source.lock()->getName())
            << " -> " << (Destination.expired() ? "null" : Destination.lock()->getName());
        return buf.str();
    }

    std::weak_ptr<Pin> Source;
    std::weak_ptr<Pin> Destination;
};


//=============================================================================
inline static const std::map<std::string, NodeType> BuiltinNodeTypes = {
    { "Source",    { "Source",    {}, {PinType::of<float>()},                                             false, false }},
    { "Transform", { "Transform", {PinType::of<float>()}, {PinType::of<int>(), PinType::of<float>()},     true,  true  }},
    { "Sink",      { "Sink",                              {PinType::of<float>(), PinType::of<int>()}, {}, false, false }},
};

//=============================================================================
class Graph {
public:
    Graph() {
        fillWithSimpleGraph();
    }

    void fillWithSimpleGraph() {
        float x = 0.1f, y = 0.1f, s = 0.2f;

        auto floatNode = addNode({"Float", {}, {PinType::of<float>()}}, { x,    y });
        auto boolNode  = addNode({"Bool",  {}, {PinType::of<bool>()}}, { x+=s, y });
        auto intNode   = addNode({"Int",   {}, {PinType::of<int>()}},  { x+=s, y });

        x = 0.2f, y = 0.4f;

        auto xfm = addNode({"Transform (f, f, f)->(i, f)",
                           {PinType::of<float>(), PinType::of<float>(), PinType::of<float>()},
                           {PinType::of<int>(), PinType::of<float>()}}, { x, y });

        addConnection(floatNode->getOutput(0), xfm->getInput(0));
        addConnection(boolNode-> getOutput(0), xfm->getInput(1));
        addConnection(intNode->  getOutput(0), xfm->getInput(2));

        x = 0.2f, y = 0.6f;

        auto sink = addNode({"Sink (f, i)->()",
                           {PinType::of<float>(), PinType::of<int>(), PinType::of<int>(), PinType::of<int>()},
                           {}},                                         { x, y });

        addConnection(floatNode->getOutput(0), sink->getInput(1));
        addConnection(boolNode-> getOutput(0), sink->getInput(2));
        addConnection(intNode->  getOutput(0), sink->getInput(3));


        addConnection(xfm->getOutput(0), sink->getInput(0));
        addConnection(xfm->getOutput(1), sink->getInput(1));  // Will not connect, silently ignored
    }

    bool checkEmptyConnections() {
        for (const auto& connection : Connections) {
            if (!connection) {
                return false;
            }
        }
        return true;
    }

    bool checkGraph() {
        return checkEmptyConnections();
    }

    Node* addNode(const NodeType& type, Point position) {
        auto node = std::make_shared<Node>(type);
        node->setPosition(position);
        Nodes.push_back(std::move(node));
        return Nodes.back().get();
    }

    Connection* addConnection(std::shared_ptr<Pin> source, std::shared_ptr<Pin> destination) {
        if (!source || !destination) {
            DBG("Source or destination is empty");
            return nullptr;
        }
        if (!canConnect(source.get(), destination.get())) {
            DBG("Source can not be connected to destination");
            return nullptr;
        }
        DBG("Adding connection to graph");
        Connections.push_back(std::make_shared<Connection>(std::move(source), std::move(destination)));
        return Connections.back().get();
    }

    Connection* addConnection(const Connection& connection) {
        return addConnection(connection.Source.lock(), connection.Destination.lock());
    }

    Point getNodePosition(const Node* node) {
        if (auto found = findNode(node)) {
            return found->getPosition();
        }
        return {};
    }

    void setNodePosition(const Node* node, Point position) {
        if (auto found = findNode(node)) {
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
        disconnectNode(node);
        if (auto it = std::find_if(Nodes.begin(), Nodes.end(), [node](const auto& n) { return n.get() == node; });
            it != Nodes.end()) {
            Nodes.erase(it);
        }
    }

    void disconnectNode(const Node* node) {
        // iterate from the end to avoid invalidating the iterator
        for (auto it = Connections.rbegin(); it != Connections.rend(); ++it) {
            if (!*it || (*it)->Destination.lock()->getOwner() == node || (*it)->Source.lock()->getOwner() == node) {
                Connections.erase(std::next(it).base());
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

    Connection* findConnection(const Connection& connection) {
        if (auto it = std::find_if(Connections.begin(), Connections.end(), [connection](const auto& c) { return *c == connection; });
            it != Connections.end()) {
            return it->get();
        }
        return nullptr;
    }

    bool canConnect(Pin* source, Pin* destination) {
        if (!source || !destination) return false;
        // check if input is not already connected
        for (const auto& connection : Connections) {
            if (connection->Destination.lock().get() == destination) {
                return false;
            }
        }
        return destination->canRecieveConnectionFrom(*source);
    }

    bool canConnect(const Connection& connection) {
        return canConnect(connection.Source.lock().get(), connection.Destination.lock().get());
    }

    void removeConnection(const Connection& connection) {
        // iterate from end to avoid invalidating the iterator
        for (auto it = Connections.rbegin(); it != Connections.rend(); ++it) {
            if (*it && **it == connection) {
                Connections.erase(std::next(it).base());
            }
        }
    }

private:
    std::vector<std::shared_ptr<Node>> Nodes;
    std::vector<std::shared_ptr<Connection>> Connections;
};


}  // namespace MoTool::Nodes
