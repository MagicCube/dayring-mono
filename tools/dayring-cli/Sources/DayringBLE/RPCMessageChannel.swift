import Foundation

struct RPCFragment {
    let kind: RPCMessage.Kind
    let id: UInt16
    let method: UInt16
    let total: Int
    let offset: Int
    let acknowledgment: Bool
    let payload: Data

    func encoded() -> Data {
        var bytes: [UInt8] = [0xD2, kind.rawValue | (acknowledgment ? 0x80 : 0)]
        for word in [Int(id), Int(method), total, offset] {
            bytes += [UInt8(truncatingIfNeeded: word), UInt8(truncatingIfNeeded: word >> 8)]
        }
        return Data(bytes) + payload
    }

    static func decode(_ data: Data) throws -> RPCFragment {
        let b = Array(data)
        guard (10...244).contains(b.count), b[0] == 0xD2, b[1] & 0x7F <= 1,
              let kind = RPCMessage.Kind(rawValue: b[1] & 0x7F) else { throw RPCError.invalidFrame }
        func word(_ i: Int) -> Int { Int(b[i]) | Int(b[i + 1]) << 8 }
        let f = RPCFragment(kind: kind, id: UInt16(word(2)), method: UInt16(word(4)),
                            total: word(6), offset: word(8), acknowledgment: b[1] & 0x80 != 0,
                            payload: Data(b.dropFirst(10)))
        guard f.id != 0, (13...RPCPeer.maximumPayloadSize).contains(f.total), f.offset <= f.total,
              f.acknowledgment ? (f.payload.isEmpty && f.offset > 0)
                : (!f.payload.isEmpty && f.offset + f.payload.count <= f.total) else { throw RPCError.invalidFrame }
        return f
    }
}

/// Bounded message storage; only one fragmented message is in flight per direction.
final class RPCMessageChannel {
    private struct Outgoing {
        let message: RPCMessage
        let started: TimeInterval
        let timeout: TimeInterval
        var sentAt: TimeInterval = 0
        var offset = 0
        var awaiting = 0
    }
    private struct Small { let bytes: Data; let started: TimeInterval }
    private struct Incoming {
        let kind: RPCMessage.Kind
        let id: UInt16
        let method: UInt16
        let total: Int
        var payload = Data()
        var receivedAt: TimeInterval
    }
    private var _outgoing: [Outgoing] = []
    private var _small: [Small] = []
    private var _ack: Small?
    private var _incoming: Incoming?
    private(set) var queuedBytes = 0
    private(set) var isFailed = false
    private var _replyReserved = false

    func enqueue(_ message: RPCMessage, now: TimeInterval, timeout: TimeInterval = 120) -> Bool {
        guard !isFailed, message.payload.count <= RPCPeer.maximumPayloadSize else { return false }
        if message.payload.count <= 12 {
            guard _small.count < (_replyReserved ? 7 : 8), let bytes = try? message.encoded() else { return false }
            _small.append(Small(bytes: bytes, started: now))
        } else {
            guard _outgoing.count < (_replyReserved ? 7 : 8),
                  queuedBytes + message.payload.count <= (_replyReserved ? 1 : 2) * RPCPeer.maximumPayloadSize else { return false }
            queuedBytes += message.payload.count
            _outgoing.append(Outgoing(message: message, started: now, timeout: timeout))
        }
        return true
    }

    func receive(_ bytes: Data, now: TimeInterval) throws -> RPCMessage? {
        guard !isFailed else { throw RPCError.unavailable }
        if bytes.first == 0xD1 { return try RPCMessage.decode(bytes) }
        let f = try RPCFragment.decode(bytes)
        if f.acknowledgment { _acknowledge(f); return nil }
        guard _ack == nil else { throw RPCError.invalidFrame }
        if f.offset == 0 {
            _incoming = Incoming(kind: f.kind, id: f.id, method: f.method, total: f.total, receivedAt: now)
            _incoming?.payload.reserveCapacity(f.total)
        }
        guard let rx = _incoming, rx.kind == f.kind, rx.id == f.id, rx.method == f.method,
              rx.total == f.total, rx.payload.count == f.offset else { throw RPCError.invalidFrame }
        _incoming?.payload.append(f.payload)
        _incoming?.receivedAt = now
        let next = f.offset + f.payload.count
        _ack = Small(bytes: RPCFragment(kind: f.kind, id: f.id, method: f.method, total: f.total,
                                       offset: next, acknowledgment: true, payload: Data()).encoded(), started: now)
        guard next == f.total else { return nil }
        let message = RPCMessage(kind: f.kind, id: f.id, method: f.method, payload: _incoming!.payload)
        _incoming = nil
        return message
    }

    private func _acknowledge(_ f: RPCFragment) {
        guard let tx = _outgoing.first, f.kind == tx.message.kind, f.id == tx.message.id,
              f.method == tx.message.method, f.total == tx.message.payload.count,
              tx.awaiting > 0, f.offset == tx.awaiting else { return }
        _outgoing[0].offset = tx.awaiting
        _outgoing[0].awaiting = 0
        if tx.awaiting == tx.message.payload.count {
            queuedBytes -= tx.message.payload.count
            _outgoing.removeFirst()
        }
    }

    func pump(now: TimeInterval, packetSize: Int, send: (Data) -> Bool) {
        guard !isFailed else { return }
        if let rx = _incoming, now - rx.receivedAt >= 5 { _incoming = nil }
        if let tx = _outgoing.first, now - tx.started >= tx.timeout || (tx.awaiting > 0 && now - tx.sentAt >= 5) {
            isFailed = true; return
        }
        func transmit(_ item: Small) -> Bool {
            if now - item.started >= 5 { isFailed = true; return false }
            return send(item.bytes)
        }
        if let ack = _ack {
            guard transmit(ack) else { return }
            _ack = nil
        }
        for _ in 0..<4 {
            guard let small = _small.first else { break }
            guard transmit(small) else { return }
            _small.removeFirst()
        }
        guard _small.isEmpty, let tx = _outgoing.first, tx.awaiting == 0 else { return }
        let count = min(max(20, min(packetSize, 244)) - 10, tx.message.payload.count - tx.offset)
        let f = RPCFragment(kind: tx.message.kind, id: tx.message.id, method: tx.message.method,
                            total: tx.message.payload.count, offset: tx.offset, acknowledgment: false,
                            payload: tx.message.payload.subdata(in: tx.offset..<(tx.offset + count)))
        if send(f.encoded()) {
            _outgoing[0].awaiting = tx.offset + count
            _outgoing[0].sentAt = now
        }
    }

    func reserveReply() -> Bool {
        guard !_replyReserved, !isFailed, queuedBytes <= RPCPeer.maximumPayloadSize,
              _outgoing.count < 8, _small.count < 8 else { return false }
        _replyReserved = true
        return true
    }

    func releaseReply() { _replyReserved = false }

    func cancel(_ id: UInt16) {
        _outgoing.removeAll { tx in
            guard tx.message.kind == .request, tx.message.id == id else { return false }
            queuedBytes -= tx.message.payload.count
            return true
        }
        _small.removeAll { tx in
            guard let m = try? RPCMessage.decode(tx.bytes) else { return false }
            return m.kind == .request && m.id == id
        }
    }

    func reset() {
        _outgoing.removeAll(); _small.removeAll(); _ack = nil; _incoming = nil
        queuedBytes = 0; isFailed = false; _replyReserved = false
    }
}
