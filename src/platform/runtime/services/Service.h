#pragma once

#include <cstdint>

namespace platform::runtime {

// Lifecycle runs on the owning loop, outside task execution and callbacks.
// Failed start cleans up partial resources; stop is synchronous and idempotent.
class Service {
   public:
    virtual ~Service() = default;
    [[nodiscard]] virtual bool start() = 0;
    virtual void stop() = 0;

    virtual void update(std::uint32_t now) {
        (void)now;
    }
};

}  // namespace platform::runtime
