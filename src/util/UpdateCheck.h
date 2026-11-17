#pragma once

#include <JuceHeader.h>

#include <functional>

namespace MoTool::UpdateCheck {

/** A published release of an app: its version, as its tag has it after the app's prefix, and its page on GitHub. */
struct Release
{
    juce::String version;
    juce::URL page;
};

/** The page that lists every release, for a person to read. */
juce::URL getReleasesPage();

/** Negative, zero or positive as `a` is older than, the same as or newer than `b`. A version is
    "major.minor.patch" with an optional "-channel", as in "0.7.1-alpha"; one with a channel is a pre-release,
    older than the same numbers without. */
int compareVersions(const juce::String& a, const juce::String& b);

/** The newest release that is newer than `currentVersion`, among those of `releases` (GitHub's answer, see
    ReleasesRequest) whose tag is `tagPrefix` and a version: "v" picks Studio's releases, "player-v" the
    Player's. A stable `currentVersion` is not offered pre-releases. Nothing if there is none. */
juce::Optional<Release>
findNewerRelease(const juce::var& releases, const juce::String& tagPrefix, const juce::String& currentVersion);

//==============================================================================
/** Asks GitHub for the project's published releases without blocking the message thread, and calls back on
    it once: with the parsed list, or with a void var when GitHub could not be reached. Destroying the request
    cancels it, and no call comes after. */
class ReleasesRequest : private juce::Thread, private juce::AsyncUpdater
{
public:
    explicit ReleasesRequest(std::function<void(const juce::var&)> onAnswer);
    ~ReleasesRequest() override;

private:
    void run() override;
    void handleAsyncUpdate() override;

    juce::WebInputStream stream_;
    std::function<void(const juce::var&)> onAnswer_;
    juce::var answer_;  // written by the thread before it triggers the update that reads it
};

}  // namespace MoTool::UpdateCheck
