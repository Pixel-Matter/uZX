#pragma once

#include <JuceHeader.h>

#include "MainController.h"
#include "RememberedStereo.h"

namespace MoTool {

class PlayerController : public BaseController {
public:
    using BaseController::BaseController;

    ~PlayerController() override;

    void initialize() override;

    void handleOpen();
    /** Opens the PSG files together in a new edit (offering a single PSG's Turbo Sound set),
        or else the first .uzx project. */
    void handleOpenFiles(const Array<File>& files);
    void handleOpenRecent(int fileIndex);
    void handleClearRecentFiles();
    /** Saves in place; an edit that only lives in the temp folder goes through Save As. */
    void handleSave();
    void handleSaveAs();
    void handleImportPsgReplace();

    /** Viz mode: full screen with the notes and each chip's scopes, as the VizOptions in the app
        settings choose. An app state, not saved: it stays on for the next file opened. */
    void setVizMode(bool on);
    bool isVizMode() const noexcept { return vizMode_; }

private:
    PropertiesFile& getAppSettings();
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void releaseEditDependents() override;
    void showEdit() override;
    void updateWindowTitle();
    /** Rebuilds the view for Viz mode or out of it, telling what Viz mode shows when its layout or
        scopes change. */
    void showView(VizOptions options);
    /** The app settings changed: maybe the Viz options, from the View menu, the V key or Render Video. */
    void appSettingsChanged();

    void importPsgsToNewTracks(const Array<File>& files);
    void importPsgsToNewEdit(const Array<File>& files);

    std::unique_ptr<RememberedStereo> rememberedStereo_;
    bool vizMode_ = false;
    int keptVizScopesWidth_ = 0, keptFootersWidth_ = 0;  // from the edit being replaced

    struct AppSettingsListener : juce::ChangeListener
    {
        explicit AppSettingsListener(PlayerController& p) : player(p) {}
        void changeListenerCallback(juce::ChangeBroadcaster*) override { player.appSettingsChanged(); }
        PlayerController& player;
    } appSettingsListener_ {*this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayerController)
};

}  // namespace MoTool
