#pragma once

#include <string_view>

#include "ViewController.h"

namespace platform::runtime {

class Application;
class ApplicationRouter;

}  // namespace platform::runtime

namespace platform::ui {

class PageController : public ViewController {
   public:
    explicit PageController(bool fullscreen = false) : _fullscreen(fullscreen) {
    }

    [[nodiscard]] bool isFullscreen() const {
        return _fullscreen;
    }

    void setFullscreen(bool fullscreen);
    PageController(const PageController&) = delete;
    PageController& operator=(const PageController&) = delete;
    PageController(PageController&&) = delete;
    PageController& operator=(PageController&&) = delete;

    // Borrowed application; null until this page is registered in its router.
    [[nodiscard]] runtime::Application* owner() {
        return _owner;
    }

    [[nodiscard]] const runtime::Application* owner() const {
        return _owner;
    }

    // Navigation supplies the full local location, including query and fragment.
    // Each entered page is left before another entry or application deactivation.
    // Callbacks must not recursively navigate or switch applications.
    // Validation must not mutate page state; navigation calls it before leaving the current page.
    [[nodiscard]] virtual bool acceptsLocation(std::string_view) const {
        return true;
    }

    virtual void onEnter(std::string_view) {
    }

    virtual void onLeave() {
    }

   private:
    friend class runtime::ApplicationRouter;
    runtime::Application* _owner = nullptr;
    bool _fullscreen = false;
};

}  // namespace platform::ui
