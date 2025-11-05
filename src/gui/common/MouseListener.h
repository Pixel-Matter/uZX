#pragma once

#include <functional>

#include <JuceHeader.h>

class MouseListenerWithCallback : public MouseListener {
public:
    explicit MouseListenerWithCallback(Component& c);
    ~MouseListenerWithCallback() override;

    void setRmbCallback(std::function<void()> cb);
    void mouseDown(const MouseEvent& e) override;

private:
    Component& component;
    std::function<void()> rmbCallback;
};
