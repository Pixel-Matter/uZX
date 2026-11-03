#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../formats/psg/PsgData.h"

namespace MoTool::Tests {

class ClipHeaderTests : public juce::UnitTest {
public:
    ClipHeaderTests() : UnitTest("ClipHeader", "MoTool") {}

    void runTest() override {
        beginTest("The legend goes first, then the info, then the warnings; the name keeps 48 px");
        {
            const auto expectFit = [this](int width, size_t items, bool legend) {
                const auto fit = ClipHeader::fit(width, 100, {50, 40}, 67);
                expectEquals((int) fit.items, (int) items, "items at width " + String(width));
                expect(fit.legend == legend, "legend at width " + String(width));
            };
            expectFit(1000, 2, true);
            expectFit(48 + 50 + 40 + 67, 2, true);
            expectFit(48 + 50 + 40 + 67 - 1, 2, false);
            expectFit(48 + 50 + 40, 2, false);
            expectFit(48 + 50 + 40 - 1, 1, false);
            expectFit(48 + 50 - 1, 0, false);
            expectFit(0, 0, false);
            const auto shortName = ClipHeader::fit(20 + 50 + 40 + 67, 20, {50, 40}, 67);
            expect(shortName.items == 2 && shortName.legend, "a short name needs only its own width");
        }

        beginTest("Header text reads at 4.5:1 or more on every clip colour, selected or not");
        {
            // WCAG relative luminance and contrast ratio
            const auto luminance = [](Colour c) {
                const auto linear = [](float x) {
                    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
                };
                return 0.2126f * linear(c.getFloatRed()) + 0.7152f * linear(c.getFloatGreen())
                     + 0.0722f * linear(c.getFloatBlue());
            };
            const auto contrast = [&](Colour a, Colour b) {
                const auto la = luminance(a), lb = luminance(b);
                return (jmax(la, lb) + 0.05f) / (jmin(la, lb) + 0.05f);
            };
            Array<Colour> colours {Colour(0xff0088ff), Colours::yellow, Colours::blue, Colours::red,
                                   Colours::white, Colours::black};  // saved before OKLCH, or chosen by hand
            for (int i = 0; i < 18; ++i)
                colours.add(PsgClip::colourForTrackIndex(i));
            for (const auto colour : colours)
                for (const bool selected : {false, true})
                    expect(contrast(ClipHeader::getFill(colour, selected).overlaidWith(ClipHeader::textColour),
                                    ClipHeader::getFill(colour, selected)) >= 4.5f,
                           colour.toDisplayString(false) + (selected ? " selected" : " unselected"));
        }

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        uZX::PsgData data {{{{uZX::PsgRegType::VolumeA, 12}}}, {50.0, 1}};
        auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                      {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}}, "clip");
        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);
        TimelineGrid grid(view);

        // The header spans x 0..2000 of a track body showing 1000..1400 of it.
        Component trackBody;
        trackBody.setSize(400, ClipHeader::height);
        PsgClipHeader header(view, grid, clip);
        trackBody.addAndMakeVisible(header);
        header.setBounds(-1000, 0, 2000, ClipHeader::height);
        const auto legendColumns = [&] {
            Image image(Image::ARGB, 2000, ClipHeader::height, true);
            {
                Graphics g(image);
                header.paint(g);
            }
            Range<int> columns;
            for (int x = 0; x < image.getWidth(); ++x)
                if (image.getPixelAt(x, ClipHeader::height / 2) == Colors::PSG::A)
                    columns = columns.isEmpty() ? Range<int>(x, x + 1) : columns.getUnionWith(x);
            return columns;
        };

        beginTest("Studio shows the legend on the selected clip, at the right of the view");
        expect(legendColumns().isEmpty(), "an unselected clip has no legend");
        selection.selectOnly(clip.get());
        const auto columns = legendColumns();
        expect(! columns.isEmpty(), "the selected clip has a legend");
        expect(columns.getStart() > 1300 && columns.getEnd() < 1400, "the legend ends at the view's right edge");

        beginTest("The Player, where clips cannot be selected, always shows the legend");
        selection.deselectAll();
        view.disableClipSelection = true;
        expect(! legendColumns().isEmpty());
    }
};

static ClipHeaderTests clipHeaderTests;

}  // namespace MoTool::Tests
