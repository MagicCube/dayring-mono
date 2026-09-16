#include "Application.h"

#include "TransitionGuard.h"

namespace platform::runtime {
Application::Application() : _router(*this), _navigation(*this) {
}

void Application::_activate(const Intent& intent) {
    _navigation._active = true;
    requestRender();

    if (intent.reason == Intent::Reason::Open) {
        onEnter(intent);
    } else {
        // Restore and temporary presentation cannot rewrite normal history.
        TransitionGuard guard(_navigation._transitioning);
        onEnter(intent);
    }
    _navigation._resume();
}

void Application::_deactivate() {
    _navigation._suspend();

    onLeave();
}
}  // namespace platform::runtime
