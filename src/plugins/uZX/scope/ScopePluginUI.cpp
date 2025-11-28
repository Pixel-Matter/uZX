#include "ScopePluginUI.h"

namespace MoTool::uZX {

//==============================================================================
// WaveformDisplay implementation
//==============================================================================

WaveformDisplay::WaveformDisplay(juce::Colour colour, const juce::String& label)
    : colour_(colour)
    , label_(label)
    , triggerStrategy_(TriggerStrategy::risingEdge())
{
    displaySamples_.resize(static_cast<size_t>(windowSize_));
    workBuffer_.resize(static_cast<size_t>(windowSize_ * kTriggerSearchMultiplier));
    startTimerHz(kRefreshRateHz);
}

WaveformDisplay::~WaveformDisplay() {
    stopTimer();
}

void WaveformDisplay::setBuffer(const ScopeBuffer* buffer) {
    sourceBuffer_ = buffer;
}

void WaveformDisplay::setWindowSize(int samples) {
    if (windowSize_ != samples) {
        windowSize_ = samples;
        displaySamples_.resize(static_cast<size_t>(windowSize_));
        workBuffer_.resize(static_cast<size_t>(windowSize_ * kTriggerSearchMultiplier));
    }
}

void WaveformDisplay::setGain(float gain) {
    gain_ = gain;
}

void WaveformDisplay::setTriggerStrategy(const TriggerStrategy& strategy) {
    triggerStrategy_ = strategy;
}

void WaveformDisplay::setTriggerLevel(float level) {
    triggerLevel_ = level;
}

void WaveformDisplay::timerCallback() {
    updateWaveform();
    repaint();
}

void WaveformDisplay::updateWaveform() {
    if (sourceBuffer_ == nullptr)
        return;

    // Copy samples from ring buffer into work buffer for trigger search
    int workSamples = windowSize_ * kTriggerSearchMultiplier;
    sourceBuffer_->copyTo(workBuffer_.data(), workSamples);

    // Find trigger point
    int triggerOffset = findTriggerPoint();

    // Copy display window starting from trigger point
    int copyStart = triggerOffset;
    int copyCount = std::min(windowSize_, workSamples - triggerOffset);
    
    if (copyCount > 0) {
        std::copy(workBuffer_.begin() + copyStart,
                  workBuffer_.begin() + copyStart + copyCount,
                  displaySamples_.begin());
    }
    
    // Fill remaining with zeros if needed
    if (copyCount < windowSize_) {
        std::fill(displaySamples_.begin() + copyCount, displaySamples_.end(), 0.0f);
    }

    buildPath();
}

int WaveformDisplay::findTriggerPoint() {
    // Search in the first half of work buffer to leave room for display window
    int searchLength = windowSize_;
    return triggerStrategy_(workBuffer_.data(), searchLength, triggerLevel_);
}

void WaveformDisplay::buildPath() {
    waveformPath_.clear();
    
    if (displaySamples_.empty() || getWidth() <= 0 || getHeight() <= 0)
        return;

    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    const float midY = height * 0.5f;
    const float scaleY = midY * gain_;
    
    const int numSamples = static_cast<int>(displaySamples_.size());
    const float xStep = width / static_cast<float>(numSamples - 1);

    // Start path
    float x = 0.0f;
    float y = midY - displaySamples_[0] * scaleY;
    y = juce::jlimit(0.0f, height, y);
    waveformPath_.startNewSubPath(x, y);

    // Draw line through all samples
    for (int i = 1; i < numSamples; ++i) {
        x = static_cast<float>(i) * xStep;
        y = midY - displaySamples_[static_cast<size_t>(i)] * scaleY;
        y = juce::jlimit(0.0f, height, y);
        waveformPath_.lineTo(x, y);
    }
}

void WaveformDisplay::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(Colors::Theme::backgroundDark);
    g.fillRect(bounds);

    // Center line (zero crossing reference)
    g.setColour(Colors::Theme::border);
    const float midY = bounds.getHeight() * 0.5f;
    g.drawHorizontalLine(static_cast<int>(midY), 0.0f, bounds.getWidth());

    // Trigger level line
    if (triggerStrategy_.mode != TriggerMode(TriggerModeEnum::FreeRunning)) {
        g.setColour(Colors::Theme::warning.withAlpha(0.5f));
        const float trigY = midY - triggerLevel_ * midY * gain_;
        g.drawHorizontalLine(static_cast<int>(trigY), 0.0f, bounds.getWidth());
    }

    // Waveform
    g.setColour(colour_);
    g.strokePath(waveformPath_, juce::PathStrokeType(1.5f));

