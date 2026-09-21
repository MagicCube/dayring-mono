# cJSON native-test dependency

Unmodified cJSON 1.7.19 (`cJSON.c`, `cJSON.h`, MIT `LICENSE`) from
[the upstream v1.7.19 release](https://github.com/DaveGamble/cJSON/tree/v1.7.19).

Firmware links the bundled ESP-IDF cJSON 1.7.19 component. Native tests and previews
compile this matching release through `src/platform/calendar/Json.cpp`, which
limits nesting and isolates a macOS deprecation diagnostic in third-party code.
Calendar parsing independently bounds input bytes, structural tokens, and nesting
before calling either implementation. No global cJSON allocation hooks are changed.
