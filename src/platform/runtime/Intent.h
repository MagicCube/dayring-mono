#pragma once

#include "AppURL.h"

namespace platform::runtime {
struct Intent {
    enum class Reason { Open, Restore, Present };
    AppURL url;
    // Restore keeps history; Present is a temporary foreground page outside history.
    Reason reason = Reason::Open;
};
}  // namespace platform::runtime
