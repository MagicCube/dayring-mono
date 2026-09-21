#pragma once

#include <algorithm>

#include "QRCode.h"
#include "View.h"

namespace platform::ui {

struct QRCodeProps {
    const QRCode* code = nullptr;
    int16_t cornerRadius = 0;
};

// Always black on white, with an integer module scale and four-module quiet zone.
class QRCodeView final : public View<QRCodeProps> {
   public:
    void render(Canvas& canvas, const Rect& bounds, const Props& props) const override {
        using namespace freeink::ui;
        if (!props.code || props.code->size() == 0) return;
        const auto& code = *props.code;
        const int scale = std::min(bounds.width, bounds.height) / (code.size() + 8);
        if (scale < 1) return;
        const int side = (code.size() + 8) * scale;
        const int x = bounds.x + (bounds.width - side) / 2;
        const int y = bounds.y + (bounds.height - side) / 2;
        canvas.fill(
            {static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(side), static_cast<int16_t>(side)},
            Paint::solid(Color::White), std::clamp<int16_t>(props.cornerRadius, 0, 2 * scale));
        for (int row = 0; row < code.size(); ++row)
            for (int col = 0; col < code.size(); ++col)
                if (code.module(col, row))
                    canvas.fill(
                        {static_cast<int16_t>(x + (col + 4) * scale), static_cast<int16_t>(y + (row + 4) * scale),
                         static_cast<int16_t>(scale), static_cast<int16_t>(scale)},
                        Paint::solid(Color::Black));
    }
};

}  // namespace platform::ui
