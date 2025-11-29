//open UNIX domain socket for receiving data
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "GC9A01.h"
#define SOCKET_PATH "/tmp/gc9a01_socket"

int setup_socket() {
    int server_fd;
    struct sockaddr_un server_addr;
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 20000, // 20 ms timeout to avoid blocking forever
    };

    // Create socket
    if ((server_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket error");
        return -1;
    }

    // Remove any existing socket file
    unlink(SOCKET_PATH);

    // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    snprintf(server_addr.sun_path,
         sizeof(server_addr.sun_path),
         "%s",
         SOCKET_PATH);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error");
        close(server_fd);
        return -1;
    }

    // Set a receive timeout so recvfrom doesn't block forever
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        perror("setsockopt SO_RCVTIMEO");
        close(server_fd);
        return -1;
    }
    printf("Socket setup complete at %s\n", SOCKET_PATH);

    return server_fd;
}
int receive_data(int server_fd, uint8_t *buffer, size_t buffer_size) {

    ssize_t num_bytes = recvfrom(server_fd, buffer, buffer_size, 0, NULL, NULL);
    if (num_bytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Timeout with no data
            return 0;
        } else {
            perror("recvfrom error");
            return -1;
        }
    }
    return (int)num_bytes;
}
void close_socket(int server_fd) {
    close(server_fd);
    unlink(SOCKET_PATH);
}
// Example usage
/*int main() {
    int server_fd = setup_socket();
    if (server_fd == -1) {
        exit(EXIT_FAILURE);
    }  
    uint8_t buffer[1024];
    int bytes_received = receive_data(server_fd, buffer, sizeof(buffer));
    if (bytes_received > 0) {
        printf("Received %d bytes\n", bytes_received);
        // Process the received data...
    }
    close_socket(server_fd);
    return 0;
}*/
