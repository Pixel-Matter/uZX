#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace MoTool {

//==============================================================================
/**
    Maintains an ongoing measurement of the time spent in paint() calls.
    Similar to juce::AudioProcessLoadMeasurer but for GUI rendering.
*/
class GUIPaintMeasurer {
public:
    GUIPaintMeasurer() : lastPaintTimestamp_(juce::Time::getMillisecondCounterHiRes()) {}

    /** Resets the statistics. */
    void reset() {
        lastPaintTimeMs_.store(0.0);
        avgPaintTimeMs_.store(0.0);
        maxPaintTimeMs_.store(0.0);
        paintCount_.store(0);
        fps_.store(0.0);
        lastPaintTimestamp_ = juce::Time::getMillisecondCounterHiRes();
    }

    /** Returns the last paint time in milliseconds. */
    double getLastPaintTimeMs() const { return lastPaintTimeMs_.load(); }

    /** Returns the average paint time in milliseconds. */
    double getAvgPaintTimeMs() const { return avgPaintTimeMs_.load(); }

    /** Returns the maximum paint time recorded. */
    double getMaxPaintTimeMs() const { return maxPaintTimeMs_.load(); }

    /** Returns the number of paint calls recorded. */
    int getPaintCount() const { return paintCount_.load(); }

    /** Returns the current FPS (how often paint is called). */
    double getFPS() const { return fps_.load(); }

    /** Returns the interval since last paint in milliseconds. */
    double getIntervalMs() const { return intervalMs_.load(); }

    //==============================================================================
    /** RAII timer for measuring paint duration. */
    struct ScopedTimer {
        ScopedTimer(GUIPaintMeasurer& m) : measurer(m), startTime(juce::Time::getMillisecondCounterHiRes()) {
            measurer.updateFpsTracking();
        }

        ~ScopedTimer() {
            double elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
            measurer.registerPaintTime(elapsed);
        }

    private:
        GUIPaintMeasurer& measurer;
        double startTime;

        JUCE_DECLARE_NON_COPYABLE(ScopedTimer)
    };

    double getInstantIntervalMs() const {
        return juce::Time::getMillisecondCounterHiRes() - lastPaintTimestamp_;
    }

    void updateFpsTracking() {
        double now = juce::Time::getMillisecondCounterHiRes();
        double interval = now - lastPaintTimestamp_;
        lastPaintTimestamp_ = now;
        intervalMs_.store(interval);

        if (interval > 0.0) {
            double instantFps = 1000.0 / interval;
            double currentFps = fps_.load();
            if (currentFps == 0.0) {
                fps_.store(instantFps);
            } else {
                fps_.store(currentFps + alpha_ * (instantFps - currentFps));
            }
        }
    }

    /** Manually register a paint time (normally use ScopedTimer instead). */
    void registerPaintTime(double milliseconds) {

        lastPaintTimeMs_.store(milliseconds);

        // Update max
        double currentMax = maxPaintTimeMs_.load();
        while (milliseconds > currentMax && !maxPaintTimeMs_.compare_exchange_weak(currentMax, milliseconds)) {}

        // Update running average (exponential moving average)
        int count = paintCount_.fetch_add(1) + 1;
        if (count == 1) {
            avgPaintTimeMs_.store(milliseconds);
        } else {
            double currentAvg = avgPaintTimeMs_.load();
            avgPaintTimeMs_.store(currentAvg + alpha_ * (milliseconds - currentAvg));
        }
    }

    //==============================================================================
    /** Draws the paint statistics overlay in the visible (clipped) area. */
    void drawOverlay(juce::Graphics& g) const {
        double ms = lastPaintTimeMs_.load();
        double currentFps = fps_.load();
        double interval = intervalMs_.load();
        juce::String text = juce::String::formatted("%.1fms @%.0fms %.0ffps", ms, interval, currentFps);

        juce::Font font(juce::FontOptions("Iosevka Aile", 10.0f, juce::Font::plain));
        g.setFont(font);

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, 0.0f, 0.0f);
        auto textWidth = static_cast<int>(glyphs.getBoundingBox(0, -1, false).getWidth()) + 6;
        auto textHeight = 14;

        // Use clip bounds to position in visible area (top-right of what's actually visible)
        auto clipBounds = g.getClipBounds();

        auto textBounds = juce::Rectangle<int>(
            clipBounds.getRight() - textWidth - 2,
            clipBounds.getY() + 2,
            textWidth,
            textHeight
        );

        // Semi-transparent background
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(textBounds.toFloat(), 3.0f);

        // Text color based on performance (paint time)
        juce::Colour textColour = ms < 8.0 ? juce::Colours::lightgreen
                                 : ms < 16.0 ? juce::Colours::yellow
                                 : juce::Colours::red;

        g.setColour(textColour);
        g.drawText(text, textBounds, juce::Justification::centred);
    }

private:
    inline static constexpr double alpha_ = 0.1;  // EMA smoothing factor

    std::atomic<double> lastPaintTimeMs_{0.0};
    std::atomic<double> avgPaintTimeMs_{0.0};
    std::atomic<double> maxPaintTimeMs_{0.0};
    std::atomic<int> paintCount_{0};
    std::atomic<double> fps_{0.0};
    std::atomic<double> intervalMs_{0.0};
    double lastPaintTimestamp_{0.0};
};

} // namespace MoTool
