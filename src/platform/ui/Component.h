#pragma once

#include <FreeInkUICore.h>

#include "../runtime/InputEvent.h"

namespace platform::ui {
using Canvas = freeink::ui::DrawTarget;
using Rect = freeink::ui::Rect;

// Adapt FreeInk's immediate-mode drawing primitives to an owned UI component.
class Component {
   public:
    virtual ~Component() = default;
    virtual void render(Canvas& canvas, const Rect& bounds) = 0;
    virtual bool onInput(const runtime::InputEvent&) {
        return false;
    }
};
}  // namespace platform::ui
