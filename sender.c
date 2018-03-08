#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "commands.h"

#define SERIAL_DEVICE "/dev/serial0"

// TODO: proper error checking and handling here
void readAllOrDie(int fd, void *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        result = read(fd, buf + offset, len - offset);
        if (result < 0) {
            perror("Error reading from file descriptor");
            exit(-1);
        }
        offset += result;
    }
}

// TODO: proper error handling here
void writeAllOrDie(int fd, const void *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        result = write(fd, buf + offset, len - offset);
        if (result < 0) {
            perror("Error writing to file descriptor");
            exit(-1);
        }
        offset += result;
    }
}

void writeMessage(int fd, const Message m)
{
    uint8_t outHeader[3];
    outHeader[0] = m.code;
    memcpy(outHeader + 1, &m.payloadLen, 2);

    writeAllOrDie(fd, outHeader, sizeof(outHeader));
    if (m.payload != NULL)
        writeAllOrDie(fd, m.payload, m.payloadLen);
}

Message readMessage(int fd)
{
    uint8_t inHeader[3];
    readAllOrDie(fd, inHeader, sizeof(inHeader));

    Message m;
    m.code = inHeader[0];
    memcpy(&m.payloadLen, inHeader + 1, 2);

    m.payload = NULL;
    if (m.payloadLen > 0) {
        m.payload = malloc(m.payloadLen);
        readAllOrDie(fd, m.payload, m.payloadLen);
    }

    return m;
}

int main()
{
    // Open the serial device
    int serialfd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY);
    if (serialfd < 0) {
        perror("Unable to open serial device " SERIAL_DEVICE);
        exit(-1);
    }

    // start a transfer of a file to tom, then request that file be transferred back
}
