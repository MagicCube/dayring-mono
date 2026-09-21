import XCTest
@testable import DayringBLE

@MainActor
final class ConnectionRecoveryTests: XCTestCase {
    func testRetriesAreBoundedUntilApplicationHandshakeSucceeds() {
        var recovery = ConnectionRecovery()
        XCTAssertEqual(recovery.nextDelay(), 1)
        XCTAssertEqual(recovery.nextDelay(), 2)
        XCTAssertNil(recovery.nextDelay())
        recovery.connected()
        XCTAssertEqual(recovery.nextDelay(), 1)
    }

    func testWriteTimeoutCleanupDoesNotReportASecondHandshakeFailure() {
        let central = BLECentral()
        var errors: [String] = []
        var stops = 0
        central.onEvent = {
            if case .failed(let reason) = $0 { errors.append(reason) }
            if case .stopped = $0 { stops += 1 }
        }
        central._running = true
        central._rpc.send = { _ in true }
        central.beginRPCHandshake()
        central._fail("RPC write acknowledgment timeout")
        XCTAssertEqual(errors, ["RPC write acknowledgment timeout"])
        XCTAssertEqual(stops, 1)
        XCTAssertFalse(central._running)
        XCTAssertFalse(central._rpcReady)
        central._fail("late callback")
        XCTAssertEqual(errors.count, 1)
    }

    func testServiceRediscoveryCancelsOldHandshakeWithoutStoppingNewSession() throws {
        let central = BLECentral()
        var statuses: [String] = []
        var errors: [String] = []
        central.onEvent = {
            if case .rpcStatus(let status) = $0 { statuses.append(status) }
            if case .failed(let reason) = $0 { errors.append(reason) }
        }
        central._running = true
        central._rpc.send = { _ in true }
        central.beginRPCHandshake()
        let epoch = central._rpcEpoch
        central.resetRPC()
        XCTAssertGreaterThan(central._rpcEpoch, epoch)
        XCTAssertTrue(central._running)
        XCTAssertTrue(errors.isEmpty)
        XCTAssertFalse(central._rpcReady)
        var writes: [RPCMessage] = []
        central._rpc.send = { writes.append(try! RPCMessage.decode($0)); return true }
        _ = central._recovery.nextDelay()
        central.beginRPCHandshake()
        let hello = try XCTUnwrap(writes.first)
        try central._rpc.receive(RPCMessage(kind: .response, id: hello.id, method: 0, payload: Data()).encoded())
        XCTAssertTrue(central._rpcReady)
        XCTAssertEqual(central._recovery.attempts, 0)
        XCTAssertEqual(statuses, ["RPC ready"])
        XCTAssertEqual(writes.last?.method, 1)
        central.stop()
        XCTAssertTrue(errors.isEmpty)
    }
}
