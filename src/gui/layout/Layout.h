#pragma once

#include <memory>
#include <vector>

// ****************************************************************************
// This is a simplified version of the layout system
// ****************************************************************************

namespace Layout {

// ==============================
//  Example usage:
// ==============================
/**
    lo::Vertical editor_layout {
        Toolbar {} >> 32_px,
        lo::Horizontal {
            LeftDock {} >> 300_px,
            VideoViewer {} >> 1.0_fr,
            RightDock {} >> 300_px
        } >> fill,
        ResizerBar {},
        TransportBar {} >> 32_px,
        Timeline {} >> 300_px
    };
*/
// ==============================


// Base classes for layout elements
class LayoutElement {
public:
    LayoutElement() = default;  // TODO remove
    explicit LayoutElement(const LayoutElement&) = default;
    LayoutElement& operator=(const LayoutElement&) = default;
    virtual ~LayoutElement() = default;
};


class JuceComponent : public LayoutElement {
public:
    explicit JuceComponent(juce::Component* component) : component_(component) {}
    juce::Component* getComponent() { return component_; }
private:
    juce::Component* component_;
};


class LayoutContainer : public LayoutElement {
public:
    template<typename... Children>
    explicit LayoutContainer(Children&&... children) {
        (add(std::forward<Children>(children)), ...);
    }

    template<typename T>
    T& add(T&& child) {
        auto ptr = std::make_unique<T>(std::move(child));
        T& ref = *ptr;
        children_.push_back(std::move(ptr));
        return ref;
    }
protected:
    std::vector<std::unique_ptr<LayoutElement>> children_;
};

// // Size specifiers
// struct Fill {};

// struct Fixed {
//     int size;
//     constexpr explicit Fixed(int s) : size(s) {}
// };

// struct Resizable {
//     int initial_size;
//     constexpr explicit Resizable(int s) : initial_size(s) {}
// };

// // Operator for size specification
// template<typename T>
// struct SizedElement {
//     T element;
//     int size;

//     SizedElement(int s, T&& e) : element(std::move(e)), size(s) {}
// };

// User-defined literal for pixels
// constexpr auto operator""_px(unsigned long long size) {
//     return Fixed(static_cast<int>(size));
// }

// Containers
class Vertical : public LayoutContainer {
public:
    using LayoutContainer::LayoutContainer;
};

class Horizontal : public LayoutContainer {
public:
    using LayoutContainer::LayoutContainer;
};


// // Operator overloads for intuitive syntax
// template<typename T>
// auto operator>>(T&& component, Fixed size) {
//     return SizedElement(size.size, std::forward<T>(component));
// }

// template<typename T>
// auto operator>>(T&& component, Resizable size) {
//     return SizedElement(size.initial_size, std::forward<T>(component));
// }


} // namespace Layout
