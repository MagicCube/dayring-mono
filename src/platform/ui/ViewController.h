#pragma once
#include "../runtime/InputEvent.h"
#include "View.h"

namespace platform::ui {

class ViewController {
   public:
    virtual ~ViewController() = default;
    virtual void render(Canvas& canvas, const Rect& bounds) = 0;

    // Return true when an update changes visible state.
    [[nodiscard]] virtual bool update() {
        return false;
    }

    virtual bool onInput(const runtime::InputEvent&) {
        return false;
    }
};

}  // namespace platform::ui
