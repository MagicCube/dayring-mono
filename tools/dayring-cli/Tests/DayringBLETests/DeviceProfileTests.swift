import CoreBluetooth
import XCTest
@testable import DayringBLE

final class DeviceProfileTests: XCTestCase {
    func testUnknownServiceNeverMatchesEvenWithKnownIdentifier() {
        let id = UUID()
        let profile = DeviceProfile(peripheralID: id)
        XCTAssertFalse(profile.matches(identifier: id, advertisedServices: []))
        XCTAssertFalse(profile.matches(identifier: id, advertisedServices: [CBUUID(string: "180D")]))
    }

    func testServiceAndOptionalDeviceSelection() {
        let id = UUID()
        let service = CBUUID(nsuuid: DeviceProfile.dayringServiceUUID)
        XCTAssertTrue(DeviceProfile().matches(identifier: id, advertisedServices: [service]))
        let profile = DeviceProfile(peripheralID: id)
        XCTAssertTrue(profile.matches(identifier: id, advertisedServices: [service]))
        XCTAssertFalse(profile.matches(identifier: UUID(), advertisedServices: [service]))
    }
}
