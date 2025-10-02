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
                   te::BackgroundJobManager& jm);

    ~ProgressDialog() override;

    void backgroundJobsChanged() override;

private:
    void timerCallback() override;

    class ContentComponent : public Component {
    public:
        void resized() override;

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
