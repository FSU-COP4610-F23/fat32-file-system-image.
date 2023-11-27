#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

// Define the structure for the boot sector information
typedef struct __attribute__((packed)) BPB
{

    int dataRegionAddress; // address to data region
    int dataSec;           // sectors in data region
    int rootClusPosition;  // address to root cluster/directory
    int rootDirSectors;    // sectors in root directory

    off_t image_size;

    uint32_t BPB_RootClus;   // print
    uint16_t BPB_BytsPerSec; // print
    uint8_t BPB_SecPerClus;  // print
    uint16_t BPB_RsvdSecCnt;
    uint32_t BPB_TotSec32;
    uint16_t BPB_RootEntCnt;
    unsigned int total_clusters; // print
    uint8_t BPB_NumFATs;
    uint32_t BPB_FATSz32; // print
} boot_sector_info;

typedef struct
{
    char filename[256];
    int file_descriptor;
    char mode[3];
    off_t offset;
} OpenedFile;

typedef struct
{
    boot_sector_info bootInfo;
    OpenedFile openedFiles[100];
    int openedFilesCount;
} FileSystemState;

typedef struct __attribute__((packed)) DirectoryEntry
{
    char name[11];
    uint8_t attributes;
} DirectoryEntry;

// Function prototypes
// void parse_boot_sector(int file, boot_sector_info *info);
void parse_boot_sector(int file, FileSystemState *fsState);
// void display_boot_sector_info(const boot_sector_info *info);
void display_boot_sector_info(const FileSystemState *fsState);
// void run_shell(const char *imageName, boot_sector_info *info);
void run_shell(const char *imageName, FileSystemState *fsState, int file);

void print_directory_entries(int file, FileSystemState *fsState, uint32_t cluster);
void list_directory(int file, FileSystemState *fsState);


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: ./filesys [FAT32 ISO]\n");
        return 1;
    }

    const char *image_path = argv[1];
    int file = open(image_path, O_RDONLY);
    if (file < 0)
    {
        perror("Error opening image file");
        return 1;
    }

    // boot_sector_info bootInfo;
    // FileSystemState *fsState;
    // memset(&fsState, 0, sizeof(fsState));

    FileSystemState *fsState = malloc(sizeof(FileSystemState));
    if (fsState == NULL)
    {
        perror("Failed to allocate memory for FileSystemState");
        return 1;
    }
    memset(fsState, 0, sizeof(FileSystemState));

    // parse_boot_sector(file, &bootInfo);
    parse_boot_sector(file, fsState);
    // run_shell(image_path, &bootInfo);
    run_shell(image_path, fsState, file);
    close(file);
    free(fsState);
    return 0;
}

