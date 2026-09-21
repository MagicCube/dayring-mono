#include <iostream>
#include <iterator>

#include "CalendarTestSupport.h"

int main() {
    using namespace calendar_test;
    const std::string json{std::istreambuf_iterator<char>(std::cin), {}};
    const auto parsed = calendar::parseSnapshot(json);
    assert(parsed && parsed->events.size() == 91);
    Fixture fixture;
    fixture.peer.json = json;
    fixture.settle(1000);
    assert(fixture.service.snapshot() && fixture.service.snapshot()->events.size() == 91);
    assert(fixture.service.upcoming().size() == 90);
    assert(fixture.service.upcoming(1)[0].event.title == "会议 \"0\" 😀");
    assert(fixture.service.upcoming(1)[0].event.location == "Room\n二楼");
    fixture.service.stop();
    calendar::CalendarService restored(fixture.rpc, [&] { return fixture.clock; }, storage(fixture.disk));
    assert(restored.start() && restored.upcoming().size() == 90);
    std::cout << "Swift snapshot -> ESP32 RPC pull, UTF-8 JSON, full cache/upcoming separation and restore passed\n";
}
