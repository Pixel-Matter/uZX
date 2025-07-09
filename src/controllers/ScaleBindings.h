#pragma once

#include <JuceHeader.h>
#include "ParamAttachments.h"
#include "../models/tuning/Scales.h"

using namespace juce;

namespace MoTool {

// Subclass for Scale::ScaleType to handle categorized menu structure
class ScaleComboBoxBinding : public ComboBoxBinding<Scale::ScaleType> {
public:
    ScaleComboBoxBinding(ComboBox& cb, ChoiceParamAttachment<Scale::ScaleType>& cp)
        : ComboBoxBinding<Scale::ScaleType>(cb, cp)
    {
        init(); // Call init() after construction to ensure virtual function works
    }

protected:
    void fillItems() override {
        comboBox.clear();
        auto categories = Scale::getAllScaleCategories();
        int menuItemId = 1;

        for (auto category : categories) {
            if (category == Scale::ScaleCategory::User) continue;

            comboBox.addSectionHeading(Scale::getNameForCategory(category));
            auto scalesInCategory = Scale::getAllScaleTypesForCategory(category);

            for (auto scaleType : scalesInCategory) {
                comboBox.addItem(Scale::getNameForType(scaleType), menuItemId++);
            }

            if (category != categories.back() || categories.back() == Scale::ScaleCategory::User) {
                comboBox.addSeparator();
            }
        }
    }
};

}  // namespace MoTool