#pragma once

#include <JuceHeader.h>


namespace te = tracktion;

namespace MoTool {

class ProgressDialog : public DialogWindow,
                       private te::BackgroundJobManager::Listener,
                       private Timer
{
public:
    ProgressDialog(const String& title,
                   te::ThreadPoolJobWithProgress& j,
                   te::BackgroundJobManager& jm)
        : DialogWindow(title,
                       LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                       true)
        , job(j)
        , jobManager(jm)
    {
        // Setup components
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

        // Layout
        content.setSize(400, 100);
        setContentOwned(&content, true);
        centreAroundComponent(nullptr, getWidth(), getHeight());

        // Register with job manager
        jobManager.addListener(this);

        // Add job to manager
        // jobManager.addJob(&job, false); // Don't take ownership if caller needs to check result

        // Start refreshing if component isn't regularly updating
        startTimer(30); // Backup timer if manager's update is slower
    }

    ~ProgressDialog() override {
        jobManager.removeListener(this);
    }

    void backgroundJobsChanged() override {
        content.progress = job.getCurrentTaskProgress();
        if (content.progress >= 1.0 && !job.isRunning()) {
            exitModalState(0);
            return;
        }
    }

private:
    void timerCallback() override {
        // Just trigger a UI update
        backgroundJobsChanged();
    }

    class ContentComponent : public Component {
    public:
        void resized() override {
            auto bounds = getLocalBounds().reduced(10);

            auto buttonArea = bounds.removeFromBottom(30);
            cancelButton.setBounds(buttonArea.removeFromRight(100));

            bounds.removeFromBottom(6); // spacing
            progressBar.setBounds(bounds.removeFromBottom(24));

            bounds.removeFromBottom(6); // spacing
            taskLabel.setBounds(bounds);
        }

        ProgressBar progressBar { progress };
        Label taskLabel;
        TextButton cancelButton;
        double progress = 0.0;
    };

    ContentComponent content;
    te::ThreadPoolJobWithProgress& job;
    te::BackgroundJobManager& jobManager;
};

} // namespace MoTool