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
    uint32_t firstCluster;
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
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirectoryEntry;

void parse_boot_sector(int file, FileSystemState *fsState);
void display_boot_sector_info(const FileSystemState *fsState);
void print_directory_entries(int file, FileSystemState *fsState);
void list_directory(int file, FileSystemState *fsState);
bool find_directory_in_cluster(int fileDescriptor, FileSystemState *fsState, const char *dirName, uint32_t *nextCluster);
bool find_file_in_cluster(int fileDescriptor, FileSystemState *fsState, const char *fileName, uint32_t *firstCluster);
uint32_t get_next_cluster(int file, FileSystemState *fsState);
void change_directory(FileSystemState *fsState, const char *dirname, int fileDescriptor);
void run_shell(const char *imageName, FileSystemState *fsState, int file);
void split_name_ext(const char *entryName, char *name, char *ext);
void format_dir_name(const char *input, char *formatted);
void push_cluster(ClusterStack *stack, uint32_t cluster);
void add_to_opened_files(FileSystemState *fsState, const char *filename, uint32_t firstCluster, int fd, const char *mode);
uint32_t pop_cluster(ClusterStack *stack);
int determine_open_flags(const char *mode);

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

    FileSystemState *fsState = malloc(sizeof(FileSystemState));
    if (fsState == NULL)
    {
        perror("Failed to allocate memory for FileSystemState");
        close(file);
        return 1;
    }
    memset(fsState, 0, sizeof(FileSystemState));
    strncpy(fsState->currentWorkingDir, "/", sizeof(fsState->currentWorkingDir) - 1); // Initialize to root

    parse_boot_sector(file, fsState);

    // Set the current cluster to the root cluster after parsing the boot sector
    fsState->currentCluster = fsState->bootInfo.BPB_RootClus;
    fsState->directoryStack.top = -1;
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
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_BytsPerSec))
    {
        perror("Error reading Bytes Per Sector");
        return;
    }

    // Read Sectors Per Cluster
    rd_bytes = pread(file, &fsState->bootInfo.BPB_SecPerClus, sizeof(fsState->bootInfo.BPB_SecPerClus), 13);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_SecPerClus))
    {
        perror("Error reading Sectors Per Cluster");
        return;
    }

    // Read Number of Reserved Sectors
    rd_bytes = pread(file, &fsState->bootInfo.BPB_RsvdSecCnt, sizeof(fsState->bootInfo.BPB_RsvdSecCnt), 14);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RsvdSecCnt))
    {
        perror("Error reading Number of Reserved Sectors");
        return;
    }

    // Read Number of FATs
    rd_bytes = pread(file, &fsState->bootInfo.BPB_NumFATs, sizeof(fsState->bootInfo.BPB_NumFATs), 16);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_NumFATs))
    {
        perror("Error reading Number of FATs");
        return;
    }

    // Read FAT Size (FAT32)
    rd_bytes = pread(file, &fsState->bootInfo.BPB_FATSz32, sizeof(fsState->bootInfo.BPB_FATSz32), 36);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_FATSz32))
    {
        perror("Error reading FAT Size");
        return;
    }

    // Read Total Sectors (FAT32)
    rd_bytes = pread(file, &fsState->bootInfo.BPB_TotSec32, sizeof(fsState->bootInfo.BPB_TotSec32), 32);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_TotSec32))
    {
        perror("Error reading Total Sectors");
        return;
    }

    // Read Root Cluster
    rd_bytes = pread(file, &fsState->bootInfo.BPB_RootClus, sizeof(fsState->bootInfo.BPB_RootClus), 44);
    if (rd_bytes != sizeof(fsState->bootInfo.BPB_RootClus))
    {
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

void print_directory_entries(int file, FileSystemState *fsState)
{
    while (fsState->currentCluster < 0x0FFFFFF8)
    {
        for (int sectorOffset = 0; sectorOffset < fsState->bootInfo.BPB_SecPerClus; ++sectorOffset)
        {
            uint32_t sector = ((fsState->currentCluster - 2) * fsState->bootInfo.BPB_SecPerClus) + fsState->bootInfo.BPB_RsvdSecCnt +
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

                if ((unsigned char)entry.DIR_Name[0] == 0x00)
                {
                    printf("\n"); // End of directory listing
                    return;
                }

                if ((unsigned char)entry.DIR_Name[0] == 0xE5 || entry.DIR_Attr == 0x0F)
                {
                    continue; // Skip deleted or LFN entries
                }

                char name[9], ext[4];
                split_name_ext(entry.DIR_Name, name, ext);

                if (!isprint((unsigned char)name[0]) || strncmp(name, "xN", 2) == 0 || strncmp(name, "l", 1) == 0)
                {
                    continue; // Skip non-printable names and names starting with xN
                }

                if (entry.DIR_Attr & 0x10)
                {
                    // Directory: Blue color
                    printf("\033[1;34m%s\033[0m ", name);
                }
                else
                {
                    // Regular File: Default color
                    if (ext[0] != '\0')
                    {
                        printf("%s.%s ", name, ext); // Print file name with extension
                    }
                    else
                    {
                        printf("%s ", name); // Print file name only
                    }
                }
            }
        }

        // Update the cluster value using get_next_cluster
        fsState->currentCluster = get_next_cluster(file, fsState);
        if (fsState->currentCluster == 0)
        {
            perror("Error: Reached the end of the cluster chain or an error occurred");
            return;
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

uint32_t get_next_cluster(int file, FileSystemState *fsState)
{
    // printf("Debug: Entering get_next_cluster with currentCluster = %u\n", fsState->currentCluster);

    // Validate cluster range
    if (fsState->currentCluster < 2 || fsState->currentCluster >= 0x0FFFFFF8)
    {
        // printf("Error: Invalid current cluster number %u\n", fsState->currentCluster);
        return 0;
    }

    // Calculate FAT entry offset
    uint32_t FATOffset = fsState->currentCluster * 4;
    uint32_t FATSecNum = fsState->bootInfo.BPB_RsvdSecCnt + (FATOffset / fsState->bootInfo.BPB_BytsPerSec);
    uint32_t FATEntOffset = FATOffset % fsState->bootInfo.BPB_BytsPerSec;

    // Seek to FAT entry
    off_t seekResult = lseek(file, (FATSecNum * fsState->bootInfo.BPB_BytsPerSec) + FATEntOffset, SEEK_SET);
    if (seekResult == (off_t)-1)
    {
        // perror("Error seeking FAT entry");
        return 0;
    }

    // Read FAT entry
    uint32_t FATEntry;
    ssize_t bytesRead = read(file, &FATEntry, sizeof(FATEntry));
    if (bytesRead != sizeof(FATEntry))
    {
        // perror("Error reading FAT entry");
        return 0;
    }

    // printf("Debug: FATEntry (raw) read is %u (Hex: %08X)\n", FATEntry, FATEntry);

    // Extract next cluster value
    uint32_t nextCluster = FATEntry & 0x0FFFFFFF;
    // printf("Debug: nextCluster calculated as %u\n", nextCluster);
    if (nextCluster >= 0x0FFFFFF8)
    {
        // printf("Reached the end of the file or directory cluster chain.\n");
    }
    return nextCluster;
}

void change_directory(FileSystemState *fsState, const char *dirname, int fileDescriptor)
{
    char formattedDirName[12];
    uint32_t nextCluster;

    if (strcmp(dirname, "/") == 0)
    {
        strcpy(fsState->currentWorkingDir, "/");
        fsState->currentCluster = fsState->bootInfo.BPB_RootClus;
        return;
    }

    if (strcmp(dirname, "..") == 0)
    {
        if (fsState->directoryStack.top >= 0)
        {
            fsState->currentCluster = pop_cluster(&fsState->directoryStack);

            // Update currentWorkingDir to reflect the parent directory
            if (strcmp(fsState->currentWorkingDir, "/") != 0)
            {
                char *lastSlash = strrchr(fsState->currentWorkingDir, '/');
                if (lastSlash != fsState->currentWorkingDir)
                {
                    *lastSlash = '\0'; // Cut the string at the last slash
                }
                else
                {
                    *(lastSlash + 1) = '\0'; // Keep the root '/'
                }
            }
        }

        if (strcmp(fsState->currentWorkingDir, "/"))
        {
            strcpy(fsState->currentWorkingDir, "/");
            fsState->currentCluster = fsState->bootInfo.BPB_RootClus;
            return;
        }
        return;
    }

    push_cluster(&fsState->directoryStack, fsState->currentCluster);

    // Format the directory name to FAT32 8.3 format
    format_dir_name(dirname, formattedDirName);

    if (find_directory_in_cluster(fileDescriptor, fsState, formattedDirName, &nextCluster))
    {
        // printf("change_directory This is nextCLuster%u\n", nextCluster );
        if (fsState->currentWorkingDir[strlen(fsState->currentWorkingDir) - 1] != '/')
        {
            strcat(fsState->currentWorkingDir, "/");
        }
        strcat(fsState->currentWorkingDir, dirname);

        fsState->currentCluster = nextCluster;
        // printf("change_directory Changed directory to '%s', new cluster: %u\n", fsState->currentWorkingDir, fsState->currentCluster);
    }
    else
    {
        printf("error: '%s' does not exist\n", dirname);
    }
}

void push_cluster(ClusterStack *stack, uint32_t cluster)
{
    if (stack->top < MAX_DEPTH - 1)
    {
        stack->clusters[++stack->top] = cluster;
    }
}

uint32_t pop_cluster(ClusterStack *stack)
{
    if (stack->top >= 0)
    {
        return stack->clusters[stack->top--];
    }
    return 0; // Return an invalid cluster number if stack is empty
}

int case_insensitive_compare(const char *str1, const char *str2)
{
    while (*str1 && *str2)
    {
        if (tolower((unsigned char)*str1) != tolower((unsigned char)*str2))
        {
            return tolower((unsigned char)*str1) - tolower((unsigned char)*str2);
        }
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

bool find_directory_in_cluster(int fileDescriptor, FileSystemState *fsState, const char *dirName, uint32_t *nextCluster)
{
    bool check = false;
    char formattedDirName[12];
    format_dir_name(dirName, formattedDirName);

    while (fsState->currentCluster < 0x0FFFFFF8)
    {

        uint32_t sector = ((fsState->currentCluster - 2) * fsState->bootInfo.BPB_SecPerClus) + fsState->bootInfo.BPB_RsvdSecCnt +
                          (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32);

        for (int i = 0; i < fsState->bootInfo.BPB_SecPerClus; ++i)
        {
            // printf("find_directory_in_cluster Inspecting cluster: %u\n", fsState->currentCluster);

            off_t offset = (sector + i) * fsState->bootInfo.BPB_BytsPerSec;
            lseek(fileDescriptor, offset, SEEK_SET);

            for (int j = 0; j < fsState->bootInfo.BPB_BytsPerSec; j += sizeof(DirectoryEntry))
            {
                DirectoryEntry entry;
                ssize_t read_bytes = read(fileDescriptor, &entry, sizeof(DirectoryEntry));
                if (read_bytes != sizeof(DirectoryEntry))
                {

                    break;
                }

                if ((unsigned char)entry.DIR_Name[0] == 0x00 || (unsigned char)entry.DIR_Name[0] == 0xE5)
                {

                    continue;
                } // End or deleted entry

                char formattedEntryName[12];
                format_dir_name(entry.DIR_Name, formattedEntryName);

                if ((entry.DIR_Attr & 0x10) && case_insensitive_compare(formattedEntryName, formattedDirName) == 0)
                {
                    *nextCluster = (entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;
                    // printf("find directory in clster: nextCluster %u", *nextCluster);
                    return true;
                }
            }
        }

        fsState->currentCluster = get_next_cluster(fileDescriptor, fsState);

        if (fsState->currentCluster == 0)
        {
            break;
        } // Error or end of chain

        if (check == true)
        {
            return check;
        }
    }

    // printf("find_directory_in_cluster Directory '%s' not found in cluster: %u\n", dirName, fsState->currentCluster);
    return false;
}

void format_dir_name(const char *input, char *formatted)
{
    memset(formatted, ' ', 11); // Fill with spaces as per FAT32 8.3 format
    formatted[11] = '\0';       // Null-terminate for safety

    int i, j;
    // Convert name part
    for (i = 0, j = 0; i < 8 && input[i] != '\0' && input[i] != '.'; ++i, ++j)
    {
        formatted[j] = toupper((unsigned char)input[i]);
    }

    // Skip to extension in input
    while (input[i] && input[i] != '.')
        i++;
    if (input[i] == '.')
        i++;

    // Convert extension part
    for (j = 8; j < 11 && input[i] != '\0'; ++i, ++j)
    {
        formatted[j] = toupper((unsigned char)input[i]);
    }

    // printf("format_dir_name Formatted directory name: '%s'\n", formatted); // Debug print
}

bool find_file_in_cluster(int fileDescriptor, FileSystemState *fsState, const char *fileName, uint32_t *firstCluster)
{
    char formattedFileName[12];
    format_dir_name(fileName, formattedFileName);

    // printf("Debug: Searching for file '%s' in the cluster %u\n", formattedFileName, fsState->currentCluster);

    while (fsState->currentCluster < 0x0FFFFFF8)
    {
        for (int sectorOffset = 0; sectorOffset < fsState->bootInfo.BPB_SecPerClus; ++sectorOffset)
        {
            uint32_t sector = ((fsState->currentCluster - 2) * fsState->bootInfo.BPB_SecPerClus) + fsState->bootInfo.BPB_RsvdSecCnt +
                              (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32) + sectorOffset;
            off_t offset = sector * fsState->bootInfo.BPB_BytsPerSec;

            for (int entryOffset = 0; entryOffset < fsState->bootInfo.BPB_BytsPerSec; entryOffset += sizeof(DirectoryEntry))
            {
                lseek(fileDescriptor, offset + entryOffset, SEEK_SET);
                DirectoryEntry entry;
                ssize_t bytesRead = read(fileDescriptor, &entry, sizeof(DirectoryEntry));
                if (bytesRead != sizeof(DirectoryEntry))
                {
                    printf("Debug: Unable to read entry at offset %lld\n", (long long)(offset + entryOffset));
                    continue;
                }

                // printf("Debug: Found entry '%.11s'\n", entry.DIR_Name);

                if (strncmp(entry.DIR_Name, formattedFileName, 11) == 0)
                {
                    // printf("Debug: Match found for file '%s'\n", formattedFileName);
                    *firstCluster = ((uint32_t)entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;
                    return true;
                }
                if ((unsigned char)entry.DIR_Name[0] == 0x00)
                {
                    printf("Debug: Reached end of directory entries\n");
                    return false; // End of directory entries
                }

                if ((unsigned char)entry.DIR_Name[0] == 0xE5 || entry.DIR_Attr == 0x0F || (entry.DIR_Attr & 0x18))
                {
                    continue; // Deleted, LFN, system file, or volume label
                }

                // Check if the entry is not a directory, not a volume label, and not an LFN entry
                if (!(entry.DIR_Attr & 0x10) && !(entry.DIR_Attr & 0x08) && entry.DIR_Attr != 0x0F)
                {
                    if (case_insensitive_compare(entry.DIR_Name, formattedFileName) == 0)
                    {
                        *firstCluster = ((uint32_t)entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;
                        return true;
                    }
                }
                else
                {
                    printf("Debug: Entry '%.11s' not a file or not matching, attributes: %02X\n", entry.DIR_Name, entry.DIR_Attr);
                }
            }
        }

        fsState->currentCluster = get_next_cluster(fileDescriptor, fsState);
    }
    printf("Debug: File '%s' not found\n", formattedFileName);
    return false; // File not found
}

bool open_file(int fileDescriptor, FileSystemState *fsState, const char *filename, const char *mode)
{
    char formattedFilename[12];
    format_dir_name(filename, formattedFilename); // Format the filename to FAT32 8.3 format

    // printf("Debug: Attempting to open file '%s' (formatted as '%s') with mode '%s'\n", filename, formattedFilename, mode);

    // Check if file is already opened
    for (int i = 0; i < fsState->openedFilesCount; i++)
    {
        if (strcmp(fsState->openedFiles[i].filename, formattedFilename) == 0)
        {
            printf("Error: file '%s' already opened\n", formattedFilename);
            return false;
        }
    }

    // Check if the file exists in the current directory
    uint32_t firstCluster;
    if (!find_file_in_cluster(fileDescriptor, fsState, formattedFilename, &firstCluster))
    {
        printf("Error: File '%s' does not exist in %s.\n", formattedFilename, fsState->currentWorkingDir);
        return false;
    }

    // printf("Debug: File '%s' found with first cluster %u\n", formattedFilename, firstCluster);

    // Process opening the file with appropriate flags
    int flags = determine_open_flags(mode);
    if (flags == -1)
    {
        printf("Error: Invalid mode '%s'\n", mode);
        return false;
    }

    // Add to the openedFiles array
    add_to_opened_files(fsState, formattedFilename, firstCluster, flags, mode);
    // printf("Debug: File '%s' added to openedFiles\n", formattedFilename);
    printf("Opened File\n");
    return true;
}

int determine_open_flags(const char *mode)
{
    if (strcmp(mode, "-r") == 0)
        return O_RDONLY;
    else if (strcmp(mode, "-w") == 0)
        return O_WRONLY;
    else if (strcmp(mode, "-rw") == 0 || strcmp(mode, "-wr") == 0)
        return O_RDWR;
    return -1; // Invalid mode
}

void add_to_opened_files(FileSystemState *fsState, const char *filename, uint32_t firstCluster, int fd, const char *mode)
{
    // Assuming fsState->openedFilesCount is the count of opened files
    if (fsState->openedFilesCount < 100)
    { // Assuming 100 is the max number of opened files
        strncpy(fsState->openedFiles[fsState->openedFilesCount].filename, filename, sizeof(fsState->openedFiles[fsState->openedFilesCount].filename) - 1);
        fsState->openedFiles[fsState->openedFilesCount].file_descriptor = fd;
        strncpy(fsState->openedFiles[fsState->openedFilesCount].mode, mode, sizeof(fsState->openedFiles[fsState->openedFilesCount].mode) - 1);
        fsState->openedFiles[fsState->openedFilesCount].offset = 0;
        fsState->openedFiles[fsState->openedFilesCount].firstCluster = firstCluster;
        fsState->openedFilesCount++;
    }
}

/*
bool open_file(int fileDescriptor, FileSystemState *fsState, const char *filename, const char *mode) {
    // Check if file is already opened
    for (int i = 0; i < fsState->openedFilesCount; i++) {
        if (strcmp(fsState->openedFiles[i].filename, filename) == 0) {
            printf("Error: file '%s' already opened\n", filename);
            return false;
        }
    }

    // Check if the file exists in the current directory
    uint32_t firstCluster;
    if (!find_file_in_cluster(fileDescriptor, fsState, filename, &firstCluster)) {
        printf("Error: File '%s' does not exist in the current directory.\n", filename);
        return false;
    }

    // Process opening the file with appropriate flags
    int flags = determine_open_flags(mode);
    if (flags == -1) {
        printf("Error: Invalid mode '%s'\n", mode);
        return false;
    }

    int fd = open(filename, flags);
    if (fd == -1) {
        perror("Error opening file");
        return false;
    }

    // Add to the openedFiles array
    add_to_opened_files(fsState, filename, fd, firstCluster, mode);
    return true;
}
*/

void run_shell(const char *imageName, FileSystemState *fsState, int file)
{
    char command[100];
    while (1)
    {
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
            print_directory_entries(file, fsState);
        }
        else if (strcmp(command, "open") == 0)
        {
            char filename[256], mode[3];
            scanf("%s %s", filename, mode);
            if (!open_file(file, fsState, filename, mode))
            {
                printf("Failed to open file '%s'\n", filename);
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