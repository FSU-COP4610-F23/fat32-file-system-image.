#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>


uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num) {
    uint32_t fat_region_offset = 0x4000;
    return fat_region_offset + clus_num * 4;
}

// returns the free cluster number
uint32_t alloca_cluster(int fd) {
    uint32_t fat_region = 0x4000;
    uint32_t min_clus_num = 2;
    uint32_t max_clus_num = 1009;
    uint32_t curr_clus_num = min_clus_num;
    uint32_t next_clus_num = 0xffffffff;
    uint32_t offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num);

    while (clus_num != 0) {
        pread(fd, &next_clus_num, sizeof(uint32_t), offset);
        if (next_clus_num == 0) {
            // current cluster number is free.
            return next_clus_num;
        } else {
            // check the next cluster number
            ++clus_num;
        }
    }
    // no free cluster found.
    return 0;
}

// add a new cluster to the cluster chain.
void extend_cluster_chain(int fd, uint32_t final_clus_num) {
    // get a free cluster number
    uint32_t new_clus_num = alloca_cluster(fd);

    // set the new custer as the final cluster of a file.
    uint32_t offset = convert_clus_num_to_offset_in_fat_region(new_clus_num);
    uint32_t end_of_file = 0xffffffff;
    pwrite(fd, &end_of_file, sizeof(uint32_t), offset);

    // update the cluster chain by updating the old final cluster.
    uint32_t offset = convert_clus_num_to_offset_in_fat_region(final_clus_num);
    pwrite(fd, &new_clus_num, sizeof(uint32_t), offset);
}
