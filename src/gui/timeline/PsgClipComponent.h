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
        // , vblankAttachment_(this, [this](double) { onVBlank(); })
    {
        // setBufferedToImage(true);
    }

    PsgClip* getPsgClip();

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void paintRegisters(Graphics& g);
    void paintParameters(Graphics& g);
    void paintNotes(Graphics& g);
    void paintLegend(Graphics& g);

private:
    // void onVBlank() {
    //     // Throttle to ~30fps (skip every other vblank on 60Hz display)
    //     if (++vblankCounter_ < vblankDivider_)
    //         return;
    //     vblankCounter_ = 0;

    //     if (needsRepaint_) {
    //         needsRepaint_ = false;
    //         repaint();
    //     }
    // }

    // // Mark for repaint on next VBlank instead of immediate
    // void visibilityChanged() override {
    //     MidiClipComponent::visibilityChanged();
    //     needsRepaint_ = true;
    // }

    // void moved() override {
    //     MidiClipComponent::moved();
    //     needsRepaint_ = true;
    // }

    // void resized() override {
    //     MidiClipComponent::resized();
    //     needsRepaint_ = true;
    // }

    GUIPaintMeasurer paintMeasurer_;
    // VBlankAttachment vblankAttachment_;
    // bool needsRepaint_ = true;
    // int vblankCounter_ = 0;
    // int vblankDivider_ = 2;  // 60Hz / 2 = 30fps
};

}  // namespace MoTool
