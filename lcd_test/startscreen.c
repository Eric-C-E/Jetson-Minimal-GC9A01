#include <stdint.h>
#include <string.h>
#include "font8x16.h"
#include "startscreen.h"
//draws a startup screen with gradients and text
void draw_startup_screen(uint8_t *framebuffer) {
    //clear framebuffer
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 240; x++) {
            size_t index = (y * 240 + x) * 3;
            framebuffer[index] = 0;     // R
            framebuffer[index + 1] = 0; // G
            framebuffer[index + 2] = 0; // B
        }
    }
    //draw diffuse gradient background
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 240; x++) {
            size_t index = (y * 240 + x) * 3;
            uint8_t r = (uint8_t)((x + y) / 2);
            uint8_t g = (uint8_t)(y / 2);
            uint8_t b = (uint8_t)(x / 2);
            framebuffer[index] = r;
            framebuffer[index + 1] = g;
            framebuffer[index + 2] = b;
        }
    }
    //draw text "Live Language Lens" in white at center
    const char *text = "LIVE LANGUAGE LENS";
    int text_length = 19; //including spaces
    int start_x = (240 - (text_length * 8)) / 2;
    int start_y = 110;
    for (int i = 0; i < text_length; i++) {
        char c = text[i];
        //draw character at (start_x + i*8, start_y)
        const uint8_t *char_bitmap = font8x16[(uint8_t)c];
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 8; col++) {
                if (char_bitmap[row] & (1 << (7 - col))) {
                    int pixel_x = start_x + i * 8 + col;
                    int pixel_y = start_y + row;
                    size_t fb_index = (pixel_y * 240 + pixel_x) * 3;
                    framebuffer[fb_index] = 255;     // R
                    framebuffer[fb_index + 1] = 255; // G
                    framebuffer[fb_index + 2] = 255; // B
                }
            }
        }
    }
}
