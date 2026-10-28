#pragma once

#include "PsgClipComponent.h"
#include "TimelineGrid.h"

namespace MoTool::Tests {

inline Image renderPsgClip(te::Engine& engine, uZX::PsgData data, int width, int height,
                           Range<int> visibleFrames = {}, bool header = false, Rectangle<int> repaint = {}) {
    jassert(! data.frames.empty());
    if (visibleFrames.isEmpty())
        visibleFrames = {0, static_cast<int>(data.frames.size())};
    jassert(visibleFrames.getStart() >= 0 && visibleFrames.getEnd() <= static_cast<int>(data.frames.size()));

    auto edit = te::Edit::createSingleTrackEdit(engine);
    const auto duration = te::TimeDuration::fromSeconds(data.getLengthSeconds());
    auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                {{te::TimePosition(), duration}, {}}, "PSG example");
    te::SelectionManager selection(engine);
    EditViewState view(*edit, selection);
    const auto frameDuration = duration / static_cast<double>(data.frames.size());
    view.zoom.setViewWidthPx(width);
    view.zoom.setRange({te::TimePosition() + frameDuration * visibleFrames.getStart(),
                        frameDuration * visibleFrames.getLength()});
    TimelineGrid grid(view);
    PsgClipComponent component(view, grid, clip);
    const int frameWidth = width / visibleFrames.getLength();
    component.setSize(frameWidth * static_cast<int>(data.frames.size()), height);
    Image image(Image::ARGB, width, height, true);
    Graphics graphics(image);
    if (! repaint.isEmpty())
        graphics.reduceClipRegion(repaint);
    graphics.addTransform(AffineTransform::translation(-static_cast<float>(frameWidth * visibleFrames.getStart()), 0));
    if (header)
        component.paintHeader(graphics);
    else
        component.paintNotes(graphics);
    return image;
}

}  // namespace MoTool::Tests
