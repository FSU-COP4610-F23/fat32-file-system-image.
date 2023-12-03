#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>


// Define the structure for the boot sector information
typedef struct __attribute__((packed)) BPB {

    int dataRegionAddress; //address to data region
    int dataSec; //sectors in data region
    int rootClusPosition; //address to root cluster/directory
    int rootDirSectors; //sectors in root directory

    off_t image_size;

    uint32_t BPB_RootClus; //print
    uint16_t BPB_BytsPerSec; //print
    uint8_t BPB_SecPerClus; //print
    uint16_t BPB_RsvdSecCnt;
    uint32_t BPB_TotSec32;
    uint16_t BPB_RootEntCnt;
    unsigned int total_clusters; //print
    uint8_t BPB_NumFATs;
    uint32_t BPB_FATSz32; //print
} boot_sector_info;

// Function prototypes
void parse_boot_sector(int file, boot_sector_info *info);
void display_boot_sector_info(const boot_sector_info *info);
void run_shell(const char *imageName, boot_sector_info *info);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./filesys [FAT32 ISO]\n");
        return 1;
    }

    const char *image_path = argv[1];

    int file = open(image_path, O_RDONLY);
    if (file < 0) {
        perror("Error opening image file");
        return 1;
    }

    boot_sector_info bootInfo;

    parse_boot_sector(file, &bootInfo);
    run_shell(image_path, &bootInfo);

    close(file);
    return 0;
}

void parse_boot_sector(int file, boot_sector_info *info) {
    
    // Read and parse the boot sector
    // These are fake values, we will need to change these
      // Example value
    
    info->BPB_RootClus = 0;
    ssize_t rd_bytes = pread(file, (void*)&info->BPB_RootClus, sizeof(info->BPB_RootClus), 44);
    // check if rd_bytes == sizeof(info->BPB_RootClus)
    if (rd_bytes != sizeof(info->BPB_RootClus)) {
        printf("(1) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_RootClus), rd_bytes);
        close(file);
        return;
    }//printf("BPB_RootClus: %u\n", info->BPB_RootClus);

    info->BPB_RsvdSecCnt = 0;
    rd_bytes = pread(file, &info->BPB_RsvdSecCnt, sizeof(info->BPB_RsvdSecCnt), 14);
    // check if rd_bytes == sizeof(info->BPB_RsvdSecCnt)
    if (rd_bytes != sizeof(info->BPB_RsvdSecCnt)) {
        printf("(2) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_RsvdSecCnt), rd_bytes);
        close(file);
        return;
    }//printf("BPB_RsvdSecCnt: %u\n", info->BPB_RsvdSecCnt);

    info->BPB_BytsPerSec = 0;
    rd_bytes = pread(file, &info->BPB_BytsPerSec, sizeof(info->BPB_BytsPerSec), 11);
    // check if rd_bytes == sizeof(info->BPB_BytsPerSec)
    if (rd_bytes != sizeof(info->BPB_BytsPerSec)) {
        printf("(3) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_BytsPerSec), rd_bytes);
        close(file);
        return;
    }//printf("BPB_BytsPerSec: %u\n", info->BPB_BytsPerSec);

    info->BPB_SecPerClus = 0;
    rd_bytes = pread(file, &info->BPB_SecPerClus, sizeof(info->BPB_SecPerClus), 13);
    // check if rd_bytes == sizeof(info->BPB_SecPerClus)
    if (rd_bytes != sizeof(info->BPB_SecPerClus)) {
        printf("(4) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_SecPerClus), rd_bytes);
        close(file);
        return;
    }//printf("BPB_SecPerClus: %u\n", info->BPB_SecPerClus);

    info->BPB_TotSec32 = 0;
    rd_bytes = pread(file, &info->BPB_TotSec32, sizeof(info->BPB_TotSec32), 32);
    // check if rd_bytes == sizeof(info->BPB_TotSec32)
    if (rd_bytes != sizeof(info->BPB_TotSec32)) {
        printf("(5) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_TotSec32), rd_bytes);
        close(file);
        return;
    }//printf("BPB_TotSec32: %u\n", info->BPB_TotSec32);

    info->BPB_RootEntCnt = 0;
    rd_bytes = pread(file, &info->BPB_RootEntCnt, sizeof(info->BPB_RootEntCnt), 17);
    // check if rd_bytes == sizeof(info->BPB_RootEntCnt)
    if (rd_bytes != sizeof(info->BPB_RootEntCnt)) {
        printf("(6) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_RootEntCnt), rd_bytes);
        close(file);
        return;
    }//printf("BPB_TotSec32: %u\n", info->BPB_RootEntCnt);


    info->total_clusters = 0;  // Example value

    info->BPB_NumFATs = 0;
    rd_bytes = pread(file, &info->BPB_NumFATs, sizeof(info->BPB_NumFATs), 16);
    // check if rd_bytes == sizeof(info->BPB_FATSz32)
    if (rd_bytes != sizeof(info->BPB_NumFATs)) {
        printf("(7) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_NumFATs), rd_bytes);
        close(file);
        return;
    }//printf("BPB_NumFATs: %u\n", info->BPB_NumFATs);

    info->BPB_FATSz32 = 0;
    rd_bytes = pread(file, &info->BPB_FATSz32, sizeof(info->BPB_FATSz32), 36);
    // check if rd_bytes == sizeof(info->BPB_FATSz32)
    if (rd_bytes != sizeof(info->BPB_FATSz32)) {
        printf("(8) request %lu bytes, but read %ld bytes\n", sizeof(info->BPB_FATSz32), rd_bytes);
        close(file);
        return;
    }//printf("BPB_FATSz32: %u\n", info->BPB_FATSz32);
    info->dataRegionAddress = info->BPB_BytsPerSec * info->BPB_RsvdSecCnt + //in decimal expression
                              info->BPB_FATSz32 * info->BPB_NumFATs * info->BPB_BytsPerSec;

    //should be same as dataRegionAddress using formula
    info->rootClusPosition = info->dataRegionAddress + info->BPB_BytsPerSec * (info->BPB_RootClus - 2);

    //sectors in root directory
    info->rootDirSectors = ((info->BPB_RootEntCnt * 32) + (info->BPB_BytsPerSec - 1)) / info->BPB_BytsPerSec;
    //sectors in data region
    info->dataSec = info->BPB_TotSec32 - (info->BPB_RsvdSecCnt + (info->BPB_NumFATs * info->BPB_FATSz32) + info->rootDirSectors);
    //count of clusters in data region
    info->total_clusters = info->dataSec / info->BPB_SecPerClus;
    //size of image disk
    info->image_size = lseek(file, 0, SEEK_END);
    info->BPB_FATSz32 = info->BPB_FATSz32 * info->BPB_BytsPerSec / 4; //4 bytes in 1 entry
}

void display_boot_sector_info(const boot_sector_info *info) {
    printf("\nRoot Cluster: %x\n", info->rootClusPosition);
    printf("Bytes Per Sector: %u\n", info->BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", info->BPB_SecPerClus);
    printf("Total Clusters in Data Region: %u\n", info->total_clusters);
    printf("Entries Per FAT: %u\n", info->BPB_FATSz32);
    printf("Image Size: %lld bytes\n", (long long)info->image_size);
}

void run_shell(const char *imageName, boot_sector_info *info) {
    char command[100];
    while (1) {
        printf("[%s]/>", imageName);
        scanf("%s", command);

        if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "info") == 0) {
            display_boot_sector_info(info);
        } else {
            printf("Unknown command: %s\n", command);
        }
    }
}
