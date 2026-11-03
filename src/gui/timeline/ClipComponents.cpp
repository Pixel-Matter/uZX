#include "ClipComponents.h"
#include "TimelineGrid.h"
#include "../common/LookAndFeel.h"
#include "../../util/Oklch.h"


namespace te = tracktion;
using namespace std::literals;

namespace MoTool {

namespace {

/** The colour's hue at an OKLCH lightness, its chroma scaled: every clip colour, from the
    palette or chosen by hand, comes out equally light, so one text colour reads on all. */
Colour atLightness(Colour colour, float lightness, float chromaScale) {
    auto lch = Oklch::fromColour(colour);
    lch.L = lightness;
    lch.C *= chromaScale;
    return Oklch::toColour(lch);
}

}  // namespace

Rectangle<int> getShownArea(const Component& c) {
    auto area = c.getLocalBounds();
    for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
        area = area.getIntersection(c.getLocalArea(p, p->getLocalBounds()));
    return area;
}

//==============================================================================
ClipComponent::ClipComponent (EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c)
    : editViewState (evs), grid (g), clip (c)
{}

Colour ClipComponent::getFill(Colour lane, bool selected) {
    // Neutral, so the notes' channel colours stand out; lighter than the lane, a card on it
    auto lch = Oklch::fromColour(lane);
    lch.L += selected ? 0.07f : 0.035f;
    return Oklch::toColour(lch);
}

Colour ClipComponent::getRingColour(Colour clipColour) {
    return atLightness(clipColour, 0.92f, 0.4f);
}

void ClipComponent::paint(Graphics& g) {
    if (editViewState.transparentClips)
        return;

    // TODO Move to lookAndFeel
    const bool selected = editViewState.selectionManager.isSelected(clip.get());
    const auto bounds = getLocalBounds().toFloat();
    Path shape;
    shape.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                              cornerSize, cornerSize, false, false, true, true);
    const auto* track = clip->getTrack();
    const bool trackSelected = track != nullptr && editViewState.selectionManager.isSelected(track);
    g.setColour(getFill(Colors::Timeline::lane(trackSelected), selected));
    g.fillPath(shape);
}

void ClipComponent::mouseDown(const MouseEvent& e) {
    if (e.mods.isMiddleButtonDown())
        return;  // view navigation, see EditComponent
    if (!editViewState.disableClipSelection)
        editViewState.selectionManager.selectOnly(clip.get());
    clip->edit.getTransport().setPosition(grid.playheadTimeAtX(e.x + getX()));
}

//==============================================================================
ClipHeader::ClipHeader(EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c)
    : editViewState(evs), grid(g), clip(c)
{}

ClipHeader::Fit ClipHeader::fit(int width, int nameWidth, const std::vector<int>& itemWidths, int legendWidth) {
    constexpr int minNameWidth = 48;
    Fit result {itemWidths.size(), legendWidth > 0};
    const auto needed = [&] {
        int sum = jmin(nameWidth, minNameWidth) + (result.legend ? legendWidth : 0);
        for (size_t i = 0; i < result.items; ++i)
            sum += itemWidths[i];
        return sum;
    };
    if (needed() > width)
        result.legend = false;
    while (result.items > 0 && needed() > width)
        --result.items;
    return result;
}

Colour ClipHeader::getFill(Colour clipColour, bool selected) {
    // L 0.72 is the palette's own, see PsgClip::colourForTrackIndex(); selection lifts it
    return atLightness(clipColour, selected ? 0.82f : 0.72f, 1.0f);
}

