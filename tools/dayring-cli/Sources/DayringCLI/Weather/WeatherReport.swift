import Foundation

struct WeatherReport: Equatable {
    let weatherCode: UInt16
    let minTempC: Int16
    let maxTempC: Int16
    let city: String

    enum Failure: Error { case invalidResponse }

    static func decode(_ data: Data) throws -> WeatherReport {
        struct Name: Decodable { let value: String }
        struct Area: Decodable { let areaName: [Name] }
        struct Current: Decodable { let weatherCode: String }
        struct Day: Decodable { let date: String; let mintempC: String; let maxtempC: String }
        struct Response: Decodable {
            let current_condition: [Current]
            let weather: [Day]
            let nearest_area: [Area]
        }
        let response = try JSONDecoder().decode(Response.self, from: data)
        guard let current = response.current_condition.first, let day = response.weather.first,
              let city = response.nearest_area.first?.areaName.first?.value,
              let code = UInt16(current.weatherCode), let low = Int16(day.mintempC),
              let high = Int16(day.maxtempC) else { throw Failure.invalidResponse }
        let report = WeatherReport(weatherCode: code, minTempC: low, maxTempC: high,
                                   city: city.trimmingCharacters(in: .whitespacesAndNewlines))
        _ = try report.encoded()
        return report
    }

    func encoded() throws -> Data {
        let name = Array(city.utf8)
        guard weatherCode > 0, minTempC >= -100, maxTempC <= 100, minTempC <= maxTempC,
              !name.isEmpty, name.count <= 128,
              !city.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              !city.unicodeScalars.contains(where: { $0.value < 32 || (127...159).contains($0.value) })
        else { throw Failure.invalidResponse }
        var bytes: [UInt8] = [1]
        for value in [weatherCode, UInt16(bitPattern: minTempC), UInt16(bitPattern: maxTempC)] {
            bytes += [UInt8(truncatingIfNeeded: value), UInt8(value >> 8)]
        }
        bytes.append(UInt8(name.count))
        return Data(bytes + name)
    }
}
