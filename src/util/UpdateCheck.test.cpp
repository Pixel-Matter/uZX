#include "UpdateCheck.h"

#include <JuceHeader.h>

namespace MoTool::Tests {

class UpdateCheckTests : public juce::UnitTest
{
public:
    UpdateCheckTests() : UnitTest("UpdateCheck", "MoTool") {}

    void runTest() override {
        using namespace UpdateCheck;

        beginTest("Versions compare by their numbers, then a pre-release is older than its release");
        {
            expect(compareVersions("0.7.1", "0.7.1") == 0);
            expect(compareVersions("0.7.1-alpha", "0.7.1-alpha") == 0);
            expect(compareVersions("0.10.0", "0.9.9") > 0);  // by number, not by text
            expect(compareVersions("0.7.1", "0.7.2") < 0);
            expect(compareVersions("1.0.0", "0.99.99") > 0);
            expect(compareVersions("0.7.1-alpha", "0.7.1") < 0);
            expect(compareVersions("0.7.1", "0.7.1-rc") > 0);
            expect(compareVersions("0.7.1-alpha", "0.7.1-beta") < 0);
            expect(compareVersions("0.7.2-alpha", "0.7.1") > 0);
        }

        // as api.github.com/repos/Pixel-Matter/uZX/releases answers, cut down to what is read
        const auto releases = juce::JSON::parse(R"([
            {"tag_name": "v0.7.1-alpha", "prerelease": true},
            {"tag_name": "player-v0.8.0-alpha", "prerelease": true},
            {"tag_name": "v0.7.0-alpha", "prerelease": true},
            {"tag_name": "player-v0.7.1", "prerelease": false},
            {"tag_name": "nightly", "prerelease": true},
            {"tag_name": "v0.6.0-alpha", "prerelease": true}
        ])");

        beginTest("Each app is offered the newest of its own releases");
        {
            const auto studio = findNewerRelease(releases, "v", "0.7.0-alpha");
            expect(studio.hasValue());
            expectEquals(studio->version, juce::String("0.7.1-alpha"));
            expectEquals(studio->page.toString(false),
                         juce::String("https://github.com/Pixel-Matter/uZX/releases/tag/v0.7.1-alpha"));

            const auto player = findNewerRelease(releases, "player-v", "0.7.0-alpha");
            expect(player.hasValue());
            expectEquals(player->version, juce::String("0.8.0-alpha"));
            expectEquals(player->page.toString(false),
                         juce::String("https://github.com/Pixel-Matter/uZX/releases/tag/player-v0.8.0-alpha"));
        }

        beginTest("Nothing is offered to a build that is the newest, or newer than any release");
        {
            expect(!findNewerRelease(releases, "v", "0.7.1-alpha").hasValue());
            expect(!findNewerRelease(releases, "v", "0.8.0-alpha").hasValue());
            expect(!findNewerRelease(releases, "player-v", "0.8.0-alpha").hasValue());
        }

        beginTest("A stable build is not offered a pre-release");
        {
            const auto player = findNewerRelease(releases, "player-v", "0.7.0");
            expect(player.hasValue());
            expectEquals(player->version, juce::String("0.7.1"));
            expect(!findNewerRelease(releases, "player-v", "0.7.1").hasValue());
        }

        beginTest("An answer that is not a list of releases offers nothing");
        {
            expect(!findNewerRelease(juce::var(), "v", "0.1.0").hasValue());
            expect(
                !findNewerRelease(juce::JSON::parse(R"({"message": "rate limit exceeded"})"), "v", "0.1.0").hasValue());
        }
    }
};

static UpdateCheckTests updateCheckTests;

}  // namespace MoTool::Tests
