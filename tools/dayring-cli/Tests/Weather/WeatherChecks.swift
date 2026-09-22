import DayringBLE
import Foundation

@main
struct WeatherChecks {
    static let report = WeatherReport(weatherCode: 113, minTempC: -5, maxTempC: 12, city: "上海")

    static func main() throws {
        let json = Data(#"{"current_condition":[{"weatherCode":"113"}],"weather":[{"date":"2026-09-22","mintempC":"-5","maxtempC":"12"}],"nearest_area":[{"areaName":[{"value":"上海"}]}]}"#.utf8)
        assert(try! WeatherReport.decode(json) == report)
        assert((try? WeatherReport.decode(Data("{}".utf8))) == nil)
        assert((try? WeatherReport(weatherCode: 0, minTempC: 1, maxTempC: 2, city: "x").encoded()) == nil)
        assert((try? WeatherReport(weatherCode: 113, minTempC: 3, maxTempC: 2, city: "x").encoded()) == nil)
        assert((try? WeatherReport(weatherCode: 113, minTempC: 1, maxTempC: 2, city: "\n").encoded()) == nil)
        assert(WttrWeatherSource.url.absoluteString == "https://wttr.in/?format=j1")
        let missingCity = String(data: json, encoding: .utf8)!.replacingOccurrences(of: "上海", with: " ")
        assert((try? WeatherReport.decode(Data(missingCity.utf8))) == nil)
        try serviceChecks()
        try asyncChecks()
        if CommandLine.arguments.count == 2 { try report.encoded().write(to: URL(fileURLWithPath: CommandLine.arguments[1])) }
        print("Weather on-demand fetching, failures, isolation, URL and wire checks passed")
    }

    static func serviceChecks() throws {
        var fetches = 0
        var callbacks: [(Result<WeatherReport, Error>) -> Void] = []
        let service = WeatherRPCService(fetch: { callback in fetches += 1; callbacks.append(callback) })
        service.setConnected(true)
        assert(fetches == 0)
        var responses: [Result<Data, RPCPeer.UInt8Error>] = []
        service.get(Data([1])) { responses.append($0) }
        assert(code(responses.removeLast()) == 2 && fetches == 0)
        service.get(Data()) { responses.append($0) }
        assert(fetches == 1 && responses.isEmpty)
        service.get(Data()) { responses.append($0) }
        assert(code(responses.removeLast()) == 3)
        callbacks[0](.success(report))
        let payload = try responses.removeLast().get()
        assert(payload == (try! report.encoded()))
        service.get(Data()) { responses.append($0) }
        assert(fetches == 2)
        callbacks[0](.success(report))
        assert(responses.isEmpty)
        callbacks[1](.failure(WeatherReport.Failure.invalidResponse))
        assert(code(responses.removeLast()) == 7)
        service.get(Data()) { responses.append($0) }
        service.setConnected(false)
        assert(code(responses.removeLast()) == 7)
        service.setConnected(true)
        callbacks[2](.success(report))
        assert(responses.isEmpty && fetches == 3)
    }

    static func code(_ result: Result<Data, RPCPeer.UInt8Error>) -> UInt8 {
        if case .failure(let error) = result { return error.code }
        return 0
    }

    static func asyncChecks() throws {
        var now: TimeInterval = 0
        let peer = RPCPeer(now: { now })
        var sent: [Data] = []
        peer.send = { sent.append($0); return true }
        var callbacks: [(Result<Data, RPCPeer.UInt8Error>) -> Void] = []
        peer.registerAsync(method: 12) { _, callback in callbacks.append(callback) }
        func request(_ id: UInt16, method: UInt16 = 12) throws {
            try peer.receive(RPCMessage(kind: .request, id: id, method: method, payload: Data()).encoded())
        }
        try request(1)
        assert(sent.isEmpty)
        try request(2, method: 1)
        assert(try! RPCMessage.decode(sent.removeFirst()).id == 2)
        callbacks[0](.success(Data([42])))
        assert(try! RPCMessage.decode(sent.removeFirst()).payload == Data([42]))
        callbacks[0](.success(Data([43])))
        assert(sent.isEmpty)
        try request(3)
        peer.reset()
        peer.send = { sent.append($0); return true }
        try request(3)
        callbacks[1](.success(Data([44])))
        assert(sent.isEmpty)
        callbacks[2](.success(Data([45])))
        assert(try! RPCMessage.decode(sent.removeFirst()).payload == Data([45]))
        try request(4)
        now = 121
        peer.poll()
        assert(try! RPCMessage.decode(sent.removeFirst()).kind == .error)
        callbacks[3](.success(Data([46])))
        assert(sent.isEmpty)
        for id in UInt16(10)..<19 { try request(id) }
        assert(callbacks.count == 12)
        assert(try! RPCMessage.decode(sent.removeFirst()).payload == Data([3]))
        peer.reset()
    }
}
