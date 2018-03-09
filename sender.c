#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "commands.h"
#include "sha256_utils.h"

#define SERIAL_DEVICE "/dev/serial0"

long fileLength(FILE *fp)
{
    if (fseek(fp, 0, SEEK_END) == -1) {
        // TODO: proper error handling/return code
        perror("Error seeking file end");
        exit(-1);
    }

    long len = ftell(fp);
    rewind(fp);

    return len;
}

uint8_t* readFile(FILE *fp, size_t *size)
{
    long len = fileLength(fp);

    uint8_t *data = malloc(len);
    size_t read = fread(data, 1, len, fp);
    if (read != len) {
        // TODO: proper error handling/return code
        printf("Error reading file\n");
        exit(-1);
    }

    *size = len;
    return data;
}

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
    // Debug info
    printf("Sending  command %-19s with %-8u bytes of data\n", command_strs[m.code], m.payloadLen);

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

    // Debug info
    printf("Received reply   %-19s with %-8u bytes of data\n", reply_strs[m.code], m.payloadLen);

    return m;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("%s expects a file as an argument\n", argv[0]);
        exit(1);
    }

    // Open the serial device
    int serialfd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY | O_SYNC);
    if (serialfd < 0) {
        perror("Unable to open serial device " SERIAL_DEVICE);
        exit(-1);
    }

    Message m, r;

    printf("Running file upload test...\n\n");
    // start a transfer of a file to Tom, then request that file be transferred back
    FILE *test1 = fopen(argv[1], "r");
    if (test1 == NULL) {
        perror("error opening file");
    }
    size_t dataLen;
    uint8_t *data = readFile(test1, &dataLen);
    fclose(test1);

    uint8_t shaSum[32];
    sha256calc(data, dataLen, shaSum);

    m.code = START_UPLOAD;
    m.payloadLen = 32;
    m.payload = shaSum;

    writeMessage(serialfd, m);
    r = readMessage(serialfd);
    if (r.code != SUCCESS)
        exit(-1);

    size_t offset = 0;
    while (offset < dataLen) {
        m.code = SEND_PACKET;
        m.payloadLen = dataLen - offset > PACKET_SIZE ? PACKET_SIZE : dataLen - offset;

        m.payload = data + offset;

        writeMessage(serialfd, m);
        r = readMessage(serialfd);
        if (r.code != SUCCESS)
            exit(-1);

        offset += m.payloadLen;
    }

    free(data);

    char *filename = "test-upload-file";

    m.code = FINALIZE_UPLOAD;
    m.payloadLen = strlen(filename);
    m.payload = (uint8_t *)filename;

    writeMessage(serialfd, m);
    r = readMessage(serialfd);
    if (r.code != SUCCESS)
        exit(-1);

    printf("\nUpload test successful\n\n\n");

    printf("Running file download test...\n\n");
    // Download the file back from Tom
    m.code = START_DOWNLOAD;
    m.payloadLen = strlen(filename);
    m.payload = (uint8_t *)filename;

    writeMessage(serialfd, m);
    r = readMessage(serialfd);
    if (r.code != SUCCESS)
        exit(-1);

    uint8_t *dataDown = malloc(dataLen);
    offset = 0;
    while (true) {
        m = EMPTY_MESSAGE(REQUEST_PACKET);

        writeMessage(serialfd, m);
        r = readMessage(serialfd);
        if (r.code == ERROR_DOWNLOAD_OVER)
            break;
        else if (r.code != SUCCESS)
            exit(-1);

        if (offset >= dataLen)
            exit(-1);

        memcpy(dataDown + offset, r.payload, r.payloadLen);
        offset += r.payloadLen;

        if (r.payload != NULL)
            free(r.payload);
    }

    uint8_t shaSumDown[32];
    sha256calc(dataDown, dataLen, shaSumDown);

    free(dataDown);

    if (!sha256cmp(shaSum, shaSumDown))
        exit(-1);

    printf("\nDownload test successful\n");

    // Start an upload and halt it part way

    // Start a download and halt if part way
}
