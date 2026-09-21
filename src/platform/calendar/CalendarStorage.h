#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace platform::calendar {

class CalendarStorage {
   public:
    virtual ~CalendarStorage() = default;
    virtual std::optional<std::string> load() = 0;
    virtual bool save(std::string_view json) = 0;
};

// The file boundary also lets host tests simulate interrupted writes and corrupted slots.
class CalendarFiles {
   public:
    virtual ~CalendarFiles() = default;
    virtual std::optional<std::string> read(unsigned slot) = 0;
    virtual bool write(unsigned slot, std::string_view bytes) = 0;
};

class FileCalendarStorage final : public CalendarStorage {
   public:
    explicit FileCalendarStorage(std::unique_ptr<CalendarFiles> files);
    std::optional<std::string> load() override;
    bool save(std::string_view json) override;

   private:
    std::unique_ptr<CalendarFiles> _files;
    uint64_t _generation = 0;
    unsigned _slot = 1;
};

[[nodiscard]] std::unique_ptr<CalendarStorage> makeCalendarStorage();

}  // namespace platform::calendar
