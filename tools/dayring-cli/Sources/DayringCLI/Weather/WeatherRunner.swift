import DayringBLE
import Foundation

final class WeatherRunner {
    private let _source = WttrWeatherSource()
    private let _central: BLECentral
    private var _connected = false
    private lazy var _service: WeatherRPCService = WeatherRPCService(fetch: { [weak self] completion in
        self?._service.onStatus?("Weather HTTP: requesting wttr.in using public-IP city (30s request / 45s resource timeout)")
        self?._source.fetch { result in
            if case .failure(let error) = result {
                let detail = error as NSError
                self?._service.onStatus?("Weather HTTP/response failed: \(detail.domain) code=\(detail.code): \(detail.localizedDescription)")
            }
            completion(result)
        }
    }, cancel: { [weak self] in self?._source.cancel() })

    init(central: BLECentral) {
        _central = central
        _service.onStatus = { fputs("\($0)\n", stderr) }
        central.registerAsyncRPCHandler(method: WeatherRPCService.getMethod) { [weak self] payload, completion in
            guard let self else { completion(.failure(.init(7))); return }
            self._service.get(payload, completion: completion)
        }
    }

    func ready() {
        _connected = true
        _service.setConnected(true)
    }

    func update() {
        if _connected && !_central.isRPCReady {
            _connected = false
            _service.setConnected(false)
        }
    }
}
