#pragma once

#include <string_view>

#include "../ui/ApplicationContainer.h"
#include "ApplicationManager.h"
#include "services/ServiceManager.h"

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
    [[nodiscard]] bool prepareFirmwareUpdate();
    [[nodiscard]] bool isFirmwareUpdateReady() const;
    [[nodiscard]] bool open(std::string_view url, OpenMode mode = OpenMode::Default);
    [[nodiscard]] static std::optional<AppURL> resolveURL(std::string_view url);
    [[nodiscard]] bool goHome();
    [[nodiscard]] bool lock();
    [[nodiscard]] bool unlock();
    [[nodiscard]] bool isLocked() const;
    [[nodiscard]] bool isHome() const;
    [[nodiscard]] ApplicationManager& applicationManager();
    [[nodiscard]] const ApplicationManager& applicationManager() const;
    [[nodiscard]] tasking::TaskDispatchService& tasks();
    [[nodiscard]] const tasking::TaskDispatchService& tasks() const;
    [[nodiscard]] bool startServices();
    [[nodiscard]] ServiceManager& services();
    [[nodiscard]] const ServiceManager& services() const;
    bool onInput(const InputEvent& event);

   private:
    Shell();
    void _handleHomeGesture();
    ~Shell() = default;
    [[nodiscard]] bool _lock(const Intent& intent,
                             power::PowerService::LockReason reason = power::PowerService::LockReason::Manual);
    bool _firmwareUpdating = false;
    // Declared before applications so their handles are destroyed first.
    ServiceManager _services;
    ApplicationManager _applicationManager;
    ui::ApplicationContainer _applicationContainer{_applicationManager, [this] { _handleHomeGesture(); }};
};

}  // namespace platform::runtime
