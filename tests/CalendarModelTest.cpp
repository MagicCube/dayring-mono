#include <iostream>

#include "CalendarTestSupport.h"

using namespace calendar_test;

void parsing() {
    const auto json = snapshot(usualEvents());
    const auto parsed = calendar::parseSnapshot(json);
    assert(parsed && parsed->events.size() == 5);
    assert(!calendar::parseSnapshot(json + "trailing"));
    assert(!calendar::parseSnapshot(snapshot(event("x", "2026-09-21T14:00:00+08:00", "2026-09-21T13:00:00+08:00"))));
    const auto one = event("x", "2026-09-21T14:00:00+08:00", "2026-09-21T15:00:00+08:00");
    assert(!calendar::parseSnapshot(snapshot(one + "," + one)));
    assert(!calendar::parseSnapshot(snapshot(one, "2026-09-21", "2026-09-24")));
    assert(!calendar::parseSnapshot(snapshot(one, "2026-09-22", "2026-09-24")));
    assert(
        !calendar::parseSnapshot(snapshot(event("x", "2026-09-21T14:00:00+08:00", "2026-09-21T15:00:00+08:00", true))));
    const auto unicode =
        snapshot(event("x", "2026-09-21T14:00:00+08:00", "2026-09-21T15:00:00+08:00", false, "会议 \\uD83D\\uDE00"));
    assert(calendar::parseSnapshot(unicode));
    for (const auto& title :
         {std::string("a\\u0000b"), std::string("bad\nraw"), std::string("\xC0\x80"), std::string("\\uD800")}) {
        assert(!calendar::parseSnapshot(
            snapshot(event("x", "2026-09-21T14:00:00+08:00", "2026-09-21T15:00:00+08:00", false, title))));
    }
    assert(calendar::parseSnapshot(
        snapshot(event("x", "2026-09-21T14:00:00+08:00", "2026-09-21T15:00:00+08:00", false, "literal \\\\u0000"))));
    assert(!calendar::parseSnapshot("{\"schemaVersion\":1," + json.substr(1)));
    assert(!calendar::parseDateTime("2026-02-29T00:00:00Z"));
    assert(!calendar::parseDateTime("2026-09-21T00:00:00+14:01"));
    assert(calendar::parseDateTime("2024-02-29T00:00:00Z"));
    const auto before = calendar::parseDateTime("2026-11-01T01:30:00-04:00");
    const auto after = calendar::parseDateTime("2026-11-01T01:30:00-05:00");
    assert(before && after && after->utcSeconds - before->utcSeconds == 3600);
    assert(!calendar::parseManifest("{\"snapshotId\":\"a\",\"byteLength\":65537,\"chunkBytes\":10240}"));
    assert(!calendar::parseManifest("{\"snapshotId\":\"a\",\"byteLength\":1.5,\"chunkBytes\":10240}"));
}

void fileRecovery() {
    auto disk = std::make_shared<Disk>();
    auto first = storage(disk);
    const auto old = snapshot(usualEvents());
    const auto empty = snapshot("");
    assert(!first->load() && first->save(old));
    disk->interrupt = true;
    assert(!first->save(empty));
    assert(storage(disk)->load() == old);
    disk->interrupt = false;
    assert(first->save(empty));
    assert(storage(disk)->load() == empty);
    assert(disk->slots[1]);
    (*disk->slots[1])[8] ^= 1;  // Corrupt generation metadata: checksum must also protect it.
    assert(storage(disk)->load() == old);
    disk->writable = false;
    assert(!first->save(empty));
    assert(storage(disk)->load() == old);
    disk->writable = true;
    assert(first->save(empty));
    assert(storage(disk)->load() == empty);
    assert(!first->save("{}"));
}

int main() {
    parsing();
    fileRecovery();
    std::cout << "Calendar schema, UTF-8, dates, bounds and interrupted/corrupt FAT slot recovery passed\n";
}
