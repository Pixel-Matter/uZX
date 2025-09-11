#include <JuceHeader.h>

#include "PluginComponent.h"
#include "PluginTree.h"

#include <common/Utilities.h>  // from Tracktion

using namespace std::literals;

namespace MoTool {

//==============================================================================
te::Plugin::Ptr showMenuAndCreatePlugin(te::Edit& edit) {
    if (auto tree = EngineHelpers::createPluginTree(edit.engine)) {
        PluginTreeGroup root(edit, *tree, te::Plugin::Type::allPlugins);
        PluginMenu m(root);

        if (auto type = m.runMenu(root))
            return type->create(edit);
    }
    return {};
}

//==============================================================================
// PluginComponent
//==============================================================================
PluginPlaceholderComponent::PluginPlaceholderComponent(EditViewState& evs, te::Plugin::Ptr p)
    : PluginDeviceUI(p)
{
    jassert(plugin != nullptr);
    addAndMakeVisible(button);
    button.setButtonText(plugin->getName());
    button.onClick = [this] () {
        if (plugin) {
            // editViewState.selectionManager.selectOnly(plugin.get());

            auto modifiers = ModifierKeys::currentModifiers;
            DBG("plugin clicked with modifiers: " << modifiers.getRawFlags());

            if (modifiers.isPopupMenu()) {
                DBG("show popup menu");
                juce::PopupMenu m;
                m.addItem("Delete", [this] { plugin->deleteFromParent(); });
                m.showAt(this);
            } else {
                plugin->showWindowExplicitly();
            }
        }
    };
    setSize(200, 400);
}

PluginPlaceholderComponent::~PluginPlaceholderComponent() {}

void PluginPlaceholderComponent::resized() {
    button.setBounds(getLocalBounds());
}

}  // namespace MoTool
