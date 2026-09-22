#include "Fonts.h"

#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cstdio>
#include <string>

#include "../storage/FatFilesystem.h"
#include "FontAsset.h"

namespace platform::fonts {
namespace {

std::array<FontAsset, 8> assets;
bool loaded = false;
constexpr std::array<const char*, 8> filenames{"sans-s.bin",   "sans-m.bin",        "sans-l.bin",        "sans-xl.bin",
                                               "sans-2xl.bin", "ndot-2xl.bin", "ndot-4xl.bin", "ndot-120.bin"};

}  // namespace

bool loadFonts(const char* directory) {
    if (loaded) return true;
#ifdef ARDUINO
    if (!storage::mountFatFilesystem()) {
        std::fprintf(stderr, "Cannot mount FATFS for fonts\n");
        return false;
    }
#endif
    std::array<FontAsset, 8> pending;
    for (size_t i = 0; i < pending.size(); ++i) {
        const std::string path = std::string(directory) + "/" + filenames[i];
        if (!pending[i].load(path.c_str())) {
            std::fprintf(stderr, "Cannot load font: %s\n", path.c_str());
            return false;
        }
    }
    assets = std::move(pending);
    loaded = true;
    return true;
}

bool registerFonts(freeink::ui::DisplayTarget& target) {
    static_assert(freeink::ui::DisplayTarget::FONT_SLOTS == 8);
    if (!loaded) return false;
    for (size_t i = 0; i < assets.size(); ++i) target.setFont(static_cast<freeink::ui::FontId>(i), assets[i].font());
    return true;
}

}  // namespace platform::fonts
