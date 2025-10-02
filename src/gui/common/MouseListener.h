#pragma once

#include <JuceHeader.h>

class MouseListenerWithCallback : public MouseListener {
public:
    explicit MouseListenerWithCallback(Component& c)
        : component(c)
    {
        component.addMouseListener(this, false);
    }

    ~MouseListenerWithCallback() override {
        component.removeMouseListener(this);
    }

    void setRmbCallback(std::function<void()> cb) {
        rmbCallback = std::move(cb);
    }

    void mouseDown(const MouseEvent& e) override {
        if (rmbCallback && e.mods.isRightButtonDown()) {
            rmbCallback();
        }
    }

private:
    Component& component;
    std::function<void()> rmbCallback;
};