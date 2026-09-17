#pragma once

namespace platform::runtime {

// Callers check the flag before acquiring the guard.
class TransitionGuard {
   public:
    explicit TransitionGuard(bool& flag) : _flag(flag) {
        _flag = true;
    }

    ~TransitionGuard() {
        _flag = false;
    }

    TransitionGuard(const TransitionGuard&) = delete;
    TransitionGuard& operator=(const TransitionGuard&) = delete;

   private:
    bool& _flag;
};

}  // namespace platform::runtime
