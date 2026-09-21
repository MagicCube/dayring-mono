#pragma once

#include <cstddef>
#include <memory>

#include "../../rpc/Transport.h"
#include "../../runtime/services/Service.h"

namespace platform::ble {

// The public API has no SDK types. Native builds never initialize a radio.
class BLEService final : public runtime::Service, public rpc::Transport {
   public:
    enum class State { Stopped, Unavailable, Advertising, Connected, Secured, Failed };

    BLEService();
    ~BLEService() override;
    BLEService(const BLEService&) = delete;
    BLEService& operator=(const BLEService&) = delete;

    [[nodiscard]] bool start() override;
    void stop() override;
    void update(uint32_t now) override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] State state() const;
    [[nodiscard]] std::size_t bondCount() const;
    [[nodiscard]] uint32_t session() const override;
    bool receive(rpc::Packet& packet) override;
    bool send(uint32_t session, std::span<const uint8_t> bytes) override;

   private:
    class Backend;
    std::unique_ptr<Backend> _backend;
};

}  // namespace platform::ble
