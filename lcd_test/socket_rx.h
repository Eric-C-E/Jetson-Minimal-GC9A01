#ifndef SOCKET_RX_H
#define SOCKET_RX_H

#include <stdint.h>
#include <stddef.h>


int setup_socket();
int receive_data(int server_fd, uint8_t *buffer, size_t buffer_size);
void close_socket(int server_fd);

#endif