// void parse_boot_sector(int file, boot_sector_info *info)
void parse_boot_sector(int file, FileSystemState *fsState)
{

    // Read and parse the boot sector
    // These are fake values, we will need to change these
    // Example value

    // info->BPB_RootClus = 0;
    fsState->bootInfo.BPB_RootClus = 0;
    ssize_t rd_bytes = pread(file, (void *)&fsState->bootInfo.BPB_RootClus, sizeof(fsState->bootInfo.BPB_RootClus), 44);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_RootClus)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RootClus))
    {
        printf("(1) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_RootClus), rd_bytes);
        close(file);
        return;
    } // printf("BPB_RootClus: %u\n", fsState->bootInfo.BPB_RootClus);

    // info->BPB_RsvdSecCnt = 0;
    fsState->bootInfo.BPB_RsvdSecCnt = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_RsvdSecCnt, sizeof(fsState->bootInfo.BPB_RsvdSecCnt), 14);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_RsvdSecCnt)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RsvdSecCnt))
    {
        printf("(2) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_RsvdSecCnt), rd_bytes);
        close(file);
        return;
    } // printf("BPB_RsvdSecCnt: %u\n", fsState->bootInfo.BPB_RsvdSecCnt);

    // info->BPB_BytsPerSec = 0;
    fsState->bootInfo.BPB_BytsPerSec = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_BytsPerSec, sizeof(fsState->bootInfo.BPB_BytsPerSec), 11);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_BytsPerSec)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_BytsPerSec))
    {
        printf("(3) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_BytsPerSec), rd_bytes);
        close(file);
        return;
    } // printf("BPB_BytsPerSec: %u\n", fsState->bootInfo.BPB_BytsPerSec);

    // fsState->bootInfo.BPB_SecPerClus = 0;
    fsState->bootInfo.BPB_SecPerClus = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_SecPerClus, sizeof(fsState->bootInfo.BPB_SecPerClus), 13);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_SecPerClus)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_SecPerClus))
    {
        printf("(4) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_SecPerClus), rd_bytes);
        close(file);
        return;
    } // printf("BPB_SecPerClus: %u\n", fsState->bootInfo.BPB_SecPerClus);

    // info->BPB_TotSec32 = 0;
    fsState->bootInfo.BPB_TotSec32 = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_TotSec32, sizeof(fsState->bootInfo.BPB_TotSec32), 32);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_TotSec32)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_TotSec32))
    {
        printf("(5) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_TotSec32), rd_bytes);
        close(file);
        return;
    } // printf("BPB_TotSec32: %u\n", fsState->bootInfo.BPB_TotSec32);

    // info->BPB_RootEntCnt = 0;
    fsState->bootInfo.BPB_RootEntCnt = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_RootEntCnt, sizeof(fsState->bootInfo.BPB_RootEntCnt), 17);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_RootEntCnt)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RootEntCnt))
    {
        printf("(6) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_RootEntCnt), rd_bytes);
        close(file);
        return;
    } // printf("BPB_TotSec32: %u\n", fsState->bootInfo.BPB_RootEntCnt);

    fsState->bootInfo.total_clusters = 0; // Example value

    // info->BPB_NumFATs = 0;
    fsState->bootInfo.BPB_NumFATs = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_NumFATs, sizeof(fsState->bootInfo.BPB_NumFATs), 16);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_FATSz32)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_NumFATs))
    {
        printf("(7) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_NumFATs), rd_bytes);
        close(file);
        return;
    } // printf("BPB_NumFATs: %u\n", fsState->bootInfo.BPB_NumFATs);

    // fsState->bootInfo.BPB_FATSz32 = 0;
    fsState->bootInfo.BPB_FATSz32 = 0;
    rd_bytes = pread(file, &fsState->bootInfo.BPB_FATSz32, sizeof(fsState->bootInfo.BPB_FATSz32), 36);
    // check if rd_bytes == sizeof(fsState->bootInfo.BPB_FATSz32)
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_FATSz32))
    {
        printf("(8) request %lu bytes, but read %ld bytes\n", sizeof(fsState->bootInfo.BPB_FATSz32), rd_bytes);
        close(file);
        return;
    } // printf("BPB_FATSz32: %u\n", fsState->bootInfo.BPB_FATSz32);

    fsState->bootInfo.dataRegionAddress = fsState->bootInfo.BPB_BytsPerSec * fsState->bootInfo.BPB_RsvdSecCnt + // in decimal expression
                                          fsState->bootInfo.BPB_FATSz32 * fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_BytsPerSec;

    // should be same as dataRegionAddress using formula
    fsState->bootInfo.rootClusPosition = fsState->bootInfo.dataRegionAddress + fsState->bootInfo.BPB_BytsPerSec * (fsState->bootInfo.BPB_RootClus - 2);

    // sectors in root directory
    fsState->bootInfo.rootDirSectors = ((fsState->bootInfo.BPB_RootEntCnt * 32) + (fsState->bootInfo.BPB_BytsPerSec - 1)) / fsState->bootInfo.BPB_BytsPerSec;
    // sectors in data region
    fsState->bootInfo.dataSec = fsState->bootInfo.BPB_TotSec32 - (fsState->bootInfo.BPB_RsvdSecCnt + (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32) + fsState->bootInfo.rootDirSectors);
    // count of clusters in data region
    fsState->bootInfo.total_clusters = fsState->bootInfo.dataSec / fsState->bootInfo.BPB_SecPerClus;
    // size of image disk
    fsState->bootInfo.image_size = lseek(file, 0, SEEK_END);
}

