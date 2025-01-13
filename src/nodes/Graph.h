#pragma once

#include <string>


namespace MoTool::Nodes {

class NodeDescriptor {
public:
    std::string name;
    bool hasInput = false;
    bool hasOutput = false;
};


class Pin {
public:
    bool isInput;
};

class Graph {

};


}  // namespace MoTool::Nodes
