//framebuffer header
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

void fb_draw_char(uint8_t *framebuffer, char c, int x, int y, 
                 uint8_t r, uint8_t g, uint8_t b);
void fb_draw_string(uint8_t *framebuffer, const char *str, int x, int y, 
                   uint8_t r, uint8_t g, uint8_t b);
void fb_draw_test_cross(uint8_t *framebuffer, int x, int y, 
                       uint8_t r, uint8_t g, uint8_t b);
                       
#endif