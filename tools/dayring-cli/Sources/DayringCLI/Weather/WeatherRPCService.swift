import DayringBLE
import Foundation

/// A device request triggers a fresh lookup. No polling, prefetch or weather cache on the CLI.
final class WeatherRPCService {
    static let getMethod: UInt16 = 12
    typealias Fetch = (@escaping (Result<WeatherReport, Error>) -> Void) -> Void
    typealias Completion = (Result<Data, RPCPeer.UInt8Error>) -> Void
    var onStatus: ((String) -> Void)?
    private let _fetch: Fetch
    private let _cancel: () -> Void
    private var _connected = false
    private var _completion: Completion?
    private var _epoch = 0

    init(fetch: @escaping Fetch, cancel: @escaping () -> Void = {}) {
        _fetch = fetch; _cancel = cancel
    }

    func setConnected(_ connected: Bool) {
        _epoch += 1
        _connected = connected
        let completion = _completion
        _completion = nil
        _cancel()
        completion?(.failure(.init(7)))
    }

    func get(_ payload: Data, completion: @escaping Completion) {
        guard payload.isEmpty else { completion(.failure(.init(2))); return }
        guard _connected, _completion == nil else { completion(.failure(.init(3))); return }
        _completion = completion
        _epoch += 1
        let epoch = _epoch
        _fetch { [weak self] result in
            guard let self, self._connected, self._epoch == epoch, let completion = self._completion else { return }
            self._completion = nil
            do {
                let report = try result.get()
                let bytes = try report.encoded()
                self.onStatus?("Weather served: \(report.city), WWO \(report.weatherCode), \(report.minTempC)…\(report.maxTempC) °C")
                completion(.success(bytes))
            } catch {
                self.onStatus?("Weather request failed: \(error)")
                completion(.failure(.init(7)))
            }
        }
    }
}