void ClipHeader::paint(Graphics& g) {
    constexpr int pad = 3;

    const bool selected = editViewState.selectionManager.isSelected(clip.get());
    const auto bounds = getLocalBounds().toFloat();
    Path shape;
    shape.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                              ClipComponent::cornerSize, ClipComponent::cornerSize, true, true, false, false);
    g.setColour(getFill(clip->getColour(), selected));
    g.fillPath(shape);

    auto row = getShownArea(*this).reduced(pad);

    const Font nameFont(FontOptions(11.0f).withStyle("Bold"));
    const Font font(FontOptions(11.0f));
    const auto name = clip->getName();
    const auto warnings = getWarnings();
    const auto info = getInfo();

    // Each item with the gap before it; warnings are chips, with padding inside.
    std::vector<int> itemWidths;
    for (const auto& warning : warnings)
        itemWidths.push_back(pad + GlyphArrangement::getStringWidthInt(font, warning) + 2 * pad);
    if (info.isNotEmpty())
        itemWidths.push_back(pad + GlyphArrangement::getStringWidthInt(font, info));
    const bool showLegend = selected || editViewState.disableClipSelection;
    const int legendWidth = showLegend ? getLegendWidth() : 0;
    const int nameWidth = GlyphArrangement::getStringWidthInt(nameFont, name);
    const auto fitted = fit(row.getWidth(), nameWidth, itemWidths, legendWidth == 0 ? 0 : legendWidth + pad);

    if (fitted.legend) {
        auto legend = row.removeFromRight(legendWidth);
        row.removeFromRight(pad);
        // The legend's envelope and noise swatches are see-through, drawn for the dark body.
        g.setColour(Colors::Theme::backgroundAlt);
        g.fillRoundedRectangle(legend.expanded(1).toFloat(), 2.0f);
        paintLegend(g, legend);
    }

    int itemsWidth = 0;
    for (size_t i = 0; i < fitted.items; ++i)
        itemsWidth += itemWidths[i];
    g.setColour(textColour);
    g.setFont(nameFont);
    const auto nameArea = row.removeFromLeft(jmin(nameWidth, row.getWidth() - itemsWidth));
    g.drawText(name, nameArea, Justification::centredLeft, true);

    g.setFont(font);
    for (size_t i = 0; i < fitted.items; ++i) {
        row.removeFromLeft(pad);
        auto area = row.removeFromLeft(itemWidths[i] - pad);
        if (i < (size_t) warnings.size()) {
            g.setColour(Colors::Theme::warning.withAlpha(0.9f));
            g.fillRoundedRectangle(area.toFloat(), 2.0f);
            g.setColour(Colours::black);
            g.drawText(warnings[(int) i], area.reduced(pad, 0), Justification::centred, true);
        } else {
            g.setColour(textColour);
            g.drawText(info, area, Justification::centredLeft, true);
        }
    }
}

void ClipHeader::mouseDown(const MouseEvent& e) {
    if (e.mods.isMiddleButtonDown())
        return;  // view navigation, see EditComponent
    if (e.mods.isPopupMenu())
        return;
    if (editViewState.disableClipSelection)  // the Player: like anywhere else on the clip
        clip->edit.getTransport().setPosition(grid.playheadTimeAtX(e.x + getX()));
    else
        editViewState.selectionManager.selectOnly(clip.get());
}

AudioClipComponent::AudioClipComponent(EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c)
    : ClipComponent(evs, g, c)
{
    updateThumbnail();
}

void AudioClipComponent::paint(Graphics& g) {
    ClipComponent::paint(g);

    if (thumbnail != nullptr)
        drawWaveform(
            g, *getWaveAudioClip(), *thumbnail, Colours::white.withAlpha(0.5f), getX(), getRight(), 0, getHeight(), 0);
}

void AudioClipComponent::drawWaveform(Graphics& g, te::AudioClipBase& c, te::SmartThumbnail& thumb, Colour colour,
                                      int left, int right, int y, int h, int xOffset) {
    // left and right are in local coordinates of the clip component, simetimes wider than parent window
    auto p = getParentComponent();
    if (p == nullptr)
        return;

    auto localBounds = p->getLocalBounds();
    // draw bounds is intersection of clip component and parent component
    left = jmax(left, localBounds.getX());
    right = jmin(right, localBounds.getRight());

    auto t1 = editViewState.zoom.xToTime(left);
    auto t2 = editViewState.zoom.xToTime(right);

    tracktion::TimeRange region {t1, t2};

    jassert(left <= right);
    const auto gain = c.getGain();
    const auto pan = thumb.getNumChannels() == 1 ? 0.0f : c.getPan();

    const float pv = pan * gain;
    const float gainL = (gain - pv);
    const float gainR = (gain + pv);

    const bool usesTimeStretchedProxy = c.usesTimeStretchedProxy();

    const auto clipPos = c.getPosition();
    auto offset = clipPos.getOffset();
    auto speedRatio = c.getSpeedRatio();

    g.setColour(colour);

    // where to draw channles relative to clip component coordinates
    const Rectangle<int> area(-getX() + xOffset, y, right - left, h);

    if (usesTimeStretchedProxy) {
        if (!thumb.isOutOfDate()) {
            drawChannels(g,
                         thumb,
                         area,
                         region,
                         c.isLeftChannelActive(),
                         c.isRightChannelActive(),
                         gainL,
                         gainR);
        }
    } else if (c.getLoopLength() == 0s) {
        t1 = (region.getStart() + offset) * speedRatio;
        t2 = (region.getEnd() + offset) * speedRatio;

        drawChannels(g,
                     thumb,
                     area,
                     {t1, t2},
                     c.isLeftChannelActive(),
                     c.isRightChannelActive(),
                     gainL,
                     gainR);
    }
}

