import Foundation

public enum RPCError: Error, Equatable {
    case invalidFrame, unavailable, busy, timeout, disconnected, cancelled, remote(UInt8)
}

public struct RPCMessage: Equatable {
    public enum Kind: UInt8 { case request = 0, response = 1, error = 2 }
    public let kind: Kind
    public let id: UInt16
    public let method: UInt16
    public let payload: Data

    public init(kind: Kind, id: UInt16, method: UInt16, payload: Data) {
        self.kind = kind; self.id = id; self.method = method; self.payload = payload
    }
    public func encoded() throws -> Data {
        guard id != 0, payload.count <= 12, kind != .error || payload.count == 1 else { throw RPCError.invalidFrame }
        return Data([0xD1, kind.rawValue, UInt8(truncatingIfNeeded: id), UInt8(id >> 8),
                     UInt8(truncatingIfNeeded: method), UInt8(method >> 8), UInt8(payload.count), 0]) + payload
    }
    public static func decode(_ data: Data) throws -> RPCMessage {
        let bytes = Array(data)
        guard bytes.count >= 8, bytes[0] == 0xD1, let kind = Kind(rawValue: bytes[1]), bytes[7] == 0,
              bytes[6] <= 12, bytes.count == 8 + Int(bytes[6]) else { throw RPCError.invalidFrame }
        let id = UInt16(bytes[2]) | UInt16(bytes[3]) << 8
        guard id != 0, kind != .error || (bytes[6] == 1 && bytes[8] != 0) else { throw RPCError.invalidFrame }
        return RPCMessage(kind: kind, id: id, method: UInt16(bytes[4]) | UInt16(bytes[5]) << 8,
                          payload: Data(bytes.dropFirst(8)))
    }
}

