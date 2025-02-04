#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>


using namespace juce;

class TransportBar: public Component,
                    private ChangeListener {
public:

    explicit TransportBar(te::Edit& edit)
        : edit_ {edit}
        , transport_ {edit_.getTransport()}
    {
        transport_.addChangeListener(this);
        updatePlayButtonText();

        Helpers::addAndMakeVisible (*this, { &playPauseButton_ });
        playPauseButton_.onClick = [this] { EngineHelpers::togglePlay(edit_); };
    }

    void resized() override {
        // TODO use layout system
        auto b = getLocalBounds();
        int w = 60;
        int side = (b.getWidth() - w) / 2;
        b.removeFromLeft(side);
        b.removeFromRight(side);
        playPauseButton_.setBounds(b.reduced(2));
    }

    void changeListenerCallback (ChangeBroadcaster*) override {
        updatePlayButtonText();
    }

private:
    te::Edit& edit_;
    te::TransportControl& transport_;

    TextButton playPauseButton_ { "Play" };

    void updatePlayButtonText() {
        playPauseButton_.setButtonText(transport_.isPlaying() ? "||" : "[>]");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