    // Channel label
    g.setColour(colour_.withAlpha(0.7f));
    g.setFont(12.0f);
    g.drawText(label_, bounds.reduced(4.0f), juce::Justification::topLeft);
}

void WaveformDisplay::resized() {
    buildPath();
}


//==============================================================================
// ScopePluginUI implementation
//==============================================================================

ScopePluginUI::ScopePluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , plugin_(*dynamic_cast<ScopePlugin*>(pluginPtr.get()))
    , triggerButton_(plugin_, plugin_.staticParams.triggerMode)
    , windowSlider_(plugin_, plugin_.staticParams.windowSamples)
    , gainSlider_(plugin_, plugin_.staticParams.gain)
    , levelSlider_(plugin_, plugin_.staticParams.triggerLevel)
{
    jassert(dynamic_cast<ScopePlugin*>(pluginPtr.get()) != nullptr);

    constrainer_.setMinimumWidth(160);
    constrainer_.setMinimumHeight(kChannelHeight + kControlsHeight + kSpacing * 2);

    // Create display components (will be configured in updateDisplayCount)
    for (size_t i = 0; i < displays_.size(); ++i) {
        displays_[i] = std::make_unique<WaveformDisplay>(
            channelColours_[i],
            juce::String(ayChannelLabels_[i])
        );
        displays_[i]->setBuffer(plugin_.getBuffer(static_cast<int>(i)));
        addChildComponent(*displays_[i]);
    }

    // Add controls
    addAndMakeVisible(triggerButton_);
    addAndMakeVisible(windowSlider_);
    addAndMakeVisible(gainSlider_);
    addAndMakeVisible(levelSlider_);

    // Listen for parameter changes
    plugin_.staticParams.triggerMode.addListener(this);
    plugin_.staticParams.windowSamples.addListener(this);
    plugin_.staticParams.gain.addListener(this);
    plugin_.staticParams.triggerLevel.addListener(this);

    // Initial setup
    updateDisplayParams();
    updateDisplayCount();

    // Initial size based on expected channels (will adjust dynamically)
    setSize(200, kChannelHeight * 2 + kControlsHeight + kSpacing * 3);
}

ScopePluginUI::~ScopePluginUI() {
    plugin_.staticParams.triggerMode.removeListener(this);
    plugin_.staticParams.windowSamples.removeListener(this);
    plugin_.staticParams.gain.removeListener(this);
    plugin_.staticParams.triggerLevel.removeListener(this);
}

void ScopePluginUI::valueChanged(juce::Value&) {
    updateDisplayParams();
}

void ScopePluginUI::updateDisplayParams() {
    const int window = plugin_.staticParams.windowSamples.getStoredValue();
    const float gain = plugin_.staticParams.gain.getStoredValue();
    const auto mode = plugin_.staticParams.triggerMode.getStoredValue();
    const float level = plugin_.staticParams.triggerLevel.getStoredValue();
    
    auto strategy = TriggerStrategy::fromMode(mode);

    for (auto& display : displays_) {
        if (display) {
            display->setWindowSize(window);
            display->setGain(gain);
            display->setTriggerStrategy(strategy);
            display->setTriggerLevel(level);
        }
    }
}

void ScopePluginUI::updateDisplayCount() {
    const auto inputMode = plugin_.getInputMode();
    const int numDisplayChannels = plugin_.getNumDisplayChannels();
    
    // Check if mode changed
    if (currentInputMode_ == inputMode && visibleDisplayCount_ == numDisplayChannels)
        return;
    
    currentInputMode_ = inputMode;
    visibleDisplayCount_ = numDisplayChannels;
    
    if (inputMode == ScopePlugin::InputMode::AYSeparate) {
        // AY 5-channel mode: Show 3 displays (A, B, C)
        for (size_t i = 0; i < 3; ++i) {
            displays_[i] = std::make_unique<WaveformDisplay>(
                channelColours_[i],
                juce::String(ayChannelLabels_[i])
            );
            displays_[i]->setBuffer(plugin_.getBuffer(static_cast<int>(i)));
            addAndMakeVisible(*displays_[i]);
        }
    } else {
        // Stereo mode: Show 2 displays (L, R)
        for (size_t i = 0; i < 2; ++i) {
            displays_[i] = std::make_unique<WaveformDisplay>(
                channelColours_[i],
                juce::String(stereoLabels_[i])
            );
            displays_[i]->setBuffer(plugin_.getBuffer(static_cast<int>(i)));
            addAndMakeVisible(*displays_[i]);
        }
        
        // Hide third display
        if (displays_[2])
            displays_[2]->setVisible(false);
    }
    
    // Show/hide displays based on count
    for (size_t i = 0; i < displays_.size(); ++i) {
        if (displays_[i])
            displays_[i]->setVisible(static_cast<int>(i) < visibleDisplayCount_);
    }
    
    updateDisplayParams();
    resized();
}


