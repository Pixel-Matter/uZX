#include "ClipComponents.h"
#include "juce_core/juce_core.h"


namespace te = tracktion;
using namespace std::literals;

namespace MoTool {

//==============================================================================
ClipComponent::ClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : editViewState (evs), clip (c)
{}

void ClipComponent::paint(Graphics& g) {
    // TODO Move to lookAndFeel
    g.setColour(clip->getColour().withMultipliedBrightness(0.25f).withAlpha(0.75f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    if (editViewState.selectionManager.isSelected(clip.get())) {
        g.setColour(Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0), 5.0f, 2.0f);
    } else {
        // g.setColour(Colors::Theme::border.withAlpha(0.5f));
        // g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0), 6.0f, 2.0f);
    }
}

void ClipComponent::mouseDown(const MouseEvent&) {
    editViewState.selectionManager.selectOnly(clip.get());
}

//==============================================================================
AudioClipComponent::AudioClipComponent(EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent(evs, c)
{
    updateThumbnail();
}

void AudioClipComponent::paint(Graphics& g) {
    ClipComponent::paint(g);

    if (editViewState.drawWaveforms && thumbnail != nullptr)
        drawWaveform(g, *getWaveAudioClip(), *thumbnail, Colours::white.withAlpha(0.5f));
}

void AudioClipComponent::drawWaveform(Graphics& g, te::AudioClipBase& c, te::SmartThumbnail& thumb, Colour colour) {
    const auto rect = getLocalBounds();
    const auto clipRange = c.getEditTimeRange();

    auto timeToX = [width = rect.getWidth(), clipRange, l = clipRange.getLength()] (auto time) {
        return roundToInt(((time - clipRange.getStart()) * width) / l);
    };
    auto region = editViewState.zoom.getRange();

    const auto gain = c.getGain();
    const auto pan = thumb.getNumChannels() == 1 ? 0.0f : c.getPan();

    const float pv = pan * gain;
    const float gainL = (gain - pv);
    const float gainR = (gain + pv);

    const bool usesTimeStretchedProxy = c.usesTimeStretchedProxy();

    const auto clipPos = c.getPosition();
    auto offset = clipPos.getOffset();
    auto speedRatio = c.getSpeedRatio();

    int left = timeToX(region.getStart());
    int right = timeToX(region.getEnd());
    int xOffset = 0;
    // int xOffset = timeToX(offset);
    int h = rect.getHeight();
    int y = rect.getY();
    const Rectangle<int> area(left + xOffset, y, right - left, h);

    g.setColour(colour);

    if (usesTimeStretchedProxy) {
        if (!thumb.isOutOfDate()) {
            drawChannels(g, thumb, area, region,
                         c.isLeftChannelActive(), c.isRightChannelActive(),
                         gainL, gainR);
        }
    } else if (c.getLoopLength() == 0s) {

        auto t1 = (region.getStart() + offset) * speedRatio;
        auto t2 = (region.getEnd()   + offset) * speedRatio;

        drawChannels(g, thumb, area, { t1, t2 },
                     c.isLeftChannelActive(), c.isRightChannelActive(),
                     gainL, gainR);
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
MidiClipComponent::MidiClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent(evs, c)
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
    g.fillAll(Colours::red.withAlpha(0.5f));
    // no outline
    // g.setColour(Colours::black);
    // g.drawRect(getLocalBounds());

    if (editViewState.drawWaveforms)
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
    auto editTimeToX = [this] (te::TimePosition t) {
        if (auto p = getParentComponent())
            return editViewState.zoom.timeToX(t, p->getWidth()) - getX();

        return 0;
    };

    auto xToEditTime = [this] (int x) {
        if (auto p = getParentComponent())
            return editViewState.zoom.xToTime(x + getX(), p->getWidth());

        return te::TimePosition();
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

            t1 = jmax(editViewState.zoom.getRange().getStart(), t1);
            t2 = jmin(editViewState.zoom.getRange().getEnd(), t2);
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

        t1 = jmax(t1, editViewState.zoom.getRange().getStart());
        t2 = jmin(t2, editViewState.zoom.getRange().getEnd());

        if (auto p = getParentComponent()) {
            int x1 = editViewState.zoom.timeToX(t1, p->getWidth());
            int x2 = editViewState.zoom.timeToX(t2, p->getWidth());

            setBounds(x1, 0, x2 - x1, p->getHeight());
            return;
        }
    }

    setBounds({});
}

}  // namespace MoTool
