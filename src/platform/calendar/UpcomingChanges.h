#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace platform::calendar {

struct UpcomingChange {
    enum class Reason { Snapshot, Time, Restored };
    uint64_t revision = 0;
    Reason reason = Reason::Time;
};

class UpcomingChanges;

class UpcomingSubscription {
   public:
    UpcomingSubscription() = default;
    ~UpcomingSubscription();
    UpcomingSubscription(UpcomingSubscription&& other) noexcept;
    UpcomingSubscription& operator=(UpcomingSubscription&& other) noexcept;
    UpcomingSubscription(const UpcomingSubscription&) = delete;
    UpcomingSubscription& operator=(const UpcomingSubscription&) = delete;
    void reset();

   private:
    friend class UpcomingChanges;

    struct Listener {
        std::function<void(const UpcomingChange&)> callback;
        bool active = true;
    };

    explicit UpcomingSubscription(std::shared_ptr<Listener> listener);
    std::shared_ptr<Listener> _listener;
};

class UpcomingChanges {
   public:
    UpcomingSubscription subscribe(std::function<void(const UpcomingChange&)> callback);
    void publish(const UpcomingChange& change);
    void clear();

   private:
    std::vector<std::weak_ptr<UpcomingSubscription::Listener>> _listeners;
};

}  // namespace platform::calendar
