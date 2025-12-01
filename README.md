# Jetson-Minimal-GC9A01
Minimal program to drive GC9A01 graphics chip based LCD displays using the NVidia Jetson Orin Nano Devkit

Requires Jetpack 6.2 +

# Setup
Configure the Jetson using Jetson-io or manual device tree overlay to expose the SPI0 functionality.

Requires Jetpack 6.2 for dynamic pinmux.

Connect SPI0 and launch the program.

To print characters in a stream, use with any UNIX-domain socket char datastream.

To assess battery SoC, use with: 

# Notes

This software is extremely custom for a specific purpose of serving as an AR goggle display with specific positions for text
and only one way of handling incoming strings. Though, it can easily be repurposed.

