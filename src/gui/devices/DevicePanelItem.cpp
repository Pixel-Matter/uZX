#include "DevicePanelItem.h"
#include "PluginDeviceUI.h"


namespace MoTool {

//==============================================================================
// BackgroundlessIconButton implementation

BackgroundlessIconButton::BackgroundlessIconButton(const String& name, const Path& icon)
    : IconButton(name, icon)
{
    setupColors();
}

void BackgroundlessIconButton::setupColors() {
    setColour(TextButton::buttonColourId, Colours::transparentBlack);
    setColour(TextButton::buttonOnColourId, Colours::transparentBlack);
    setColour(TextButton::textColourOffId, Colors::Theme::textDisabled);
    setColour(TextButton::textColourOnId, Colors::Theme::primary);
}

Colour BackgroundlessIconButton::getHighlightedTextColor(Colour textColor) const {
    return textColor.brighter(0.4f);
}

void BackgroundlessIconButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    // Only the icon, no background
    Colour textColour;
    if (getToggleState()) {
        textColour = findColour(TextButton::textColourOnId);
    } else {
        textColour = findColour(TextButton::textColourOffId);
    }

    if (shouldDrawButtonAsHighlighted)
        textColour = getHighlightedTextColor(textColour);

    if (shouldDrawButtonAsDown)
        textColour = textColour.darker(0.3f);

    Icons::draw(g, getIcon(), getLocalBounds().toFloat(), textColour);
}

//==============================================================================
// TitleBar implementation
TitleBar::TitleBar(tracktion::Plugin::Ptr plugin, PluginDeviceUI* deviceUI)
    : plugin_(plugin)
    , deviceUI_(deviceUI)
{
    setSize(getWidth(), titleBarHeight);

    // Setup enable/disable button
    enableButton_.setSize(buttonWidth, buttonWidth);
    enableButton_.setToggleable(true);
    enableButton_.setToggleState(plugin->isEnabled(), dontSendNotification);
    enableButton_.onClick = [this]() {
        plugin_->setEnabled(!enableButton_.getToggleState());
        enableButton_.setToggleState(plugin_->isEnabled(), dontSendNotification);
    };
    addAndMakeVisible(enableButton_);

    menuButton_.setTooltip("Plugin options");
    menuButton_.onClick = [this]() { showDeviceMenu(&menuButton_); };
    addAndMakeVisible(menuButton_);
    refreshMenuButtonState();
}

void TitleBar::setCollapseToggle(std::function<void()> handler) {
    toggleCollapsed_ = std::move(handler);
}

void TitleBar::setCollapsed(bool collapsed) {
    if (isCollapsed_ == collapsed)
        return;

    isCollapsed_ = collapsed;
    resized();
    repaint();
}

void TitleBar::paint(Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    Path path;
    if (isCollapsed_) {
        path.addRoundedRectangle(bounds, FramedDeviceItem::cornerSize);
    } else {
        path.addRoundedRectangle(
            bounds.getX(), bounds.getY(),
            bounds.getWidth(), bounds.getHeight(),
            FramedDeviceItem::cornerSize, FramedDeviceItem::cornerSize,
            true, true, false, false
        );
    }
    g.setColour(findColour(TextButton::buttonColourId));
    g.fillPath(path);

    if (plugin_) {
        g.setColour(Colors::Theme::textPrimary);
        auto font = g.getCurrentFont();
        g.setFont(font.withPointHeight(Fonts::caption).withExtraKerningFactor(0.03f).withStyle(Font::bold));

        if (isCollapsed_) {
            const int controlBottom = menuButton_.isVisible()
                                          ? menuButton_.getBottom()
                                          : enableButton_.getBottom();
            const int textEnd = controlBottom;
            const int available = getHeight() - textEnd - 4;
            if (available > 0) {
                Graphics::ScopedSaveState guard(g);
                g.addTransform(AffineTransform::translation((float) -getHeight(), 0.0f)
                                 .followedBy(AffineTransform::rotation(-MathConstants<float>::halfPi)));
                Rectangle<int> textBounds(4, 0, available, getWidth());
                g.drawText(plugin_->getName(), textBounds, Justification::centred, true);
            }
        } else {
            // Draw plugin name, accounting for button and menu controls
            int textX = buttonWidth + buttonMargin * 2 + 4;
            int rightMargin = menuButton_.isVisible() ? (buttonWidth + buttonMargin * 2 + 4) : 4;
            auto textBounds = Rectangle<int>(textX, 0, getWidth() - textX - rightMargin, getHeight());
            g.drawText(plugin_->getName(), textBounds, Justification::centredLeft, true);
        }
    }
}

