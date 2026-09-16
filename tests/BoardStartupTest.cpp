#include <PaperMonoBoard.h>

#include <cassert>
#include <iostream>
#include <string_view>

namespace {
unsigned long nowUs = 0;
}

unsigned long millis() {
    return nowUs / 1000;
}
void delay(unsigned long ms) {
    nowUs += ms * 1000;
}
void delayMicroseconds(unsigned int us) {
    nowUs += us;
}

void checkSuccessfulBoot() {
    assert(freeink::papermono::ensureBooted());
    assert(freeink::m5ioe1::g_addr == 0x4F);
    assert((Wire.registers[0x6E][freeink::m5pm1::REG_BTN_CFG_1] & 0x81) == 0x01);
    freeink::papermono::setEpdPower(true);
    const auto transactions = Wire.transactions;
    assert(freeink::papermono::ensureBooted());
    assert(Wire.transactions == transactions);
    assert((Wire.registers[0x4F][freeink::m5ioe1::REG_GPIO_OUT_L] & freeink::m5ioe1::PIN_EPD_POWER) != 0);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string_view scenario = argv[1];
    if (scenario == "ready") {
        checkSuccessfulBoot();
        assert(millis() < 100);
    } else if (scenario == "delayed" || scenario == "expander-delayed") {
        if (scenario == "delayed")
            Wire.readyAt = 120;
        else
            Wire.expanderReadyAt = 120;
        checkSuccessfulBoot();
        assert(millis() >= 120 && millis() < 250);
    } else if (scenario == "probe-retry") {
        Wire.unavailable = true;
        assert(!freeink::m5ioe1::begin());
        Wire.unavailable = false;
        checkSuccessfulBoot();
    } else if (scenario == "config-retry" || scenario == "output-retry") {
        Wire.failedRegister =
            scenario == "config-retry" ? freeink::m5ioe1::REG_I2C_CFG : freeink::m5ioe1::REG_PWM1_DUTY_L;
        Wire.failuresRemaining = 1;
        checkSuccessfulBoot();
        assert(Wire.failuresRemaining == 0);
    } else if (scenario == "timeout-retry") {
        Wire.unavailable = true;
        assert(!freeink::papermono::ensureBooted());
        assert(millis() >= 1000 && millis() < 1050);
        Wire.unavailable = false;
        checkSuccessfulBoot();
    } else {
        assert(false);
    }
    std::cout << "Board startup " << scenario << " tests passed\n";
}
