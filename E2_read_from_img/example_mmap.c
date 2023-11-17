#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
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
	int fd;
	struct stat st;
    char *map = NULL;
    char *ptr = NULL;

    // open the file in read only
    fd = open("fat32.img", O_RDONLY);
    if (fd < 0) {
        perror("Error opening file failed: ");
        return 1;
    }

    // get the file size
    if (fstat(fd, &st) == -1) {
        perror("Error getting file size");
        return 1;
    }

    // mmap the file
    map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        perror("Error mapping the file");
        return 1;
    }

    // 1. convert byte to uint16_t
    BPB_BytsPerSec = 0;
    BPB_BytsPerSec = (uint16_t)(map[11]) + (uint16_t)(map[12] << 8);
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 2. memory copy to uint16_t
    BPB_BytsPerSec = 0;
    ptr = map + 11;
	memcpy(&BPB_BytsPerSec, ptr, sizeof(uint16_t));    
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 3. cast ptr to uint16_t ptr
    BPB_BytsPerSec = 0;
    ptr = map + 11;
    BPB_BytsPerSec = *(uint16_t*)ptr;
    printf("BPB_BytsPerSec: %u\n", BPB_BytsPerSec);

    // 4. use ptr to access the int directly
    ptr = map + 11;
    printf("BPB_BytsPerSec: %u\n", *(uint16_t*)ptr);

    // 5. use ptr to access the int directly
    uint16_t *BPB_BytsPerSec_ptr = NULL;
    BPB_BytsPerSec_ptr = (uint16_t*)(map + 11);
    printf("BPB_BytsPerSec: %u\n", *BPB_BytsPerSec_ptr);

    // 6. use struct pointer to access the data
    bpb_t *bpb_ptr = NULL;
    bpb_ptr = (bpb_t*)map;
    printf("BPB_BytsPerSec: %u\n", bpb_ptr->BPB_BytsPerSec);

    // Unmap the memory and close the file
    if (munmap(map, st.st_size) == -1) {
        perror("Error unmapping the file");
    }
    close(fd);
}