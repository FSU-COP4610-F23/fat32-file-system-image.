#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

// the offset to the beginning of the file.
uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset) {
    uint32_t fat_region_offset = 0x4000;
    return (offset - fat_region_offset)/4;
}

uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num) {
    uint32_t fat_region_offset = 0x4000;
    return fat_region_offset + clus_num * 4;
}

// This example shows how to decode a cluster chain.
// You need to implement it in your way instead of the code shown below.
// Might need to define a data structure to store such information.
int main(int argc, char const *argv[])
{
    int fd = open("fat32.img", O_RDONLY);
    if (fd < 0) {
        perror("open fat32.img failed");
        return 0;
    }

    uint32_t offset = 0;
    uint32_t curr_clus_num = 3;
    uint32_t next_clus_num = 0;
    uint32_t BPB_SecPerClus = 1;
    uint32_t BPB_FATSz32 = 1009;
    uint32_t max_clus_num = BPB_FATSz32 / BPB_SecPerClus;
    uint32_t min_clus_num = 2;

    while (curr_clus_num >= min_clus_num && curr_clus_num <= max_clus_num) {
        // if the cluster number is not in the range, this cluster is:
        // 1. reserved cluster
        // 2. end of the file
        // 3. bad cluster.
        // No matter which kind of number, we can consider it is the end of a file.
        offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num);
        pread(fd, &next_clus_num, sizeof(uint32_t), offset);
        printf("current cluster number: %u, next cluster number: %u\n", \
                curr_clus_num, next_clus_num);
        curr_clus_num = next_clus_num;
    }

    close(fd);

    return 0;
}
