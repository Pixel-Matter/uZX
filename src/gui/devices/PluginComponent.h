#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "PluginDeviceUI.h"

namespace MoTool {

//==============================================================================
class PluginPlaceholderComponent : public PluginDeviceUI
{
public:
    PluginPlaceholderComponent(te::Plugin::Ptr);
    ~PluginPlaceholderComponent() override;

    void resized() override;

private:
    TextButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPlaceholderComponent)
};

te::Plugin::Ptr showMenuAndCreatePlugin(te::Edit& edit);

}  // namespace MoTool
