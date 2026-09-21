#include "PairingPageController.h"

#include "../../../platform/runtime/Shell.h"

namespace apps::shell::pages {

PairingPageController::PairingPageController() : PageController(true) {
    (void)_code.encode("https://dayring.ai");
}

void PairingPageController::onEnter(std::string_view) {
    (void)update();
}

bool PairingPageController::update() {
    const auto state = platform::runtime::Shell::instance().services().ble().state();
    const bool pairing =
        state == platform::ble::BLEService::State::Connected || state == platform::ble::BLEService::State::Secured;
    if (_pairing == pairing) return false;
    _pairing = pairing;
    return true;
}

void PairingPageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    _view.render(canvas, bounds, {.code = &_code, .pairing = _pairing});
}

}  // namespace apps::shell::pages
