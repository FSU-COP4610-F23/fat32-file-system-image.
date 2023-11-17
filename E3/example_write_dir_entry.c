#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

typedef struct __attribute__((packed)) directory_entry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8]; // DIR_NTRes, DIR_CrtTimeTenth, DIR_CrtTime, DIR_CrtDate, 
                       // DIR_LstAccDate. Since these fields are not used in
                       // Project 3, just define as a placeholder.
    uint16_t DIR_FstClusHI;
    char padding_2[4]; // DIR_WrtTime, DIR_WrtDate
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;

uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num) {
    uint32_t fat_region_offset = 0x4000;
    return fat_region_offset + clus_num * 4;
}

uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num) {
    uint32_t clus_size = 512;
    uint32_t data_region_offset = 0x100400;
    return data_region_offset + (clus_num - 2) * clus_size;
}

// This is just an example and pseudocode. The real logic may different from
// what is shown here.
// This function returns one directory entry.
dentry_t *encode_dir_entry(int fat32_fd, uint32_t offset) {
    dentry_t *dentry = (dentry_t*)malloc(sizeof(dentry_t));
    ssize_t rd_bytes = pread(fat32_fd, (void*)dentry, sizeof(dentry_t), offset);
    
    // omitted: check rd_bytes == sizeof(dentry_t)

    return dentry;
}

dentry_t *encode_dir_entry(int fd, uint32_t offset) {
    dentry_t *dentry = (dentry_t*)malloc(sizeof(dentry_t));
    ssize_t rd_bytes = pread(fd, (void*)dentry, sizeof(dentry_t), offset);
    
    // omitted: check rd_bytes == sizeof(dentry_t)

    return dentry;
}

// This is just an example and pseudocode. The real logic may different from
// what is shown here.
// This function writes the dentry to the offset.
void write_dir_entry(int fd, dentry_t *dentry, uint32_t offset) {
    uint32_t wr_bytes = pwrite(fd, (void)dentry, sizeof(dentry_t), offset);
    // omitted: check wr_bytes == sizeof(dentry_t)

    return dentry;
}

// This is just an example and pseudocode. The real logic may different from
// what is shown here.
// This function appends a dentry to the cluster in the data region.
void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num) {
    uint32_t curr_clus_num = clus_num;
    uint32_t next_clus_num = 0;
    uint32_t fat_offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num);
    uint32_t data_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num);

    while (/* the condition of valid cluster number */) {

        if (/* check if the offset reaches the end of the data cluster */) {

            // if reaches, go to the next cluster
            // get the next clusters;
            if (/* check if we reach the end of the file/dir */) {
                // if we reach the end, allocate a new cluster number
            }

            // reset the fat_offset;
            // reset the data_offset;
            continue;
        }

        dentry_t *dentry = encode_dir_entry(fd, data_offset);
        if (/* check if this dentry is free */) {
            // if it is free
            write_dir_entry(fd, new_dentry, data_offset);
            break;
        }
    }

}