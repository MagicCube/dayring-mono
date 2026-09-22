#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "WeatherReport.h"

namespace platform::weather {

[[nodiscard]] std::optional<uint32_t> weatherDate(unsigned year, unsigned month, unsigned day);

struct CachedWeather {
    uint32_t date;
    WeatherReport report;
    bool operator==(const CachedWeather&) const = default;
};

class WeatherStorage {
   public:
    virtual ~WeatherStorage() = default;
    virtual std::optional<CachedWeather> load() = 0;
    virtual bool save(const CachedWeather& value) = 0;
};

class WeatherFiles {
   public:
    virtual ~WeatherFiles() = default;
    virtual std::optional<std::string> read(unsigned slot) = 0;
    virtual bool write(unsigned slot, std::string_view bytes) = 0;
};

class FileWeatherStorage final : public WeatherStorage {
   public:
    explicit FileWeatherStorage(std::unique_ptr<WeatherFiles> files);
    std::optional<CachedWeather> load() override;
    bool save(const CachedWeather& value) override;

   private:
    std::unique_ptr<WeatherFiles> _files;
    uint64_t _generation = 0;
    unsigned _slot = 1;
};

[[nodiscard]] std::unique_ptr<WeatherStorage> makeWeatherStorage();

}  // namespace platform::weather
