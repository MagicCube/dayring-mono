#pragma once

#include "PageController.h"

namespace platform::ui {

template <typename PageType>
class StaticPageController final : public PageController {
   public:
    explicit StaticPageController(bool fullscreen = false) : PageController(fullscreen) {
    }

    void render(Canvas& canvas, const Rect& bounds) override {
        _view.render(canvas, bounds, typename PageType::Props{});
    }

   private:
    PageType _view;
};

}  // namespace platform::ui
