#include "MouseListener.h"

#include <utility>

MouseListenerWithCallback::MouseListenerWithCallback(Component& c)
    : component(c)
{
    component.addMouseListener(this, false);
}

MouseListenerWithCallback::~MouseListenerWithCallback()
{
    component.removeMouseListener(this);
}

void MouseListenerWithCallback::setRmbCallback(std::function<void()> cb)
{
    rmbCallback = std::move(cb);
}

void MouseListenerWithCallback::mouseDown(const MouseEvent& e)
{
    if (rmbCallback && e.mods.isRightButtonDown()) {
        rmbCallback();
    }
}
