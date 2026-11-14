#pragma once

#include <JuceHeader.h>

namespace MoTool {

//==============================================================================
/**
    Icons as paths, so they look alike on every OS: a text glyph such as ⏸ comes from whichever
    font has it, colour emoji on Windows. Each is on a 24×24 grid, from Google's Material Icons
    (Apache License 2.0).
*/
namespace Icons {
const Path& play();
const Path& pause();
const Path& stop();
const Path& record();
const Path& toStart();
const Path& loop();
const Path& followScroll();  // an arrow
const Path& followPage();    // an arrow to a bar
const Path& power();
const Path& menu();
const Path& add();
const Path& stopwatch();

/** Fills `icon` in `colour`, its 24×24 grid scaled to the largest square in `area`, centred. */
void draw(Graphics& g, const Path& icon, Rectangle<float> area, Colour colour);

/** Draws `icon` in `area` as `button`'s label: in the text colour of its toggle state,
    half see-through while disabled. */
void drawOnButton(Graphics& g, const TextButton& button, const Path& icon, Rectangle<float> area);
}  // namespace Icons

//==============================================================================
/**
    A TextButton that shows an icon instead of its text; the text stays its accessible title.
*/
class IconButton : public TextButton
{
public:
    IconButton(const String& name, const Path& icon);

    void setIcon(const Path& icon);
    void paintButton(Graphics& g, bool highlighted, bool down) override;

protected:
    const Path& getIcon() const { return *icon_; }

private:
    const Path* icon_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconButton)
};

}  // namespace MoTool
