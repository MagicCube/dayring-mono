import XCTest
@testable import DayringBLE

final class RPCPeerTests: XCTestCase {
    func testWireContractAndMalformedFrames() throws {
        let frame = RPCMessage(kind: .request, id: 0x1234, method: 2, payload: Data())
        XCTAssertEqual(try frame.encoded(), Data([0xD1, 0, 0x34, 0x12, 2, 0, 0, 0]))
        XCTAssertEqual(try RPCMessage.decode(frame.encoded()), frame)
        for data in [Data(), Data([0xD1, 0, 0, 0, 2, 0, 0, 0]),
                     Data([0xD1, 1, 1, 0, 2, 0, 0, 1]), Data([0xD1, 2, 1, 0, 2, 0, 0, 0])] {
            XCTAssertThrowsError(try RPCMessage.decode(data))
        }
    }

    func testBidirectionalRequestsAndTimeouts() throws {
        var now: TimeInterval = 0
        let peer = RPCPeer(now: { now })
        var sent: [Data] = []
        peer.send = { sent.append($0); return true }
        var result: Result<Data, RPCError>?
        let id = peer.request(method: 1, payload: Data([7]), timeout: 1) { result = $0 }!
        try peer.receive(RPCMessage(kind: .response, id: id, method: 2, payload: Data()).encoded())
        XCTAssertNil(result)
        try peer.receive(RPCMessage(kind: .request, id: 123, method: 1, payload: Data([9])).encoded())
        XCTAssertEqual(try RPCMessage.decode(sent.last!).payload, Data([9]))
        try peer.receive(RPCMessage(kind: .response, id: id, method: 1, payload: Data([7])).encoded())
        XCTAssertEqual(try result?.get(), Data([7]))
        result = nil
        let timeoutID = peer.request(method: 1, timeout: 1) { result = $0 }!
        now = 1
        try peer.receive(RPCMessage(kind: .response, id: timeoutID, method: 1, payload: Data()).encoded())
        XCTAssertEqual(result, .failure(.timeout))
        _ = peer.request(method: 1) { result = $0 }
        peer.reset()
        XCTAssertEqual(result, .failure(.disconnected))
        XCTAssertNil(peer.request(method: 1) { result = $0 })
        XCTAssertEqual(result, .failure(.unavailable))
    }

    func testHandlersBoundsAndClockEncoding() throws {
        let peer = RPCPeer()
        var sent = Data()
        peer.send = { sent = $0; return true }
        try peer.receive(RPCMessage(kind: .request, id: 1, method: 99, payload: Data()).encoded())
        XCTAssertEqual(try RPCMessage.decode(sent).payload, Data([1]))
        peer.register(method: 2) { data in data.isEmpty ? .success(Data([42])) : .failure(.init(2)) }
        try peer.receive(RPCMessage(kind: .request, id: 2, method: 2, payload: Data()).encoded())
        XCTAssertEqual(try RPCMessage.decode(sent).payload, Data([42]))
        for _ in 0..<8 { XCTAssertNotNil(peer.request(method: 1) { _ in }) }
        XCTAssertNil(peer.request(method: 1) { XCTAssertEqual($0, .failure(.busy)) })
        let date = Date(timeIntervalSince1970: 1709251199)
        let sample = ClockSample.encode(date: date, timeZone: TimeZone(secondsFromGMT: 28800)!)!
        XCTAssertEqual(sample, Data([127, 26, 225, 101, 0, 0, 0, 0, 128, 112, 0, 0]))
        XCTAssertNil(ClockSample.encode(date: Date(timeIntervalSince1970: 0)))
    }
}
