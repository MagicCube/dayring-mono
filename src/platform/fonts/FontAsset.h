#pragma once

#include <FreeInkUIFont.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace platform::fonts {

struct FontMemoryDeleter {
    void operator()(void* memory) const {
        std::free(memory);
    }
};

template <typename T>
using FontMemory = std::unique_ptr<T, FontMemoryDeleter>;

// Owns the storage borrowed by BitmapFont and every target using it.
class FontAsset {
   public:
    [[nodiscard]] bool load(const char* path);

    [[nodiscard]] const freeink::ui::BitmapFont& font() const {
        return _font;
    }

   private:
    bool _readTables(std::FILE* file, uint8_t version);

    FontMemory<uint8_t> _bitmap;
    FontMemory<freeink::ui::FontGlyph> _glyphs;
    FontMemory<uint16_t> _codepoints;
    freeink::ui::BitmapFont _font{};
};

}  // namespace platform::fonts
