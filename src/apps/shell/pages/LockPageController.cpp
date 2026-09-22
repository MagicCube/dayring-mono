#include "LockPageController.h"

#include <Arduino.h>

#include <algorithm>
#include <cstdio>

#include "../../../platform/hal/Hardware.h"
#include "../../../platform/runtime/Shell.h"

namespace apps::shell::pages {

void LockPageController::onEnter(std::string_view) {
    _calendarSubscription = platform::runtime::Shell::instance().services().calendar().subscribeUpcoming(
        [this](const auto&) { _calendarDirty = true; });
    _calendarDirty = true;
    _minuteRevision = 0;
    _powerSampled = false;
    _powerKnown = false;
    _charging = false;
    _batteryKnown = false;
    _showUnlockHint = false;
    _interactionSeen = false;
    _touchPressed = false;
    _hintChanged = false;
    (void)update();
}

void LockPageController::onLeave() {
    _calendarSubscription.reset();
    _calendarItems.clear();
}

bool LockPageController::update() {
    const auto now = static_cast<uint32_t>(millis());
    if (_interactionSeen && now - _lastLockInteractionAt >= platform::power::PowerService::lockInteractionDurationMs)
        _interactionSeen = false;
    if (_showUnlockHint && now - _hintShownAt >= _hintDurationMs) {
        _hintChanged = true;
        _showUnlockHint = false;
    }
    const bool hintChanged = _hintChanged;
    _hintChanged = false;
    const auto revision = platform::runtime::Shell::instance().services().time().minuteRevision();
    const bool clockChanged = _minuteRevision != revision;
    _minuteRevision = revision;
    const bool calendarChanged = (_calendarDirty || clockChanged) && _refreshCalendar();
    return _samplePower(clockChanged) || clockChanged || hintChanged || calendarChanged;
}

bool LockPageController::onInput(const platform::runtime::InputEvent& event) {
    using Type = platform::runtime::InputEvent::Type;
    if (event.type != Type::PowerPress && event.type != Type::TouchPress && event.type != Type::TouchRelease &&
        event.type != Type::Swipe)
        return false;
    if (event.type == Type::TouchPress) {
        if (_touchPressed) return true;
        _touchPressed = true;
    } else if ((event.type == Type::TouchRelease || event.type == Type::Swipe) && _touchPressed) {
        _touchPressed = false;
        return true;
    }
    const auto now = static_cast<uint32_t>(millis());
    const bool showHint =
        _interactionSeen && now - _lastLockInteractionAt < platform::power::PowerService::lockInteractionDurationMs;
    _hintChanged = _hintChanged || _showUnlockHint != showHint;
    _showUnlockHint = showHint;
    if (showHint) _hintShownAt = now;
    _interactionSeen = true;
    _lastLockInteractionAt = now;
    platform::runtime::Shell::instance().services().power().notifyLockInteraction();
    return true;
}

bool LockPageController::_refreshCalendar() {
    _calendarDirty = false;
    auto& services = platform::runtime::Shell::instance().services();
    const auto clock = platform::calendar::sampleClock(services.time());
    const int64_t today = clock.localSeconds.value_or(0) / 86400;
    std::vector<LockCalendarItem> items;
    auto events = services.calendar().upcoming();
    std::stable_sort(events.begin(), events.end(), [](const auto& left, const auto& right) {
        if (left.event.start.localSeconds() / 86400 != right.event.start.localSeconds() / 86400)
            return left.event.start.localSeconds() < right.event.start.localSeconds();
        if (left.event.isAllDay != right.event.isAllDay) return left.event.isAllDay;
        return left.event.instanceId < right.event.instanceId;
    });
    if (events.size() > 3) events.resize(3);
    for (const auto& upcoming : events) {
        const auto& event = upcoming.event;
        const auto startDay = event.start.localSeconds() / 86400;
        const auto start = event.start.localSeconds() % 86400;
        const auto end = event.end.localSeconds() % 86400;
        char startLabel[8] = "", endLabel[8] = "";
        if (event.isAllDay) {
            snprintf(startLabel, sizeof(startLabel), "All day");
        } else {
            snprintf(startLabel, sizeof(startLabel), "%02u:%02u", static_cast<unsigned>(start / 3600), static_cast<unsigned>((start / 60) % 60));
            snprintf(endLabel, sizeof(endLabel), "%02u:%02u", static_cast<unsigned>(end / 3600), static_cast<unsigned>((end / 60) % 60));
        }
        auto singleLine = [](std::string value) {
            std::replace(value.begin(), value.end(), '\n', ' ');
            std::replace(value.begin(), value.end(), '\r', ' ');
            return value;
        };
        items.push_back({singleLine(event.title.empty() ? "Untitled" : event.title), singleLine(event.location), startLabel,
                         endLabel, startDay == today + 1, event.isAllDay});
    }
    if (items == _calendarItems) return false;
    _calendarItems = std::move(items);
    return true;
}

bool LockPageController::_samplePower(bool refreshPercent) {
    const auto now = static_cast<uint32_t>(millis());
    if (_powerSampled && !refreshPercent && now - _powerSampledAt < 1000U) return false;
    _powerSampled = true;
    _powerSampledAt = now;
    bool charging = false;
    bool changed = false;
    if (platform::hal::readCharging(charging)) {
        changed = charging != _charging;
        if (_powerKnown && changed)
            platform::runtime::Shell::instance().services().power().notifyPowerConnectionChanged();
        _powerKnown = true;
        _charging = charging;
    }
    if (_charging && (refreshPercent || changed || !_batteryKnown)) {
        uint8_t percent = 0;
        if (platform::hal::readBatteryPercent(percent)) {
            changed = changed || !_batteryKnown || percent != _percent;
            _percent = percent;
            _batteryKnown = true;
        }
    }
    if (!_charging) _batteryKnown = false;
    return changed;
}

void LockPageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto& time = platform::runtime::Shell::instance().services().time().displayTime();
    _view.render(canvas, bounds,
                 {.hour = time.hour,
                  .minute = time.minute,
                  .month = time.month,
                  .day = time.day,
                  .weekday = time.weekday,
                  .charging = _charging,
                  .batteryKnown = _batteryKnown,
                  .percent = _percent,
                  .showUnlockHint = _showUnlockHint,
                  .events = _calendarItems});
}

}  // namespace apps::shell::pages
