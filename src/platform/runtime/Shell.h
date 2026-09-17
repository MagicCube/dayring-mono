#pragma once

#include <string_view>

#include "../hal/PowerManager.h"
#include "../ui/ApplicationContainer.h"
#include "ApplicationManager.h"

namespace platform::runtime {

class Shell {
   public:
    [[nodiscard]] static Shell& instance();
    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;
    Shell(Shell&&) = delete;
    Shell& operator=(Shell&&) = delete;

    [[nodiscard]] bool registerApplication(std::string_view name, ApplicationManager::Factory factory,
                                           Residency residency);
    // Dispatch input, update the foreground app, and render when the display is ready.
    void update();
    [[nodiscard]] bool open(std::string_view url, OpenMode mode = OpenMode::Default);
    [[nodiscard]] static std::optional<AppURL> resolveURL(std::string_view url);
    [[nodiscard]] bool goHome();
    [[nodiscard]] bool lock();
    [[nodiscard]] bool unlock();
    [[nodiscard]] bool isLocked() const;
    [[nodiscard]] bool isHome() const;
    [[nodiscard]] ApplicationManager& applicationManager();
    [[nodiscard]] const ApplicationManager& applicationManager() const;
    bool onInput(const InputEvent& event);

   private:
    Shell();
    void _handleHomeGesture();
    ~Shell() = default;
    [[nodiscard]] bool _lock(const Intent& intent,
                             hal::PowerManager::LockReason reason = hal::PowerManager::LockReason::Manual);
    hal::PowerManager& _powerManager;
    ApplicationManager _applicationManager;
    ui::ApplicationContainer _applicationContainer{_applicationManager, [this] { _handleHomeGesture(); }};
};

}  // namespace platform::runtime
