#pragma once

#include <JuceHeader.h>

class MouseListenerWithCallback : public MouseListener {
public:
    void setRmbCallback(std::function<void()> cb) {
        rmbCallback = std::move(cb);
    }

    void mouseDown(const MouseEvent& e) override {
        if (rmbCallback && e.mods.isRightButtonDown()) {
            rmbCallback();
        }
    }
private:
    std::function<void()> rmbCallback;
};