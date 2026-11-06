#include "ScopePluginUI.h"
#include "juce_core/juce_core.h"

#include <algorithm>
#include <cstring>

namespace MoTool::uZX {

std::vector<std::pair<float, float>> minMaxColumns(const float* samples, int numSamples, int columns) {
    jassert(columns > 0 && numSamples >= columns);
    std::vector<std::pair<float, float>> spans;
    spans.reserve(static_cast<size_t>(columns));

    for (int c = 0; c < columns; ++c) {
        const auto sliceStart = static_cast<int>(static_cast<juce::int64>(c) * numSamples / columns);
        const int begin = std::max(0, sliceStart - 1);
        const auto end = static_cast<int>(static_cast<juce::int64>(c + 1) * numSamples / columns);
        const auto [lo, hi] = std::minmax_element(samples + begin, samples + end);
        const bool falling = samples[end - 1] < samples[begin];
        spans.push_back(falling ? std::pair{*hi, *lo} : std::pair{*lo, *hi});
    }
    return spans;
}

//==============================================================================
// WaveformDisplay implementation
//==============================================================================

WaveformDisplay::WaveformDisplay(juce::Colour colour,
                                 const juce::String& label,
                                 const ScopeBuffer& buffer,
                                 ScopeSettings& settings,
                                 Baseline baseline)
    : sourceBuffer_(buffer)
    , scopeSettings_(settings)
    , colour_(colour)
    , label_(label)
    , baseline_(baseline)
    , triggerStrategy_(TriggerStrategy::risingEdge())
{
    displaySamples_.resize(static_cast<size_t>(windowSize_));
    workBuffer_.resize(static_cast<size_t>(windowSize_ * kTriggerSearchMultiplier));
}

void WaveformDisplay::setWindowSize(int samples) {
    if (windowSize_ != samples) {
        windowSize_ = samples;
        displaySamples_.resize(static_cast<size_t>(windowSize_));
        workBuffer_.resize(static_cast<size_t>(windowSize_ * kTriggerSearchMultiplier));
        settingsChanged_ = true;
    }
}

void WaveformDisplay::setGain(float gain) {
    gain_ = gain;
    settingsChanged_ = true;
}

void WaveformDisplay::setTriggerStrategy(const TriggerStrategy& strategy) {
    triggerStrategy_ = strategy;
    settingsChanged_ = true;
}

void WaveformDisplay::setTriggerLevel(float level) {
    triggerLevel_ = level;
    settingsChanged_ = true;
}

void WaveformDisplay::onVBlank() {
    const bool samplesChanged = updateWaveform();  // always runs: a settings change takes the new window too
    if (! samplesChanged && ! settingsChanged_)
        return;

    settingsChanged_ = false;
    buildPath();
    repaint();
}

bool WaveformDisplay::updateWaveform() {
    // Copy samples from ring buffer into work buffer for trigger search
    const int workSamples = windowSize_ * kTriggerSearchMultiplier;
    sourceBuffer_.copyTo(workBuffer_.data(), workSamples);

    // The trigger is searched in the first window, so a whole window always follows it
    const int triggerOffset = findTriggerPoint();
    jassert(triggerOffset >= 0 && triggerOffset + windowSize_ <= workSamples);
    const float* window = workBuffer_.data() + triggerOffset;
    const auto bytes = displaySamples_.size() * sizeof(float);

    // Silence, a held tone or a stopped transport give the same window: nothing to repaint
    if (std::memcmp(displaySamples_.data(), window, bytes) == 0)
        return false;

    std::memcpy(displaySamples_.data(), window, bytes);
    return true;
}

int WaveformDisplay::findTriggerPoint() {
    // Search in the first half of work buffer to leave room for display window
    int searchLength = windowSize_;
    return triggerStrategy_(workBuffer_.data(), searchLength, triggerLevel_);
}

WaveformDisplay::YAxis WaveformDisplay::yAxis(float height) const {
    constexpr float margin = 0.05f;
    if (baseline_ == Baseline::Bottom)
        return {height * (1.0f - margin), height * (1.0f - 2.0f * margin) * gain_};
    return {height * 0.5f, height * (0.5f - margin) * gain_};
}

void WaveformDisplay::buildPath() {
    waveformPath_.clear();

    if (displaySamples_.empty() || getWidth() <= 0 || getHeight() <= 0)
        return;

    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    const auto axis = yAxis(height);

    const auto toY = [&](float sample) { return juce::jlimit(0.0f, height, axis.zero - sample * axis.scale); };
    const auto addPoint = [this](size_t i, float x, float y) {
        if (i == 0)
            waveformPath_.startNewSubPath(x, y);
        else
            waveformPath_.lineTo(x, y);
    };

    const int numSamples = static_cast<int>(displaySamples_.size());
    const int columns = juce::roundToInt(width * Component::getApproximateScaleFactorForComponent(this));

    // More samples than physical pixels: one min/max span per pixel column
    if (columns >= 2 && numSamples > 2 * columns) {
        const auto spans = minMaxColumns(displaySamples_.data(), numSamples, columns);
        const float xStep = width / static_cast<float>(columns - 1);
        for (size_t c = 0; c < spans.size(); ++c) {
            const float x = static_cast<float>(c) * xStep;
            addPoint(c, x, toY(spans[c].first));
            waveformPath_.lineTo(x, toY(spans[c].second));
        }
        return;
    }

    const float xStep = width / static_cast<float>(numSamples - 1);
    for (size_t i = 0; i < displaySamples_.size(); ++i)
        addPoint(i, static_cast<float>(i) * xStep, toY(displaySamples_[i]));
}

void WaveformDisplay::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(Colors::Theme::backgroundDark);
    g.fillRect(bounds);

    // Zero line; a silent chip channel's waveform lies there itself
    const auto axis = yAxis(bounds.getHeight());
    const bool chipLevels = baseline_ == Baseline::Bottom;
    if (! chipLevels) {
        g.setColour(Colors::Theme::border);
        g.drawHorizontalLine(static_cast<int>(axis.zero), 0.0f, bounds.getWidth());
    }

    // Trigger level line, unless it would lie along the chip's silence
    if (triggerStrategy_.mode != TriggerMode(TriggerModeEnum::FreeRunning) && (! chipLevels || triggerLevel_ > 0.0f)) {
        g.setColour(Colors::Theme::warning.withAlpha(0.5f));
        const float trigY = axis.zero - triggerLevel_ * axis.scale;
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
