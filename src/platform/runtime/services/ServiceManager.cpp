#include "ServiceManager.h"

#include <cassert>
#include <utility>

namespace platform::runtime {

ServiceManager::ServiceManager() {
    // Register dependencies before their consumers; startup follows this order.
    auto frontlight = std::make_unique<frontlight::FrontlightService>();
    _frontlight = frontlight.get();
    _services.push_back(std::move(frontlight));
    auto power = std::make_unique<power::PowerService>(*_frontlight);
    _power = power.get();
    _services.push_back(std::move(power));
    auto tasks = std::make_unique<tasking::TaskDispatchService>();
    _tasks = tasks.get();
    _services.push_back(std::move(tasks));
    auto time = std::make_unique<time::TimeService>();
    _time = time.get();
    _services.push_back(std::move(time));
}

ServiceManager::~ServiceManager() {
    stop();
    while (!_services.empty()) _services.pop_back();
}

bool ServiceManager::add(std::unique_ptr<Service> service) {
    if (_sealed || !service) return false;
    _services.push_back(std::move(service));
    return true;
}

bool ServiceManager::begin(std::uint32_t now) {
    if (_transitioning) return false;
    if (isRunning()) return true;
    _tasks->update(now, 0);
    _sealed = true;
    _transitioning = true;
    while (_started < _services.size()) {
        if (!_services[_started]->start()) {
            while (_started) _services[--_started]->stop();
            _transitioning = false;
            return false;
        }
        ++_started;
    }
    _transitioning = false;
    return true;
}

void ServiceManager::update(std::uint32_t now) {
    if (!isRunning()) return;
    _transitioning = true;
    for (auto& service : _services) service->update(now);
    _transitioning = false;
}

void ServiceManager::stop() {
    assert(!_transitioning);
    if (_transitioning) return;
    _transitioning = true;
    while (_started) _services[--_started]->stop();
    _transitioning = false;
}

bool ServiceManager::isRunning() const {
    return !_transitioning && _started == _services.size();
}

frontlight::FrontlightService& ServiceManager::frontlight() {
    return *_frontlight;
}

const frontlight::FrontlightService& ServiceManager::frontlight() const {
    return *_frontlight;
}

power::PowerService& ServiceManager::power() {
    return *_power;
}

const power::PowerService& ServiceManager::power() const {
    return *_power;
}

time::TimeService& ServiceManager::time() {
    return *_time;
}

const time::TimeService& ServiceManager::time() const {
    return *_time;
}

tasking::TaskDispatchService& ServiceManager::tasks() {
    return *_tasks;
}

const tasking::TaskDispatchService& ServiceManager::tasks() const {
    return *_tasks;
}

}  // namespace platform::runtime
