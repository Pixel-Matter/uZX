
/*
        █▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌
        █▌  █▌    █▌      █▌    █▌█▌    █▌
  █▌█▌█▌█▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌█▌█▌█▌
        █▌              |   |
    ,---.---.  ,----. --|---|--  ,----.  ,---
    |   |   |  ,----|   |   |    |----'  |
    '   '   `  `----'   `-- `--  `----   '

    d88o888ob.      d888888888b             d8b
    8@@8@@@8@@b ,d888b.`8@8',d888b. ,d888b. B@8
    B@8'B@8'8@8 8@8 8@8 B@8 8@8 8@8 8@8 8@8 B@8    Copyright 2025–2026
    B@8 B@8 B@8 '"" 8@8 B@8 8@b ""' '"" 8@8 B@8      pixelmatter.org
    Y@8 8@8 Y@@88@@@8P' 8@8 `Y@@@88888@@@P' Y8P

    The MoTool uses GPL licence - see LICENCE.md for details.
*/
#include "App.h"

using namespace MoTool;

#ifndef MOTOOL_APP_TARGET
#define MOTOOL_APP_TARGET 0
#endif
#ifndef MOTOOL_TARGET_VERSION
#define MOTOOL_TARGET_VERSION "0.0.0"
#endif
MoToolApp::Target MoToolApp::target_ = static_cast<MoToolApp::Target>(MOTOOL_APP_TARGET);
const char* MoToolApp::targetVersion_ = MOTOOL_TARGET_VERSION;

START_JUCE_APPLICATION(MoToolApp)
