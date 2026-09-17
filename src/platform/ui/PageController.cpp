#include "PageController.h"

#include "../runtime/Application.h"

namespace platform::ui {

void PageController::setFullscreen(bool fullscreen) {
    if (_fullscreen == fullscreen) return;
    _fullscreen = fullscreen;
    if (_owner) _owner->requestRender();
}

}  // namespace platform::ui
