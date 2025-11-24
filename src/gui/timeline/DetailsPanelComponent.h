#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"

namespace MoTool {

class TimelineGrid;

class DetailsPanelComponent: public Component,
                             public juce::ChangeListener {
public:
    DetailsPanelComponent(EditViewState& evs, TimelineGrid& g);
    ~DetailsPanelComponent() override;

    void paint(Graphics& g) override;
    void resized() override;

    // ChangeListener override
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    EditViewState& editViewState;

    TabbedComponent tabbedComponent {TabbedButtonBar::TabsAtLeft};

    void updateTabVisibility();
};

}  // namespace MoTool