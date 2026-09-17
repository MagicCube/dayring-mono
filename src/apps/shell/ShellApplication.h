#pragma once

#include "../../platform/runtime/Application.h"
#include "../../platform/ui/StaticPageController.h"
#include "pages/FirmwareUpdatePage.h"
#include "pages/HomePageController.h"
#include "pages/LockPageController.h"

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
    pages::LockPageController _lock;
    pages::HomePageController _home;
    platform::ui::StaticPageController<pages::FirmwareUpdatePage> _firmwareUpdate{true};
};

}  // namespace apps::shell
