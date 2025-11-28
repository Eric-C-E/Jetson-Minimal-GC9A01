#include "color_utils.h"

struct GC9A01_color rgb_to_12bit(uint8_t r, uint8_t g, uint8_t b) {
    struct GC9A01_color color = { .bytes = {0}, .len = 2 };
    color.bytes[0] = (r & 0xF0) | ((g & 0xF0) >> 4);     // RRRRGGGG
    color.bytes[1] = (b & 0xF0);                         // BBBB0000
    return color;
}

struct GC9A01_color rgb_to_16bit(uint8_t r, uint8_t g, uint8_t b) {
    struct GC9A01_color color = { .bytes = {0}, .len = 2 };
    uint16_t packed = (uint16_t)((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    color.bytes[0] = (uint8_t)(packed >> 8);
    color.bytes[1] = (uint8_t)(packed & 0xFF);
    return color;
}

struct GC9A01_color rgb_to_18bit(uint8_t r, uint8_t g, uint8_t b) {
    struct GC9A01_color color = { .bytes = {0}, .len = 3 };
    color.bytes[0] = r & 0xFC;   // 6 bits used by panel
    color.bytes[1] = g & 0xFC;
    color.bytes[2] = b & 0xFC;
    return color;
}
