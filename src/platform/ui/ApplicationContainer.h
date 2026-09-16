#pragma once

#include <functional>
#include <utility>

#include "../../apps/shell/components/StatusBar.h"
#include "Component.h"

namespace platform::runtime {
class ApplicationManager;
}

namespace platform::ui {
class Page;
class ApplicationContainer final : public Component {
   public:
    explicit ApplicationContainer(runtime::ApplicationManager& applications, std::function<void()> homeGesture = {})
        : _applications(applications), _homeGesture(std::move(homeGesture)) {
    }
    void showStatusBar();
    void hideStatusBar();
    [[nodiscard]] bool isStatusBarVisible() const;
    void update();
    [[nodiscard]] bool needsRender() const;
    void render(Canvas& canvas, const Rect& bounds) override;
    bool onInput(const runtime::InputEvent& event) override;

   private:
    void _synchronizeVisibility();
    runtime::ApplicationManager& _applications;
    apps::shell::components::StatusBar _statusBar;
    std::function<void()> _homeGesture;
    Rect _bounds{};
    Rect _contentBounds{};
    const Page* _renderedPage = nullptr;
    bool _renderedStatusBarVisible = false;
    bool _statusBarVisible = false;
    bool _dirty = true;
};
}  // namespace platform::ui
