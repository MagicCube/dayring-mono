#include <cassert>
#include <iostream>
#include <string>

#include "platform/ui/QRCode.h"

int main() {
    platform::ui::QRCode code;
    assert(code.size() == 0 && !code.module(0, 0));
    assert(code.encode("https://dayring.ai"));
    assert(code.size() == 25);
    assert(code.module(0, 0) && !code.module(1, 1) && code.module(3, 3));
    assert(!code.module(-1, 0) && !code.module(code.size(), 0));
    auto copy = code;
    assert(!code.encode(std::string(500, 'x')) && code.size() == 0);
    assert(copy.size() == 25 && copy.module(3, 3));
    assert(!code.encode(std::string_view("a\0b", 3)) && code.size() == 0);
    assert(!code.encode(""));
    assert(code.encode("A reusable QR component"));
    assert(code.encode(std::string(200, 'x')) && code.size() > 25);
    std::cout << "QR encoding, ownership and invalid-input tests passed\n";
}
