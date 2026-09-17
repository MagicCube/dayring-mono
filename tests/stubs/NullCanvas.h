#pragma once
#include "platform/ui/View.h"

class NullCanvas final : public platform::ui::Canvas {
   public:
    freeink::ui::Size measureText(freeink::ui::FontId, const char*, freeink::ui::TextStyle) const override {
        return {};
    }

    int16_t lineHeight(freeink::ui::FontId) const override {
        return 0;
    }

    void fill(freeink::ui::Rect, freeink::ui::Paint, uint8_t, uint8_t) override {
    }

    void stroke(freeink::ui::Rect, freeink::ui::Paint, uint8_t, uint8_t, uint8_t) override {
    }

    void line(freeink::ui::Point, freeink::ui::Point, uint8_t, freeink::ui::Paint) override {
    }

    void triangle(freeink::ui::Point, freeink::ui::Point, freeink::ui::Point, freeink::ui::Paint) override {
    }

    void text(freeink::ui::Rect, const char*, freeink::ui::TextStyle) override {
    }

    void bitmap(freeink::ui::Rect, freeink::ui::BitmapRef, freeink::ui::BitmapMode, freeink::ui::Paint,
                freeink::ui::Rotation) override {
    }
};
