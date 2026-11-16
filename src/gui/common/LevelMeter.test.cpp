#include "LevelMeter.h"

#include <JuceHeader.h>

#include <cmath>

#include "LookAndFeel.h"

namespace MoTool {

class LevelMeterTests : public UnitTest
{
public:
    LevelMeterTests() : UnitTest("LevelMeter", "MoTool") {}

    void runTest() override {
        using namespace LedMeter;
        constexpr int leds = 20;  // the master meter's: 3 dB each

        beginTest("The scale runs from -54 to +6 dBFS, with 0 dBFS nine tenths of the way");
        {
            expectEquals(dbToFill(-100.0f), 0.0f);
            expectEquals(dbToFill(minDb), 0.0f);
            expectWithinAbsoluteError(dbToFill(0.0f), 0.9f, 1.0e-6f);
            expectEquals(dbToFill(maxDb), 1.0f);
            expectEquals(dbToFill(20.0f), 1.0f);
        }

        beginTest("An LED lights as soon as the level passes its start: red only above 0 dBFS");
        {
            const int firstYellow = (int) std::lround(levelZones.mid * leds);
            const int firstRed = (int) std::lround(levelZones.high * leds);
            expectEquals(firstYellow, 16, "from -6 dB");
            expectEquals(firstRed, 18, "from 0 dB");

            expectEquals(litLeds(dbToFill(-100.0f), leds), 0, "silence");
            expectEquals(litLeds(dbToFill(-53.0f), leds), 1);
            expectEquals(litLeds(dbToFill(-6.0f), leds), firstYellow, "at -6 dB: green only");
            expectEquals(litLeds(dbToFill(-5.9f), leds), firstYellow + 1, "past -6 dB: the first yellow");
            expectEquals(litLeds(dbToFill(0.0f), leds), firstRed, "full scale itself is not a clip: no red");
            expectEquals(litLeds(dbToFill(0.1f), leds), firstRed + 1, "over full scale: the first red");
            expectEquals(litLeds(dbToFill(12.0f), leds), leds);
        }

        beginTest("A louder reading shows at once; a quieter one falls 48 dB a second");
        {
            expectEquals(fallTo(-30.0f, -3.0f), -3.0f);
            float shown = 0.0f;
            for (int i = 0; i < levelRefreshHz; ++i)  // a second of silence
                shown = fallTo(shown, -100.0f);
            expectWithinAbsoluteError(shown, -48.0f, 1.0e-3f);
        }

        beginTest("The master meter remembers a clip on either channel until it is clicked");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(*tracktion::Engine::getEngines()[0]);
            MasterLevelMeter meter(*edit);
            meter.setSize(60, 12);

            meter.showLevels(0.0f, -2.0f);
            expect(!meter.hasClipped(), "full scale is not a clip");
            meter.showLevels(-10.0f, 0.5f);
            expect(meter.hasClipped());
            meter.showLevels(-100.0f, -100.0f);
            expect(meter.hasClipped(), "still, once the sound has gone");

            // 20 LEDs, 3 px apart: the last one starts at x 57; the left row is the top one
            const auto lastLed = [&meter](int y) {
                return meter.createComponentSnapshot(meter.getLocalBounds(), false, 1.0f).getPixelAt(57, y);
            };
            expect(lastLed(9) == Colors::Meter::high, "the channel that clipped keeps its last LED red");
            expect(lastLed(2) != Colors::Meter::high, "the other one doesn't");

            const auto& mouse = Desktop::getInstance().getMainMouseSource();
            meter.mouseDown(MouseEvent(mouse,
                                       {},
                                       {},
                                       MouseInputSource::defaultPressure,
                                       MouseInputSource::defaultOrientation,
                                       MouseInputSource::defaultRotation,
                                       MouseInputSource::defaultTiltX,
                                       MouseInputSource::defaultTiltY,
                                       &meter,
                                       &meter,
                                       Time::getCurrentTime(),
                                       {},
                                       Time::getCurrentTime(),
                                       1,
                                       false));
            expect(!meter.hasClipped());
            expect(lastLed(9) != Colors::Meter::high);
        }
    }
};

static LevelMeterTests levelMeterTests;

}  // namespace MoTool
