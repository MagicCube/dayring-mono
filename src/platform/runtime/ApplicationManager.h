#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Application.h"
#include "InputEvent.h"

namespace platform::runtime {

class Shell;
enum class OpenMode { Default, Exact };
enum class RouteError { None, InvalidURL, UnknownApplication, UnknownPage, InvalidParameters, Unavailable };

struct RouteDescription {
    std::string application;
    ApplicationRouter::Description page;
};
enum class Residency : uint8_t {
    Resident,   // Retained until manager shutdown.
    Transient,  // Retained while among the most recently foregrounded transient apps.
};

// Only the active application receives updates and input. Lifecycle callbacks
// cannot recursively switch applications. Production entry points use Shell.
class ApplicationManager {
   public:
    using Factory = std::unique_ptr<Application> (*)();

    // The limit includes the foreground transient app; zero is normalized to one.
    explicit ApplicationManager(std::size_t transientLimit = 2);
    ~ApplicationManager();
    ApplicationManager(const ApplicationManager&) = delete;
    ApplicationManager& operator=(const ApplicationManager&) = delete;
    ApplicationManager(ApplicationManager&&) = delete;
    ApplicationManager& operator=(ApplicationManager&&) = delete;

    // Instances are created lazily. "home" is reserved for the Shell facade.
    [[nodiscard]] bool registerApplication(std::string_view name, Factory factory, Residency residency);
    // Opens a concrete app URL; facade aliases and lock policy belong to Shell.
    [[nodiscard]] bool open(std::string_view url, OpenMode mode = OpenMode::Default);
    // Discovery prepares applications but does not enter their pages. Intended before opening an app.
    [[nodiscard]] std::optional<std::vector<RouteDescription>> describeRoutes();
    [[nodiscard]] RouteError checkRoute(std::string_view url);
    [[nodiscard]] std::string currentURL() const;
    void leave();
    void update();
    bool onInput(const InputEvent& event);
    bool render(ui::Canvas& canvas, const ui::Rect& bounds, bool force = false);
    [[nodiscard]] ui::PageController* currentPageController() const;
    [[nodiscard]] bool needsRender() const;
    [[nodiscard]] bool allowsIdleLock() const;

   private:
    friend class Shell;
    using ApplicationId = std::size_t;

    struct Entry {
        std::string name;
        Factory factory;
        Residency residency;
        std::unique_ptr<Application> instance;
    };

    struct Interruption {
        ApplicationId previous;
        ApplicationId presented;
    };

    static constexpr ApplicationId _kNoApplication = static_cast<ApplicationId>(-1);
    [[nodiscard]] ApplicationId _find(std::string_view name) const;
    [[nodiscard]] bool _prepare(ApplicationId id);
    [[nodiscard]] bool _enter(ApplicationId id, const Intent& intent);
    [[nodiscard]] bool _interrupt(const Intent& intent);
    [[nodiscard]] bool _restore();
    void _retainTransient(ApplicationId id);
    void _leaveCurrent();
    [[nodiscard]] Application* _active() const;
    std::vector<Entry> _entries;
    ApplicationId _activeId = _kNoApplication;
    bool _transitioning = false;
    std::optional<Interruption> _interruption;
    const std::size_t _transientLimit;
    // Oldest first. Retained background instances receive no dispatch.
    std::vector<ApplicationId> _recentTransients;
};

}  // namespace platform::runtime
