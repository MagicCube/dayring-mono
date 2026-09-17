#include "Display.h"

#include <FreeInkUIDisplayTarget.h>

#include "../fonts/Fonts.h"
#include "../hal/Hardware.h"
#include "../ui/ApplicationContainer.h"

namespace platform::runtime {
namespace {

freeink::ui::DisplayTarget displayTarget() {
    const auto framebuffer = hal::framebuffer();
    freeink::ui::DisplayTarget target{
        framebuffer.pixels.data(), static_cast<int16_t>(framebuffer.width), static_cast<int16_t>(framebuffer.height),
        static_cast<int16_t>(framebuffer.strideBytes), freeink::ui::Orientation::Portrait};
    target.setGrayPreview(framebuffer.grayPreview.data());
    return target;
}

}  // namespace

freeink::ui::DeviceContext displayDevice() {
    return displayTarget().deviceContext();
}

void renderFrame(ui::ApplicationContainer& container) {
    if (!hal::displayReady() || !container.needsRender()) return;
    auto target = displayTarget();
    fonts::registerFonts(target);
    container.render(target, {0, 0, target.logicalWidth(), target.logicalHeight()});
    hal::refreshDisplay();
}

}  // namespace platform::runtime
