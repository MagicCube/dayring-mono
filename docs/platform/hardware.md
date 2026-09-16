# PaperMono Hardware Constraints

Agent reference for hardware facts that are easy to miss. Existing memory, geometry, initialization, and ownership details remain in the [HAL code map](hal.md).

Source: [M5Stack PaperMono documentation](https://docs.m5stack.com/en/core/PaperMono), checked 2026-09-17.

- **The panel supports 2-bit grayscale: four levels, including black and white.** A 1-bit framebuffer or B/W refresh path does not establish the panel's grayscale limit. Check the [SDK grayscale contract](../../freeink-sdk/docs/grayscale-capabilities.md) and `PaperMonoDriver` before choosing an upload format or waveform.
- **Touch does not reach the outermost pixels.** The documented effective range is X = 5–475, Y = 5–795 in the vendor's 480 × 800 portrait coordinates. Transform these bounds for the application's orientation; edge gestures must tolerate this inset.
- **Fast partial refreshes need cleanup and balanced drive.** M5Stack recommends a full refresh after approximately ten partial fast refreshes and warns against uninterrupted partial refreshes or DC-unbalanced custom waveforms. Its current guidance prefers the manufacturer's OTP example over unstable M5GFX waveforms. This is vendor guidance, not a description of our driver's current policy.
- **Disconnect the IP2315 charger from system I²C promptly after communication.** Its connection is gated by M5IOE1 `PYG11_PWM3`; prolonged connection can destabilize the bus. Low battery voltage during USB attachment can also prevent correct charger I²C initialization.

## Variant Boundary

The current HAL validates **PaperMono-Lite**. The linked vendor page covers PaperMono and compares both variants: Lite omits NFC and LoRa. Do not assume those peripherals exist on our target.
