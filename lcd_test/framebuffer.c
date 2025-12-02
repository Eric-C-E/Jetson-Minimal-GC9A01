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

#define MAX_ROWS 9
#define MAX_CHARS 22

#define LOWEST_ROW_Y 177
#define TOP_ROW_Y 45

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

//unoptimized function to write all framebuffer bytes to GC9A01 within the given frame
void fb_write_to_gc9a01(uint8_t *framebuffer, struct GC9A01_frame frame) {
    /* GC9A01_frame uses inclusive end coords; convert to exclusive for loops. */
    int x1 = frame.start.X;
    int y1 = frame.start.Y;
    int x2 = frame.end.X + 1;
    int y2 = frame.end.Y + 1;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > FB_WIDTH) x2 = FB_WIDTH;
    if (y2 > FB_HEIGHT) y2 = FB_HEIGHT;

    /* Column-major stream: step through each column (x) and all rows (y) inside. */
    for (int x = x1; x < x2; x++) {
        for (int y = y1; y < y2; y++) {
            size_t fb_index = (y * FB_HEIGHT + x) * FB_BPP;
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
//16 bit color assumed
void fb_write_to_gc9a01_fast(uint8_t *framebuffer, struct GC9A01_frame frame) {
    /* GC9A01_frame uses inclusive end coords; convert to exclusive for loops. */
    int x1 = frame.start.Y;
    int y1 = frame.start.X;
    int x2 = frame.end.Y + 1;
    int y2 = frame.end.X + 1;

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
    /* Match the slow-path ordering: column-major (x outer, y inner). */
    for (int x = x1; x < x2; x++) {
        for (int y = y1; y < y2; y++) {
            size_t fb_index = (y * FB_HEIGHT + x) * FB_BPP;
            uint8_t r = framebuffer[fb_index];
            uint8_t g = framebuffer[fb_index + 1];
            uint8_t b = framebuffer[fb_index + 2];
            struct GC9A01_color packed = rgb_to_16bit(r, g, b);
            packed_buffer[index++] = packed.bytes[0];
            packed_buffer[index++] = packed.bytes[1];
        }
    }

    // stream to the panel in 4 KB chunks using MEM_WR then MEM_WR_CONT
    const size_t chunk_size = 4096;
    for (size_t offset = 0; offset < packed_size; offset += chunk_size) {
        size_t bytes_to_write = (offset + chunk_size < packed_size) ? chunk_size : (packed_size - offset);
        if (offset == 0) {
            GC9A01_write(&packed_buffer[offset], bytes_to_write);
        } else {
            GC9A01_write_continue(&packed_buffer[offset], bytes_to_write);
        }
    }

    //free memory
    free(packed_buffer);

}
//clear framebuffer to black
void fb_clear(uint8_t *framebuffer) {
    memset(framebuffer, 0x00, FB_SIZE);
}

//Internal string management: keep track of existing rows and their contents. Write entire contents to framebuffer once per cycle.

static char lines[MAX_ROWS][MAX_CHARS + 1]; // +1 for null terminator
static int current_row = 0;

void textbuffer_initialize() {
    memset(lines, 0, sizeof(lines));
    current_row = 0;
}   

//shift all lines up by one, dropping the top line without populating a new line
//leaves a blank line at the bottom
void textbuffer_shift_up() {
    for (int i = MAX_ROWS - 1; i >= 1; i--) {
        strncpy(lines[i], lines[i - 1], MAX_CHARS + 1);
    }
    //clear the lowest line
    memset(lines[0], 0, MAX_CHARS + 1);
}

//function to check incoming string data over socket and receive into buffer while appending the part that fits to the lowest available row, adding lines as needed
void fb_receive_and_update_text(uint8_t *framebuffer, char receive_buffer[]) {
    //assumes null-terminated string in receive_buffer
    //recursive function
    //check if the string doesn't fit in the current line
    size_t bytes_received = strlen(receive_buffer);

    size_t space_left = MAX_CHARS - strlen(lines[0]);
    if (space_left < bytes_received) {
        //fits exactly or overflows
        //if the last character in bottom line is not a space and the first character in receive buffer is not a space, add a space
        if (strlen(lines[0]) > 0 && lines[0][strlen(lines[0])+1] != ' ' &&
            receive_buffer[0] != ' ') {
            if (space_left > 0) {
                lines[0][strlen(lines[0]+1)] = ' ';
                space_left--;
            }
        }
        //append what fits
        strncat(lines[0], receive_buffer, space_left);
        //shift up
        textbuffer_shift_up();
        //recurse with leftovers
        fb_receive_and_update_text(framebuffer, receive_buffer + space_left);
        return;
    } else if (space_left == bytes_received) {
        //fits exactly
        strncat(lines[0], receive_buffer, bytes_received);
        //shift up for next line
        textbuffer_shift_up();
        return;
    }
    else {
        //fits with space left
        strncat(lines[0], receive_buffer, bytes_received);
        return;
    }
}

void textbuffer_render(uint8_t *framebuffer) {
    fb_clear(framebuffer);

    //render the entire framebuffer from text lines
    fb_draw_string(framebuffer, lines[0], 30, 177, 0, 255, 0); //lowest string on screen
	fb_draw_string(framebuffer, lines[1], 30, 161, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[2], 30, 141, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[3], 30, 125, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[4], 30, 109, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[5], 30, 93, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[6], 30, 77, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[7], 30, 61, 0, 255, 0); //green text
	fb_draw_string(framebuffer, lines[8], 30, 45, 0, 255, 0); //highest string on screen
}





//end of framebuffer.c
