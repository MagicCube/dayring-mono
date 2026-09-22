#include "../storage/FatFilesystem.h"
#include "Calendar.h"
#include "CalendarStorage.h"

#ifdef ARDUINO
#include <FFat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace platform::calendar {
namespace {

class FatCalendarFiles final : public CalendarFiles {
   public:
    std::optional<std::string> read(unsigned slot) override {
#ifdef ARDUINO
        if (!storage::mountFatFilesystem()) return std::nullopt;
        auto file = FFat.open(_path(slot), "r");
        if (!file || file.size() > maxSnapshotBytes + 24) return std::nullopt;
        std::string bytes(file.size(), '\0');
        const auto count = file.read(reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
        if (count != bytes.size()) return std::nullopt;
        return bytes;
#else
        (void)slot;
        return std::nullopt;
#endif
    }

    bool write(unsigned slot, std::string_view bytes) override {
#ifdef ARDUINO
        if (!storage::mountFatFilesystem()) return false;
        const std::string path = std::string("/ffat") + _path(slot);
        const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) return false;
        size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
            if (count <= 0) break;
            offset += static_cast<size_t>(count);
        }
        const bool flushed = offset == bytes.size() && ::fsync(fd) == 0;
        const bool closed = ::close(fd) == 0;
        return flushed && closed;
#else
        (void)slot;
        (void)bytes;
        return false;
#endif
    }

   private:
#ifdef ARDUINO
    static const char* _path(unsigned slot) {
        return slot == 0 ? "/calendar-a.cache" : "/calendar-b.cache";
    }

#endif
};

}  // namespace

std::unique_ptr<CalendarStorage> makeCalendarStorage() {
    return std::make_unique<FileCalendarStorage>(std::make_unique<FatCalendarFiles>());
}

}  // namespace platform::calendar
