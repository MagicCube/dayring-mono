import Foundation

extension RPCInterop {
    static func checkBoundsAndLifecycle() throws {
        var now: TimeInterval = 0
        let peer = RPCPeer(now: { now })
        var sent: [Data] = []
        var writable = false
        peer.send = { if writable { sent.append($0) }; return writable }
        let large = Data(repeating: 42, count: RPCPeer.maximumPayloadSize)
        var result: Result<Data, RPCError>?
        precondition(peer.request(method: 42, payload: large + Data([0])) { result = $0 } == nil)
        precondition(result == .failure(.invalidFrame))
        let first = peer.request(method: 42, payload: large) { result = $0 }!
        let second = peer.request(method: 42, payload: large) { _ in }!
        precondition(peer.request(method: 42, payload: large) { result = $0 } == nil)
        precondition(result == .failure(.busy))
        var invoked = false
        peer.register(method: 80) { _ in invoked = true; return .success(Data()) }
        try peer.receive(RPCMessage(kind: .request, id: 99, method: 80, payload: Data()).encoded())
        precondition(!invoked)
        peer.cancel(first)
        precondition(result == .failure(.cancelled))
        let third = peer.request(method: 42, payload: large, timeout: 1) { result = $0 }!
        writable = true
        peer.poll()
        precondition(try! RPCFragment.decode(sent.last!).id == second)
        now = 1
        peer.poll()
        precondition(result == .failure(.timeout))
        precondition(!peer.cancel(third))
        peer.reset()
        precondition(peer.request(method: 42, payload: large) { result = $0 } == nil)
        precondition(result == .failure(.unavailable))
        peer.send = { sent.append($0); return true }
        precondition(peer.request(method: 42, payload: large) { result = $0 } != nil)
        var failed = false
        peer.onTransportFailure = { failed = true }
        now = 6
        peer.poll()
        precondition(failed && result == .failure(.disconnected))
        peer.reset()
        peer.send = { sent.append($0); return true }
        peer.register(method: 99) { _ in .success(large + Data([0])) }
        try peer.receive(RPCMessage(kind: .request, id: 50, method: 99, payload: Data()).encoded())
        precondition(try! RPCMessage.decode(sent.last!).kind == .error)
        precondition(try! RPCMessage.decode(sent.last!).payload == Data([2]))
        print("Swift payload bounds, queue budget, cancellation, timeout, reset and oversized return checks passed")
    }

    static func checkFragmentValidation() throws {
        let channel = RPCMessageChannel()
        let data = Data(repeating: 7, count: 13)
        let first = RPCFragment(kind: .request, id: 0x1234, method: 42, total: 13, offset: 0,
                                acknowledgment: false, payload: data.prefix(10))
        precondition(first.encoded() == Data([0xD2, 0, 0x34, 0x12, 42, 0, 13, 0, 0, 0]) + data.prefix(10))
        precondition(try! channel.receive(first.encoded(), now: 0) == nil)
        var ack = Data()
        channel.pump(now: 0, packetSize: 20) { ack = $0; return true }
        precondition(ack == Data([0xD2, 0x80, 0x34, 0x12, 42, 0, 13, 0, 10, 0]))
        let wrong = RPCFragment(kind: .request, id: 0x1234, method: 42, total: 13, offset: 11,
                                acknowledgment: false, payload: data.prefix(2))
        do { _ = try channel.receive(wrong.encoded(), now: 1); preconditionFailure("offset accepted") }
        catch RPCError.invalidFrame {} catch { throw error }
        channel.reset()
        var oversized = first.encoded()
        oversized[6] = 1; oversized[7] = 40
        do { _ = try channel.receive(oversized, now: 1); preconditionFailure("oversized total accepted") }
        catch RPCError.invalidFrame {} catch { throw error }
        _ = try channel.receive(first.encoded(), now: 1)
        channel.pump(now: 1, packetSize: 20) { _ in true }
        channel.pump(now: 6, packetSize: 20) { _ in true }
        let tail = RPCFragment(kind: .request, id: 0x1234, method: 42, total: 13, offset: 10,
                               acknowledgment: false, payload: data.prefix(3))
        do { _ = try channel.receive(tail.encoded(), now: 6); preconditionFailure("expired assembly retained") }
        catch RPCError.invalidFrame {} catch { throw error }
        channel.reset()
        _ = try channel.receive(first.encoded(), now: 6)
        channel.pump(now: 6, packetSize: 20) { _ in true }
        let completed = try channel.receive(tail.encoded(), now: 6)
        precondition(completed?.payload == data)
        print("Swift wire vectors, malformed fragments, assembly expiry and reset checks passed")
    }
}
