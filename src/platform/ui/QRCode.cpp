#include "QRCode.h"

#include <algorithm>

#include "../../../third_party/qrcodegen/src/qrcodegen.h"

namespace platform::ui {

bool QRCode::encode(std::string_view text) {
    _data.fill(0);
    if (text.empty() || text.size() >= 408 || text.find('\0') != std::string_view::npos) return false;
    std::array<char, 408> input{};
    std::copy(text.begin(), text.end(), input.begin());
    std::array<uint8_t, 408> scratch{};
    static_assert(qrcodegen_BUFFER_LEN_FOR_VERSION(10) == 408);
    return qrcodegen_encodeText(input.data(), scratch.data(), _data.data(), qrcodegen_Ecc_MEDIUM, 1, 10,
                                qrcodegen_Mask_AUTO, true);
}

int QRCode::size() const {
    return _data[0] == 0 ? 0 : qrcodegen_getSize(_data.data());
}

bool QRCode::module(int x, int y) const {
    return size() != 0 && qrcodegen_getModule(_data.data(), x, y);
}

}  // namespace platform::ui
