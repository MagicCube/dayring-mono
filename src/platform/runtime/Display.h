#pragma once

#include <FreeInkUICore.h>

namespace platform::ui {

class ApplicationContainer;

}

namespace platform::runtime {

[[nodiscard]] freeink::ui::DeviceContext displayDevice();
void renderFrame(ui::ApplicationContainer& container);

}  // namespace platform::runtime
