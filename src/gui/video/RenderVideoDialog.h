#pragma once

#include <JuceHeader.h>

namespace MoTool {

/** File > Render Video: the settings, last used ones remembered, then where to save, then the render
    with its progress, and where the video went. */
void showRenderVideoDialog(tracktion::Edit&);

}  // namespace MoTool
