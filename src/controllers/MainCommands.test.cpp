#include "MainCommands.h"

#include <JuceHeader.h>

namespace MoTool::Tests {

using Commands::MainAppCommands;

class MainCommandsTests : public juce::UnitTest
{
public:
    MainCommandsTests() : UnitTest("MainCommands", "MoTool") {}

    void runTest() override {
        beginTest("Every command belongs to the Player or Studio");
        for (auto id : MainAppCommands::getCommandIDs())
            expect(MainAppCommands::isSupported(id, true) || MainAppCommands::isSupported(id, false), String(id));

        beginTest("A command an app lacks has no shortcut there");
        {
            Registration player{true}, studio{false};

            expectEquals(player.commandFor(KeyPress(KeyPress::deleteKey)), 0);
            expectEquals(player.commandFor(KeyPress('n', ModifierKeys::commandModifier, 0)), 0);
            expectEquals(player.commandFor(KeyPress('t', ModifierKeys::commandModifier, 0)), 0);
            expectEquals(player.commandFor(KeyPress('z')), 0);
            expectEquals(player.commandFor(KeyPress('f')), (int) MainAppCommands::viewVizMode);
            expectEquals(player.commandFor(KeyPress(KeyPress::escapeKey)), (int) MainAppCommands::viewExitVizMode);

            expectEquals(studio.commandFor(KeyPress(KeyPress::deleteKey)), (int) MainAppCommands::editDelete);
            expectEquals(studio.commandFor(KeyPress('f')), 0);
            expectEquals(studio.commandFor(KeyPress('s')), 0);
            expectEquals(studio.commandFor(KeyPress(KeyPress::escapeKey)), 0);

            for (auto* r : {&player, &studio})
                expectEquals(r->commandFor(KeyPress(KeyPress::spaceKey)), (int) MainAppCommands::transportPlay);
        }
    }

private:
    /** An app's commands registered the way AppController does it. */
    struct Registration : ApplicationCommandTarget
    {
        explicit Registration(bool isPlayer_) : isPlayer(isPlayer_) {
            manager.registerAllCommandsForTarget(this);
            manager.getKeyMappings()->resetToDefaultMappings();
        }

        int commandFor(const KeyPress& key) { return manager.getKeyMappings()->findCommandForKeyPress(key); }

        ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
        void getAllCommands(Array<CommandID>& c) override { c.addArray(MainAppCommands::getCommandIDs(isPlayer)); }
        void getCommandInfo(CommandID id, ApplicationCommandInfo& r) override {
            MainAppCommands::getCommandInfo(id, r);
        }
        bool perform(const InvocationInfo&) override { return false; }

        const bool isPlayer;
        ApplicationCommandManager manager;
    };
};

static MainCommandsTests mainCommandsTests;

}  // namespace MoTool::Tests
