
/*
        █▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌
        █▌  █▌    █▌      █▌    █▌█▌    █▌
  █▌█▌█▌█▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌█▌█▌█▌
        █▌              |   |
    ,---.---.  ,----. --|---|--  ,----.  ,---
    |   |   |  ,----|   |   |    |----'  |
    '   '   `  `----'   `-- `--  `----   '

    d88o888ob.      d888888888b             d8b
    8@@8@@@8@@b ,d888b.`8@8',d888b. ,d888b. B@8       Copyright 2025
    B@8'B@8'8@8 8@8 8@8 B@8 8@8 8@8 8@8 8@8 B@8    Ruslan Grohovecki
    B@8 B@8 B@8 '"" 8@8 B@8 8@b ""' '"" 8@8 B@8    www.pixelmatter.org
    Y@8 8@8 Y@@88@@@8P' 8@8 `Y@@@88888@@@P' Y8P

    The MoTool uses GPL licence - see LICENCE.md for details.
*/
#include "App.h"

using namespace MoTool;

#ifndef MOTOOL_APP_TARGET
#define MOTOOL_APP_TARGET 0
#endif
MoToolApp::Target MoToolApp::target_ = static_cast<MoToolApp::Target>(MOTOOL_APP_TARGET);

START_JUCE_APPLICATION(MoToolApp)
