#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"

namespace MoTool {

class TimelineGrid;
class PsgParamEditorComponent;

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

    // Wrapper components for tab content with proper padding
    class PsgParamWrapper : public Component {
    public:
        PsgParamWrapper(EditViewState& evs, PsgParamEditorComponent* editor, TabbedComponent* tabbedComp);
        void resized() override;
    private:
        EditViewState& editViewState_;
        TabbedComponent* tabbedComponent_;
        std::unique_ptr<PsgParamEditorComponent> editor_;
    };

    void updateTabVisibility();
};

}  // namespace MoTool