void ScopePluginUI::paint(juce::Graphics& g) {
    // Background handled by displays and parent
}

void ScopePluginUI::resized() {
    auto r = getLocalBounds().reduced(kSpacing);

    // Controls at bottom
    auto controlsArea = r.removeFromBottom(kControlsHeight);
    
    // Layout controls horizontally
    const int controlWidth = controlsArea.getWidth() / 4;
    triggerButton_.setBounds(controlsArea.removeFromLeft(controlWidth).reduced(2));
    levelSlider_.setBounds(controlsArea.removeFromLeft(controlWidth).reduced(2));
    windowSlider_.setBounds(controlsArea.removeFromLeft(controlWidth).reduced(2));
    gainSlider_.setBounds(controlsArea.reduced(2));

    r.removeFromBottom(kSpacing);

    // Check for input mode changes
    const auto currentMode = plugin_.getInputMode();
    const int currentChannels = plugin_.getNumDisplayChannels();
    if (currentMode != currentInputMode_ || currentChannels != visibleDisplayCount_) {
        updateDisplayCount();
    }

    // Distribute remaining space among visible displays
    if (visibleDisplayCount_ > 0) {
        const int displayHeight = r.getHeight() / visibleDisplayCount_;
        
        for (int i = 0; i < visibleDisplayCount_; ++i) {
            if (displays_[static_cast<size_t>(i)]) {
                displays_[static_cast<size_t>(i)]->setBounds(r.removeFromTop(displayHeight));
            }
        }
    }
}

juce::ComponentBoundsConstrainer* ScopePluginUI::getBoundsConstrainer() {
    return &constrainer_;
}

bool ScopePluginUI::hasDeviceMenu() const {
    return true;
}

void ScopePluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    // Window size presets
    juce::PopupMenu windowMenu;
    const std::array<int, 5> windowPresets = {256, 512, 1024, 2048, 4096};
    for (int preset : windowPresets) {
        windowMenu.addItem(juce::String(preset) + " samples",
            [this, preset]() {
                plugin_.staticParams.windowSamples.setStoredValue(preset);
            });
    }
    menu.addSubMenu("Window Size", windowMenu);

    // Gain presets
    juce::PopupMenu gainMenu;
    const std::array<float, 5> gainPresets = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    for (float preset : gainPresets) {
        gainMenu.addItem(juce::String(preset, 1) + "x",
            [this, preset]() {
                plugin_.staticParams.gain.setStoredValue(preset);
            });
    }
    menu.addSubMenu("Gain", gainMenu);

    // Sidechain source selection
    juce::PopupMenu sidechainMenu;

    // Add "None" option to clear sidechain
    sidechainMenu.addItem("None",
        [this]() {
            plugin_.setSidechainSourceID(te::EditItemID());
            DBG("ScopePlugin: Cleared sidechain source");
        });

    sidechainMenu.addSeparator();

    // Add "Auto (Previous Plugin)" option
    sidechainMenu.addItem("Auto (Previous Plugin)",
        [this]() {
            plugin_.autoSetSidechainToPreviousPlugin();
        });

    sidechainMenu.addSeparator();

    // Add all plugins on the track
    auto pluginsOnTrack = plugin_.getPluginsOnTrack();
    const auto currentSidechainID = plugin_.getSidechainSourceID();

    for (auto& otherPlugin : pluginsOnTrack) {
        if (otherPlugin.get() == &plugin_)
            continue;  // Skip self

        const bool isCurrent = (otherPlugin->itemID == currentSidechainID);
        const juce::String itemText = otherPlugin->getName() + (isCurrent ? " ✓" : "");

        sidechainMenu.addItem(itemText,
            [this, pluginPtr = otherPlugin]() {
                plugin_.setSidechainSourceID(pluginPtr->itemID);
                DBG("ScopePlugin: Set sidechain source to " << pluginPtr->getName());
            });
    }

    menu.addSubMenu("Sidechain Source", sidechainMenu);
}

//==============================================================================
// Register plugin UI adapter
//==============================================================================

REGISTER_PLUGIN_UI_ADAPTER(ScopePlugin, ScopePluginUI)

}  // namespace MoTool::uZX
