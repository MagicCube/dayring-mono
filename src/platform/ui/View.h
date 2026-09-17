#pragma once
#include <FreeInkUICore.h>

namespace platform::ui {

using Canvas = freeink::ui::DrawTarget;
using Rect = freeink::ui::Rect;

// Each View declares its input type without runtime property erasure.
template <typename PropsType>
class View {
   public:
    using Props = PropsType;
    virtual ~View() = default;
    virtual void render(Canvas& canvas, const Rect& bounds, const Props& props) const = 0;
};

}  // namespace platform::ui
