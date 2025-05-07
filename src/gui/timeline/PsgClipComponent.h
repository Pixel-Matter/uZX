#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

#include "ClipComponents.h"
#include "../../models/PsgClip.h"

namespace MoTool {

//==============================================================================
class PsgClipComponent : public MidiClipComponent {
public:

    using MidiClipComponent::MidiClipComponent;

    PsgClip* getPsgClip();

    void paint(Graphics& g) override;
    void paintRegisters(Graphics& g);
    void paintParameters(Graphics& g);
};

}  // namespace MoTool
