#pragma once

#include <JuceHeader.h>

namespace MoTool {

namespace te = tracktion;

//==============================================================================
class PlayheadViewState :
        private ValueTree::Listener,
        private ChangeListener {
public:

    PlayheadViewState(te::TransportControl& tc);
    ~PlayheadViewState() override;

private:
    void changeListenerCallback(ChangeBroadcaster*) override;
    void valueTreePropertyChanged(ValueTree&, const Identifier& prop) override;

    te::TransportControl& transport_;
};


}  // namespace MoTool
