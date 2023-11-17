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

void dbg_print_dentry(dentry_t *dentry) {
    if (dentry == NULL) {
        return ;
    }

    printf("DIR_Name: %s\n", dentry->DIR_Name);
    printf("DIR_Attr: 0x%x\n", dentry->DIR_Attr);
    printf("DIR_FstClusHI: 0x%x\n", dentry->DIR_FstClusHI);
    printf("DIR_FstClusLO: 0x%x\n", dentry->DIR_FstClusLO);
    printf("DIR_FileSize: %u\n", dentry->DIR_FileSize);
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

int main(int argc, char const *argv[]) {
    int fd = open("fat32.img", O_RDONLY);
    if (fd < 0) {
        perror("open fat32.img failed");
        return 0;
    }

    uint32_t offset = 0x100420;

    dentry_t *dentry = encode_dir_entry(fd, offset);
    dbg_print_dentry(dentry);

    free(dentry);
    close(fd);

    return 0;
}

