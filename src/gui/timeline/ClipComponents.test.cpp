#include <JuceHeader.h>

#include "PsgClipComponent.h"
#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../util/Oklch.h"
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

        beginTest("An unselected header keeps the clip's colour; selection only lifts its lightness");
        for (int i = 0; i < 18; ++i) {
            const auto colour = PsgClip::colourForTrackIndex(i);
            const auto unselected = Oklch::fromColour(ClipHeader::getFill(colour, false));
            const auto selected = Oklch::fromColour(ClipHeader::getFill(colour, true));
            const auto original = Oklch::fromColour(colour);
            expectWithinAbsoluteError(unselected.L, original.L, 0.005f);
            expectWithinAbsoluteError(unselected.C, original.C, 0.005f);
            expect(selected.L > unselected.L + 0.05f, "selected is lighter");
            // Blues can't keep full chroma that light in sRGB, so they sit on its boundary
            expectWithinAbsoluteError(selected.C, jmin(original.C, Oklch::maxChroma(selected.L, original.H)), 0.005f);
        }

        beginTest("A clip body is its lane's slate, lighter than the lane, more when selected");
        for (const bool trackSelected : {false, true}) {
            const auto lane = Oklch::fromColour(Colors::Timeline::lane(trackSelected));
            const auto body = Oklch::fromColour(ClipComponent::getFill(Colors::Timeline::lane(trackSelected), false));
            const auto selected = Oklch::fromColour(ClipComponent::getFill(Colors::Timeline::lane(trackSelected), true));
            expect(body.L > lane.L + 0.02f && selected.L > body.L + 0.02f);
            expectWithinAbsoluteError(body.C, lane.C, 0.005f);
            expectWithinAbsoluteError(body.H, lane.H, 3.0f);
        }

        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        uZX::PsgData data {{{{uZX::PsgRegType::VolumeA, 12}}}, {50.0, 1}};
        auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                      {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}}, "clip");
        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);

        // The header spans x 0..2000 of a track body showing 1000..1400 of it.
        Component trackBody;
        trackBody.setSize(400, ClipHeader::height);
        PsgClipHeader header(view, clip);
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

        beginTest("Clicking the header selects the clip and leaves the playhead where it is");
        const auto click = [&] {
            const auto now = Time::getCurrentTime();
            const Point<float> pos {1200.0f, 9.0f};
            header.mouseDown({Desktop::getInstance().getMainMouseSource(), pos, ModifierKeys::leftButtonModifier,
                              MouseInputSource::defaultPressure, MouseInputSource::defaultOrientation,
                              MouseInputSource::defaultRotation, MouseInputSource::defaultTiltX,
                              MouseInputSource::defaultTiltY, &header, &header, now, pos, now, 1, false});
        };
        auto& transport = edit->getTransport();
        transport.setPosition(te::TimePosition::fromSeconds(0.25));
        selection.deselectAll();
        click();
        expect(selection.isSelected(clip.get()));
        expectEquals(transport.getPosition().inSeconds(), 0.25);

        beginTest("The Player, where clips cannot be selected, always shows the legend");
        selection.deselectAll();
        view.disableClipSelection = true;
        expect(! legendColumns().isEmpty());
        click();
        expect(! selection.isSelected(clip.get()), "the Player selects no clips");
        expectEquals(transport.getPosition().inSeconds(), 0.25, "nor does its header move the playhead");
    }
};

static ClipHeaderTests clipHeaderTests;

//==============================================================================
/** The timeline grid runs under a clip's contents: over its fill, but not over its notes. */
class ClipGridTests : public juce::UnitTest {
public:
    ClipGridTests() : UnitTest("ClipGrid", "MoTool") {}

