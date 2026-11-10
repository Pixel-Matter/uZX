#include "RenderVideoDialog.h"

#include "../../controllers/EditState.h"
#include "../../formats/video/VideoExport.h"
#include "../common/Utilities.h"
#include "VideoRenderJob.h"

namespace MoTool {

namespace {

juce::PropertiesFile& appSettings(tracktion::Engine& engine) {
    return engine.getPropertyStorage().getPropertiesFile();
}

juce::String sizeText(int height) {
    return juce::String(Video::Settings::widthFor(height)) + juce::String::fromUTF8(" × ") + juce::String(height);
}

void renderTo(tracktion::Edit& edit,
              const Video::Settings& settings,
              const juce::File& ffmpeg,
              const juce::File& dest) {
    VideoRenderJob job(edit, settings, ffmpeg, dest);
    if (job.getError().isEmpty())
        edit.engine.getUIBehaviour().runTaskWithProgressBar(job);

    if (job.hasWritten()) {
#if JUCE_MAC
        const juce::String reveal = "Reveal in Finder";
#elif JUCE_WINDOWS
        const juce::String reveal = "Show in Explorer";
#else
        const juce::String reveal = "Show in Folder";
#endif
        auto options = juce::MessageBoxOptions()
                           .withIconType(juce::MessageBoxIconType::InfoIcon)
                           .withTitle("Video Rendered")
                           .withMessage(dest.getFullPathName())
                           .withButton(reveal)
                           .withButton("OK");
        juce::NativeMessageBox::showAsync(options, [dest](int result) {
            if (result == 0)
                dest.revealToUser();
        });
    } else if (job.getError().isNotEmpty()) {
        auto options = juce::MessageBoxOptions()
                           .withIconType(juce::MessageBoxIconType::WarningIcon)
                           .withTitle("Render Video")
                           .withMessage(job.getError())
                           .withButton("OK");
        juce::NativeMessageBox::showAsync(options, nullptr);
    }
}

//==============================================================================
class RenderVideoComponent : public juce::Component
{
public:
    explicit RenderVideoComponent(tracktion::Edit& e) : edit(e) {
        settings.load(appSettings(e.engine));
        ffmpeg = Video::findFfmpeg(settings.ffmpeg);

        for (auto* label : {&showLabel,
                            &scopesLabel,
                            &coloursLabel,
                            &ffmpegLabel,
                            &renderLabel,
                            &outputLabel,
                            &fpsLabel,
                            &videoLabel,
                            &qualityLabel,
                            &audioLabel,
                            &ffmpegPath,
                            &note})
            addAndMakeVisible(label);
        for (auto* box :
             {&showBox, &scopesBox, &coloursBox, &renderBox, &outputBox, &fpsBox, &codecBox, &qualityBox, &audioBox})
            addAndMakeVisible(box);

        // Viz mode's own options, saved as they change: the Player's view behind shows each choice
        const auto viz = VizOptions::load(appSettings(e.engine));
        showBox.addItemList(VizOptions::layoutNames, 1);
        showBox.setSelectedId((int) viz.layout + 1, juce::dontSendNotification);
        showBox.onChange = [this] {
            changeViz([this](VizOptions& o) { o.layout = (VizOptions::Layout) (showBox.getSelectedId() - 1); });
        };
        scopesBox.addItemList(VizOptions::scopesNames, 1);
        scopesBox.setSelectedId((int) viz.scopes + 1, juce::dontSendNotification);
        scopesBox.onChange = [this] {
            changeViz([this](VizOptions& o) { o.scopes = (VizOptions::Scopes) (scopesBox.getSelectedId() - 1); });
        };
        coloursBox.addItemList(VizOptions::colourNames, 1);
        coloursBox.setSelectedId((int) viz.colours + 1, juce::dontSendNotification);
        coloursBox.onChange = [this] {
            changeViz([this](VizOptions& o) { o.colours = (VizOptions::Colours) (coloursBox.getSelectedId() - 1); });
        };
        for (auto* button : {&chooseButton, &cancelButton, &renderButton})
            addAndMakeVisible(button);

        for (auto height : Video::Settings::renderHeights)
            renderBox.addItem(sizeText(height), height);
        renderBox.setSelectedId(settings.renderHeight, juce::dontSendNotification);
        renderBox.onChange = [this] {
            const auto outputHeight = settings.outputHeight();
            settings.renderHeight = renderBox.getSelectedId();
            settings.scale = Video::Settings::nearestScale(settings.renderHeight, outputHeight);
            fillOutputs();
        };
        fillOutputs();
        outputBox.onChange = [this] { settings.scale = outputBox.getSelectedId(); };

        for (auto fps : Video::Settings::frameRates)
            fpsBox.addItem(juce::String(fps) + " fps", fps);
        fpsBox.setSelectedId(settings.fps, juce::dontSendNotification);
        fpsBox.onChange = [this] { settings.fps = fpsBox.getSelectedId(); };

        codecBox.addItemList({"H.264", "HEVC (H.265)", "AV1"}, 1);
        qualityBox.addItemList({"High", "Very high", "Near lossless"}, 1);
        qualityBox.setSelectedId((int) settings.quality + 1, juce::dontSendNotification);
        qualityBox.onChange = [this] {
            settings.quality = (Video::Settings::Quality) (qualityBox.getSelectedId() - 1);
        };
        audioBox.addItemList({"AAC 384 kbps", "Opus 256 kbps", "ALAC, lossless"}, 1);
        codecBox.onChange = [this] { settings.codec = (Video::Settings::Codec) (codecBox.getSelectedId() - 1); };
        audioBox.onChange = [this] { settings.audio = (Video::Settings::Audio) (audioBox.getSelectedId() - 1); };

        note.setText("Each frame shows Viz mode at the zoom on screen, from the start of the tune to "
                         + juce::String(Video::Settings::tailSeconds) + " seconds after its end.",
                     juce::dontSendNotification);
        note.setJustificationType(juce::Justification::topLeft);
        ffmpegPath.setMinimumHorizontalScale(0.5f);

        chooseButton.onClick = [this] { chooseFfmpeg(); };
        cancelButton.onClick = [this] { close(); };
        renderButton.onClick = [this] { chooseDestination(); };
        renderButton.addShortcut(juce::KeyPress(juce::KeyPress::returnKey));

        showFfmpeg();
        setSize(540, 328);
    }

