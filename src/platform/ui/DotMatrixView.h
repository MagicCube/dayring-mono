#pragma once

#include "DotMatrix.h"
#include "View.h"

namespace platform::ui {

struct DotMatrixProps {
    DotMatrix matrix{};
    freeink::ui::Color color = freeink::ui::Color::Black;
    uint8_t dotDiameter = 12;
};

// Matrix dimensions and dot diameter independently control the centered footprint.
// Unset cells are transparent; the parent owns background painting.
class DotMatrixView final : public View<DotMatrixProps> {
   public:
    void render(Canvas& canvas, const Rect& bounds, const Props& props) const override {
        const int diameter = props.dotDiameter;
        const int extent = props.matrix.size * diameter;
        if (!props.matrix.isValid() || diameter < 2 || bounds.width < extent || bounds.height < extent) return;
        const int x = bounds.x + (bounds.width - extent) / 2;
        const int y = bounds.y + (bounds.height - extent) / 2;
        for (size_t row = 0; row < props.matrix.size; ++row) {
            for (size_t col = 0; col < props.matrix.size; ++col) {
                if (!props.matrix.isSet(col, row)) continue;
                _drawDot(canvas, x + col * diameter, y + row * diameter, diameter, props.color);
            }
        }
    }

   private:
    static void _drawDot(Canvas& canvas, int x, int y, int diameter, freeink::ui::Color color) {
        // Test pixel centers against the circle, avoiding rounded-rectangle corner conventions.
        for (int row = 0; row < diameter; ++row) {
            const int dy = 2 * row + 1 - diameter;
            int inset = 0;
            while (inset < diameter / 2) {
                const int dx = 2 * inset + 1 - diameter;
                if (dx * dx + dy * dy <= diameter * diameter) break;
                ++inset;
            }
            canvas.fill({static_cast<int16_t>(x + inset), static_cast<int16_t>(y + row),
                         static_cast<int16_t>(diameter - 2 * inset), 1},
                        freeink::ui::Paint::solid(color));
        }
    }
};

}  // namespace platform::ui