void TitleBar::resized() {
    refreshMenuButtonState();

    if (isCollapsed_) {
        auto area = getLocalBounds().reduced(buttonMargin);
        const int controlWidth = jmin(buttonWidth, jmax(0, area.getWidth()));
        const int controlHeight = controlWidth;
        const int x = area.getX() + (area.getWidth() - controlWidth) / 2;
        enableButton_.setBounds(x, area.getY(), controlWidth, controlHeight);
        auto nextY = enableButton_.getBottom() + buttonMargin;
        if (menuButton_.isVisible()) {
            menuButton_.setBounds(x, nextY, controlWidth, controlHeight);
        } else {
            menuButton_.setBounds(0, 0, 0, 0);
        }
    } else {
        auto buttonHeight = getHeight() - buttonMargin * 2;
        enableButton_.setBounds(buttonMargin, buttonMargin, buttonWidth, buttonHeight);

        if (menuButton_.isVisible()) {
            menuButton_.setBounds(getWidth() - buttonMargin - buttonWidth, buttonMargin, buttonWidth, buttonHeight);
        }
    }
}

void TitleBar::refreshMenuButtonState() {
    const bool hasCustomMenu = deviceUI_ != nullptr && deviceUI_->hasDeviceMenu();
    const bool hasPluginMenu = plugin_ != nullptr;
    const bool shouldShowMenu = hasCustomMenu || hasPluginMenu;
    menuButton_.setVisible(shouldShowMenu);
    menuButton_.setInterceptsMouseClicks(shouldShowMenu, shouldShowMenu);
    menuButton_.setEnabled(shouldShowMenu);
}

void TitleBar::mouseUp(const MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        showDeviceMenu(this);
    }
}

void TitleBar::mouseDoubleClick(const MouseEvent& event) {
    if (!event.mods.isPopupMenu() && toggleCollapsed_ != nullptr) {
        toggleCollapsed_();
    }
}

void TitleBar::showDeviceMenu(Component* target) {
    PopupMenu menu;

    const auto addInfoLine = [&menu](const String& text) {
        if (text.isNotEmpty()) {
            PopupMenu::Item infoItem(text);
            infoItem.isEnabled = false;
            menu.addItem(std::move(infoItem));
        }
    };

    bool hasInfoSection = false;
    if (plugin_ != nullptr) {
        addInfoLine(plugin_->getName().trim());
        if (const auto vendor = plugin_->getVendor().trim(); vendor.isNotEmpty())
            addInfoLine("by " + vendor);

        // auto description = plugin_->getSelectableDescription().trim();
        // if (description.isEmpty())
        //     description = plugin_->getName();
        // addInfoLine(description);

        hasInfoSection = menu.getNumItems() > 0;
    }

    PopupMenu customItems;
    if (deviceUI_ != nullptr) {
        deviceUI_->populateDeviceMenu(customItems);
    }

    const bool hasCustomItems = customItems.getNumItems() > 0;
    const bool hasPluginActions = plugin_ != nullptr;

    if (hasInfoSection && (hasCustomItems || hasPluginActions))
        menu.addSeparator();

    if (hasCustomItems) {
        for (PopupMenu::MenuItemIterator it(customItems); it.next();) {
            menu.addItem(it.getItem());
        }
    }

    if (hasPluginActions) {
        if (menu.getNumItems() > 0)
            menu.addSeparator();

        menu.addItem("Remove Device", [this] {
            plugin_->deleteFromParent();
        });
    }

    if (menu.getNumItems() > 0) {
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(target));
    }
}

//==============================================================================
// DevicePanelItem implementation

DeviceItem::DeviceItem(std::unique_ptr<PluginDeviceUI> ui)
    : plugin_(ui->getPlugin())
    , ui_(std::move(ui))
{
}

int DeviceItem::getMinimumHeight() const {
    return ui_ != nullptr ? ui_->getMinimumHeight() : 0;
}

//==============================================================================
// FramelessDeviceItem implementation

FramelessDeviceItem::FramelessDeviceItem(std::unique_ptr<PluginDeviceUI> ui)
    : DeviceItem(std::move(ui))
{
    if (ui_) {
        addAndMakeVisible(ui_.get());
        setSize(ui_->getWidth(), ui_->getHeight());
    }
}

