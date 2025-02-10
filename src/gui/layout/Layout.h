#pragma once

#include <JuceHeader.h>

// ****************************************************************************
// This is a simplified version of the layout system
// ****************************************************************************


namespace MoTool::Layout {

struct LayoutItem {
    GridItem item;
    Grid::TrackInfo track;
};

class Layout {
protected:
    enum class Direction {
        Horizontal,
        Vertical
    };

    Direction direction_;

    Layout(Direction direction) noexcept
        : direction_ {direction}
    {
        gridInit();
    }

    template<typename... Sizes>
    explicit Layout(Direction direction, Sizes&&... size) noexcept
        : Layout {direction}
    {
        (addSection(std::forward<Sizes>(size)), ...);
    }

public:
    template<typename... Sizes>
    void addSections(Sizes&&... size) noexcept {
        (addSection(std::forward<Sizes>(size)), ...);
    }

    template<typename T>
    void addSection(T&& size) noexcept {
        addGridTrackInfo(Grid::TrackInfo(std::forward<T>(size)));
    }

    template<typename... Components>
    void addComponents(Components&... components) noexcept {
        (addGridItem(GridItem(components)), ...);
    }

    template<typename... Items>
    void addItems(Items&&... items) noexcept {
        (addGridTrackInfo(std::move(items.track)), ...);
        (addGridItem(std::move(items.item)), ...);
    }

    void performLayout(Rectangle<int> bounds) noexcept {
        grid_.performLayout(bounds);
    }

private:
    Grid grid_;

    void gridInit() noexcept {
        if (direction_ == Direction::Vertical) {
            grid_.templateColumns = { Grid::TrackInfo(1_fr) };
        } else {
            grid_.templateRows = { Grid::TrackInfo(1_fr) };
        }
    }

    void addGridItem(GridItem&& item) noexcept {
        grid_.items.add(std::move(item));
    }

    void addGridTrackInfo(Grid::TrackInfo&& track) noexcept {
        if (direction_ == Direction::Vertical) {
            grid_.templateRows.add(std::move(track));
        } else {
            grid_.templateColumns.add(std::move(track));
        }
    }

};


class VerticalLayout : public Layout {
public:
    template<typename... Sizes>
    explicit VerticalLayout(Sizes&&... size) noexcept
        : Layout {Direction::Vertical, std::forward<Sizes>(size)...}
    {}
};

class HorizontalLayout : public Layout {
public:
    template<typename... Sizes>
    explicit HorizontalLayout(Sizes&&... size) noexcept
        : Layout {Direction::Horizontal, std::forward<Sizes>(size)...}
    {}
};

namespace Operators {

// Forward declare all operator>> overloads
LayoutItem operator>>(Component& component, Grid::Fr size);
LayoutItem operator>>(Component& component, Grid::Px size);
LayoutItem operator>>(Component* component, Grid::Fr size);
LayoutItem operator>>(Component* component, Grid::Px size);

// Define the operators
LayoutItem operator>>(Component& component, Grid::Fr size) {
    return {
        GridItem(component),
        Grid::TrackInfo(size)
    };
}

LayoutItem operator>>(Component& component, Grid::Px size) {
    return {
        GridItem(component),
        Grid::TrackInfo(size)
    };
}

LayoutItem operator>>(Component* component, Grid::Fr size) {
    return {
        GridItem(component),
        Grid::TrackInfo(size)
    };
}

LayoutItem operator>>(Component* component, Grid::Px size) {
    return {
        GridItem(component),
        Grid::TrackInfo(size)
    };
}

}  // namespace Layout::Operators
}  // namespace Layout


namespace Helpers {

template<class LayoutType, class... Components>
void addToLayoutAndMakeVisible(Component& parent, LayoutType& layout, Components&&... components) {
    (parent.addAndMakeVisible(std::forward<Components>(components)), ...);
    layout.addComponents(components...);
}

//==============================================================================
// Example
//==============================================================================
/**
    Helpers::addLayoutItemsAndMakeVisible(*this, layout,
        transportBar  >> 32_px,
        timelinePanel >> 1_fr,
        footer        >> 32_px
    );
*/
//==============================================================================

template<class LayoutType, typename... Items>
void addLayoutItemsAndMakeVisible(Component& parent, LayoutType& layout, Items&&... items) noexcept {
    (parent.addAndMakeVisible(items.item.associatedComponent), ...);
    layout.addItems(std::forward<Items>(items)...);
}

} // namespace Helpers
