/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from Tracktion

#include "EditState.h"

namespace MoTool {

//==============================================================================
class PluginComponent : public TextButton {
public:
    PluginComponent (EditViewState&, te::Plugin::Ptr);
    ~PluginComponent() override;

    using TextButton::clicked;
    void clicked(const ModifierKeys& modifiers) override;

private:
    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
};

te::Plugin::Ptr showMenuAndCreatePlugin(te::Edit& edit);

//==============================================================================
class PlayheadComponent : public Component,
                          private Timer {
public:
    PlayheadComponent(te::Edit&, EditViewState&);

    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseDrag(const MouseEvent&) override;
    void mouseDown(const MouseEvent&) override;
    void mouseUp(const MouseEvent&) override;

private:
    void timerCallback() override;

    te::Edit& edit;
    EditViewState& editViewState;

    int xPosition = 0;
    bool firstTimer = true;
};

}  // namespace MoTool