/// Main-queue RPC endpoint, independent of Core Bluetooth and reusable in iOS.
/// The transport must preserve message boundaries and reset this peer on disconnection.
public final class RPCPeer {
    public static let maximumPayloadSize = 10 * 1024
    public var maximumPacketSize: () -> Int = { 20 }
    public var onTransportFailure: (() -> Void)?
    private let _channel = RPCMessageChannel()
    public typealias Completion = (Result<Data, RPCError>) -> Void
    public typealias AsyncHandler = (Data, @escaping (Result<Data, UInt8Error>) -> Void) -> Void
    public typealias Handler = (Data) -> Result<Data, UInt8Error>
    public struct UInt8Error: Error { public let code: UInt8; public init(_ code: UInt8) { self.code = code } }
    /// Enqueue a packet; never synchronously reenter this peer from the send callback.
    public var send: ((Data) -> Bool)?
    private struct Pending { let method: UInt16; let deadline: TimeInterval; let completion: Completion }
    private var _pending: [UInt16: Pending] = [:]
    private struct Deferred { let token: UUID; let message: RPCMessage; let deadline: TimeInterval }
    private var _deferred: [UInt16: Deferred] = [:]
    private var _asyncHandlers: [UInt16: AsyncHandler] = [:]
    private var _handlers: [UInt16: Handler] = [:]
    private var _nextID: UInt16 = 1
    private let _now: () -> TimeInterval
    public init(now: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime }) { _now = now }

    public func register(method: UInt16, handler: @escaping Handler) {
        _asyncHandlers.removeValue(forKey: method)
        _handlers[method] = handler
    }
    /// Handler and completion must execute on the owning queue. Completion is accepted at most once.
    public func registerAsync(method: UInt16, handler: @escaping AsyncHandler) {
        _handlers.removeValue(forKey: method)
        _asyncHandlers[method] = handler
    }
    @discardableResult
    public func request(method: UInt16, payload: Data = Data(), timeout: TimeInterval = 120,
                        completion: @escaping Completion) -> UInt16? {
        guard payload.count <= Self.maximumPayloadSize, timeout.isFinite, timeout > 0 else { completion(.failure(.invalidFrame)); return nil }
        guard _pending.count < 8, _nextID != 0 else { completion(.failure(.busy)); return nil }
        let id = _nextID
        _nextID &+= 1
        let message = RPCMessage(kind: .request, id: id, method: method, payload: payload)
        guard send != nil else { completion(.failure(.unavailable)); return nil }
        guard _channel.enqueue(message, now: _now(), timeout: timeout) else { completion(.failure(.busy)); return nil }
        _pending[id] = Pending(method: method, deadline: _now() + timeout, completion: completion)
        _pump()
        return id
    }
    @discardableResult
    public func cancel(_ id: UInt16) -> Bool {
        guard let pending = _pending.removeValue(forKey: id) else { return false }
        _channel.cancel(id)
        pending.completion(.failure(.cancelled))
        return true
    }
    public func poll() {
        let now = _now()
        let ids = _pending.filter { $0.value.deadline <= now }.map(\.key)
        for id in ids {
            _channel.cancel(id)
            _pending.removeValue(forKey: id)?.completion(.failure(.timeout))
        }
        let expired = _deferred.values.filter { $0.deadline <= now }
        for entry in expired { _completeDeferred(entry, result: .failure(.init(7))) }
        _pump()
    }
    public func reset() {
        send = nil
        _channel.reset()
        _deferred.removeAll()
        let pending = _pending; _pending.removeAll(); _nextID = 1
        for entry in pending.values { entry.completion(.failure(.disconnected)) }
    }
    public func receive(_ bytes: Data) throws {
        poll()
        guard let message = try _channel.receive(bytes, now: _now()) else { _pump(); return }
        _pump()
        if message.kind != .request {
            guard let pending = _pending[message.id], pending.method == message.method else { return }
            _channel.cancel(message.id)
            _pending.removeValue(forKey: message.id)
            pending.completion(message.kind == .response ? .success(message.payload) : .failure(.remote(message.payload.first!)))
            return
        }
        if let handler = _asyncHandlers[message.method] {
            guard _deferred[message.id] == nil else { return }
            guard _deferred.count < 8, _channel.reserveReply() else {
                try _respond(message, result: .failure(.init(3)))
                return
            }
            _channel.releaseReply()
            let header = RPCMessage(kind: message.kind, id: message.id, method: message.method, payload: Data())
            let entry = Deferred(token: UUID(), message: header, deadline: _now() + 120)
            _deferred[message.id] = entry
            handler(message.payload) { [weak self] result in self?._completeDeferred(entry, result: result) }
            return
        }
        let result: Result<Data, UInt8Error>
        if !_channel.reserveReply() { result = .failure(UInt8Error(3)) }
        else if message.method == 1 { result = .success(message.payload) }
        else if let handler = _handlers[message.method] { result = handler(message.payload) }
        else { result = .failure(UInt8Error(1)) }
        _channel.releaseReply()
        try _respond(message, result: result)
    }

    private func _completeDeferred(_ entry: Deferred, result: Result<Data, UInt8Error>) {
        guard _deferred[entry.message.id]?.token == entry.token else { return }
        _deferred.removeValue(forKey: entry.message.id)
        let reply: Result<Data, UInt8Error>
        if !_channel.reserveReply() { reply = .failure(.init(3)) }
        else { _channel.releaseReply(); reply = _now() >= entry.deadline ? .failure(.init(7)) : result }
        do { try _respond(entry.message, result: reply) }
        catch { reset(); onTransportFailure?() }
    }

    private func _respond(_ message: RPCMessage, result: Result<Data, UInt8Error>) throws {
        let response: RPCMessage
        switch result {
        case .success(let data) where data.count <= Self.maximumPayloadSize:
            response = RPCMessage(kind: .response, id: message.id, method: message.method, payload: data)
        case .success:
            response = RPCMessage(kind: .error, id: message.id, method: message.method, payload: Data([2]))
        case .failure(let error):
            response = RPCMessage(kind: .error, id: message.id, method: message.method, payload: Data([error.code]))
        }
        if !_channel.enqueue(response, now: _now()) {
            let busy = RPCMessage(kind: .error, id: message.id, method: message.method, payload: Data([3]))
            guard _channel.enqueue(busy, now: _now()) else { throw RPCError.busy }
        }
        _pump()
    }

    private func _pump() {
        guard let send else { return }
        _channel.pump(now: _now(), packetSize: maximumPacketSize(), send: send)
        if _channel.isFailed {
            reset()
            onTransportFailure?()
        }
    }
}
