#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include <stdint.h>
#include <stddef.h>

/* Simple container for packed color data to send over SPI */
struct GC9A01_color {
    uint8_t bytes[3];
    size_t len;
};

/* Pack 8-bit RGB into 12-bit (4-4-4) color.
 * Layout: [RRRR GGGG] [BBBB 0000]
 */
struct GC9A01_color rgb_to_12bit(uint8_t r, uint8_t g, uint8_t b);

/* Pack 8-bit RGB into 16-bit (5-6-5) color. */
struct GC9A01_color rgb_to_16bit(uint8_t r, uint8_t g, uint8_t b);

/* Pack 8-bit RGB into 18-bit (6-6-6) color. */
struct GC9A01_color rgb_to_18bit(uint8_t r, uint8_t g, uint8_t b);

#endif
