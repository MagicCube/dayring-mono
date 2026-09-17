#pragma once

#include <string>

#include "../../platform/runtime/Application.h"
#include "LandingPageController.h"

namespace apps::common {

// An application entry page until the application's own features are implemented.
class PlaceholderApplication final : public platform::runtime::Application {
   public:
    explicit PlaceholderApplication(std::string title);
    void onCreate() override;
    void onEnter(const platform::runtime::Intent& intent) override;
    void onLeave() override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;
    bool onInput(const platform::runtime::InputEvent& event) override;

   private:
    LandingPageController _page;
};

}  // namespace apps::common
