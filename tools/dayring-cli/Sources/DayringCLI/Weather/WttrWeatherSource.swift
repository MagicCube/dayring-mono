import Foundation

final class WttrWeatherSource {
    private let _session: URLSession
    private var _task: URLSessionDataTask?

    init() {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.urlCache = nil
        configuration.requestCachePolicy = .reloadIgnoringLocalCacheData
        configuration.timeoutIntervalForRequest = 30
        configuration.timeoutIntervalForResource = 45
        _session = URLSession(configuration: configuration)
    }

    deinit { _session.invalidateAndCancel() }

    static let url = URL(string: "https://wttr.in/?format=j1")!

    func cancel() { _task?.cancel(); _task = nil }

    func fetch(completion: @escaping (Result<WeatherReport, Error>) -> Void) {
        cancel()
        _task = _session.dataTask(with: Self.url) { data, response, error in
            let result = Result<WeatherReport, Error> {
                if let error { throw error }
                guard let response = response as? HTTPURLResponse, response.statusCode == 200,
                      let data, data.count <= 1024 * 1024 else { throw WeatherReport.Failure.invalidResponse }
                return try WeatherReport.decode(data)
            }
            DispatchQueue.main.async { completion(result) }
        }
        _task?.resume()
    }
}
