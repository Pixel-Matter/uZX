#pragma once

#include <JuceHeader.h>

#include "../../util/UpdateCheck.h"

namespace MoTool {

/** Check for Updates: asks GitHub for the app's releases as it opens, then says whether one is newer than this
    build, with a link to that release's page. */
class UpdateDialogComponent : public juce::Component
{
public:
    /** `appName` is the app as the user knows it; `tagPrefix` and `currentVersion` pick its releases and
        compare them, see UpdateCheck::findNewerRelease. */
    UpdateDialogComponent(const juce::String& appName,
                          const juce::String& tagPrefix,
                          const juce::String& currentVersion);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void showAnswer(const juce::var& releases);
    void showLink(const juce::String& text, const juce::URL& url);

    const juce::String appName_, tagPrefix_, currentVersion_;
    juce::Label message_;
    juce::HyperlinkButton link_;
    juce::TextButton closeButton_;
    UpdateCheck::ReleasesRequest request_;  // last: it stops before what its answer is shown in goes
};

}  // namespace MoTool
