#include <stdio.h>
#include <string.h>
#include <inttypes.h>

typedef struct __attribute__((packed)) BPB {
    char BS_jmpBoot[3]; // unnecessary to parse it, use a char array to hold space.
    char BS_OEMName[8]; // unnecessary to parse it, use a char array to hold space.
    uint16_t BPB_BytsPerSec; // offset 11, size 2.
    char OtherBytes[499]; // omit other variables.
} bpb_t;

int main() {
    uint16_t BPB_BytsPerSec = 0;
    FILE * fp = fopen("fat32.img", "r");


    // 1. read by bytes then convert to uint16_t
    BPB_BytsPerSec = 0;
    char buf[2];
    fseek(fp, 11, SEEK_SET);
    fread(&buf, sizeof(BPB_BytsPerSec), 1, fp);
    BPB_BytsPerSec = buf[0] + buf[1] << 8;
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 2. read to uint16_t directly
    BPB_BytsPerSec = 0;
    fseek(fp, 11, SEEK_SET);
    fread(&BPB_BytsPerSec, sizeof(BPB_BytsPerSec), 1, fp);
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 3. read by structure
    bpb_t bpb;
    fseek(fp, 0, SEEK_SET);
    fread(&bpb, sizeof(bpb_t), 1, fp);
    printf("BPB_BytsPerSec: %u\n", bpb.BPB_BytsPerSec);

    fclose(fp);

    return 0;
}