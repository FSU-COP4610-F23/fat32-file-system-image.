#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd;
    char *map;
    struct stat st;

    // Prepare a file
    fd = open("example.txt", O_RDWR | O_CREAT, S_IRWXU);
    if (fd == -1) {
        perror("Error opening file for writing");
        return 1;
    }

        char *init_msg = "a123456789b123456789c123456789d123456789e123456789";
    pwrite(fd, init_msg, strlen(init_msg), 0);

        close(fd);

    // Open the file
    fd = open("example.txt", O_RDWR);
    if (fd == -1) {
        perror("Error opening file for reading");
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

    // Read from the memory-mapped file
    printf("Contents of the file: %.*s\n", (int)st.st_size, map);

    // Modify the contents of the memory-mapped file
    char *message = "Hello, World!";
        int offset = 10;
    memcpy(map + offset, message, strlen(message));

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
