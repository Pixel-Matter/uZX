#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "TimelineGrid.h"
#include "../../formats/psg/PsgData.h"

namespace MoTool {

class PsgClipPaintingTest : public UnitTest
{
public:
    PsgClipPaintingTest() : UnitTest("PsgClipPainting", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto render = [&engine](uint8_t mixer, uint8_t volumeA, uint8_t volumeB) {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            uZX::PsgData data {
                {{{uZX::PsgRegType::Mixer, mixer}, {uZX::PsgRegType::NoisePeriod, 16},
                  {uZX::PsgRegType::VolumeA, volumeA}, {uZX::PsgRegType::VolumeB, volumeB}}},
                {50.0, 1}
            };
            auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                        {{te::TimePosition(), te::TimeDuration::fromSeconds(0.02)}, {}}, "noise");
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(100);
            view.zoom.setRange(clip->getEditTimeRange());
            TimelineGrid grid(view);
            PsgClipComponent component(view, grid, clip);
            component.setSize(100, 100);
            Image image(Image::ARGB, 100, 100, true);
            Graphics g(image);
            component.paintNotes(g);
            return image;
        };
        auto isBlank = [](const Image& image) {
            for (int y = 0; y < image.getHeight(); ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                    if (! image.getPixelAt(x, y).isTransparent())
                        return false;
            return true;
        };

        beginTest("Disabled and silent noise channels draw no band");
        expect(isBlank(render(0x3f, 15, 15)), "Nonzero volume must not draw disabled noise");
        expect(isBlank(render(0x37, 0, 0)), "Enabled but silent noise must not draw a band");

        beginTest("Only noise-enabled channels contribute color and opacity");
        const auto noiseA = render(0x37, 15, 0);
        const auto noiseAWithB = render(0x37, 15, 15);
        expect(! isBlank(noiseA), "Audible noise must draw a band");
        bool equal = true;
        for (int y = 0; y < noiseA.getHeight(); ++y)
            for (int x = 0; x < noiseA.getWidth(); ++x)
                equal = equal && noiseA.getPixelAt(x, y) == noiseAWithB.getPixelAt(x, y);
        expect(equal, "Channel B without noise must not change channel A's noise band");

        beginTest("Envelope-driven noise remains visible with zero fixed volume");
        expect(! isBlank(render(0x37, 16, 0)));
    }
};

static PsgClipPaintingTest psgClipPaintingTest;

}  // namespace MoTool