// void display_boot_sector_info(const boot_sector_info *info)
void display_boot_sector_info(const FileSystemState *fsState)
{
    printf("\nRoot Cluster: %x\n", fsState->bootInfo.rootClusPosition);
    printf("Bytes Per Sector: %u\n", fsState->bootInfo.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", fsState->bootInfo.BPB_SecPerClus);
    printf("Total Clusters in Data Region: %u\n", fsState->bootInfo.total_clusters);
    printf("Entries Per FAT: %u\n", fsState->bootInfo.BPB_FATSz32);
    printf("Image Size: %lld bytes\n", (long long)fsState->bootInfo.image_size);
}



// hereeeeeeeeeeeeeeee
// 12:58 am - im assuming its this function breaking ls but fuuuuuuuuuuuuuuuuuuuuuuuuu
void print_directory_entries(int file, FileSystemState *fsState, uint32_t cluster) {
     while (cluster < 0x0FFFFFF8) {
        uint32_t sector = fsState->bootInfo.rootClusPosition + (cluster - 2) * fsState->bootInfo.BPB_SecPerClus;
        for (int i = 0; i < fsState->bootInfo.BPB_SecPerClus; ++i) {
            off_t offset = sector * fsState->bootInfo.BPB_BytsPerSec + i * sizeof(DirectoryEntry);
            lseek(file, offset, SEEK_SET);

            DirectoryEntry entry;
            ssize_t read_bytes = read(file, &entry, sizeof(DirectoryEntry));
            if (read_bytes != sizeof(DirectoryEntry)) {
                printf("Error reading directory entry\n");
                break;
            }

            if (entry.name[0] == 0x00 || entry.name[0] == 0xE5 || entry.attributes == 0x0F) {
                continue; // Skip empty or deleted entries
            }

            printf("Name: %s\n", entry.name);
            printf("Attributes: %u\n", entry.attributes);
        }

        // Interpret FAT entries to update the cluster value
        uint32_t FATOffset = cluster * 4 / fsState->bootInfo.BPB_BytsPerSec;
        uint32_t FATSector = fsState->bootInfo.dataRegionAddress + FATOffset;
        lseek(file, FATSector * fsState->bootInfo.BPB_BytsPerSec, SEEK_SET);

        uint32_t FATEntry;
        ssize_t read_bytes = read(file, &FATEntry, sizeof(FATEntry));
        if (read_bytes != sizeof(FATEntry)) {
            printf("Error reading FAT entry\n");
            break;
        }

        cluster = FATEntry & 0x0FFFFFFF;

        // Check for end-of-chain marker to exit the loop
        if (cluster >= 0x0FFFFFF8) {
            break;
        }


        /*
            Take 2 omg

        while (cluster < 0x0FFFFFF8) {
        uint32_t sector = fsState->bootInfo.rootClusPosition + (cluster - 2) * fsState->bootInfo.BPB_SecPerClus;
        for (int i = 0; i < fsState->bootInfo.BPB_SecPerClus; ++i) {
            off_t offset = sector * fsState->bootInfo.BPB_BytsPerSec + i * sizeof(DirectoryEntry);
            lseek(file, offset, SEEK_SET);

            DirectoryEntry entry;
            ssize_t read_bytes = read(file, &entry, sizeof(DirectoryEntry));
            if (read_bytes != sizeof(DirectoryEntry)) {
                printf("Error reading directory entry\n");
                break;
            }

            if (entry.name[0] == 0x00 || entry.name[0] == 0xE5 || entry.attributes == 0x0F) {
                continue; // Skip empty or deleted entries
            }

            printf("Name: %s\n", entry.name);
            printf("Attributes: %u\n", entry.attributes);
        }

        // Read directory entry
        // Parse and print the directory entry details

    */


    /*

    TAKE 1 

    uint32_t sector = fsState->bootInfo.dataRegionAddress + (cluster - 2) * fsState->bootInfo.BPB_SecPerClus;

    for (int i = 0; i < fsState->bootInfo.BPB_SecPerClus; ++i) {
        off_t offset = sector * fsState->bootInfo.BPB_BytsPerSec + i * fsState->bootInfo.BPB_BytsPerSec;
        lseek(file, offset, SEEK_SET);

         DirectoryEntry entry;
        ssize_t read_bytes = read(file, &entry, sizeof(DirectoryEntry));
        if (read_bytes != sizeof(DirectoryEntry)) {
            printf("Error reading directory entry\n");
            break;
        }

        // Interpret the directory entry fields and print relevant information
        if (entry.name[0] == 0x00 || entry.name[0] == 0xE5 || entry.attributes == 0x0F) {
            // Skip empty or deleted entries
            continue;
        }

        // Print directory entry details
        printf("Name: %s\n", entry.name);
        printf("Attributes: %u\n", entry.attributes);

        // Read directory entry
        // Parse and print the directory entry details
    
    }
        */
    } 
}

void list_directory(int file, FileSystemState *fsState) {
    uint32_t cluster = fsState->bootInfo.rootClusPosition;
    while (cluster < 0x0FFFFFF8) {
        print_directory_entries(file, fsState, cluster);
        // Follow the cluster chain
        // Update 'cluster' to the next cluster in the chain
    }
}



// void run_shell(const char *imageName, boot_sector_info *info)
void run_shell(const char *imageName, FileSystemState *fsState, int file)
{
    char command[100];
    while (1)
    {
        printf("[%s]/> ", imageName);
        scanf("%s", command);

        if (strcmp(command, "exit") == 0)
        {
            break;
        }
        else if (strcmp(command, "info") == 0)
        {
            // display_boot_sector_info(info);
            display_boot_sector_info(fsState);
        }
        else if (strcmp(command, "ls") == 0)
        {
            list_directory(file, fsState);
        }
        else if (strcmp(command, "open") == 0)
        {
            char filename[256];
            char mode[3];
            scanf("%s %s", filename, mode);

            int already_opened = 0;
            for (int i = 0; i < fsState->openedFilesCount; i++)
            {
                if (strcmp(fsState->openedFiles[i].filename, filename) == 0)
                {
                    printf("Error: file already opened\n");
                    already_opened = 1;
                    break;
                }
            }

            if (!already_opened)
            {
                int flags = 0;
                if (strcmp(mode, "-r") == 0)
                    flags = O_RDONLY;
                else if (strcmp(mode, "-w") == 0)
                    flags = O_WRONLY;
                else if (strcmp(mode, "-rw") == 0 || strcmp(mode, "-wr") == 0)
                    flags = O_RDWR;
                else
                {
                    printf("Error: Invalid mode\n");
                    continue;
                }

                int fd = open(filename, flags);
                if (fd == -1)
                {
                    perror("Error opening file");
                    continue;
                }

                // Add to the openedFiles array
                strncpy(fsState->openedFiles[fsState->openedFilesCount].filename, filename, sizeof(fsState->openedFiles[fsState->openedFilesCount].filename));
                fsState->openedFiles[fsState->openedFilesCount].file_descriptor = fd;
                strncpy(fsState->openedFiles[fsState->openedFilesCount].mode, mode, sizeof(fsState->openedFiles[fsState->openedFilesCount].mode));
                fsState->openedFiles[fsState->openedFilesCount].offset = 0;
                fsState->openedFilesCount++;
            }
        }
        else if (strcmp(command, "close") == 0)
        {
            char filename[256];
            scanf("%s", filename);
            int found = 0;
            for (int i = 0; i < fsState->openedFilesCount; i++)
            {
                if (strcmp(fsState->openedFiles[i].filename, filename) == 0)
                {
                    close(fsState->openedFiles[i].file_descriptor);
                    for (int j = i; j < fsState->openedFilesCount - 1; j++)
                    {
                        fsState->openedFiles[j] = fsState->openedFiles[j + 1];
                    }
                    fsState->openedFilesCount--;
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                printf("Error: File not opened or does not exist\n");
            }
        }
        else if (strcmp(command, "lsof") == 0)
        {
            if (fsState->openedFilesCount == 0)
            {
                printf("No files are opened\n");
            }
            else
            {
                for (int i = 0; i < fsState->openedFilesCount; i++)
                {
                    printf("Index: %d, Filename: %s, Mode: %s, Offset: %ld\n",
                           i, fsState->openedFiles[i].filename, fsState->openedFiles[i].mode, fsState->openedFiles[i].offset);
                }
            }
        }
        else if (strcmp(command, "lssek") == 0)
        {
            char filename[256];
            off_t offset;
            scanf("%s %ld", filename, &offset);
            int found = 0;
            for (int i = 0; i < fsState->openedFilesCount; i++)
            {
                if (strcmp(fsState->openedFiles[i].filename, filename) == 0)
                {
                    if (offset >= 0 && offset <= lseek(fsState->openedFiles[i].file_descriptor, 0, SEEK_END))
                    {
                        fsState->openedFiles[i].offset = offset;
                        found = 1;
                    }
                    else
                    {
                        printf("Error: Invalid offset\n");
                    }
                    break;
                }
            }
            if (!found)
            {
                printf("Error: File not opened or does not exist\n");
            }
        }
        else if (strcmp(command, "read") == 0)
        {
            char filename[256];
            size_t size;
            scanf("%s %zu", filename, &size);
            int found = 0;
            for (int i = 0; i < fsState->openedFilesCount; i++)
            {
                if (strcmp(fsState->openedFiles[i].filename, filename) == 0)
                {
                    if (strcmp(fsState->openedFiles[i].mode, "-r") == 0 || strcmp(fsState->openedFiles[i].mode, "-rw") == 0 || strcmp(fsState->openedFiles[i].mode, "-wr") == 0)
                    {
                        char *buffer = malloc(size);
                        if (buffer == NULL)
                        {
                            printf("Error: Memory allocation failed\n");
                            break;
                        }
                        ssize_t read_bytes = pread(fsState->openedFiles[i].file_descriptor, buffer, size, fsState->openedFiles[i].offset);
                        if (read_bytes >= 0)
                        {
                            fsState->openedFiles[i].offset += read_bytes;
                            buffer[read_bytes] = '\0';
                            printf("%s\n", buffer);
                        }
                        else
                        {
                            printf("Error reading file\n");
                        }
                        free(buffer);
                        found = 1;
                        break;
                    }
                    else
                    {
                        printf("Error: File not opened for reading\n");
                        break;
                    }
                }
            }
            if (!found)
            {
                printf("Error: File not opened or does not exist\n");
            }
        }
        else
        {
            printf("Unknown command: %s\n", command);
        }
    }
}