#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>

typedef struct __attribute__((packed)) BPB {
    char BS_jmpBoot[3]; // unnecessary to parse it, use a char array to hold space.
    char BS_OEMName[8]; // unnecessary to parse it, use a char array to hold space.
    uint16_t BPB_BytsPerSec; // offset 11, size 2.
    char OtherBytes[499]; // omit other variables.
} bpb_t;

int main() {
    uint16_t BPB_BytsPerSec = 0;

    // open file.
    int fd = open("fat32.img", O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return 0;
    }

    // 1. read by bytes then convert to uint16_t
    BPB_BytsPerSec = 0;
    char buf[2];
    lseek(fd, 11, SEEK_SET);
    ssize_t rd_bytes = read(fd, buf, sizeof(BPB_BytsPerSec));
    // check if rd_bytes == sizeof(BPB_BytsPerSec)
    if (rd_bytes != sizeof(BPB_BytsPerSec)) {
        printf("request %u bytes, but read %u bytes\n", sizeof(BPB_BytsPerSec), rd_bytes);
        close(fd);
        return 0;
    }
    BPB_BytsPerSec = buf[0] + buf[1] << 8;
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 2. read to uint16_t directly
    BPB_BytsPerSec = 0;
    lseek(fd, 11, SEEK_SET);
    rd_bytes = read(fd, &BPB_BytsPerSec, sizeof(BPB_BytsPerSec));
    // check if rd_bytes == sizeof(BPB_BytsPerSec)
    if (rd_bytes != sizeof(BPB_BytsPerSec)) {
        printf("request %u bytes, but read %u bytes\n", sizeof(BPB_BytsPerSec), rd_bytes);
        close(fd);
        return 0;
    }
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 3. read by structure
    bpb_t bpb;
    lseek(fd, 0, SEEK_SET);
    rd_bytes = read(fd, &bpb, sizeof(bpb_t));
    // check if rd_bytes == sizeof(BPB_BytsPerSec)
    if (rd_bytes != sizeof(bpb_t)) {
        printf("request %u bytes, but read %u bytes\n", sizeof(bpb_t), rd_bytes);
        close(fd);
        return 0;
    }
    printf("BPB_BytsPerSec: %u\n", bpb.BPB_BytsPerSec);

    close(fd);

    return 0;
}