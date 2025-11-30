/* contains framebuffer rendering functions
including retrieval of bitmap fonts and into framebuffer
as well as icons such as battery SoC
function to shift text rows up
and framebuffer size tests*/

#include "framebuffer.h"
#include "font8x16.h"
#include "color_utils.h"
#include "GC9A01.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>

#define FB_WIDTH 240
#define FB_HEIGHT 240
#define FB_BPP 3 //bytes per pixel RGB888
#define FB_SIZE (FB_WIDTH * FB_HEIGHT * FB_BPP)

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

//function to draw a character at (x,y) in the framebuffer
void fb_draw_char(uint8_t *framebuffer, char c, int x, int y,
                  uint8_t r, uint8_t g, uint8_t b)
{
    const uint8_t *char_bitmap = font8x16[(uint8_t)c]; //get pointer to character bitmap (ASCII)

    for (int row = 0; row < FONT_HEIGHT; row++) {
        for (int col = 0; col < FONT_WIDTH; col++) {

            // check pixel bit
            if (char_bitmap[row] & (1 << (7 - col))) {

                // standard coordinate system:
                int pixel_x = x + col;
                int pixel_y = y + row;

                int fb_index = (pixel_y * FB_WIDTH + pixel_x) * FB_BPP;

                framebuffer[fb_index]     = r;
                framebuffer[fb_index + 1] = g;
                framebuffer[fb_index + 2] = b;
            }
        }
    }
}
void fb_draw_string(uint8_t *framebuffer, const char *str, int x, int y,
                    uint8_t r, uint8_t g, uint8_t b)
{
    int orig_x = x;

    while (*str) {

        if (*str == '\n') {
            x = orig_x;         // reset x (column)
            y += FONT_HEIGHT;   // move down one line
        } else {
            fb_draw_char(framebuffer, *str, x, y, r, g, b);
            x += FONT_WIDTH;    // advance horizontally
        }

        str++;
    }
}
//function to write a test cross at the point (x,y)
void fb_draw_test_cross(uint8_t *framebuffer, int x, int y, 
                       uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) {
        return; //out of bounds
    }
    for (int dx = -5; dx <= 5; dx++) {
        int fb_index = (y * FB_WIDTH + (x + dx)) * FB_BPP;
        if (x + dx >= 0 && x + dx < FB_WIDTH) {
            framebuffer[fb_index] = r;
            framebuffer[fb_index + 1] = g;
            framebuffer[fb_index + 2] = b;
        }
    }
    for (int dy = -5; dy <= 5; dy++) {
        int fb_index = ((y + dy) * FB_WIDTH + x) * FB_BPP;
        if (y + dy >= 0 && y + dy < FB_HEIGHT) {
            framebuffer[fb_index] = r;
            framebuffer[fb_index + 1] = g;
            framebuffer[fb_index + 2] = b;
        }
    }
}

//function to write all framebuffer bytes to GC9A01 within (x1,x2,y1,y2) IMPORTANT: define frame as same
void fb_write_to_gc9a01(uint8_t *framebuffer, int x1, int y1, int x2, int y2) {
    /* x2/y2 are treated as exclusive bounds (like width/height),
     * matching how the test patterns stream pixels: x in [x1, x2) then y in [y1, y2). */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > FB_WIDTH) x2 = FB_WIDTH;
    if (y2 > FB_HEIGHT) y2 = FB_HEIGHT;

    for (int x = x1; x < x2; x++) {
        for (int y = y1; y < y2; y++) {
            size_t fb_index = (y * FB_WIDTH + x) * FB_BPP;
            uint8_t r = framebuffer[fb_index];
            uint8_t g = framebuffer[fb_index + 1];
            uint8_t b = framebuffer[fb_index + 2];
            struct GC9A01_color packed = rgb_to_16bit(r, g, b);
            if (x == x1 && y == y1) {
                GC9A01_write(packed.bytes, packed.len);
            } else {
                GC9A01_write_continue(packed.bytes, packed.len);
            }
        }
    }
}
//optimized function to write all framebuffer bytes to GC9A01 within (x1,x2,y1,y2) using a packed buffer
//IMPORTANT: define frame as same
//as long as frame is larger, will work
//smaller will be more optimized, so if keep index tracking text size, can make faster
void fb_write_to_gc9a01_fast(uint8_t *framebuffer, int x1, int y1, int x2, int y2) {
    /* x2/y2 are treated as exclusive bounds (like width/height),
     * matching how the test patterns stream pixels: x in [x1, x2) then y in [y1, y2). */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > FB_WIDTH) x2 = FB_WIDTH;
    if (y2 > FB_HEIGHT) y2 = FB_HEIGHT;

    size_t total_pixels = (x2 - x1) * (y2 - y1);
    size_t packed_size = total_pixels * 2; // 2 bytes per pixel in 16-bit format
    uint8_t *packed_buffer = malloc(packed_size);
    if (!packed_buffer) {
        perror("malloc packed_buffer");
        return;
    }

    size_t index = 0;
    for (int x = x1; x < x2; x++) {
        for (int y = y1; y < y2; y++) {
            size_t fb_index = (y * FB_WIDTH + x) * FB_BPP;
            uint8_t r = framebuffer[fb_index];
            uint8_t g = framebuffer[fb_index + 1];
            uint8_t b = framebuffer[fb_index + 2];
            struct GC9A01_color packed = rgb_to_16bit(r, g, b);
            packed_buffer[index++] = packed.bytes[0];
            packed_buffer[index++] = packed.bytes[1];
        }
    }

    GC9A01_write(packed_buffer, packed_size);
    free(packed_buffer);
}



//end of framebuffer.c

