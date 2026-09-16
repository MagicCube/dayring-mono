#pragma once

#include <string>

#include "../../platform/runtime/Application.h"
#include "../../platform/ui/Page.h"

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
    class LandingPage final : public platform::ui::Page {
       public:
        explicit LandingPage(std::string title);
        void onEnter(std::string_view location) override;
        void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;
        bool onInput(const platform::runtime::InputEvent& event) override;

       private:
        std::string _title;
        int _count = 0;
        freeink::ui::InteractionBuffer<3> _interactions;
    };
    LandingPage _page;
};
}  // namespace apps::common
