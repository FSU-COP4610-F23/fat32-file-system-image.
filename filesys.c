#define _POSIX_C_SOURCE 200809L
#define MAX_DEPTH 256
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

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
    uint32_t clusters[MAX_DEPTH];
    int top;
} ClusterStack;

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
    char currentWorkingDir[256];
    uint32_t currentCluster;
    ClusterStack directoryStack;
} FileSystemState;

typedef struct __attribute__((packed)) DirectoryEntry
{
    char name[11];
    uint8_t attributes;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_FstClusLO;

} DirectoryEntry;

void parse_boot_sector(int file, FileSystemState *fsState);
void display_boot_sector_info(const FileSystemState *fsState);
void print_directory_entries(int file, FileSystemState *fsState, uint32_t cluster);
void list_directory(int file, FileSystemState *fsState);
bool find_directory_in_cluster(int fileDescriptor, FileSystemState *fsState, const char *dirName, uint32_t currentCluster, uint32_t *nextCluster);
void pushCluster(ClusterStack *stack, uint32_t cluster);
uint32_t popCluster(ClusterStack *stack);
uint32_t get_next_cluster(int file, FileSystemState *fsState, uint32_t currentCluster);
uint32_t get_cluster_of_path(int fileDescriptor, FileSystemState *fsState, const char *path);
uint32_t get_directory_cluster(int fileDescriptor, FileSystemState *fsState, const char *dirName, uint32_t parentCluster);
void change_directory(FileSystemState *fsState, const char *dirname, int fileDescriptor);
void run_shell(const char *imageName, FileSystemState *fsState, int file);
void initClusterStack(ClusterStack *stack);
void split_name_ext(const char *entryName, char *name, char *ext);
void format_dir_name(const char *input, char *formatted);
bool is_valid_directory_name(const char *name);

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
        close(file);
        return 1;
    }
    memset(fsState, 0, sizeof(FileSystemState));
    initClusterStack(&fsState->directoryStack);                                       // Initialize the directory stack
    strncpy(fsState->currentWorkingDir, "/", sizeof(fsState->currentWorkingDir) - 1); // Initialize to root
    fsState->currentCluster = fsState->bootInfo.BPB_RootClus;

    // parse_boot_sector(file, &bootInfo);
    parse_boot_sector(file, fsState);
    // run_shell(image_path, &bootInfo);
    run_shell(image_path, fsState, file);
    close(file);
    free(fsState);

    return 0;
}

void parse_boot_sector(int file, FileSystemState *fsState)
{
    // Reset boot sector info to zero
    memset(&fsState->bootInfo, 0, sizeof(fsState->bootInfo));

    // Read Bytes Per Sector
    ssize_t rd_bytes = pread(file, &fsState->bootInfo.BPB_BytsPerSec, sizeof(fsState->bootInfo.BPB_BytsPerSec), 11);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_BytsPerSec)) {
        perror("Error reading Bytes Per Sector");
        return;
    }

    // Read Sectors Per Cluster
    rd_bytes = pread(file, &fsState->bootInfo.BPB_SecPerClus, sizeof(fsState->bootInfo.BPB_SecPerClus), 13);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_SecPerClus)) {
        perror("Error reading Sectors Per Cluster");
        return;
    }

    // Read Number of Reserved Sectors
    rd_bytes = pread(file, &fsState->bootInfo.BPB_RsvdSecCnt, sizeof(fsState->bootInfo.BPB_RsvdSecCnt), 14);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RsvdSecCnt)) {
        perror("Error reading Number of Reserved Sectors");
        return;
    }

    // Read Number of FATs
    rd_bytes = pread(file, &fsState->bootInfo.BPB_NumFATs, sizeof(fsState->bootInfo.BPB_NumFATs), 16);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_NumFATs)) {
        perror("Error reading Number of FATs");
        return;
    }

    // Read FAT Size (FAT32)
    rd_bytes = pread(file, &fsState->bootInfo.BPB_FATSz32, sizeof(fsState->bootInfo.BPB_FATSz32), 36);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_FATSz32)) {
        perror("Error reading FAT Size");
        return;
    }

    // Read Total Sectors (FAT32)
    rd_bytes = pread(file, &fsState->bootInfo.BPB_TotSec32, sizeof(fsState->bootInfo.BPB_TotSec32), 32);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_TotSec32)) {
        perror("Error reading Total Sectors");
        return;
    }

    // Read Root Cluster
    rd_bytes = pread(file, &fsState->bootInfo.BPB_RootClus, sizeof(fsState->bootInfo.BPB_RootClus), 44);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RootClus)) {
        perror("Error reading Root Cluster");
        return;
    }

    // Initialize current cluster
    fsState->currentCluster = fsState->bootInfo.BPB_RootClus;

    // Calculate the data region's first sector
    fsState->bootInfo.dataRegionAddress = fsState->bootInfo.BPB_RsvdSecCnt + (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32);

    // Calculate total data sectors
    fsState->bootInfo.dataSec = fsState->bootInfo.BPB_TotSec32 - fsState->bootInfo.dataRegionAddress;

    // Calculate total clusters in data region
    fsState->bootInfo.total_clusters = fsState->bootInfo.dataSec / fsState->bootInfo.BPB_SecPerClus;
}

