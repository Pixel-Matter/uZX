#include <JuceHeader.h>

using namespace tracktion;

// Theese are classes for selectable objects
// see https://forum.juce.com/t/deleteselected-issues/53935

namespace MoTool {

class ClipSelectableClass : public SelectableClass{
public:
    void deleteSelected(const DeleteSelectedParams& params) override {
        for (auto* elem : params.items) {
            if (auto* clip = dynamic_cast<Clip*>(elem)) {
                if (auto* sm = params.selectionManager) {
                    sm->deselect(clip);
                    clip->removeFromParent();
                }
            }
        }
    }
};
DECLARE_SELECTABLE_CLASS(Clip)


class TrackSelectableClass : public SelectableClass {
public:
    void deleteSelected(const DeleteSelectedParams& params) override {
        for (auto* elem : params.items) {
            if (auto* track = dynamic_cast<Track*>(elem)) {
                if (!(track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack())) {
                    if (auto* sm = params.selectionManager) {
                        sm->deselect(track);
                        track->edit.deleteTrack(track);
                    }
                }
            }
        }
    }
};
DECLARE_SELECTABLE_CLASS(Track)


class PluginSelectableClass : public SelectableClass {
    public:
        void deleteSelected(const DeleteSelectedParams& params) override {
            for (auto* elem : params.items) {
                if (auto* plugin = dynamic_cast<Plugin*>(elem)) {
                    if (auto* sm = params.selectionManager) {
                        sm->deselect(plugin);
                        plugin->deleteFromParent();
                    }
                }
            }
        }
};
DECLARE_SELECTABLE_CLASS(Plugin)

} // namespace MoTool
