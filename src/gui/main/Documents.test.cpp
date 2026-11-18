#include <JuceHeader.h>

#include "MainDocument.h"
#include "PlayerDocument.h"

namespace MoTool {

class DocumentsTests : public UnitTest
{
public:
    DocumentsTests() : UnitTest("Documents", "MoTool") {}

    void runTest() override {
        auto& engine = *tracktion::Engine::getEngines()[0];
        const auto hasTooltipWindow = [](Component& document) {
            for (auto* child : document.getChildren())
                if (dynamic_cast<TooltipWindow*>(child) != nullptr)
                    return true;
            return false;
        };

        beginTest("The Player's and Studio's windows show tooltips");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            tracktion::SelectionManager selectionManager(engine);
            EditViewState viewState(*edit, selectionManager);
            PlayerDocumentComponent player(*edit, viewState);
            expect(hasTooltipWindow(player), "Player");
            MainDocumentComponent studio(*edit, viewState);
            expect(hasTooltipWindow(studio), "Studio");
        }
    }
};

static DocumentsTests documentsTests;

}  // namespace MoTool
