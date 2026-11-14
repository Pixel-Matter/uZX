#include "Icons.h"

#include <JuceHeader.h>

namespace MoTool {

class IconsTests : public UnitTest
{
public:
    IconsTests() : UnitTest("Icons", "MoTool") {}

    void runTest() override {
        beginTest("Every icon parses to a shape inside its 24-unit grid");
        {
            const Path* icons[] = {&Icons::play(),
                                   &Icons::pause(),
                                   &Icons::stop(),
                                   &Icons::record(),
                                   &Icons::toStart(),
                                   &Icons::loop(),
                                   &Icons::followScroll(),
                                   &Icons::followPage(),
                                   &Icons::power(),
                                   &Icons::menu(),
                                   &Icons::add(),
                                   &Icons::stopwatch()};
            for (auto* icon : icons) {
                expect(!icon->isEmpty());
                // A typo in the path data still parses, into a shape that strays off the grid
                expect(Rectangle<float>(0.0f, 0.0f, 24.0f, 24.0f).contains(icon->getBounds()),
                       icon->getBounds().toString());
            }
        }
    }
};

static IconsTests iconsTests;

}  // namespace MoTool
