#include <JuceHeader.h>

#include "ClipComponents.h"
#include "PsgParamEditorComponent.h"
#include "../../models/PsgClip.h"
#include "../common/GUIPaintMeasurer.h"

namespace MoTool {

//==============================================================================
/** Frame hover popup text as coloured runs: A/B/C, Env and Noise as a borderless
    table with right-aligned numbers, in legend colours, half-alpha unless this frame
    changed the value. */
std::vector<std::pair<String, Colour>> formatPsgFrame(const PsgParamFrameData&, double chipClockHz);

//==============================================================================
class PsgClipComponent : public MidiClipComponent,
                         private juce::ValueTree::Listener,
                         private juce::Timer {
public:
    PsgClipComponent(EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c);
    ~PsgClipComponent() override;

    PsgClip* getPsgClip();

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    void paintRegisters(Graphics& g);
    void paintNotes(Graphics& g);

    /** Top row shared by the channel legend, clip name and fps-mismatch warning. */
    void paintHeader(Graphics& g);

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void timerCallback() override;

    /** Hover popup text for the frame under local x; empty before the first frame. */
    std::vector<std::pair<String, Colour>> describeFrameAt(int x);
    void showFrameHover();
    void hideFrameHover();

    class FrameHover;
    std::unique_ptr<FrameHover> frameHover_;

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
