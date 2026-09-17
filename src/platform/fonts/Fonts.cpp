#include "Fonts.h"

#include <FreeInkUIDisplayTarget.h>

#include "generated/NDot120.h"
#include "generated/NDot2XL.h"
#include "generated/NDot4XL.h"
#include "generated/Roboto2XL.h"
#include "generated/RobotoL.h"
#include "generated/RobotoM.h"
#include "generated/RobotoS.h"
#include "generated/RobotoXL.h"

namespace platform::fonts {

void registerFonts(freeink::ui::DisplayTarget& target) {
    static_assert(freeink::ui::DisplayTarget::FONT_SLOTS == 8);
    target.setFont(fontId(Font::RobotoS), generated::kRobotoS);
    target.setFont(fontId(Font::RobotoM), generated::kRobotoM);
    target.setFont(fontId(Font::RobotoL), generated::kRobotoL);
    target.setFont(fontId(Font::RobotoXL), generated::kRobotoXL);
    target.setFont(fontId(Font::Roboto2XL), generated::kRoboto2XL);
    target.setFont(fontId(Font::NDot2XL), generated::kNDot2XL);
    target.setFont(fontId(Font::NDot4XL), generated::kNDot4XL);
    target.setFont(fontId(Font::NDot120), generated::kNDot120);
}

}  // namespace platform::fonts
