#include <iostream>

#include "CalendarTestSupport.h"

using namespace calendar_test;

void syncAndUpcoming() {
    Fixture f;
    std::vector<calendar::UpcomingChange> notices;
    auto subscription = f.service.subscribeUpcoming([&](const auto& change) { notices.push_back(change); });
    f.settle();
    assert(f.service.syncState() == calendar::CalendarService::SyncState::Synchronized);
    assert(f.service.persistenceState() == calendar::CalendarService::PersistenceState::Saved);
    assert(f.service.snapshot()->events.size() == 5 && f.service.upcoming().size() == 4);
    assert(f.service.upcoming(0).empty() && f.service.upcoming(2).size() == 2);
    assert(f.service.upcoming()[1].event.instanceId == "ongoing" && f.service.upcoming()[1].isOngoing);
    assert(notices.size() == 1 && notices[0].reason == calendar::UpcomingChange::Reason::Snapshot);
    f.peer.status();
    f.settle();
    const auto& status = f.peer.replies.back();
    assert(status.method == 11 && status.payload.size() == 12 && status.payload[0] == 1);
    assert(status.payload[1] == 2 && status.payload[2] == 1 && status.payload[4] == 5 && status.payload[6] == 4);
    const int writes = f.disk->writes;
    f.clock = clockAt("2026-09-21T13:00:00+08:00");
    f.tick();
    assert(f.service.upcoming().size() == 3 && notices.size() == 2);
    assert(notices.back().reason == calendar::UpcomingChange::Reason::Time);
    f.clock = clockAt("2026-09-21T14:00:00+08:00");
    f.tick();
    assert(f.service.upcoming()[1].isOngoing && notices.size() == 3);
    f.clock = clockAt("2026-09-21T14:00:01+08:00");
    f.tick();
    assert(notices.size() == 3 && f.disk->writes == writes && f.service.snapshot()->events.size() == 5);
    f.peer.changed();
    f.settle();
    assert(notices.size() == 3 && f.disk->writes == writes);
    f.peer.json = snapshot("");
    f.peer.changed();
    f.settle();
    assert(f.service.upcoming().empty() && f.service.snapshot()->events.empty() && notices.size() == 4);
    subscription.reset();
    f.peer.json = snapshot(usualEvents());
    f.peer.changed();
    f.settle();
    assert(notices.size() == 4);
}

void racesAndFailures() {
    Fixture f;
    f.peer.busyCount = 2;
    f.settle(400);
    assert(f.service.hasSnapshot() && f.peer.begins == 3);
    const auto retained = f.service.snapshot();
    f.peer.invalidManifest = true;
    f.peer.changed();
    f.settle();
    assert(f.service.syncState() == calendar::CalendarService::SyncState::Failed);
    assert(f.service.snapshot() == retained);
    f.peer.invalidManifest = false;
    f.peer.shortPage = true;
    f.peer.changed();
    f.settle();
    assert(f.service.snapshot() == retained);
    f.peer.shortPage = false;
    f.peer.onRead = [&] {
        f.peer.json = snapshot("");
        f.peer.changed();
    };
    f.peer.changed();
    f.settle(400);
    assert(f.service.snapshot()->events.empty());
    f.peer.json = snapshot(usualEvents());
    f.peer.changed();
    f.settle();
    f.peer.changed(false);
    f.settle();
    assert(f.peer.replies.back().kind == rpc::Kind::Error && f.peer.replies.back().payload[0] == 2);
    f.peer.generation = 0;
    f.tick();
    assert(f.service.syncState() == calendar::CalendarService::SyncState::Waiting);
    f.peer.json = snapshot("");
    f.peer.connect(2);
    f.settle();
    assert(f.service.snapshot()->events.empty());
    f.service.stop();
    f.peer.changed();
    f.settle();
    assert(f.peer.replies.back().payload[0] == 1);
    assert(f.service.start());
    f.settle();
}

void persistenceAndRestore() {
    Fixture f;
    f.disk->writable = false;
    f.settle();
    assert(f.service.hasSnapshot() && f.service.upcoming().size() == 4);
    assert(f.service.persistenceState() == calendar::CalendarService::PersistenceState::Failed);
    f.disk->writable = true;
    f.tick(30000);
    assert(f.service.persistenceState() == calendar::CalendarService::PersistenceState::Saved);
    f.service.stop();
    auto clock = f.clock;
    clock.timeZone.clear();
    clock.utcOffsetSeconds.reset();
    calendar::CalendarService restored(f.rpc, [&] { return clock; }, storage(f.disk));
    assert(restored.start() && restored.upcoming().size() == 4 && restored.hasCurrentCoverage());
    f.peer.generation = 0;
    f.tasks.update(f.now + 10);
    clock = clockAt("2026-09-22T00:00:00+08:00");
    restored.update(f.now + 10);
    assert(restored.upcoming().size() == 1 && !restored.hasCurrentCoverage());
    clock = clockAt("2026-09-23T00:00:00+08:00");
    restored.update(f.now + 20);
    assert(restored.upcoming().empty() && !restored.hasUsableTime());
}

void paginationAndContext() {
    Fixture f;
    std::string events;
    for (int i = 0; i < 90; ++i) {
        if (i) events += ',';
        events += event(std::to_string(i), "2026-09-22T10:00:00+08:00", "2026-09-22T11:00:00+08:00");
    }
    f.peer.json = snapshot(events);
    f.settle(500);
    assert(f.service.upcoming().size() == 90 && f.peer.reads > 1);
    f.clock = clockAt("2026-09-22T00:00:00+08:00");
    f.peer.json = snapshot("", "2026-09-22", "2026-09-24");
    f.settle();
    assert(f.service.hasCurrentCoverage() && f.service.snapshot()->events.empty());
    f.peer.json = snapshot(usualEvents());
    f.peer.changed();
    f.settle();
    assert(f.service.failure() == calendar::CalendarService::Failure::TimeContext);
}

void subscriptions() {
    calendar::UpcomingChanges changes;
    calendar::UpcomingSubscription second;
    int count = 0;
    auto first = changes.subscribe([&](auto) {
        ++count;
        second.reset();
    });
    second = changes.subscribe([&](auto) { count += 100; });
    changes.publish({1, calendar::UpcomingChange::Reason::Time});
    assert(count == 1);
    first = {};
    changes.publish({2, calendar::UpcomingChange::Reason::Time});
    assert(count == 1);
}

int main() {
    syncAndUpcoming();
    racesAndFailures();
    persistenceAndRestore();
    paginationAndContext();
    subscriptions();
    std::cout << "Calendar pull, upcoming/time events, persistence retry, restore, paging and session tests passed\n";
}