    void runTest() override {
        beginTest("Grid lines cross a clip's empty part and pass under an opaque note");
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        // Channel A at full volume, an opaque note, for two seconds of 50 fps frames
        uZX::PsgData data {{}, {50.0, 1}};
        for (int i = 0; i < 100; ++i)
            data.frames.push_back({{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::VolumeA, 15},
                                   {uZX::PsgRegType::TonePeriodFineA, 0x1c}, {uZX::PsgRegType::TonePeriodCoarseA, 1}});
        auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                      {{te::TimePosition(), te::TimeDuration::fromSeconds(2.0)}, {}}, "grid");
        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);
        view.zoom.setViewWidthPx(1000);
        view.zoom.setRange({te::TimePosition(), te::TimeDuration::fromSeconds(2.0)});  // frames 10 px wide
        TimelineGrid grid(view);
        const bool wereLanes = areNoteLanesShown(engine);
        setNoteLanesShown(engine, false);
        PsgClipComponent component(view, grid, clip);
        component.setBounds(0, 0, 1000, 200);
        Image image(Image::ARGB, 1000, 200, true);
        {
            Graphics g(image);
            component.paint(g);
        }
        setNoteLanesShown(engine, wereLanes);

        // A tick clear of its neighbours, so the column right of it shows the clip without the grid
        const auto ticks = grid.getTicks();
        auto tick = std::find_if(ticks.begin(), ticks.end(), [&](const auto& t) {
            return t.x > 0 && t.x < 998 && std::none_of(ticks.begin(), ticks.end(), [&](const auto& o) { return o.x == t.x + 1; });
        });
        expect(tick != ticks.end(), "the view has grid ticks");
        if (tick == ticks.end())
            return;

        int onNote = 0, onFill = 0, gridOverNote = 0, gridOnFill = 0;
        for (int y = 0; y < image.getHeight(); ++y) {
            const auto beside = image.getPixelAt(tick->x + 1, y), at = image.getPixelAt(tick->x, y);
            if (beside == Colors::PSG::A) {
                ++onNote;
                gridOverNote += at != beside;
            } else if (beside.isOpaque()) {
                ++onFill;
                gridOnFill += at != beside;
            }
        }
        expect(onNote > 0 && onFill > 0, "the column crosses the note and the empty clip");
        expectEquals(gridOverNote, 0, "the grid must not be drawn over the note");
        expectEquals(gridOnFill, onFill, "the grid must show on the clip's empty fill");

        beginTest("Lane lines take the grid's colours, the medium one each octave; where they cross a tick the brighter shows");
        {
            setNoteLanesShown(engine, true);
            clip->chooseKey({Scale::Tonic(9), false});  // A major, for a shaded tonic lane
            component.setBounds(0, 0, 1000, 600);  // tall enough for a line on every lane
            Image lanes(Image::ARGB, 1000, 600, true);
            {
                Graphics g(lanes);
                component.paint(g);
            }
            setNoteLanesShown(engine, wereLanes);

            const auto background = component.getBackground();
            const auto minor = background.overlaidWith(Colors::Timeline::trackGridTickColors[0]);
            const auto major = background.overlaidWith(Colors::Timeline::trackGridTickColors[1]);
            const auto hasTick = [&](int x) {
                return std::any_of(ticks.begin(), ticks.end(), [x](const auto& t) { return t.x == x; });
            };
            // The lane lines, counted in a column without ticks
            int minorRows = 0, majorRows = 0;
            int column = 61;
            while (column < 999 && (hasTick(column) || hasTick(column + 1)))
                ++column;
            int longestMinorRun = 0, run = 0, blendedRows = 0;
            for (int y = 0; y < lanes.getHeight(); ++y) {
                const auto c = lanes.getPixelAt(column, y);
                minorRows += c == minor;
                majorRows += c == major;
                run = c == minor ? run + 1 : 0;
                longestMinorRun = jmax(longestMinorRun, run);
                // The lanes are whole rows: nothing between the fill, the lines and the note
                blendedRows += c != background && c != minor && c != major && c != Colors::PSG::A;
            }
            expect(longestMinorRun > 2, "the tonic lane is filled in the lines' colour: " + String(longestMinorRun));
            expectEquals(blendedRows, 0, "no row of the lanes blends into its neighbours");
            // Every tick right of the names, with no other tick beside it, against every lane line it crosses
            int blended = 0;
            size_t levelsSeen = 0;
            for (const auto& tick : ticks) {
                if (tick.x <= 60 || tick.x >= 998 || hasTick(tick.x + 1))
                    continue;
                const auto tickColour = background.overlaidWith(Colors::Timeline::trackGridTickColors[tick.level]);
                for (int y = 0; y < lanes.getHeight(); ++y) {
                    const auto beside = lanes.getPixelAt(tick.x + 1, y);
                    if (beside != minor && beside != major)
                        continue;
                    const size_t lineLevel = beside == minor ? 0 : 1;
                    blended += lanes.getPixelAt(tick.x, y) != (tick.level > lineLevel ? tickColour : beside);
                }
                levelsSeen |= size_t(1) << tick.level;
            }
            expect(minorRows > 10, "lane lines in the grid's finest colour: " + String(minorRows));
            expect(majorRows > 0 && majorRows * 8 < minorRows, "a line in the grid's medium colour each octave: "
                                                                 + String(majorRows) + " of " + String(minorRows + majorRows));
            expect(levelsSeen == 0b111, "ticks of every level cross the lanes");
            expectEquals(blended, 0, "where a tick crosses a lane line, the brighter of the two shows");
        }
    }
};

static ClipGridTests clipGridTests;

}  // namespace MoTool::Tests
