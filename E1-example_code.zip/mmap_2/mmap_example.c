#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

typedef struct __attribute__((packed)){
    unsigned char DIR_Name[8];             // 8 bytes (1 byte read), this representation is fine as it is character string.
    uint16_t DIR_FstClusHi;          // 2 bytes (2 byte read), this representation is in high word of first clus
    uint32_t DIR_FileSize;             // 4 bytes (4 byte read), this representation is size of file.
    uint16_t DIR_FstClusLo;          // 2 bytes (2 byte read), this representation is in low word of first clus.
} DIR;

int main() {
    int fd;
    char *map = NULL;
    struct stat st;
    DIR *dentry = NULL;

    // Prepare a file
    fd = open("entries", O_RDWR);
    if (fd == -1) {
        perror("Error opening file failed: ");
        return 1;
    }

    // Get the file size
    if (fstat(fd, &st) == -1) {
        perror("Error getting file size");
        return 1;
    }

    // Map the file to memory
    map = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        perror("Error mapping the file");
        return 1;
    }

    // Let the DIR's pointer that points to the memory mapped address
    dentry = (DIR*)map;
    printf("dir_name: %s, clusHi: %u, fileSize: %u, clusLo: %u\n",
           dentry->DIR_Name, dentry->DIR_FstClusHi,
           dentry->DIR_FileSize, dentry->DIR_FstClusLo);

    // Update the file name
    if (memcmp(dentry->DIR_Name, "LONGFILE", 8) == 0) {
        memcpy(dentry->DIR_Name, "SHORTFNM", 8);
    } else {
        memcpy(dentry->DIR_Name, "LONGFILE", 8);
    }

    // Synchronize changes to the file
    if (msync(map, st.st_size, MS_SYNC) == -1) {
        perror("Error syncing the file");
    }

    // Unmap the memory and close the file
    if (munmap(map, st.st_size) == -1) {
        perror("Error unmapping the file");
    }
    close(fd);

    return 0;
}
