#pragma once

#include "../ui/View.h"
#include "ApplicationNavigation.h"
#include "ApplicationRouter.h"
#include "InputEvent.h"
#include "Intent.h"

namespace platform::runtime {

class ApplicationManager;

class Application {
   public:
    Application();
    virtual ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // onCreate runs once per instance; onEnter receives every open request,
    // including requests for the foreground app. onLeave runs on deactivation.
    // Background instances are paused, not necessarily destroyed. Stop any app-owned
    // background work in onLeave(); release owned resources in the destructor.
    // Own page controllers and other resources as members or unique_ptrs so destruction releases them together.
    virtual void onCreate() = 0;
    virtual void onEnter(const Intent& intent) = 0;
    virtual void onLeave() = 0;

    [[nodiscard]] virtual bool allowsIdleLock() const {
        return true;
    }

    virtual void update() {
    }

    virtual void render(ui::Canvas& canvas, const ui::Rect& bounds) = 0;

    virtual bool onInput(const InputEvent&) {
        return false;
    }

    // Borrowed manager, available before onCreate(); null until attached.
    [[nodiscard]] ApplicationManager* owner() {
        return _owner;
    }

    [[nodiscard]] const ApplicationManager* owner() const {
        return _owner;
    }

    [[nodiscard]] bool needsRender() const {
        return _needsRender;
    }

    [[nodiscard]] ApplicationRouter& router() {
        return _router;
    }

    [[nodiscard]] const ApplicationRouter& router() const {
        return _router;
    }

    [[nodiscard]] ApplicationNavigation& navigation() {
        return _navigation;
    }

    [[nodiscard]] const ApplicationNavigation& navigation() const {
        return _navigation;
    }

   protected:
    void requestRender() {
        _needsRender = true;
    }

   private:
    friend class ui::PageController;
    friend class ApplicationManager;
    friend class ApplicationNavigation;
    void _activate(const Intent& intent);
    void _deactivate();
    ApplicationManager* _owner = nullptr;
    bool _needsRender = true;
    ApplicationRouter _router;
    ApplicationNavigation _navigation;
};

}  // namespace platform::runtime
