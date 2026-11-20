#include "Icons.h"

namespace MoTool {

namespace Icons {

// Path data from github.com/google/material-design-icons, src/<category>/<name>/materialicons/24px.svg
const Path& play() {
    static const Path p = Drawable::parseSVGPath("M8 5v14l11-7z");
    return p;
}
const Path& pause() {
    static const Path p = Drawable::parseSVGPath("M6 19h4V5H6v14zm8-14v14h4V5h-4z");
    return p;
}
const Path& stop() {
    static const Path p = Drawable::parseSVGPath("M6 6h12v12H6z");
    return p;
}

/** fiber_manual_record, a <circle> there */
const Path& record() {
    static const Path p = Drawable::parseSVGPath("M12 4a8 8 0 1 0 0 16a8 8 0 1 0 0-16z");
    return p;
}

/** skip_previous */
const Path& toStart() {
    static const Path p = Drawable::parseSVGPath("M6 6h2v12H6zm3.5 6l8.5 6V6z");
    return p;
}

/** repeat */
const Path& loop() {
    static const Path p = Drawable::parseSVGPath("M7 7h10v3l4-4-4-4v3H5v6h2V7zm10 10H7v-3l-4 4 4 4v-3h12v-6h-2v4z");
    return p;
}

/** arrow_right_alt */
const Path& followScroll() {
    static const Path p = Drawable::parseSVGPath("M16.01 11H4v2h12.01v3L20 12l-3.99-4z");
    return p;
}

/** keyboard_tab */
const Path& followPage() {
    static const Path p = Drawable::parseSVGPath(
        "M11.59 7.41L15.17 11H1v2h14.17l-3.59 3.59L13 18l6-6-6-6-1.41 1.41zM20 6v12h2V6h-2z");
    return p;
}

/** power_settings_new */
const Path& power() {
    static const Path p = Drawable::parseSVGPath(
        "M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42C17.99 7.86 19 9.81 19 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.19 1.01-4.14 "
        "2.58-5.42L6.17 5.17C4.23 6.82 3 9.26 3 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.74-1.23-5.18-3.17-6.83z");
    return p;
}

const Path& menu() {
    static const Path p = Drawable::parseSVGPath("M3 18h18v-2H3v2zm0-5h18v-2H3v2zm0-7v2h18V6H3z");
    return p;
}

const Path& add() {
    static const Path p = Drawable::parseSVGPath("M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z");
    return p;
}

/** timer, from materialiconsoutlined */
const Path& stopwatch() {
    static const Path p = Drawable::parseSVGPath(
        "M15 1H9v2h6V1z M11 14h2V8h-2V14z M19.03 7.39l1.42-1.42c-0.43-0.51-0.9-0.99-1.41-1.41l-1.42 1.42"
        "C16.07 4.74 14.12 4 12 4c-4.97 0-9 4.03-9 9s4.02 9 9 9s9-4.03 9-9C21 10.88 20.26 8.93 19.03 7.39z"
        "M12 20c-3.87 0-7-3.13-7-7s3.13-7 7-7s7 3.13 7 7S15.87 20 12 20z");
    return p;
}

void draw(Graphics& g, const Path& icon, Rectangle<float> area, Colour colour) {
    const float side = jmin(area.getWidth(), area.getHeight());
    const auto origin = area.withSizeKeepingCentre(side, side).getPosition();
    g.setColour(colour);
    g.fillPath(icon, AffineTransform::scale(side / 24.0f).translated(origin));
}

void drawOnButton(Graphics& g, const TextButton& button, const Path& icon, Rectangle<float> area) {
    const auto colour =
        button.findColour(button.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);
    draw(g, icon, area, colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
}

}  // namespace Icons

//==============================================================================
IconButton::IconButton(const String& name, const Path& icon) : TextButton(name), icon_(&icon) {
    setWantsKeyboardFocus(false);  // the keys stay with playback and the commands (docs/ux.md)
}

void IconButton::setIcon(const Path& icon) {
    icon_ = &icon;
    repaint();
}

void IconButton::paintButton(Graphics& g, bool highlighted, bool down) {
    getLookAndFeel().drawButtonBackground(
        g, *this, findColour(getToggleState() ? buttonOnColourId : buttonColourId), highlighted, down);
    // The icons' 24-unit grid has its own margin, so the grid takes most of the button
    const float side = 0.8f * static_cast<float>(jmin(getWidth(), getHeight()));
    Icons::drawOnButton(g, *this, *icon_, getLocalBounds().toFloat().withSizeKeepingCentre(side, side));
}

}  // namespace MoTool
