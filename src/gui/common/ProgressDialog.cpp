#include "ProgressDialog.h"

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
    content.taskLabel.setJustificationType(Justification::centred);
    content.taskLabel.setText(job.getJobName(), dontSendNotification);

    content.cancelButton.setButtonText("Cancel");
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
    content.addAndMakeVisible(content.taskLabel);

    content.setSize(400, 100);
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
}

void ProgressDialog::timerCallback() {
    backgroundJobsChanged();
}

void ProgressDialog::ContentComponent::resized() {
    auto bounds = getLocalBounds().reduced(10);

    auto buttonArea = bounds.removeFromBottom(30);
    cancelButton.setBounds(buttonArea.removeFromRight(100));

    bounds.removeFromBottom(6);
    progressBar.setBounds(bounds.removeFromBottom(24));

    bounds.removeFromBottom(6);
    taskLabel.setBounds(bounds);
}

} // namespace MoTool

