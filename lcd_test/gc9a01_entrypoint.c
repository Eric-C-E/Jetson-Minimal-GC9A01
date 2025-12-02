/*
* GC9A01 Graphics Chip based LCD screen
* over SPI
* Copyright (c) 2025 Eric Liu
* MIT License
*/
#include "GC9A01.h"
#include "color_utils.h"
#include "socket_rx.h"
#include "framebuffer.h"

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <math.h>
#include <string.h>

#include <gpiod.h>

//define LINE NUMBERS <-> 40 pin hdr pins used by GPIO
#define RES 106
#define DC 105
//define display parameters for screen text
#define MAX_ROWS 10
#define MAX_CHARS 22


static const char *device = "/dev/spidev0.0";
static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 5000000;
static uint16_t delay = 0;
const char *chipname = "/dev/gpiochip0";
static const char *pinmux_script = "sh ./pinmux_setup.sh";

static struct gpiod_chip *chip;
static struct gpiod_line *line1;
static struct gpiod_line *line2;

int spi_fd;

int stop_pin = 0; //use for hardware interrupt stop later on
int stop_counter = 0; //use for testing
int stop_flag = 0;



static void pabort(const char *s){
    perror(s);
    abort();
}

static int run_pinmux(void) {
	int ret = system(pinmux_script);
	if (ret == -1) {
		perror("system(pinmux)");
		return -1;
	}

	if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
		return 0;
	}

	fprintf(stderr, "pinmux script exited with status %d\n", WEXITSTATUS(ret));
	return -1;
}


int spi_init(){
    int ret = 0;

    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("can't open device");
        return -1;
    }

    //setting SPI mode
    ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
        goto fail;

    ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
        goto fail;
    /*
	 * bits per word
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		goto fail;

	ret = ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		goto fail;

	/*
	 * max speed hz
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		goto fail;

	ret = ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		goto fail;

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	return ret;
fail:
	perror("spi config");
	if (spi_fd >= 0) {
		close(spi_fd);
		spi_fd = -1;
	}
	return -1;

}

void GC9A01_set_reset(uint8_t val){
	if (line2) {
		gpiod_line_set_value(line2, val);
	}
}

void GC9A01_set_data_command(uint8_t val){
	if (line1) {
		gpiod_line_set_value(line1, val);
	}
}

void GC9A01_spi_tx(uint8_t *data, size_t len){
	const size_t chunk_size = 4096; // conservative max transfer size for Jetson kernel

	for (size_t offset = 0; offset < len; offset += chunk_size) {
		size_t this_len = (len - offset < chunk_size) ? (len - offset) : chunk_size;

		struct spi_ioc_transfer tr = {
			.tx_buf = (unsigned long)(data + offset),
			.rx_buf = 0,
			.len = this_len,
			.delay_usecs = delay,
			.speed_hz = speed,
			.bits_per_word = bits,
		};

		if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
			pabort("can't send spi message");
		}
	}
}

int setup_2gpio(char *chipname, int line_1, int line_2) {
	int ret = -1;
	struct gpiod_chip *c = NULL;
	struct gpiod_line *l1 = NULL;
	struct gpiod_line *l2 = NULL;

	c = gpiod_chip_open(chipname);
	if (!c) {
		perror("open gpiochip");
		goto done;
	}

	l1 = gpiod_chip_get_line(c, line_1);
	if (!l1) {
		perror("get line1");
		goto done;
	}

	if (gpiod_line_request_output(l1, "gc9a01_dc", 0) < 0) {
		perror("request line1 output");
		goto done;
	}

	l2 = gpiod_chip_get_line(c, line_2);
	if (!l2) {
		perror("get line2");
		goto done;
	}

	if (gpiod_line_request_output(l2, "gc9a01_res", 0) < 0) {
		perror("request line2 output");
		goto done;
	}

	/* publish handles only after everything succeeds */
	chip = c;
	line1 = l1;
	line2 = l2;
	ret = 0;

