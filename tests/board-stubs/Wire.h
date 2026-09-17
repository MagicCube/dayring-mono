#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Arduino.h"

class TestWire {
   public:
    unsigned long readyAt = 0;
    unsigned long expanderReadyAt = 0;
    bool unavailable = false;
    int failedRegister = -1;
    unsigned int failuresRemaining = 0;
    unsigned int transactions = 0;
    std::array<std::array<uint8_t, 256>, 128> registers{};

    void begin(int, int, uint32_t) {
    }

    void setTimeOut(unsigned int) {
    }

    void beginTransmission(uint8_t address) {
        _address = address;
        _bytes.clear();
    }

    void write(uint8_t value) {
        _bytes.push_back(value);
    }

    uint8_t endTransmission(bool stop = true) {
        ++transactions;
        if (!_isAvailable()) return 2;
        _register = _bytes.front();
        if (!stop) return 0;
        if (_address == 0x4F && _register == failedRegister && failuresRemaining > 0) {
            --failuresRemaining;
            return 2;
        }
        for (unsigned int i = 1; i < _bytes.size(); ++i) registers[_address][_register + i - 1] = _bytes[i];
        return 0;
    }

    uint8_t requestFrom(uint8_t address, uint8_t count) {
        ++transactions;
        _address = address;
        return _isAvailable() ? count : 0;
    }

    uint8_t read() {
        return registers[_address][_register++];
    }

   private:
    bool _isAvailable() const {
        return !unavailable && millis() >= readyAt &&
               (_address == 0x6E || (_address == 0x4F && millis() >= expanderReadyAt));
    }

    uint8_t _address = 0;
    uint8_t _register = 0;
    std::vector<uint8_t> _bytes;
};

inline TestWire Wire;
