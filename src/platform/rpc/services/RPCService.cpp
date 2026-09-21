#include "RPCService.h"

#include <utility>

namespace platform::rpc {

class RPCService::Pump final : public tasking::Task {
   public:
    explicit Pump(RPCService& owner) : _owner(owner) {
    }

    tasking::ExecutionResult execute(const tasking::TaskContext& context) override {
        _owner._poll(context.now);
        return tasking::ExecutionResult::Complete;
    }

   private:
    RPCService& _owner;
};

RPCService::RPCService(Transport& transport, tasking::TaskDispatchService& tasks)
    : _transport(transport), _tasks(tasks) {
    _pending.reserve(8);
    _methods.reserve(8);
}

RPCService::~RPCService() {
    stop();
}

bool RPCService::start() {
    if (_running) return true;
    auto submission = _tasks.scheduleEvery(10, std::make_unique<Pump>(*this));
    if (submission.error != tasking::SubmitError::None) return false;
    _pump = std::move(submission.handle);
    _running = true;
    return true;
}

void RPCService::stop() {
    _running = false;
    _ready = false;
    _pump.cancel();
    _session = 0;
    _failPending(Error::Cancelled);
}

bool RPCService::isRunning() const {
    return _running;
}

uint32_t RPCService::session() const {
    return _running && _ready && _session == _transport.session() ? _session : 0;
}

uint16_t RPCService::request(uint16_t method, std::span<const uint8_t> payload, Completion completion, uint32_t now,
                             uint32_t timeout) {
    if (!session() || _pending.size() == 8 || payload.size() > 12 || !completion || timeout == 0 ||
        timeout > 0x7FFFFFFF || _nextId == 0)
        return 0;
    Message message{.id = _nextId++, .method = method, .size = static_cast<uint8_t>(payload.size())};
    std::copy(payload.begin(), payload.end(), message.payload.begin());
    auto packet = encode(message);
    if (!_transport.send(_session, {packet.bytes.data(), packet.size})) return 0;
    _pending.push_back({message.id, method, now, timeout, std::move(completion)});
    return message.id;
}

bool RPCService::cancel(uint16_t id) {
    const auto it = std::find_if(_pending.begin(), _pending.end(), [id](const auto& p) { return p.id == id; });
    if (it == _pending.end()) return false;
    auto completion = std::move(it->completion);
    _pending.erase(it);
    completion(Error::Cancelled, {});
    return true;
}

bool RPCService::registerHandler(uint16_t method, Handler handler) {
    if (method == helloMethod || method == pingMethod || !handler || _methods.size() == 8 ||
        std::any_of(_methods.begin(), _methods.end(), [method](const auto& m) { return m.id == method; }))
        return false;
    _methods.push_back({method, std::move(handler)});
    return true;
}

void RPCService::removeHandler(uint16_t method) {
    std::erase_if(_methods, [method](const auto& m) { return m.id == method; });
}

void RPCService::_failPending(Error error) {
    auto pending = std::move(_pending);
    _pending.clear();
    _pending.reserve(8);
    for (auto& p : pending) p.completion(error, {});
}

void RPCService::_poll(uint32_t now) {
    const auto current = _transport.session();
    if (current != _session) {
        _session = 0;
        _ready = false;
        _failPending(Error::Disconnected);
        _session = current;
    }
    // Expire first, so a late response at the deadline cannot succeed.
    std::vector<uint16_t> expired;
    for (const auto& p : _pending)
        if (now - p.started >= p.timeout) expired.push_back(p.id);
    for (auto id : expired) {
        auto it = std::find_if(_pending.begin(), _pending.end(), [id](const auto& p) { return p.id == id; });
        if (it == _pending.end()) continue;
        auto completion = std::move(it->completion);
        _pending.erase(it);
        completion(Error::Timeout, {});
    }
    Packet packet;
    for (int budget = 0; budget < 4 && _transport.receive(packet); ++budget) {
        if (!_session || packet.session != _session) continue;
        if (auto message = decode({packet.bytes.data(), packet.size})) _handle(*message);
    }
}

void RPCService::_handle(const Message& message) {
    if (message.kind != Kind::Request) {
        auto it = std::find_if(_pending.begin(), _pending.end(),
                               [&](const auto& p) { return p.id == message.id && p.method == message.method; });
        if (it == _pending.end()) return;
        auto completion = std::move(it->completion);
        _pending.erase(it);
        completion(message.kind == Kind::Error ? static_cast<Error>(message.payload[0]) : Error::None, message.data());
        return;
    }
    Reply reply;
    if (message.method == helloMethod) {
        if (message.size != 0)
            reply.error = Error::InvalidPayload;
        else
            _ready = true;
    } else if (!_ready) {
        reply.error = Error::Busy;
    } else if (message.method == pingMethod) {
        reply.payload = message.payload;
        reply.size = message.size;
    } else {
        auto it = std::find_if(_methods.begin(), _methods.end(), [&](const auto& m) { return m.id == message.method; });
        if (it == _methods.end())
            reply.error = Error::UnknownMethod;
        else {
            auto handler = it->handler;
            reply = handler(message.data());
        }
    }
    if (reply.size > 12) reply.error = Error::InvalidPayload;
    Message response{
        .kind = reply.error == Error::None ? Kind::Response : Kind::Error, .id = message.id, .method = message.method};
    response.payload = reply.payload;
    response.size = reply.size;
    if (reply.error != Error::None) {
        response.payload[0] = static_cast<uint8_t>(reply.error);
        response.size = 1;
    }
    const auto packet = encode(response);
    (void)_transport.send(_session, {packet.bytes.data(), packet.size});
}

}  // namespace platform::rpc
