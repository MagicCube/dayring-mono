#ifndef ARDUINO
// Native checks use the same cJSON release as the ESP-IDF JSON component.
#define CJSON_NESTING_LIMIT 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
extern "C" {
#include "../../../third_party/cjson/cJSON.c"
}
#pragma GCC diagnostic pop
#endif
