#pragma once

#include <JuceHeader.h>

namespace MoTool {

namespace te = tracktion;

//==============================================================================
/**
 * Audio settings panel that wraps JUCE's AudioDeviceSelectorComponent and blocks an
 * unplayable device configuration from being applied.
 *
 * On macOS, selecting an input device that differs from the output device makes CoreAudio run
 * them as an aggregate. With no usable input clock this freezes playback, and forcing input
 * channels on instead triggers a per-block engine assertion (tracktion_WaveInputDevice). uZX
 * synthesises its own audio and only needs input for recording, so a mismatched input device is
 * simply rejected: the selection is reverted to the previous valid one and the user is told why.
 */
class AudioSettingsComponent : public Component,
                               private ChangeListener {
public:
    explicit AudioSettingsComponent(juce::AudioDeviceManager& dm)
        : deviceManager_(dm)
    {
        selector_ = std::make_unique<AudioDeviceSelectorComponent>(
            deviceManager_, 0, 512, 1, 512, true, true, true, false);
        addAndMakeVisible(*selector_);

        // Remember the starting configuration as the baseline to revert to.
        deviceManager_.getAudioDeviceSetup(lastValidSetup_);
        deviceManager_.addChangeListener(this);
        validate();
    }

    ~AudioSettingsComponent() override {
        deviceManager_.removeChangeListener(this);
    }

    void resized() override {
        // Pin the selector to a fixed width and a generous height; it self-shrinks its own height
        // to fit its content (see AudioDeviceSelectorComponent::resized), so we read that back.
        // Width must be fixed - feeding back the live width lets the selector collapse it.
        selector_->setBounds(0, verticalPadding_, contentWidth_, 1000);
        fitDialogToContent();
    }

    void parentHierarchyChanged() override {
        fitDialogToContent();
    }

private:
    void changeListenerCallback(ChangeBroadcaster*) override {
        validate();
        // The selector adds/removes the input-channels row as the input device changes, so re-fit
        // the dialog to the new content height (otherwise empty space or clipping remains).
        if (selector_ != nullptr)
            selector_->setBounds(0, verticalPadding_, contentWidth_, 1000);
        fitDialogToContent();
    }

    /** Resize the enclosing dialog window so it tightly fits the device selector content height.
        The width is held fixed - the selector's internal layout collapses if it's allowed to drive
        the width, so we always pin it to contentWidth_. */
    void fitDialogToContent() {
        if (selector_ == nullptr)
            return;

        // The selector shrinks its own height to its content when laid out.
        const int contentHeight = selector_->getHeight();
        if (contentHeight <= 0)
            return;

        const int desiredContentH = contentHeight + 2 * verticalPadding_;

        if (auto* dialog = findParentComponentOfClass<ResizableWindow>()) {
            const int chromeH = dialog->getHeight() - getHeight();  // title bar + borders
            const int chromeW = dialog->getWidth()  - getWidth();
            const int targetW = contentWidth_ + chromeW;
            const int targetH = desiredContentH + chromeH;
            if (dialog->getWidth() != targetW || dialog->getHeight() != targetH)
                dialog->setSize(targetW, targetH);
        }
    }

    /** A configuration is valid when there is no separate input device: input must be either
        "none" or the same physical device as the output. */
    static bool isValid(const juce::AudioDeviceManager::AudioDeviceSetup& s) {
        return s.inputDeviceName.isEmpty() || s.inputDeviceName == s.outputDeviceName;
    }

    void validate() {
        if (reverting_)
            return;

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager_.getAudioDeviceSetup(setup);

        if (isValid(setup)) {
            lastValidSetup_ = setup;
            return;
        }

        // Reject the mismatched input device: revert to the last valid configuration. Keep the
        // output device the user just chose, but drop the incompatible input.
        const auto rejectedInput = setup.inputDeviceName;

        auto reverted = setup;
        reverted.inputDeviceName = (lastValidSetup_.inputDeviceName == setup.outputDeviceName)
                                       ? lastValidSetup_.inputDeviceName
                                       : juce::String();
        reverted.inputChannels.clear();
        reverted.useDefaultInputChannels = true;

        {
            const ScopedValueSetter<bool> svs(reverting_, true);
            deviceManager_.setAudioDeviceSetup(reverted, true);
        }
        deviceManager_.getAudioDeviceSetup(lastValidSetup_);

        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon,
            "Incompatible Audio Input",
            "The input \"" + rejectedInput + "\" can't be used together with output \""
                + setup.outputDeviceName + "\".\n\n"
                "On macOS, an input device that differs from the output device stops playback "
                "from advancing. The input has been reset. Choose the same device for input and "
                "output if you need recording, otherwise leave the input as \"<< none >>\".",
            "OK");
    }

    juce::AudioDeviceManager& deviceManager_;
    std::unique_ptr<AudioDeviceSelectorComponent> selector_;
    juce::AudioDeviceManager::AudioDeviceSetup lastValidSetup_;
    bool reverting_ = false;
    static constexpr int verticalPadding_ = 8;
    static constexpr int contentWidth_ = 500;
};

}  // namespace MoTool
