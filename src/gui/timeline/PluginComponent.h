#pragma once

#include <JuceHeader.h>
// #include <common/Utilities.h>  // from Tracktion

#include "../../controllers/EditState.h"

namespace MoTool {
    
//==============================================================================
class PluginComponent : public TextButton {
public:
    PluginComponent (EditViewState&, te::Plugin::Ptr);
    ~PluginComponent() override;

    using TextButton::clicked;
    void clicked(const ModifierKeys& modifiers) override;

private:
    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
};

te::Plugin::Ptr showMenuAndCreatePlugin(te::Edit& edit);

}  // namespace MoTool
