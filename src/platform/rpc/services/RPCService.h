#pragma once
#include <functional>
#include <vector>

#include "../../runtime/services/Service.h"
#include "../../tasking/services/TaskDispatchService.h"
#include "../MessageChannel.h"

namespace platform::rpc {

class RPCService final : public runtime::Service {
   public:
    using Completion = std::function<void(Error, std::span<const uint8_t>)>;
    using Handler = std::function<Reply(std::span<const uint8_t>)>;
    RPCService(Transport& transport, tasking::TaskDispatchService& tasks);
    ~RPCService() override;
    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] uint32_t session() const;
    [[nodiscard]] uint16_t request(uint16_t method, std::span<const uint8_t> payload, Completion completion,
                                   uint32_t now, uint32_t timeout = messageTimeout);
    bool cancel(uint16_t id);
    bool registerHandler(uint16_t method, Handler handler);
    void removeHandler(uint16_t method);

   private:
    class Pump;

    struct Pending {
        uint16_t id;
        uint16_t method;
        uint32_t started;
        uint32_t timeout;
        Completion completion;
    };

    struct Method {
        uint16_t id;
        Handler handler;
    };

    void _poll(uint32_t now);
    void _flush();
    bool _send(Message message, uint32_t timeout = messageTimeout);
    void _handle(const Message& message);
    void _failPending(Error error);
    Transport& _transport;
    tasking::TaskDispatchService& _tasks;
    tasking::TaskHandle _pump;
    std::vector<Pending> _pending;
    std::vector<Method> _methods;
    uint32_t _session = 0;
    uint16_t _nextId = 1;
    MessageChannel _channel;
    uint32_t _now = 0;
    bool _largeMessages = false;
    bool _ready = false;
    bool _running = false;
};

}  // namespace platform::rpc
