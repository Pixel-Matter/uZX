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

    struct DevicePluginUI : public juce::Component {
        virtual bool allowWindowResizing() = 0;
        virtual juce::ComponentBoundsConstrainer* getBoundsConstrainer() = 0;
    };

    virtual std::unique_ptr<DevicePluginUI> createDeviceUI()     { return {}; }

    // Methods checking if UI should has titlebar or is a special UI (such like LevelMeterUI)
    virtual bool hasCustomDeviceUI() const { return false; }
    virtual bool canHasPlusButtonAfter() const { return true; }
};

}  // namespace MoTool::uZX