void AudioClipComponent::drawChannels (Graphics& g, te::SmartThumbnail& thumb, Rectangle<int> area,
                                       te::TimeRange time, bool useLeft, bool useRight,
                                       float leftGain, float rightGain) {
    if (useLeft && useRight && thumb.getNumChannels() > 1) {
        thumb.drawChannel (g, area.removeFromTop (area.getHeight() / 2), time, 0, leftGain);
        thumb.drawChannel (g, area, time, 1, rightGain);
    } else if (useLeft) {
        thumb.drawChannel (g, area, time, 0, leftGain);
    } else if (useRight) {
        thumb.drawChannel (g, area, time, 1, rightGain);
    }
}

void AudioClipComponent::updateThumbnail() {
    if (auto* wac = getWaveAudioClip()) {
        te::AudioFile af {wac->getAudioFile()};

        if (af.getFile().existsAsFile() || (! wac->usesSourceFile())) {
            if (af.isValid()) {
                const te::AudioFile proxy((wac->hasAnyTakes() && wac->isShowingTakes()) ? wac->getAudioFile() : wac->getPlaybackFile());

                if (thumbnail == nullptr)
                    thumbnail = std::make_unique<te::SmartThumbnail>(wac->edit.engine, proxy, *this, &wac->edit);
                else
                    thumbnail->setNewFile (proxy);
            } else {
                thumbnail = nullptr;
            }
        }
    }
}


//==============================================================================
MidiClipComponent::MidiClipComponent (EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c)
    : ClipComponent(evs, g, c)
{}

void MidiClipComponent::paint(Graphics& g) {
    ClipComponent::paint(g);
    auto* mc = getMidiClip();
    if (mc == nullptr) return;

    auto r = getLocalBounds();
    auto tr = mc->getEditTimeRange();

    auto timeToX = [width = r.getWidth(), tr, l = tr.getLength()] (auto time) {
        return roundToInt(((time - tr.getStart()) * width) / l);
    };

    auto& notes = mc->getSequence().getNotes();

    if (notes.size() == 0)
        return;

    // Get vertical scale settings from the track
    auto track = dynamic_cast<te::AudioTrack*>(mc->getTrack());
    if (!track) return;

    // Calculate visible range based on track settings
    const auto visibleProportion = track->getMidiVisibleProportion();  // (maxNote - minNote) / 128.0;
    const auto verticalOffset = track->getMidiVerticalOffset();        // 1.0 - (maxNote / 128.0)
    const int visibleRange = roundToInt(visibleProportion * 128.0);
    const int maxNote = roundToInt((1.0 - verticalOffset) * 128.0);
    const int minNote = maxNote - visibleRange;

    // Calculate height of one note
    float noteH = static_cast<float>(r.getHeight()) / visibleRange;
    const auto rangeStartSec = tr.getStart().inSeconds();
    const auto rangeEndSec = tr.getEnd().inSeconds();
    const float left = static_cast<float>(r.getX());

    for (auto n : notes) {
        // Only process notes within the visible vertical range
        int noteNumber = n->getNoteNumber();
        if (noteNumber < minNote || noteNumber > maxNote)
            continue;

        // Calculate time range for this note
        auto e = n->getEditEndTime(*mc);
        if (e.inSeconds() < rangeStartSec)
            continue;

        auto s = n->getEditStartTime(*mc);
        if (s.inSeconds() > rangeEndSec)
            break;

        float t1 = (float) timeToX(s) - left;
        float t2 = (float) timeToX(e) - left;

        // Map note position in the visible range (inverted since y=0 is at top)
        float y1 = (r.getHeight() - (noteNumber - minNote) * noteH);

        g.setColour(Colours::white.withAlpha(static_cast<float>(n->getVelocity()) / 127.0f));
        g.fillRect(t1, y1, t2 - t1, noteH);
    }
}

//==============================================================================
RecordingClipComponent::RecordingClipComponent (te::Track::Ptr t, EditViewState& evs)
    : track (t), editViewState (evs)
{
    startTimerHz(15);
    initialiseThumbnailAndPunchTime();
}

