#pragma once

#include <functional>
#include <utility>

#include "../../apps/shell/components/StatusBarController.h"
#include "ViewController.h"

namespace platform::runtime {

class ApplicationManager;

}

namespace platform::ui {

class PageController;

class ApplicationContainer final {
   public:
    explicit ApplicationContainer(runtime::ApplicationManager& applications, std::function<void()> homeGesture = {})
        : _applications(applications), _homeGesture(std::move(homeGesture)) {
    }

    void showStatusBar();
    void hideStatusBar();
    [[nodiscard]] bool isStatusBarVisible() const;
    void update();
    [[nodiscard]] bool needsRender() const;
    void render(Canvas& canvas, const Rect& bounds);
    bool onInput(const runtime::InputEvent& event, int16_t minimumHomeSwipeDistance = 0);

   private:
    void _synchronizeVisibility();
    runtime::ApplicationManager& _applications;
    apps::shell::components::StatusBarController _statusBar;
    std::function<void()> _homeGesture;
    Rect _bounds{};
    Rect _contentBounds{};
    const PageController* _renderedPage = nullptr;
    bool _renderedStatusBarVisible = false;
    bool _statusBarVisible = false;
    bool _dirty = true;
};

}  // namespace platform::ui
