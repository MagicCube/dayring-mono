import Foundation

@main
struct RPCInterop {
    static func main() throws {
        precondition(CommandLine.arguments.count == 2)
        try checkBoundsAndLifecycle()
        try checkFragmentValidation()
        for packetSize in [20, 64, 244] { try exchange(packetSize: packetSize) }
        print("Swift/C++ simultaneous 10 KiB request and response interoperability passed at 20/64/244-byte packet limits")
    }

    static func exchange(packetSize: Int) throws {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: CommandLine.arguments[1])
        process.arguments = [String(packetSize)]
        let input = Pipe(), output = Pipe()
        process.standardInput = input; process.standardOutput = output
        try process.run()
        defer {
            try? input.fileHandleForWriting.close()
            if process.isRunning { process.terminate() }
        }
        var now: TimeInterval = 0
        let peer = RPCPeer(now: { now })
        peer.maximumPacketSize = { packetSize }
        var toDevice: [Data] = []
        peer.send = { bytes in
            precondition(bytes.count <= packetSize)
            toDevice.append(bytes)
            return true
        }
        var readBuffer = Data()
        func readLine() throws -> String {
            while readBuffer.firstIndex(of: 10) == nil {
                let data = output.fileHandleForReading.availableData
                guard !data.isEmpty else {
                    fatalError("C++ peer terminated before completing the exchange")
                }
                readBuffer.append(data)
            }
            let newline = readBuffer.firstIndex(of: 10)!
            let line = String(data: readBuffer[..<newline], encoding: .utf8)!
            readBuffer.removeSubrange(...newline)
            return line
        }
        var deviceCompleted = false
        func command(_ text: String) throws {
            try input.fileHandleForWriting.write(contentsOf: Data((text + "\n").utf8))
            while true {
                let line = try readLine()
                if line == "END" { break }
                if line == "COMPLETE" { deviceCompleted = true; continue }
                if line == "PASS" { continue }
                precondition(line.hasPrefix("FRAME "))
                let hex = Array(line.dropFirst(6))
                let bytes = Data(stride(from: 0, to: hex.count, by: 2).map {
                    UInt8(String(hex[$0...($0 + 1)]), radix: 16)!
                })
                try peer.receive(bytes)
            }
        }
        func hex(_ data: Data) -> String { data.map { String(format: "%02x", $0) }.joined() }
        let data = Data((0..<RPCPeer.maximumPayloadSize).map { UInt8($0 % 251) })
        var handled = 0, completed = false
        peer.register(method: 42) { bytes in
            precondition(bytes == data)
            handled += 1
            return .success(data)
        }
        // Negotiate without consuming the Swift request counter: both application requests use ID 1.
        try command(hex(try RPCMessage(kind: .request, id: 500, method: 0, payload: Data([2])).encoded()))
        try command("start")
        precondition(peer.request(method: 42, payload: data) { result in
            precondition(try! result.get() == data)
            completed = true
        } == 1)
        for _ in 0..<12000 {
            now += 0.01
            peer.poll()
            if !toDevice.isEmpty { try command(hex(toDevice.removeFirst())) }
            else { try command("tick") }
            if completed && deviceCompleted && toDevice.isEmpty { break }
        }
        precondition(completed && deviceCompleted && handled == 1)
        try command("finish")
        process.waitUntilExit()
        precondition(process.terminationStatus == 0)
    }
}
