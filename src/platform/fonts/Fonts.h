#pragma once

#include <FreeInkUICore.h>

namespace freeink::ui {

class DisplayTarget;

}

namespace platform::fonts {

enum class Font : freeink::ui::FontId {
    RobotoS = 0,
    RobotoM = 1,
    RobotoL = 2,
    RobotoXL = 3,
    Roboto2XL = 4,
    NDot2XL = 5,
    NDot4XL = 6,
    NDot120 = 7,
};

[[nodiscard]] constexpr freeink::ui::FontId fontId(Font font) {
    return static_cast<freeink::ui::FontId>(font);
}

void registerFonts(freeink::ui::DisplayTarget& target);

}  // namespace platform::fonts
