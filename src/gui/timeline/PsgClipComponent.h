#include <JuceHeader.h>

#include "ClipComponents.h"
#include "PsgParamEditorComponent.h"
#include "../../models/PsgClip.h"
#include "../common/GUIPaintMeasurer.h"

namespace MoTool {

//==============================================================================
class PsgClipComponent : public MidiClipComponent,
                         private juce::ValueTree::Listener {
public:
    PsgClipComponent(EditViewState& evs, te::Clip::Ptr c)
        : MidiClipComponent(evs, c)
        // , vblankAttachment_(this, [this](double) { onVBlank(); })
    {
        // setBufferedToImage(true);
        // The fps-warning in the header depends on the edit's timecode format,
        // so repaint when it changes.
        editViewState.edit.state.addListener(this);
    }

    ~PsgClipComponent() override {
        editViewState.edit.state.removeListener(this);
    }

    PsgClip* getPsgClip();

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void paintRegisters(Graphics& g);
    void paintNotes(Graphics& g);

    /** Top row shared by the channel legend, clip name and fps-mismatch warning. */
    void paintHeader(Graphics& g);

private:
    static constexpr int headerHeight = 14;

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

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
