#pragma once

#include <string>
#include <vector>

namespace platform::ui {
class Page;
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

    // Pages are borrowed and must outlive every use of the registry and navigation stack.
    // Paths are exact, case-sensitive, application-local, and contain no query or fragment.
    [[nodiscard]] bool registerPage(const char* path, ui::Page& page);
    [[nodiscard]] ui::Page* resolve(const char* path) const;

   private:
    struct Entry {
        std::string path;
        ui::Page* page;
    };
    Application& _application;
    std::vector<Entry> _entries;
};
}  // namespace platform::runtime
