#pragma once

#include <JuceHeader.h>

namespace MoTool {

/**
 * ComboBox that automatically maintains selectedId for popup checkmarks
 * even when displaying custom text.
 *
 * When text is set that doesn't match the selected item's text,
 * the selectedId is preserved for correct checkmark display in the popup.
 */
class ComboBoxWithOverrideId : public juce::ComboBox {
public:
    ComboBoxWithOverrideId() = default;

    // Hide base class methods to intercept calls and auto-manage override
    void setText(const juce::String& newText, juce::NotificationType notification = juce::sendNotification) {
        // Save selectedId BEFORE setText (which might clear it)
        const int savedId = juce::ComboBox::getSelectedId();
        juce::ComboBox::setText(newText, notification);

        // If we had a valid ID and text doesn't match menu item, preserve it
        if (savedId != 0) {
            const int itemIndex = indexOfItemId(savedId);
            if (itemIndex >= 0) {
                const auto itemText = getItemText(itemIndex);
                if (getText() != itemText) {
                    overrideSelectedId = savedId;
                    return;
                }
            }
        }
        overrideSelectedId.reset();
    }

    void setSelectedId(int newItemId, juce::NotificationType notification = juce::sendNotification) {
        juce::ComboBox::setSelectedId(newItemId, notification);
        updateOverride();
    }

    void showPopup() override {
        if (overrideSelectedId.has_value()) {
            // Temporarily restore selectedId for correct popup checkmarks
            const auto savedText = getText();
            juce::ComboBox::setSelectedId(overrideSelectedId.value(), juce::dontSendNotification);
            juce::ComboBox::showPopup();
            // Restore custom text
            juce::ComboBox::setText(savedText, juce::dontSendNotification);
        } else {
            juce::ComboBox::showPopup();
        }
    }

private:
    void updateOverride() {
        const int currentId = juce::ComboBox::getSelectedId();
        const auto currentText = getText();

        if (currentId == 0) {
            // No valid selection - no override needed
            overrideSelectedId.reset();
            return;
        }

        // Check if current text matches the menu item text
        const int itemIndex = indexOfItemId(currentId);
        if (itemIndex >= 0) {
            const auto itemText = getItemText(itemIndex);
            if (currentText == itemText) {
                // Text matches - no override needed
                overrideSelectedId.reset();
            } else {
                // Text doesn't match - preserve ID for checkmark
                overrideSelectedId = currentId;
            }
        }
    }

    std::optional<int> overrideSelectedId;
};

}  // namespace MoTool
