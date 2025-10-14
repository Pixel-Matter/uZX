#pragma once

#include <JuceHeader.h>
#include <functional>
#include <type_traits>

#include "../../controllers/Parameters.h"

namespace MoTool::ParameterUIHelpers {

// template <typename Type>
// constexpr int decimalPlacesFor() {
//     if constexpr (std::is_same_v<Type, bool>)
//         return 0;
//     else if constexpr (std::is_integral_v<Type> || Util::EnumChoiceConcept<Type>)
//         return 0;
//     else
//         return 2;
// }

// template <typename Type>
// inline std::function<juce::String(float)> wrapValueToString(const std::function<juce::String(Type)>& fn) {
//     if (!fn)
//         return {};

//     return [fn](float sliderValue) {
//         using Traits = ParameterConversionTraits<Type>;
//         return fn(Traits::from(sliderValue));
//     };
// }

// template <typename Type>
// inline std::function<float(const juce::String&)> wrapStringToValue(const std::function<Type(const juce::String&)>& fn) {
//     if (!fn)
//         return {};

//     return [fn](const juce::String& text) {
//         using Traits = ParameterConversionTraits<Type>;
//         auto typedValue = fn(text);
//         return Traits::template to<float>(typedValue);
//     };
// }

// template <typename Type>
// inline void configureSliderForParameterDef(
//     juce::Slider& slider,
//     const ParameterDef<Type>& def
// ) {
//     // using Traits = ParameterConversionTraits<Type>;

//     slider.setTooltip(def.description);
//     slider.setPopupDisplayEnabled(true, true, nullptr);

//     const auto floatRange = def.getFloatValueRange();
//     slider.setRange(static_cast<double>(floatRange.start),
//                     static_cast<double>(floatRange.end),
//                     static_cast<double>(floatRange.interval));
//     slider.setSkewFactor(static_cast<double>(floatRange.skew));

//     slider.setNumDecimalPlacesToDisplay(decimalPlacesFor<Type>());

//     if (def.valueToStringFunction)
//         slider.textFromValueFunction = wrapValueToString<Type>(def.valueToStringFunction);

//     if (def.stringToValueFunction)
//         slider.valueFromTextFunction = wrapStringToValue<Type>(def.stringToValueFunction);

//     if (def.units.isNotEmpty())
//         slider.setTextValueSuffix(def.units);

//     // slider.setValue(static_cast<double>(Traits::toFloat(value.getStoredValue())), juce::dontSendNotification);
// }

} // namespace MoTool::ParameterUIHelpers
