#include "ProgressDialog.h"
#include "LookAndFeel.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {

ProgressDialog::ProgressDialog(const String& title,
                               te::ThreadPoolJobWithProgress& j,
                               te::BackgroundJobManager& jm)
    : DialogWindow(title,
                   LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                   true)
    , job(j)
    , jobManager(jm) {
    content.cancelButton.setButtonText("Cancel");
    MoLookAndFeel::setLargeText(content.cancelButton);
    if (job.canCancel()) {
        content.cancelButton.setEnabled(true);
        content.cancelButton.onClick = [this] {
            job.signalJobShouldExit();
            exitModalState(0);
        };
    } else {
        content.cancelButton.setEnabled(false);
    }

    content.addAndMakeVisible(content.progressBar);
    content.addAndMakeVisible(content.cancelButton);

    content.setSize(400, 2 * Space::dialog + Controls::height + Space::related + Controls::actionHeight);
    setUsingNativeTitleBar(true);  // as the other dialogs: the system's close button and corners
    setContentOwned(&content, true);
    centreAroundComponent(nullptr, getWidth(), getHeight());

    jobManager.addListener(this);

    startTimer(30);
}

ProgressDialog::~ProgressDialog() {
    jobManager.removeListener(this);
}

void ProgressDialog::backgroundJobsChanged() {
    content.progress = job.getCurrentTaskProgress();
    if (content.progress >= 1.0 && !job.isRunning()) {
        exitModalState(0);
        return;
    }

    // The title is the job's first name; a job with stages renames itself for each, shown by the percentage
    const auto percent = String(roundToInt(content.progress * 100.0)) + "%";
    const auto stage = job.getJobName();
    content.progressBar.setTextToDisplay(stage == getName() ? percent : stage + "  " + percent);
}

void ProgressDialog::timerCallback() {
    backgroundJobsChanged();
}

void ProgressDialog::ContentComponent::resized() {
    auto bounds = getLocalBounds().reduced(Space::dialog);

    auto buttonArea = bounds.removeFromBottom(Controls::actionHeight);
    cancelButton.setBounds(buttonArea.removeFromRight(Controls::actionWidth));

    bounds.removeFromBottom(Space::related);
    progressBar.setBounds(bounds.removeFromBottom(Controls::height));
}

} // namespace MoTool

