#!/bin/sh
# Configure pinmux for GC9A01 SPI/GPIO lines
sudo busybox devmem 0x2430068 w 0x8
sudo busybox devmem 0x2430070 w 0x8