void RecordingClipComponent::initialiseThumbnailAndPunchTime()
{
    if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        for (auto idi : at->edit.getEditInputDevices().getDevicesForTargetTrack (*at))
        {
            punchInTime = idi->getPunchInTime (at->itemID);

            if (idi->getRecordingFile(at->itemID).exists())
                thumbnail = at->edit.engine.getRecordingThumbnailManager().getThumbnailFor(idi->getRecordingFile(at->itemID));
        }
    }
}

void RecordingClipComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::error.withAlpha(0.5f));
    // no outline
    // g.setColour(Colours::black);
    // g.drawRect(getLocalBounds());

    drawThumbnail(g, Colours::black.withAlpha(0.5f));
}

void RecordingClipComponent::drawThumbnail(Graphics& g, Colour waveformColour) const {
    if (thumbnail == nullptr)
        return;

    Rectangle<int> bounds;
    te::TimeRange times;
    getBoundsAndTime(bounds, times);
    auto w = bounds.getWidth();

    if (w > 0 && w < 10000) {
        g.setColour(waveformColour);
        thumbnail->thumb->drawChannels(g, bounds, times.getStart().inSeconds(), times.getEnd().inSeconds(), 1.0f);
    }
}

bool RecordingClipComponent::getBoundsAndTime(Rectangle<int>& bounds, tracktion::TimeRange& times) const {
    jassert(getParentComponent());
    auto editTimeToX = [this] (te::TimePosition t) {
        return roundToInt(editViewState.zoom.timeToX(t)) - getX();
    };

    auto xToEditTime = [this] (int x) {
        return editViewState.zoom.xToTime(x + getX());
    };

    bool hasLooped = false;
    auto& edit = track->edit;

    if (auto epc = edit.getTransport().getCurrentPlaybackContext()) {
        auto localBounds = getLocalBounds();

        auto timeStarted = thumbnail->punchInTime;
        auto unloopedPos = timeStarted + te::TimeDuration::fromSeconds (thumbnail->thumb->getTotalLength());

        auto t1 = timeStarted;
        auto t2 = unloopedPos;

        if (epc->isLooping() && t2 >= epc->getLoopTimes().getEnd()) {
            hasLooped = true;

            t1 = jmin(t1, epc->getLoopTimes().getStart());
            t2 = epc->getPosition();

            t1 = jmax(xToEditTime(0), t1);
            t2 = jmin(xToEditTime(getParentComponent()->getWidth()), t2);

        } else if (edit.recordingPunchInOut) {
            const auto in  = thumbnail->punchInTime;
            const auto out = edit.getTransport().getLoopRange().getEnd();

            t1 = jlimit(in, out, t1);
            t2 = jlimit(in, out, t2);
        }

        bounds = localBounds.withX(jmax(localBounds.getX(), editTimeToX (t1)))
                 .withRight(jmin(localBounds.getRight(), editTimeToX (t2)));

        auto loopRange = epc->getLoopTimes();
        const auto recordedTime = unloopedPos - toDuration(epc->getLoopTimes().getStart());
        const int numLoops = (int) (recordedTime / loopRange.getLength());

        const tracktion::TimeRange editTimes(xToEditTime(bounds.getX()),
                                             xToEditTime(bounds.getRight()));

        times = (editTimes + (loopRange.getLength() * numLoops)) - toDuration(timeStarted);
    }

    return hasLooped;
}

void RecordingClipComponent::timerCallback() {
    updatePosition();
}

void RecordingClipComponent::updatePosition() {
    auto& edit = track->edit;

    if (auto epc = edit.getTransport().getCurrentPlaybackContext()) {
        auto t1 = punchInTime >= 0s ? punchInTime : edit.getTransport().getTimeWhenStarted();
        auto t2 = jmax(t1, epc->getUnloopedPosition());

        if (epc->isLooping()) {
            auto loopTimes = epc->getLoopTimes();

            if (t2 >= loopTimes.getEnd()) {
                t1 = jmin(t1, loopTimes.getStart());
                t2 = loopTimes.getEnd();
            }
        } else if (edit.recordingPunchInOut) {
            auto mr = edit.getTransport().getLoopRange();
            auto in  = mr.getStart();
            auto out = mr.getEnd();

            t1 = jlimit(in, out, t1);
            t2 = jlimit(in, out, t2);
        }

        if (auto p = getParentComponent()) {
            auto x1 = roundToInt(editViewState.zoom.timeToX(t1));
            auto x2 = roundToInt(editViewState.zoom.timeToX(t2));
            x1 = jmax(x1, 0);
            x2 = jmin(x2, p->getWidth());
            setBounds(x1, 0, x2 - x1, p->getHeight());
            return;
        }
    }

    setBounds({});
}

}  // namespace MoTool