void display_boot_sector_info(const FileSystemState *fsState)
{
    printf("\nRoot Cluster: %x\n", fsState->bootInfo.rootClusPosition);
    printf("Bytes Per Sector: %u\n", fsState->bootInfo.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", fsState->bootInfo.BPB_SecPerClus);
    printf("Total Clusters in Data Region: %u\n", fsState->bootInfo.total_clusters);
    printf("Entries Per FAT: %u\n", fsState->bootInfo.BPB_FATSz32);
    printf("Image Size: %lld bytes\n", (long long)fsState->bootInfo.image_size);
}

void print_directory_entries(int file, FileSystemState *fsState, uint32_t cluster)
{
    while (cluster < 0x0FFFFFF8)
    {
        for (int sectorOffset = 0; sectorOffset < fsState->bootInfo.BPB_SecPerClus; ++sectorOffset)
        {
            uint32_t sector = ((cluster - 2) * fsState->bootInfo.BPB_SecPerClus) + fsState->bootInfo.BPB_RsvdSecCnt +
                              (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32) + sectorOffset;

            for (int entryOffset = 0; entryOffset < fsState->bootInfo.BPB_BytsPerSec; entryOffset += sizeof(DirectoryEntry))
            {
                off_t offset = sector * fsState->bootInfo.BPB_BytsPerSec + entryOffset;
                if (lseek(file, offset, SEEK_SET) == (off_t)-1)
                {
                    perror("Error seeking directory entry");
                    return;
                }

                DirectoryEntry entry;
                if (read(file, &entry, sizeof(DirectoryEntry)) != sizeof(DirectoryEntry))
                {
                    perror("Error reading directory entry");
                    return;
                }

                if ((unsigned char)entry.name[0] == 0x00)
                {
                    printf("\n"); // End of directory listing
                    return;
                }

                if ((unsigned char)entry.name[0] == 0xE5 || entry.attributes == 0x0F)
                {
                    continue; // Skip deleted or LFN entries
                }

                char name[9], ext[4];
                split_name_ext(entry.name, name, ext);

                if (!isprint((unsigned char)name[0]) || strncmp(name, "xN", 2) == 0 || strncmp(name, "l", 1) == 0)
                {
                    continue; // Skip non-printable names and names starting with xN
                }

                if (ext[0] != '\0')
                {
                    printf("%s.%s ", name, ext); // Print name.extension
                }
                else
                {
                    printf("%s ", name); // Print name only
                }
            }
        }

        // Update the cluster value
        uint32_t FATOffset = cluster * 4;
        uint32_t FATSecNum = fsState->bootInfo.BPB_RsvdSecCnt + (FATOffset / fsState->bootInfo.BPB_BytsPerSec);
        uint32_t FATEntOffset = FATOffset % fsState->bootInfo.BPB_BytsPerSec;

        if (lseek(file, (FATSecNum * fsState->bootInfo.BPB_BytsPerSec) + FATEntOffset, SEEK_SET) == (off_t)-1)
        {
            perror("Error seeking FAT entry");
            return;
        }

        uint32_t FATEntry;
        if (read(file, &FATEntry, sizeof(FATEntry)) != sizeof(FATEntry))
        {
            perror("Error reading FAT entry");
            return;
        }

        cluster = FATEntry & 0x0FFFFFFF;
        if (cluster >= 0x0FFFFFF8)
        {
            perror("Error: end of cluster chain");
            return; // End of cluster chain
        }
    }
}

void split_name_ext(const char *entryName, char *name, char *ext)
{
    int i = 0, j = 0;

    // Extract the name
    while (i < 8 && entryName[i] != ' ')
    {
        name[j++] = entryName[i++];
    }
    name[j] = '\0';

    // Reset counters for extension extraction
    i = 8;
    j = 0;

    // Extract the extension
    while (i < 11 && entryName[i] != ' ')
    {
        ext[j++] = entryName[i++];
    }
    ext[j] = '\0';
}

void list_directory(int file, FileSystemState *fsState) {
    // Use the current directory's cluster
    uint32_t cluster = fsState->currentCluster;
    printf("current cluster %u\n", fsState->currentCluster);
    // Check if the cluster number is valid
    if (cluster < 0x0FFFFFF8) {
        print_directory_entries(file, fsState, cluster);

        // Get the next cluster in the chain from the FAT
        while ((cluster = get_next_cluster(file, fsState, cluster)) < 0x0FFFFFF8) {
            print_directory_entries(file, fsState, cluster);
        }
    } else {
        printf("Error: Invalid cluster number\n");
    }
}

uint32_t get_next_cluster(int file, FileSystemState *fsState, uint32_t currentCluster) {
    uint32_t FATOffset = currentCluster * 4; // Each FAT32 entry is 4 bytes
    uint32_t FATSecNum = fsState->bootInfo.BPB_RsvdSecCnt + (FATOffset / fsState->bootInfo.BPB_BytsPerSec);
    uint32_t FATEntOffset = FATOffset % fsState->bootInfo.BPB_BytsPerSec;

    if (lseek(file, (FATSecNum * fsState->bootInfo.BPB_BytsPerSec) + FATEntOffset, SEEK_SET) == (off_t)-1)
    {
        perror("Error seeking FAT entry");
        return 0; // Error or end of chain
    }

    uint32_t FATEntry;
    if (read(file, &FATEntry, sizeof(FATEntry)) != sizeof(FATEntry))
    {
        perror("Error reading FAT entry");
        return 0; // Error or end of chain
    }

    return FATEntry & 0x0FFFFFFF; // Mask to get the next cluster number
}

// **********************************************************************************************************************
// *           Functions Used for Part 3 - Creation of cd                                                               *
// **********************************************************************************************************************

void change_directory(FileSystemState *fsState, const char *dirname, int fileDescriptor) {
    char formattedDirName[12];
    uint32_t currentCluster = fsState->currentCluster;
    uint32_t nextCluster;

    // Handle root directory case separately // remember to change .. to got to previous directory
    if (strcmp(dirname, "/") == 0 || strcmp(dirname, "..") == 0 ) {
        strcpy(fsState->currentWorkingDir, "/");
        fsState->currentCluster = fsState->bootInfo.BPB_RootClus;
        return;
    }

    // Format the directory name to FAT32 8.3 format
    format_dir_name(dirname, formattedDirName);

    if (find_directory_in_cluster(fileDescriptor, fsState, formattedDirName, currentCluster, &nextCluster)) {
        // Update the current working directory
        if (fsState->currentWorkingDir[strlen(fsState->currentWorkingDir) - 1] != '/') {
            strcat(fsState->currentWorkingDir, "/");
        }
        strcat(fsState->currentWorkingDir, dirname);

        // Update the current cluster
        fsState->currentCluster = nextCluster;
        printf("change_directory Changed directory to '%s', new cluster: %u\n", fsState->currentWorkingDir, fsState->currentCluster);
    } else {
        printf("Directory '%s' not found\n", dirname);
    }
}


uint32_t get_cluster_of_path(int fileDescriptor, FileSystemState *fsState, const char *path)
{
    uint32_t currentCluster;
    char formattedPath[256];
    char *token;

    // Check if the path is absolute or relative
    if (path[0] == '/')
    {
        currentCluster = fsState->bootInfo.BPB_RootClus;
    }
    else
    {
        currentCluster = fsState->currentCluster;
    }

    strncpy(formattedPath, path, sizeof(formattedPath));
    token = strtok(formattedPath, "/");

    while (token != NULL)
    {
        if (strcmp(token, ".") == 0)
        {
            // Current directory, no action needed
        }
        else if (strcmp(token, "..") == 0)
        {
            if (currentCluster != fsState->bootInfo.BPB_RootClus)
            {
                currentCluster = popCluster(&fsState->directoryStack);
            }
        }
        else
        {
            uint32_t nextCluster;
            char formattedToken[12];
            format_dir_name(token, formattedToken);

            if (find_directory_in_cluster(fileDescriptor, fsState, formattedToken, currentCluster, &nextCluster))
            {
                pushCluster(&fsState->directoryStack, currentCluster); 
                currentCluster = nextCluster;
            }
            else
            {
                printf("Directory '%s' not found in path '%s'\n", token, path);
                return 0;
            }
        }
        token = strtok(NULL, "/");
    }

    return currentCluster;
}

bool find_directory_in_cluster(int fileDescriptor, FileSystemState *fsState, const char *dirName, uint32_t currentCluster, uint32_t *nextCluster)
{
    char formattedDirName[12];
    format_dir_name(dirName, formattedDirName); // Format the input directory name for FAT32 comparison
    currentCluster = fsState->currentCluster; 
    while (currentCluster < 0x0FFFFFF8)
    {
        printf("find_directory_in_cluster Inspecting cluster: %u\n", currentCluster);

        uint32_t sector = ((currentCluster - 2) * fsState->bootInfo.BPB_SecPerClus) + fsState->bootInfo.BPB_RsvdSecCnt +
                          (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32);

        for (int i = 0; i < fsState->bootInfo.BPB_SecPerClus; ++i)
        {
            off_t offset = (sector + i) * fsState->bootInfo.BPB_BytsPerSec;
            lseek(fileDescriptor, offset, SEEK_SET);

            for (int j = 0; j < fsState->bootInfo.BPB_BytsPerSec; j += sizeof(DirectoryEntry))
            {
                DirectoryEntry entry;
                ssize_t read_bytes = read(fileDescriptor, &entry, sizeof(DirectoryEntry));
                if (read_bytes != sizeof(DirectoryEntry)) { break; }

                if (entry.name[0] == 0x00 || entry.name[0] == 0xE5) { continue; } // End or deleted entry

                char formattedEntryName[12];
                format_dir_name(entry.name, formattedEntryName);
                // printf("find_directory_in_cluster Comparing '%s' with directory entry '%s'\n", formattedDirName, formattedEntryName);


                if ((entry.attributes & 0x10) && strcasecmp(formattedEntryName, formattedDirName) == 0) // Case-insensitive comparison
                {
                    *nextCluster = (entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;
                    return true;
                }
            }
        }

        currentCluster = get_next_cluster(fileDescriptor, fsState, currentCluster);
        if (currentCluster == 0) { break; } // Error or end of chain
    }

    printf("find_directory_in_cluster Directory '%s' not found in cluster: %u\n", dirName, currentCluster);
    return false;
}


void initClusterStack(ClusterStack *stack)
{
    stack->top = -1;
}

void format_dir_name(const char *input, char *formatted) {
    memset(formatted, ' ', 11); // Fill with spaces as per FAT32 8.3 format
    formatted[11] = '\0'; // Null-terminate for safety

    int i, j;
    // Convert name part
    for (i = 0, j = 0; i < 8 && input[i] != '\0' && input[i] != '.'; ++i, ++j) {
        formatted[j] = toupper((unsigned char)input[i]);
    }

    // Skip to extension in input
    while (input[i] && input[i] != '.') i++;
    if (input[i] == '.') i++;

    // Convert extension part
    for (j = 8; j < 11 && input[i] != '\0'; ++i, ++j) {
        formatted[j] = toupper((unsigned char)input[i]);
    }

    // printf("format_dir_name Formatted directory name: '%s'\n", formatted); // Debug print
}

uint32_t get_directory_cluster(int fileDescriptor, FileSystemState *fsState, const char *dirName, uint32_t parentCluster)
{
    DirectoryEntry entry;
    char formattedDirName[12];
    format_dir_name(dirName, formattedDirName); // Format the input directory name for FAT32 comparison

    uint32_t sector, offset;
    // Iterate over each sector in the cluster
    for (int sectorOffset = 0; sectorOffset < fsState->bootInfo.BPB_SecPerClus; ++sectorOffset)
    {
        sector = ((parentCluster - 2) * fsState->bootInfo.BPB_SecPerClus) + fsState->bootInfo.BPB_RsvdSecCnt +
                 (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32) + sectorOffset;

        // Iterate over each entry in the sector
        for (offset = 0; offset < fsState->bootInfo.BPB_BytsPerSec; offset += sizeof(DirectoryEntry))
        {
            lseek(fileDescriptor, sector * fsState->bootInfo.BPB_BytsPerSec + offset, SEEK_SET);
            if (read(fileDescriptor, &entry, sizeof(DirectoryEntry)) != sizeof(DirectoryEntry))
            {
                perror("get_directory_cluster Error reading directory entry");
                return 0; // Error reading directory entry
            }

            // Check for end of directory
            if (entry.name[0] == 0x00)
            {
                return 0; // Directory not found
            }

            if ((unsigned char)entry.name[0] == 0xE5)
            {
                continue; // Skip deleted entries
            }

            // Format the entry name for comparison
            char formattedEntryName[12];
            format_dir_name(entry.name, formattedEntryName);

            // Check if the entry is a directory and matches dirName
            if ((entry.attributes & 0x10) && strcmp(formattedEntryName, formattedDirName) == 0)
            {
                // Found the directory, return its first cluster number
                return (entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;
            }
        }
    }
    return 0; // Directory not found
}

bool is_valid_directory_name(const char *name)
{
    // Check for valid characters, length, and other criteria
    int nameLength = strlen(name);

    // Check if the name is empty or too long
    if (nameLength == 0 || nameLength > 11)
    {
        return false;
    }

    // Check if the name contains only valid characters
    for (int i = 0; i < nameLength; i++)
    {
        char currentChar = name[i];

        // Valid characters are letters, digits, and some special characters
        if (!(isalnum(currentChar) || currentChar == ' ' || currentChar == '_'))
        {
            return false;
        }
    }

    // Check if the name starts with a space or ends with a space
    if (name[0] == ' ' || name[nameLength - 1] == ' ')
    {
        return false;
    }

    // Additional criteria can be added based on your requirements

    // If all checks pass, the name is considered valid
    return true;
}

void pushCluster(ClusterStack *stack, uint32_t cluster)
{
    if (stack->top < MAX_DEPTH - 1)
    {
        stack->clusters[++stack->top] = cluster;
    }
    else
    {
        // Handle stack overflow
    }
}

uint32_t popCluster(ClusterStack *stack)
{
    if (stack->top >= 0)
    {
        return stack->clusters[stack->top--];
    }
    else
    {
        // Handle stack underflow
        return 0; // Return an invalid cluster number or a designated error code
    }
}

void run_shell(const char *imageName, FileSystemState *fsState, int file)
{
    char command[100];
    while (1)
    {
        // printf("[%s]/> ", imageName);
        printf("[%s]%s> ", imageName, fsState->currentWorkingDir);
        scanf("%99s", command);

        if (strcmp(command, "exit") == 0)
        {
            break;
        }
        else if (strcmp(command, "info") == 0)
        {
            display_boot_sector_info(fsState);
        }
        else if (strcmp(command, "cd") == 0)
        {
            char dirname[256];
            scanf("%255s", dirname);
            change_directory(fsState, dirname, file);
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