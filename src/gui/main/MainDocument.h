#pragma once

#include <JuceHeader.h>

#include "../timeline/EditComponent.h"
#include "../common/Transport.h"
#include "../../util/Helpers.h"


using namespace juce;

namespace MoTool {

//==============================================================================

class MainDocumentComponent: public Component,
                             public FileDragAndDropTarget {
public:

    explicit MainDocumentComponent(te::Edit& edit, EditViewState& evs)
        : edit_ {edit}
        , transportBar_ {evs}
        , editComponent_ {edit, evs}
        // , footer_        {edit_.engine,}
    {
        addAndMakeVisible(transportBar_);
        addAndMakeVisible(editComponent_);
        // addAndMakeVisible(footer_);
    }

    ~MainDocumentComponent() override {}

    void resized() override {
        auto r = getLocalBounds();
        auto transportBarHeight = 40;
        transportBar_.setBounds(r.removeFromTop(transportBarHeight));
        editComponent_.setBounds(r);
    }

    bool isInterestedInFileDrag(const StringArray& paths) override {
        return std::ranges::any_of(paths, [](const String& path) { return Helpers::isTuneFile(File(path)); });
    }

    /** Adds the dropped PSG files and tracker modules on new tracks. */
    void filesDropped(const StringArray& paths, int /*x*/, int /*y*/) override {
        Array<File> files;
        for (auto& path : paths)
            if (Helpers::isTuneFile(File(path)))
                files.add(File(path));

        Helpers::withTurboSoundSet(files, [safeEdit = te::SafeSelectable<te::Edit>(edit_)](const Array<File>& set) {
            if (safeEdit != nullptr)
                Helpers::importPsgFiles(*safeEdit.get(), set);
        });
    }

private:
    te::Edit& edit_;
    TransportBar transportBar_;
    EditComponent editComponent_;  // former AudioTimeline
    // FooterBar footer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainDocumentComponent)
};

}  // namespace MoTool
