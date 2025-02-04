#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>

#include "Transport.h"
#include "timeline/Timeline.h"
// #include "layout/Layout.h"


using namespace juce;
// namespace lo = Layout;


class LayoutItem: public GridItem {

};

//==============================================================================
/** Section of layout for storing, doesn't know of components */
class LayoutSection {
public:
    template<typename T>
    explicit LayoutSection(T&& size) noexcept
        : size_ {std::forward<T>(size)}
    {}

    // Getter for the size variant
    const auto& getSize() const noexcept { return size_; }

private:
    std::variant<Grid::Fr, Grid::Px> size_;
};

class Layout {

};

class VerticalLayout: public Layout {
public:
    VerticalLayout() noexcept {
        grid_.templateColumns = { Grid::TrackInfo(1_fr) };
    }

    template<typename... Sizes>
    explicit VerticalLayout(Sizes&&... size) noexcept
        : VerticalLayout {}
    {
        (addSection(std::forward<Sizes>(size)), ...);
    }

    template<typename... Sizes>
    void addSections(Sizes&&... size) noexcept {
        (addSection(std::forward<Sizes>(size)), ...);
    }

    template<typename T>
    void addSection(T&& size) noexcept {
        grid_.templateRows.add(Grid::TrackInfo(std::forward<T>(size)));
    }

    template<typename... Components>
    void addComponents(Components&... components) {
        (grid_.items.add(GridItem(components)), ...);
    }

    void performLayout(Rectangle<int> bounds) noexcept {
        grid_.performLayout(bounds);
    }

private:
    Grid grid_;
};

namespace Helpers {
    template<class LayoutType, class... Components>
    void addToLayoutAndMakeVisible(Component& parent, LayoutType& layout, Components&&... components) {
        layout.addComponents(components...);
        (parent.addAndMakeVisible(std::forward<Components>(components)), ...);
    }
}


class MainDocumentComponent: public Component {
public:

    explicit MainDocumentComponent(te::Engine& engine)
        : engine_ {engine}
    {
        EngineHelpers::getOrInsertAudioTrackAt(edit_, 0);

        layout_.addSections(
            32_px,
            1_fr
        );
        // layout_.addComponents(
        //     transportBar_,
        //     timelinePanel_
        // );

        // TODO make single helper to addSection components to layout and addAndMakeVisible
        Helpers::addToLayoutAndMakeVisible(*this, layout_,
            &transportBar_,
            &timelinePanel_
        );

        // Helpers::addAndMakeVisible(*this, {
        //     &transportBar_,
        //     &timelinePanel_
        // });

    }

    ~MainDocumentComponent() override {
        edit_.getTempDirectory(false).deleteRecursively();
    }

    void resized() override {
        layout_.performLayout(getLocalBounds());
    }

private:
    te::Engine& engine_;
    te::Edit edit_ {engine_, te::Edit::EditRole::forEditing};

    TransportBar transportBar_   {edit_};
    TimelinePanel timelinePanel_ {edit_};

    VerticalLayout layout_;

    // lo::Vertical layout_ {
    //     lo::LayoutElement {}
    // //     transportBar_ >> 32_px,
    // };
    Grid grid_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainDocumentComponent)
};
