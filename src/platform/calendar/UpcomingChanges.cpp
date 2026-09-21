#include "UpcomingChanges.h"

#include <algorithm>
#include <utility>

namespace platform::calendar {

UpcomingSubscription::UpcomingSubscription(std::shared_ptr<Listener> listener) : _listener(std::move(listener)) {
}

UpcomingSubscription::~UpcomingSubscription() {
    reset();
}

UpcomingSubscription::UpcomingSubscription(UpcomingSubscription&& other) noexcept
    : _listener(std::move(other._listener)) {
}

UpcomingSubscription& UpcomingSubscription::operator=(UpcomingSubscription&& other) noexcept {
    if (this != &other) {
        reset();
        _listener = std::move(other._listener);
    }
    return *this;
}

void UpcomingSubscription::reset() {
    if (_listener) _listener->active = false;
    _listener.reset();
}

UpcomingSubscription UpcomingChanges::subscribe(std::function<void(const UpcomingChange&)> callback) {
    if (!callback) return {};
    std::erase_if(_listeners, [](const auto& listener) { return listener.expired(); });
    auto listener = std::make_shared<UpcomingSubscription::Listener>();
    listener->callback = std::move(callback);
    _listeners.push_back(listener);
    return UpcomingSubscription(std::move(listener));
}

void UpcomingChanges::publish(const UpcomingChange& change) {
    // Snapshot listeners so callbacks may subscribe/unsubscribe without invalidating iteration.
    const auto listeners = _listeners;
    for (const auto& weak : listeners) {
        if (const auto listener = weak.lock(); listener && listener->active) listener->callback(change);
    }
}

void UpcomingChanges::clear() {
    for (const auto& weak : _listeners)
        if (const auto listener = weak.lock()) listener->active = false;
    _listeners.clear();
}

}  // namespace platform::calendar
