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
    _channel.reset();
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
    if (!session() || _pending.size() == 8 || payload.size() > (_largeMessages ? maxPayloadSize : 12) || !completion ||
        timeout == 0 || timeout > 0x7FFFFFFF || _nextId == 0)
        return 0;
    _now = now;
    Message message{.id = _nextId++, .method = method, .payload = {payload.begin(), payload.end()}};
    const auto id = message.id;
    if (!_send(std::move(message), timeout)) return 0;
    _pending.push_back({id, method, now, timeout, std::move(completion)});
    return id;
}

bool RPCService::cancel(uint16_t id) {
    const auto it = std::find_if(_pending.begin(), _pending.end(), [id](const auto& p) { return p.id == id; });
    if (it == _pending.end()) return false;
    auto completion = std::move(it->completion);
    _channel.cancel(id);
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
    _now = now;
    const auto current = _transport.session();
    if (current != _session) {
        _session = 0;
        _ready = false;
        _largeMessages = false;
        _channel.reset();
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
        _channel.cancel(id);
        _pending.erase(it);
        completion(Error::Timeout, {});
    }
    _flush();
    if (_channel.isFailed()) return;
    Packet packet;
    for (int budget = 0; budget < 4 && _transport.receive(packet); ++budget) {
        if (!_session || packet.session != _session) continue;
        if (packet.bytes[0] == 0xD2 && !_largeMessages) continue;
        if (auto message = _channel.receive({packet.bytes.data(), packet.size}, now)) _handle(*message);
        _flush();
        if (_channel.isFailed()) break;
    }
}

void RPCService::_handle(const Message& message) {
    if (message.kind != Kind::Request) {
        auto it = std::find_if(_pending.begin(), _pending.end(),
                               [&](const auto& p) { return p.id == message.id && p.method == message.method; });
        if (it == _pending.end()) return;
        auto completion = std::move(it->completion);
        _channel.cancel(message.id);
        _pending.erase(it);
        completion(message.kind == Kind::Error ? static_cast<Error>(message.payload[0]) : Error::None, message.data());
        return;
    }
    Reply reply;
    if (!_channel.reserveReply()) {
        reply.error = Error::Busy;
    } else if (message.method == helloMethod) {
        if (message.payload.empty()) {
            _ready = true;
        } else if (message.payload == std::vector<uint8_t>{2}) {
            _largeMessages = true;
            _ready = true;
            reply.payload = {2};
        } else {
            reply.error = Error::InvalidPayload;
        }
    } else if (!_ready) {
        reply.error = Error::Busy;
    } else if (message.method == pingMethod) {
        reply.payload = message.payload;
    } else {
        auto it = std::find_if(_methods.begin(), _methods.end(), [&](const auto& m) { return m.id == message.method; });
        if (it == _methods.end())
            reply.error = Error::UnknownMethod;
        else {
            auto handler = it->handler;
            reply = handler(message.data());
        }
    }
    _channel.releaseReply();
    if (reply.payload.size() > (_largeMessages ? maxPayloadSize : 12)) reply.error = Error::InvalidPayload;
    Message response{.kind = reply.error == Error::None ? Kind::Response : Kind::Error,
                     .id = message.id,
                     .method = message.method,
                     .payload = std::move(reply.payload)};
    if (reply.error != Error::None) response.payload = {static_cast<uint8_t>(reply.error)};
    if (!_send(std::move(response))) {
        if (!_send({.kind = Kind::Error,
                    .id = message.id,
                    .method = message.method,
                    .payload = {static_cast<uint8_t>(Error::Busy)}})) {
            _ready = false;
            _transport.disconnect();
        }
    }
}

bool RPCService::_send(Message message, uint32_t timeout) {
    if (!_channel.enqueue(std::move(message), _now, timeout)) return false;
    _flush();
    return !_channel.isFailed();
}

void RPCService::_flush() {
    if (!_session) return;
    if (!_channel.isFailed())
        _channel.pump(_now, _transport.packetSize(), [this](auto bytes) { return _transport.send(_session, bytes); });
    if (_channel.isFailed()) {
        _ready = false;
        _failPending(Error::Disconnected);
        _transport.disconnect();
    }
}

}  // namespace platform::rpc
