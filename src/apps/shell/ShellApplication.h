#pragma once

#include "../../platform/runtime/Application.h"
#include "pages/HomeScreen.h"
#include "pages/LockScreen.h"

namespace apps::shell {
class ShellApplication final : public platform::runtime::Application {
   public:
    void onCreate() override;
    void onEnter(const platform::runtime::Intent& intent) override;
    void onLeave() override;
    [[nodiscard]] bool allowsIdleLock() const override;
    void update() override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;
    bool onInput(const platform::runtime::InputEvent& event) override;

   private:
    pages::LockScreen _lock;
    pages::HomeScreen _home;
};
}  // namespace apps::shell