void FramelessDeviceItem::resized() {
    if (ui_) {
        ui_->setBounds(getLocalBounds());
    }
}

void FramelessDeviceItem::paint(Graphics&) {
    // Frameless - let the UI handle all painting
}

namespace IDs {
    #define DECLARE_ID(name)  const Identifier name(#name);
    DECLARE_ID(viewCollapsed)
    #undef DECLARE_ID
}

//==============================================================================
// FramedDeviceItem implementation

FramedDeviceItem::FramedDeviceItem(std::unique_ptr<PluginDeviceUI> ui, bool editable)
    : DeviceItem(std::move(ui))
    , titleBarHeight_(editable ? TitleBar::height : 0)
    , titleBar_(plugin_, getDeviceUI())
{
    // Before setSize, whose resized() reads it
    if (plugin_ != nullptr && editable)  // only the title bar collapses it
        isCollapsed_.referTo(plugin_->state, IDs::viewCollapsed, nullptr, false);

    if (ui_) {
        if (editable)
            addAndMakeVisible(titleBar_);
        addAndMakeVisible(ui_.get());

        // Size the frame to accommodate the plugin UI plus title bar
        static constexpr int frameWidth = 0;  // 2px border on each side
        setSize(ui_->getWidth() + frameWidth,
                ui_->getHeight() + titleBarHeight_ + frameWidth);
    }
    minimumWidth_ = getWidth();
    expandedWidth_ = getWidth();
    expandedHeight_ = getHeight();

    titleBar_.setCollapseToggle([this]() {
        setCollapsed(!isCollapsed());
    });
    updateCollapsedInternal();
}

int FramedDeviceItem::getMinimumHeight() const {
    return titleBarHeight_ + DeviceItem::getMinimumHeight();
}

void FramedDeviceItem::resized() {
    if (isCollapsed()) {
        auto bounds = getLocalBounds();
        auto collapsedWidth = TitleBar::height;
        titleBar_.setBounds(bounds.removeFromLeft(collapsedWidth));
        if (ui_) {
            ui_->setBounds(0, 0, 0, 0);
        }
    } else {
        auto bounds = getLocalBounds();
        auto titleBarBounds = bounds.removeFromTop(titleBarHeight_);
        titleBar_.setBounds(titleBarBounds);
        if (ui_) {
            ui_->setBounds(bounds);
        }
        expandedWidth_ = getWidth();
        expandedHeight_ = getHeight();
    }
}

void FramedDeviceItem::paint(Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    if (isCollapsed()) {
        auto backgroundArea = bounds;
        backgroundArea.removeFromLeft((float) titleBar_.getWidth());
        if (!backgroundArea.isEmpty()) {
            g.setColour(Colors::Theme::background);
            g.fillRect(backgroundArea);
        }
        return;
    }

    bounds.removeFromTop((float) titleBarHeight_);
    const bool roundTop = titleBarHeight_ == 0;  // else the title bar rounds the top
    Path path;
    path.addRoundedRectangle(
        bounds.getX(), bounds.getY(),
        bounds.getWidth(), bounds.getHeight(),
        cornerSize, cornerSize,
        roundTop, roundTop, true, true
    );
    g.setColour(Colors::Theme::backgroundAlt);
    g.fillPath(path);
}

void FramedDeviceItem::updateCollapsedInternal() {
    titleBar_.setCollapsed(isCollapsed());

    if (ui_) {
        ui_->setVisible(!isCollapsed());
    }

    if (isCollapsed()) {
        const auto targetHeight = expandedHeight_ > 0 ? expandedHeight_ : getHeight();
        setSize(TitleBar::height, targetHeight);
    } else {
        const auto restoreWidth = expandedWidth_ > 0 ? expandedWidth_
                                                    : (ui_ ? ui_->getWidth() : getWidth());
        const auto restoreHeight = expandedHeight_ > 0 ? expandedHeight_
                                                      : (ui_ ? ui_->getHeight() + titleBarHeight_ : getHeight());
        setSize(restoreWidth, restoreHeight);
    }
}

void FramedDeviceItem::setCollapsed(bool collapsed) {
    if (isCollapsed() == collapsed)
        return;

    isCollapsed_ = collapsed;

    if (isCollapsed()) {
        expandedWidth_ = getWidth();
        expandedHeight_ = getHeight();
    }

    updateCollapsedInternal();

    resized();
    repaint();
}

}  // namespace MoTool
