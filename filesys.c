#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Define the structure for the boot sector information
typedef struct {
    unsigned int root_cluster;
    unsigned int bytes_per_sector;
    unsigned int sectors_per_cluster;
    unsigned int total_clusters;
    unsigned int num_FAT_entries;
    unsigned int image_size;
} boot_sector_info;

// Function prototypes
void parse_boot_sector(FILE *file, boot_sector_info *info);
void display_boot_sector_info(const boot_sector_info *info);
void run_shell(const char *imageName, boot_sector_info *info);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./filesys [FAT32 ISO]\n");
        return 1;
    }

    const char *image_path = argv[1];
    FILE *file = fopen(image_path, "rb");
    if (file == NULL) {
        perror("Error opening image file");
        return 1;
    }

    boot_sector_info bootInfo;
    parse_boot_sector(file, &bootInfo);

    run_shell(image_path, &bootInfo);

    fclose(file);
    return 0;
}

void parse_boot_sector(FILE *file, boot_sector_info *info) {
    // Read and parse the boot sector
    // These are fake values, we will need to change these
    info->root_cluster = 0;  // Example value
    info->bytes_per_sector = 512;  // Example value
    info->sectors_per_cluster = 8;  // Example value
    info->total_clusters = 10000;  // Example value
    info->num_FAT_entries = 2000;  // Example value
    info->image_size = 5120000;  // Example value

}

void display_boot_sector_info(const boot_sector_info *info) {
    printf("Root Cluster: %u\n", info->root_cluster);
    printf("Bytes Per Sector: %u\n", info->bytes_per_sector);
    printf("Sectors Per Cluster: %u\n", info->sectors_per_cluster);
    printf("Total Clusters: %u\n", info->total_clusters);
    printf("Number of FAT Entries: %u\n", info->num_FAT_entries);
    printf("Image Size: %u bytes\n", info->image_size);
}

void run_shell(const char *imageName, boot_sector_info *info) {
    char command[100];
    while (1) {
        printf("[%s]/>\n", imageName);
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
