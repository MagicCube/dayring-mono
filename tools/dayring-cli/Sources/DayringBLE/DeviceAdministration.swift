import Foundation

public enum DeviceAdministration {
    public static let pairingResetMethod: UInt16 = 6
    public static let rebootMethod: UInt16 = 7

    public enum ResetState: Equatable { case pending, complete }
    public static func decodeResetState(_ data: Data) throws -> ResetState {
        switch data {
        case Data([0]): return .pending
        case Data([1]): return .complete
        default: throw RPCError.invalidFrame
        }
    }
}
