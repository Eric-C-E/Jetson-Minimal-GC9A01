//framebuffer header
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>
#include "GC9A01.h"



void fb_draw_char(uint8_t *framebuffer, char c, int x, int y, 
                 uint8_t r, uint8_t g, uint8_t b);
void fb_draw_string(uint8_t *framebuffer, const char *str, int x, int y, 
                   uint8_t r, uint8_t g, uint8_t b);
void fb_draw_test_cross(uint8_t *framebuffer, int x, int y, 
                       uint8_t r, uint8_t g, uint8_t b);
void fb_write_to_gc9a01(uint8_t *framebuffer, struct GC9A01_frame frame);
void fb_write_to_gc9a01_fast(uint8_t *framebuffer, struct GC9A01_frame frame);
void fb_clear(uint8_t *framebuffer);

//Internal string management functions
void textbuffer_initialize();
void textbuffer_shift_up();
void fb_receive_and_update_text(uint8_t *framebuffer, char receive_buffer[], int bytes_received);
void textbuffer_render(uint8_t *framebuffer);


#endif
