import CoreBluetooth
import Foundation

/// Discovery is a compatibility hint, not authentication or proof of ownership.
public struct DeviceProfile {
    public static let dayringServiceUUID = UUID(uuidString: "B86E1000-7C65-4DAB-9F21-6A57D2E84010")!
    public static let pairingStatusUUID = UUID(uuidString: "B86E1001-7C65-4DAB-9F21-6A57D2E84010")!
    public static let rpcWriteUUID = UUID(uuidString: "B86E1002-7C65-4DAB-9F21-6A57D2E84010")!
    public static let rpcNotifyUUID = UUID(uuidString: "B86E1003-7C65-4DAB-9F21-6A57D2E84010")!
    public static let pairedResponse = Data("dayring-bonded-v1".utf8)
    public let serviceUUID: UUID
    public let peripheralID: UUID?

    public init(serviceUUID: UUID = Self.dayringServiceUUID, peripheralID: UUID? = nil) {
        self.serviceUUID = serviceUUID
        self.peripheralID = peripheralID
    }

    public func matches(identifier: UUID, advertisedServices: [CBUUID]) -> Bool {
        (peripheralID == nil || peripheralID == identifier)
            && advertisedServices.contains(CBUUID(nsuuid: serviceUUID))
    }
}

public enum CentralEvent {
    case bluetoothState(String)
    case scanning
    case discovered(id: UUID, name: String, rssi: Int, recognized: Bool)
    case connecting(UUID)
    case connected(UUID)
    case serviceAvailable(UUID)
    case pairing(UUID)
    case paired(UUID)
    case disconnected(UUID, String?)
    case failed(String)
    case rpcReady(UUID)
    case rpcStatus(String)
    case stopped
}
