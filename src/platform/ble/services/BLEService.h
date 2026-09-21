#pragma once

#include <cstddef>
#include <memory>

#include "../../runtime/services/Service.h"

namespace platform::ble {

// The public API has no SDK types. Native builds never initialize a radio.
class BLEService final : public runtime::Service {
   public:
    enum class State { Stopped, Unavailable, Advertising, Connected, Secured, Failed };

    BLEService();
    ~BLEService() override;
    BLEService(const BLEService&) = delete;
    BLEService& operator=(const BLEService&) = delete;

    [[nodiscard]] bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] State state() const;
    [[nodiscard]] std::size_t bondCount() const;

   private:
    class Backend;
    std::unique_ptr<Backend> _backend;
};

}  // namespace platform::ble
