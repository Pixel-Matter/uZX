#include <JuceHeader.h>

#include "ClipComponents.h"
#include "PsgParamEditorComponent.h"
#include "../../models/PsgClip.h"
#include "../common/GUIPaintMeasurer.h"

namespace MoTool {

//==============================================================================
class PsgClipComponent : public MidiClipComponent {
public:
    PsgClipComponent(EditViewState& evs, te::Clip::Ptr c)
        : MidiClipComponent(evs, c)
        // , editor_(evs, dynamic_cast<PsgClip*>(c.get()))
    {
        // addAndMakeVisible(editor_);
    }

    PsgClip* getPsgClip();

    void paint(Graphics& g) override;
    void paintRegisters(Graphics& g);
    void paintParameters(Graphics& g);
    // void resized() override;

private:
    GUIPaintMeasurer paintMeasurer_;

};

}  // namespace MoTool
