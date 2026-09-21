#pragma once

#include <memory>
#include <vector>

#include "../../ble/services/BLEService.h"
#include "../../hal/services/FrontlightService.h"
#include "../../hal/services/PowerService.h"
#include "../../tasking/services/TaskDispatchService.h"
#include "../../time/services/TimeService.h"
#include "Service.h"

namespace platform::runtime {

class ServiceManager {
   public:
    ServiceManager();
    ~ServiceManager();
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
    ServiceManager(ServiceManager&&) = delete;
    ServiceManager& operator=(ServiceManager&&) = delete;

    [[nodiscard]] ble::BLEService& ble();
    [[nodiscard]] const ble::BLEService& ble() const;
    [[nodiscard]] frontlight::FrontlightService& frontlight();
    [[nodiscard]] const frontlight::FrontlightService& frontlight() const;
    [[nodiscard]] power::PowerService& power();
    [[nodiscard]] const power::PowerService& power() const;
    [[nodiscard]] time::TimeService& time();
    [[nodiscard]] const time::TimeService& time() const;

    // Registration order is dependency order. Ownership transfers even on rejection.
    [[nodiscard]] bool add(std::unique_ptr<Service> service);
    [[nodiscard]] bool begin(std::uint32_t now = 0);
    void update(std::uint32_t now);
    void stop();
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] tasking::TaskDispatchService& tasks();
    [[nodiscard]] const tasking::TaskDispatchService& tasks() const;

   private:
    std::vector<std::unique_ptr<Service>> _services;
    frontlight::FrontlightService* _frontlight;
    power::PowerService* _power;
    tasking::TaskDispatchService* _tasks;
    time::TimeService* _time;
    ble::BLEService* _ble;
    std::size_t _started = 0;
    bool _sealed = false;
    bool _transitioning = false;
};

}  // namespace platform::runtime
