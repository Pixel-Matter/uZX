#include "Playhead.h"

namespace MoTool {

//==============================================================================
/**
  While playing
    - the playhead position is updated at a fixed rate in TransportControl from audio nodes playhead
    - ? the playhead position is updated in PlayheadViewState at a fixed rate through TransportControl's valueTreePropertyChanged IDs::position
    - ?
    - ? ZoomView state is updated at a fixed rate through TransportControl's valueTreePropertyChanged IDs::position
    - ...
    - PlayheadComponent is redrawn around current playhead position with
*/


PlayheadViewState::PlayheadViewState(te::TransportControl& tc)
    : transport_(tc)
{
    transport_.addChangeListener(this);
    transport_.state.addListener(this);
}

PlayheadViewState::~PlayheadViewState() {
    transport_.removeChangeListener(this);
    transport_.state.removeListener(this);
}

void PlayheadViewState::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &transport_) {
        if (transport_.isPlaying() || transport_.isRecording()) {
            DBG("PlayheadViewState::changeListenerCallback, playing pos: " << transport_.getPosition().inSeconds());
        } else {
            DBG("PlayheadViewState::changeListenerCallback, stopped, pos : " << transport_.getPosition().inSeconds());
        }
    }
}

void PlayheadViewState::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree == transport_.state) {
        if (prop == te::IDs::position) {
            DBG("PlayheadViewState::valueTreePropertyChanged, position: " << transport_.getPosition().inSeconds());
            // TODO can pass transport_.isPositionUpdatingFromPlayhead() if needed
            // positionChanged();
        }
    }
}

}  // namespace MoTool
