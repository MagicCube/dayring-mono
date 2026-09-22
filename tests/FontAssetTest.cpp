#include <FreeInkUIDisplayTarget.h>
#include <unistd.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "platform/fonts/FontAsset.h"
#include "platform/fonts/Fonts.h"

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        char pattern[] = "/tmp/dayring-font-assets-XXXXXX";
        const char* created = mkdtemp(pattern);
        assert(created);
        _path = created;
    }

    ~TemporaryDirectory() {
        std::filesystem::remove_all(_path);
    }

    const std::filesystem::path& path() const {
        return _path;
    }

   private:
    std::filesystem::path _path;
};

std::vector<uint8_t> read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    assert(file);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    assert(file);
}

void checkMalformedFiles(const std::filesystem::path& path) {
    const auto bytes = read("data/fonts/sans-s.bin");
    platform::fonts::FontAsset asset;
    assert(asset.load("data/fonts/sans-s.bin"));
    const auto* original = asset.font().bitmap;
    const auto reject = [&](const std::vector<uint8_t>& invalid) {
        write(path, invalid);
        assert(!asset.load(path.c_str()));
        assert(asset.font().bitmap == original);
    };
    for (const size_t size : {size_t{0}, size_t{25}, size_t{26}, bytes.size() - 1})
        reject({bytes.begin(), bytes.begin() + size});
    for (const size_t offset : {size_t{0}, size_t{8}, size_t{9}, size_t{14}, size_t{24}, size_t{26}}) {
        auto invalid = bytes;
        invalid[offset] = 0;
        reject(invalid);
    }
    auto invalid = bytes;
    invalid[19] = 0x7F;  // Reject oversized allocation requests before allocating.
    reject(invalid);
    invalid = bytes;
    invalid[29] = invalid[27];  // Duplicate sparse codepoint.
    invalid[30] = invalid[28];
    reject(invalid);
    invalid = bytes;
    const size_t table = 27 + (size_t{bytes[24]} + size_t{bytes[25]} * 256) * 2;
    invalid[table + 3] = 0x7F;  // Out-of-range glyph bitmap offset.
    reject(invalid);
    invalid = bytes;
    invalid.back() ^= 1;  // Corrupt zlib checksum.
    reject(invalid);
    invalid = bytes;
    invalid.push_back(0);  // Trailing bytes are not part of the asset format.
    reject(invalid);
}

void checkStartupTransaction(const std::filesystem::path& directory) {
    constexpr const char* names[]{"sans-s.bin",   "sans-m.bin",   "sans-l.bin",   "sans-xl.bin",
                                  "sans-2xl.bin", "ndot-2xl.bin", "ndot-4xl.bin", "ndot-120.bin"};
    assert(!platform::fonts::loadFonts(directory.c_str()));
    for (const auto* name : names) {
        if (std::string_view(name) != "ndot-120.bin")
            std::filesystem::copy_file(std::filesystem::path("data/fonts") / name, directory / name);
    }
    assert(!platform::fonts::loadFonts(directory.c_str()));
    uint8_t pixel{};
    freeink::ui::DisplayTarget target(&pixel, 1, 1, 1);
    assert(!platform::fonts::registerFonts(target));
    std::filesystem::copy_file("data/fonts/ndot-120.bin", directory / "ndot-120.bin");
    assert(platform::fonts::loadFonts(directory.c_str()));
    for (const auto* name : names) std::filesystem::remove(directory / name);
    assert(platform::fonts::loadFonts(directory.c_str()));
    assert(platform::fonts::registerFonts(target));
    assert(target.measureText(platform::fonts::fontId(platform::fonts::Font::NDot120), "12:34", {}).width == 312);
}

}  // namespace

int main() {
    TemporaryDirectory directory;
    checkMalformedFiles(directory.path() / "malformed.bin");
    checkStartupTransaction(directory.path());
    std::puts("Font asset validation, transactional startup and memory lifetime checks passed");
}
