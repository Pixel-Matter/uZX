#pragma once

#include <JuceHeader.h>

namespace MoTool::uZX {

/**
* Base class for uZX device plugins thac has UI for Track Devices Panel
* Inherits from tracktion::Plugin to integrate with Tracktion Engine.
*/
class DevicePlugin : public tracktion::Plugin {
public:
    using tracktion::Plugin::Plugin;

};

}  // namespace MoTool::uZX