    void resized() override {
        constexpr int rowHeight = 24, gap = 8, labelWidth = 80, boxWidth = 170;
        auto area = getLocalBounds().reduced(16);
        auto row = [&] {
            auto r = area.removeFromTop(rowHeight);
            area.removeFromTop(gap);
            return r;
        };
        auto pair = [&](juce::Rectangle<int> r, juce::Label& label, juce::Component& box, int width) {
            label.setBounds(r.removeFromLeft(labelWidth));
            box.setBounds(r.removeFromLeft(width));
            return r.withTrimmedLeft(gap * 2);
        };

        auto r = pair(row(), showLabel, showBox, boxWidth);
        pair(r, scopesLabel, scopesBox, r.getWidth() - labelWidth);
        pair(row(), coloursLabel, coloursBox, boxWidth);

        r = row();
        ffmpegLabel.setBounds(r.removeFromLeft(labelWidth));
        chooseButton.setBounds(r.removeFromRight(90));
        ffmpegPath.setBounds(r.withTrimmedRight(gap));

        r = pair(row(), renderLabel, renderBox, boxWidth);
        pair(r, outputLabel, outputBox, r.getWidth() - labelWidth);
        pair(row(), fpsLabel, fpsBox, boxWidth);
        r = pair(row(), videoLabel, codecBox, boxWidth);
        pair(r, qualityLabel, qualityBox, r.getWidth() - labelWidth);
        pair(row(), audioLabel, audioBox, boxWidth);

        auto buttons = area.removeFromBottom(28);
        renderButton.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(gap);
        cancelButton.setBounds(buttons.removeFromRight(100));
        note.setBounds(area.withTrimmedLeft(labelWidth));
    }

private:
    void changeViz(const std::function<void(VizOptions&)>& change) {
        if (auto* e = edit.get()) {
            auto options = VizOptions::load(appSettings(e->engine));
            change(options);
            options.save(appSettings(e->engine));
        }
    }

    void fillOutputs() {
        outputBox.clear(juce::dontSendNotification);
        for (auto scale : Video::Settings::scalesFor(settings.renderHeight)) {
            const auto height = settings.renderHeight * scale;
            const juce::String name = height == 4320 ? " (8K)" : height == 2160 ? " (4K)" : "";
            outputBox.addItem(sizeText(height) + name
                                  + (scale > 1 ? juce::String::fromUTF8(", ×") + juce::String(scale) : juce::String()),
                              scale);
        }
        outputBox.setSelectedId(settings.scale, juce::dontSendNotification);
    }

