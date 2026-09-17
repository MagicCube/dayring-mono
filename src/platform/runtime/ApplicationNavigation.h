#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace platform::ui {

class PageController;

}

namespace platform::runtime {

class Application;

class ApplicationNavigation {
   public:
    ApplicationNavigation(const ApplicationNavigation&) = delete;
    ApplicationNavigation& operator=(const ApplicationNavigation&) = delete;
    ApplicationNavigation(ApplicationNavigation&&) = delete;
    ApplicationNavigation& operator=(ApplicationNavigation&&) = delete;

    // Only foreground applications can navigate. Lifecycle callbacks cannot navigate recursively.
    // Locations start with a single slash and may include a query and fragment.
    [[nodiscard]] bool push(const char* location);
    [[nodiscard]] bool replace(const char* location);
    [[nodiscard]] bool pop();
    [[nodiscard]] bool canPop() const;
    [[nodiscard]] ui::PageController* currentPageController() const;
    // The view remains valid until the next successful navigation operation.
    [[nodiscard]] std::string_view currentLocation() const;

   private:
    friend class Application;
    friend class ApplicationManager;
    explicit ApplicationNavigation(Application& application);

    struct Entry {
        std::string location;
        ui::PageController* page;
    };

    [[nodiscard]] bool _navigate(const char* location, bool replaceTop);
    void _leavePage();
    void _enterPage();
    void _resume();
    void _suspend();
    Application& _application;
    std::vector<Entry> _entries;
    std::optional<Entry> _temporary;
    bool _active = false;
    bool _entered = false;
    bool _transitioning = false;
};

}  // namespace platform::runtime
