/*
* GC9A01 Graphics Chip based LCD screen
* over SPI
* Copyright (c) 2025 Eric Liu
*/
#include "GC9A01.h"

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <math.h>

#include <gpiod.h>

//define LINE NUMBERS <-> 40 pin hdr pins used by GPIO
#define RES 106
#define DC 105

static const char *device = "/dev/spidev0.0";
static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay = 0;
const char *chipname = "/dev/gpiochip0";
static const char *pinmux_script = "./pinmux_setup.sh";

static struct gpiod_chip *chip;
static struct gpiod_line *line1;
static struct gpiod_line *line2;

int spi_fd;


static void pabort(const char *s){
    perror(s);
    abort();
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
	int ret;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)data,
		.rx_buf = 0,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");
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

    int gpio;
	int spi;

	/* Configure pinmux before touching GPIO/SPI */
	if (system(pinmux_script) != 0) {
		pabort("pinmux setup failed");
	}
	
	gpio = setup_2gpio(chipname, DC, RES);
	if (gpio != 0) {
		pabort("failed to set up GPIO");
	}

    sleep(1);

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

	struct GC9A01_frame frame = {{0,0},{23,239}};
	GC9A01_set_frame(frame);
}

//program entrypoint

int main() {

    setup();

	uint8_t color[3];

	// Triangle
    color[0] = 0xFF;
    color[1] = 0xFF;
    for (int x = 0; x < 240; x++) {
        for (int y = 0; y < 240; y++) {
            if (x < y) {
                color[2] = 0xFF;
            } else {
                color[2] = 0x00;
            }
            if (x == 0 && y == 0) {
                GC9A01_write(color, sizeof(color));
            } else {
                GC9A01_write_continue(color, sizeof(color));
            }
        }
    }

    sleep(10);

    // Rainbow
    float frequency = 0.026;
    for (int x = 0; x < 240; x++) {
        color[0] = sin(frequency*x + 0) * 127 + 128;
        color[1] = sin(frequency*x + 2) * 127 + 128;
        color[2] = sin(frequency*x + 4) * 127 + 128;
        for (int y = 0; y < 240; y++) {
            if (x == 0 && y == 0) {
                GC9A01_write(color, sizeof(color));
            } else {
                GC9A01_write_continue(color, sizeof(color));
            }
        }
    }

    sleep(10);

    // Checkerboard
    for (int x = 0; x < 240; x++) {
        for (int y = 0; y < 240; y++) {
            if ((x / 10) % 2 ==  (y / 10) % 2) {
                color[0] = 0xFF;
                color[1] = 0xFF;
                color[2] = 0xFF;
            } else {
                color[0] = 0x00;
                color[1] = 0x00;
                color[2] = 0x00;
            }
            if (x == 0 && y == 0) {
                GC9A01_write(color, sizeof(color));
            } else {
                GC9A01_write_continue(color, sizeof(color));
            }
        }
    }

    sleep(10);

    // Swiss flag
    color[0] = 0xFF;
    for (int x = 0; x < 240; x++) {
        for (int y = 0; y < 240; y++) {
            if ((x >= 1*48 && x < 4*48 && y >= 2*48 && y < 3*48) ||
                (x >= 2*48 && x < 3*48 && y >= 1*48 && y < 4*48)) {
                color[1] = 0xFF;
                color[2] = 0xFF;
            } else {
                color[1] = 0x00;
                color[2] = 0x00;
            }
            if (x == 0 && y == 0) {
                GC9A01_write(color, sizeof(color));
            } else {
                GC9A01_write_continue(color, sizeof(color));
            }
        }
    }

    sleep(10);

	close_gpio();
	if (spi_fd >= 0) {
		close(spi_fd);
		spi_fd = -1;
	}
    return 0;

}


//TODO eventually close the connections SPI and 2x GPIOS on keyboard exit