    /** Offers only what this ffmpeg can encode, and keeps a choice it can. */
    void showFfmpeg() {
        ffmpegPath.setText(ffmpeg ? ffmpeg->program.getFullPathName() + "  (" + ffmpeg->version + ")"
                                  : juce::String("Not found: install ffmpeg, or choose it"),
                           juce::dontSendNotification);

        auto offer = [this](juce::ComboBox& box, int selected, auto choices, auto encoderFor) {
            int firstAvailable = 0;
            for (int i = 0; i < (int) choices.size(); ++i) {
                const bool available = ffmpeg && ffmpeg->has(encoderFor(choices[(size_t) i]));
                box.setItemEnabled(i + 1, available);
                if (available && firstAvailable == 0)
                    firstAvailable = i + 1;
            }
            const auto id = box.isItemEnabled(selected + 1) ? selected + 1 : firstAvailable;
            box.setSelectedId(id, juce::dontSendNotification);
            return id - 1;
        };

        using S = Video::Settings;
        const auto codec = offer(codecBox,
                                 (int) settings.codec,
                                 std::array{S::Codec::h264, S::Codec::hevc, S::Codec::av1},
                                 [](S::Codec c) { return Video::encoderFor(c); });
        const auto audio = offer(audioBox,
                                 (int) settings.audio,
                                 std::array{S::Audio::aac, S::Audio::opus, S::Audio::alac},
                                 [](S::Audio a) { return Video::encoderFor(a); });
        if (codec >= 0)
            settings.codec = (S::Codec) codec;
        if (audio >= 0)
            settings.audio = (S::Audio) audio;
        renderButton.setEnabled(codec >= 0 && audio >= 0);
    }

    void chooseFfmpeg() {
        auto chooser = std::make_shared<juce::FileChooser>("Choose ffmpeg", settings.ffmpeg.getParentDirectory());
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [chooser, safeThis = SafePointer<RenderVideoComponent>(this)](const juce::FileChooser& fc) {
                if (safeThis == nullptr || fc.getResult() == juce::File())
                    return;
                if (auto found = Video::probeFfmpeg(fc.getResult())) {
                    safeThis->ffmpeg = found;
                    safeThis->settings.ffmpeg = found->program;
                    safeThis->showFfmpeg();
                } else {
                    juce::NativeMessageBox::showAsync(
                        juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::WarningIcon)
                            .withTitle("Choose ffmpeg")
                            .withMessage(fc.getResult().getFileName() + " is not a working ffmpeg.")
                            .withButton("OK"),
                        nullptr);
                }
            });
    }

    void chooseDestination() {
        auto* e = edit.get();
        if (e == nullptr || !ffmpeg) {
            close();
            return;
        }
        settings.save(appSettings(e->engine));

        const auto folder = settings.folder.isDirectory()
                                ? settings.folder
                                : juce::File::getSpecialLocation(juce::File::userMoviesDirectory);
        const auto name = tracktion::EditFileOperations(*e).getEditFile().getFileNameWithoutExtension();
        auto chooser = std::make_shared<juce::FileChooser>("Render Video", folder.getChildFile(name + ".mp4"), "*.mp4");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [chooser, safeThis = SafePointer<RenderVideoComponent>(this)](const juce::FileChooser& fc) {
                if (safeThis != nullptr && fc.getResult() != juce::File())
                    safeThis->render(fc.getResult().withFileExtension("mp4"));
            });
    }

    void render(const juce::File& dest) {
        auto* e = edit.get();
        if (e == nullptr || !ffmpeg) {
            close();
            return;
        }
        settings.folder = dest.getParentDirectory();
        settings.save(appSettings(e->engine));

        auto& renderedEdit = *e;
        const auto renderSettings = settings;
        const auto program = ffmpeg->program;
        close();  // this goes; the render needs nothing of it
        renderTo(renderedEdit, renderSettings, program, dest);
    }

    void close() {
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(0);  // launchAsync windows delete themselves on exit
    }

    tracktion::SafeSelectable<tracktion::Edit> edit;
    Video::Settings settings;
    juce::Optional<Video::Ffmpeg> ffmpeg;

    juce::Label showLabel{{}, "Show"}, scopesLabel{{}, "Scopes"}, coloursLabel{{}, "Colours"};
    juce::Label ffmpegLabel{{}, "ffmpeg"}, renderLabel{{}, "Render"}, outputLabel{{}, "Output"},
        fpsLabel{{}, "Frame rate"}, videoLabel{{}, "Video"}, qualityLabel{{}, "Quality"}, audioLabel{{}, "Audio"},
        ffmpegPath, note;
    juce::ComboBox showBox, scopesBox, coloursBox, renderBox, outputBox, fpsBox, codecBox, qualityBox, audioBox;
    juce::TextButton chooseButton{"Choose..."}, cancelButton{"Cancel"}, renderButton{"Render..."};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RenderVideoComponent)
};

}  // namespace

void showRenderVideoDialog(tracktion::Edit& edit) {
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Render Video";
    o.dialogBackgroundColour =
        juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    o.content.setOwned(new RenderVideoComponent(edit));
    o.useNativeTitleBar = true;
    o.escapeKeyTriggersCloseButton = true;
    o.resizable = false;
    Helpers::launchDialogAsync(o);
}

}  // namespace MoTool
