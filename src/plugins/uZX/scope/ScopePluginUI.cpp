#include "ScopePluginUI.h"
#include "juce_core/juce_core.h"

namespace MoTool::uZX {

//==============================================================================
// WaveformDisplay implementation
//==============================================================================

WaveformDisplay::WaveformDisplay(juce::Colour colour,
                                 const juce::String& label,
                                 const ScopeBuffer& buffer,
                                 ScopeSettings& settings)
    : sourceBuffer_(buffer)
    , scopeSettings_(settings)
    , colour_(colour)
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
    // Copy samples from ring buffer into work buffer for trigger search
    int workSamples = windowSize_ * kTriggerSearchMultiplier;
    sourceBuffer_.copyTo(workBuffer_.data(), workSamples);

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

void WaveformDisplay::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        showPopupMenu();
    }
}

void WaveformDisplay::showPopupMenu() {
    juce::PopupMenu menu;

    // Add scope settings directly to menu (no submenu grouping)
    MoTool::addScopeSettingsMenu(menu, scopeSettings_, "");

    menu.showMenuAsync(juce::PopupMenu::Options());
}


//==============================================================================
// ScopePluginUI implementation
//==============================================================================

ScopePluginUI::ScopePluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , plugin_(*dynamic_cast<ScopePlugin*>(pluginPtr.get()))
    , triggerButton_(plugin_, plugin_.scopeSettings.triggerMode)
    , windowSlider_(plugin_, plugin_.scopeSettings.windowSamples)
    , gainSlider_(plugin_, plugin_.scopeSettings.gain)
    , levelSlider_(plugin_, plugin_.scopeSettings.triggerLevel)
{
    jassert(dynamic_cast<ScopePlugin*>(pluginPtr.get()) != nullptr);

    constrainer_.setMinimumWidth(224);
    constrainer_.setMinimumHeight(kChannelHeight * 2 + kControlsHeight + kSpacing * 3);

    // Create display components for stereo L and R
    for (size_t i = 0; i < displays_.size(); ++i) {
        displays_[i] = std::make_unique<WaveformDisplay>(
            channelColours_[i],
            juce::String(stereoLabels_[i]),
            *plugin_.getBuffer(static_cast<int>(i)),
            plugin_.scopeSettings
        );
        addAndMakeVisible(*displays_[i]);
    }

    // Add controls
    addAndMakeVisible(triggerButton_);
    addAndMakeVisible(windowSlider_);
    addAndMakeVisible(gainSlider_);
    addAndMakeVisible(levelSlider_);

    // Listen for parameter changes
    plugin_.scopeSettings.triggerMode.addListener(this);
    plugin_.scopeSettings.windowSamples.addListener(this);
    plugin_.scopeSettings.gain.addListener(this);
    plugin_.scopeSettings.triggerLevel.addListener(this);

    // Initial setup
    updateDisplayParams();

    // Initial size - match AY plugin width
    setSize(224, kChannelHeight * 2 + kControlsHeight + kSpacing * 3);
}

ScopePluginUI::~ScopePluginUI() {
    plugin_.scopeSettings.triggerMode.removeListener(this);
    plugin_.scopeSettings.windowSamples.removeListener(this);
    plugin_.scopeSettings.gain.removeListener(this);
    plugin_.scopeSettings.triggerLevel.removeListener(this);
}

void ScopePluginUI::valueChanged(juce::Value&) {
    updateDisplayParams();
}

void ScopePluginUI::updateDisplayParams() {
    const int window = plugin_.scopeSettings.windowSamples.getStoredValue();
    const float gain = plugin_.scopeSettings.gain.getStoredValue();
    const auto mode = plugin_.scopeSettings.triggerMode.getStoredValue();
    const float level = plugin_.scopeSettings.triggerLevel.getStoredValue();

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


void ScopePluginUI::paint(juce::Graphics& g) {
    // Background handled by displays and parent
}

void ScopePluginUI::resized() {
    auto r = getLocalBounds().reduced(kSpacing * 2);

    // Controls at top
    auto row = r.removeFromTop(kControlsHeight);

    // Layout controls horizontally with 40%, 20%, 20%, 20% widths
    const int totalWidth = row.getWidth();
    const int buttonWidth = static_cast<int>(row.getWidth() * 0.4f);
    triggerButton_.setBounds(row.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));

    row.removeFromLeft(kSpacing * 2);
    const int moduleWidth = row.getWidth() / 3;
    levelSlider_.setBounds(row.removeFromLeft(static_cast<int>(moduleWidth)));
    windowSlider_.setBounds(row.removeFromLeft(static_cast<int>(moduleWidth)));
    gainSlider_.setBounds(row);

    r.removeFromTop(kSpacing * 2);

    // Distribute remaining space among 2 displays (L and R)
    const int displayHeight = (r.getHeight() - (kSpacing * (displays_.size() - 1))) / displays_.size();

    for (size_t i = 0; i < displays_.size(); ++i) {
        if (displays_[i]) {
            displays_[i]->setBounds(r.removeFromTop(displayHeight));
            r.removeFromTop(kSpacing);
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
    // Add scope settings directly (Window, Gain, Trigger Mode, Trigger Level)
    MoTool::addScopeSettingsMenu(menu, plugin_.scopeSettings, "");
}

//==============================================================================
// Register plugin UI adapter
//==============================================================================

REGISTER_PLUGIN_UI_ADAPTER(ScopePlugin, ScopePluginUI)

}  // namespace MoTool::uZX
