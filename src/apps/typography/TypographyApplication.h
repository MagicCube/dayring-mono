#pragma once

#include "../../platform/runtime/Application.h"
#include "TypographyPage.h"

namespace apps::typography {
class TypographyApplication final : public platform::runtime::Application {
   public:
    void onCreate() override;
    void onEnter(const platform::runtime::Intent& intent) override;
    void onLeave() override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;
    bool onInput(const platform::runtime::InputEvent& event) override;

   private:
    TypographyPage _page;
};
}  // namespace apps::typography
