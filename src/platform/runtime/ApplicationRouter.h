#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace platform::ui {

class PageController;

}

namespace platform::runtime {

class Application;

class ApplicationRouter {
   public:
    explicit ApplicationRouter(Application& application);
    ApplicationRouter(const ApplicationRouter&) = delete;
    ApplicationRouter& operator=(const ApplicationRouter&) = delete;
    ApplicationRouter(ApplicationRouter&&) = delete;
    ApplicationRouter& operator=(ApplicationRouter&&) = delete;

    // Page controllers are borrowed and must outlive every use of the registry and navigation stack.
    // Paths are exact, case-sensitive, application-local, and contain no query or fragment.
    struct Description {
        std::string path;
        bool fullscreen;
        std::string help;
    };

    [[nodiscard]] bool registerPage(const char* path, ui::PageController& page, std::string_view help = {});
    [[nodiscard]] std::vector<Description> descriptions() const;
    [[nodiscard]] ui::PageController* resolve(const char* path) const;

   private:
    struct Entry {
        std::string path;
        ui::PageController* page;
        std::string help;
    };

    Application& _application;
    std::vector<Entry> _entries;
};

}  // namespace platform::runtime