done:
	if (ret != 0) {
		if (l1) gpiod_line_release(l1);
		if (l2) gpiod_line_release(l2);
		if (c) gpiod_chip_close(c);
	}
	return ret;
}
//cleanup function
void close_gpio(void) {
    if (line1) {
        gpiod_line_release(line1);
        line1 = NULL;
    }
    if (line2) {
        gpiod_line_release(line2);
        line2 = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

//use usleep(POSIX) for microseconds of sleep

void setup() {
	//sets up GPIO, SPI, and initializes GC9A01
    int gpio;
	int spi;

	/* Configure pinmux before touching GPIO/SPI */
	if (run_pinmux() != 0) {
		pabort("pinmux setup failed");
	}
	
	gpio = setup_2gpio(chipname, DC, RES);
	if (gpio != 0) {
		pabort("failed to set up GPIO");
	}

    sleep(1);

	printf("Initializing SPI...\n");

	spi = spi_init();
	if (spi != 0) {
		close_gpio();
		pabort("couldn't initialize spi");
	}

	sleep(1);
	if (GC9A01_init() != 0) {
		close(spi_fd);
		spi_fd = -1;
		close_gpio();
		pabort("GC9A01 init failed");
	}

	struct GC9A01_frame frame = {{0,0},{239,239}};
	GC9A01_set_frame(frame);
}

//program entrypoint

int main() {

    setup();

	printf("IO Initialized, Loading Screen\n");

	uint8_t color[2];
	const struct GC9A01_frame full_frame = {{0,0},{239,239}}; //full screen frame (inclusive)
	const struct GC9A01_frame text_frame = {{45, 30},{195, 210}}; //for displaying text 
	const struct GC9A01_frame SoC_frame = {{110, 195}, {130, 215}}; //for displaying batt soc
	//framebuffer allocation
	size_t fb_size = 240 * 240 * 3; //240x240 pixels, 2 bytes per pixel
	uint8_t *framebuffer = (uint8_t *)malloc(fb_size);
	if (framebuffer == NULL) {
		pabort("failed to allocate framebuffer");
	}
	fb_clear(framebuffer);

	//framebuffer test pattern drawing

	fb_draw_test_cross(framebuffer, 30, 45, 255, 0, 255); //magenta cross upper left
	fb_draw_test_cross(framebuffer, 210, 45, 255, 0, 255); //magenta cross upper right
	fb_draw_test_cross(framebuffer, 30, 195, 255, 0, 255); //magenta cross lower left
	fb_draw_test_cross(framebuffer, 210, 195, 255, 0, 255); //magenta cross lower right


	// put markers at starting points
	//for (int y = 45; y < 195; y += 19) {
	//	fb_draw_test_cross(framebuffer, 30, y, 255, 0, 255); //magenta horizontal center crosses
	//}
	
	//put some text
	printf("writing test filler text\n");

	fb_draw_string(framebuffer, "Hello, GC9A01!", 30, 177, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "This is a test", 30, 161, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "bottom", 30, 141, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "going up!", 30, 125, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "we don't use neli", 30, 109, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "TESTING 22 CHARACTERS!", 30, 93, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "more...", 30, 77, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "almost", 30, 61, 0, 255, 0); //green text
	fb_draw_string(framebuffer, "top!", 30, 45, 0, 255, 0); //green text


	//send framebuffer to LCD
	GC9A01_set_frame(full_frame);
	fb_write_to_gc9a01_fast(framebuffer, full_frame);
	printf("Displayed fast framebuffer test pattern\n");

	sleep(2);
	printf("simulating socket receive...\n");
	//simulate receiving data over socket, no newline characters
	char test_string[] = "This is a test of";
	fb_receive_and_update_text(framebuffer, test_string);
	textbuffer_render(framebuffer);
	GC9A01_set_frame(full_frame);
	fb_write_to_gc9a01_fast(framebuffer, full_frame);
	printf("Displayed received text over socket\n");

	sleep(2);
	printf("simulating another socket receive...\n");
	//simulate receiving data over socket, with newline characters
	char test_string2[] = "this time, it's longer and it's crazy! HAHAHAHAHA";

	fb_receive_and_update_text(framebuffer, test_string2);
	textbuffer_render(framebuffer);
	GC9A01_set_frame(text_frame);
	fb_write_to_gc9a01_fast(framebuffer, text_frame);
	printf("Displayed received text over socket\n");

	sleep(5);

	stop_counter ++;
	if (stop_counter >= 50) {
		stop_flag = 1; //for testing
	}
	printf("stop in %d\n", stop_counter);

	int server_fd = setup_socket();
	if (server_fd == -1) {
		pabort("socket setup failed");
	}
	char buffer[1024]; //buffer for receiving strings UTF-8 encoded


	while (stop_flag == 0) {
		//main loop: read socket, update text framebuffer, render, write to LCD
		int bytes_received = receive_data(server_fd, (uint8_t *)buffer, sizeof(buffer) - 1);
		if (bytes_received > 0) {
			buffer[bytes_received] = '\0'; //null-terminate
		}
		else if (bytes_received == -1) {
			printf("Error receiving data\n");
			continue;
		}
		else {
			//no data received, continue
			continue;
		}
			printf("Received %d bytes: %s\n", bytes_received, buffer);
			fb_receive_and_update_text(framebuffer, buffer);
			textbuffer_render(framebuffer);
			GC9A01_set_frame(text_frame);
			fb_write_to_gc9a01_fast(framebuffer, text_frame);
	}


	//cleanup
	close_socket(server_fd);
	printf("Socket closed\n");

	free(framebuffer);
	close_gpio();
	printf("GPIO closed\n");
	if (spi_fd >= 0) {
		close(spi_fd);
		spi_fd = -1;
		printf("SPI closed\n");
	}
    return 0;

}
