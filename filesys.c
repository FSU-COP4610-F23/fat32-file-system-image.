#define _POSIX_C_SOURCE 200809L
#define MAX_DEPTH 256
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
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
    uint32_t BPB_FATSz32;
    uint32_t EntriesPerFAT; // print
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
    char mode[5];
    char path[256];
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
bool close_file(FileSystemState *fsState, const char *filename);
uint32_t get_next_cluster(int file, FileSystemState *fsState);
void change_directory(FileSystemState *fsState, const char *dirname, int fileDescriptor);
void run_shell(const char *imageName, FileSystemState *fsState, int file);
void split_name_ext(const char *entryName, char *name, char *ext);
void format_dir_name(const char *input, char *formatted);
void push_cluster(ClusterStack *stack, uint32_t cluster);
bool custom_lseek(const char *filename, off_t offset, FileSystemState *fsState);
void add_to_opened_files(FileSystemState *fsState, const char *filename, const char *path, uint32_t firstCluster, int fd, const char *mode);
uint32_t pop_cluster(ClusterStack *stack);
int determine_open_flags(const char *mode);
//bool custom_append(const char *filename, char *str, FileSystemState *fsState);
size_t findFileSizeBytes(int i, FileSystemState *fsState);



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
    fsState->bootInfo.dataRegionAddress = fsState->bootInfo.BPB_RsvdSecCnt * fsState->bootInfo.BPB_BytsPerSec + 
    (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32 * fsState->bootInfo.BPB_BytsPerSec);

    // Set root cluster postition to data region address
    fsState->bootInfo.rootClusPosition = fsState->bootInfo.dataRegionAddress;

    // Calculate total data sectors
    fsState->bootInfo.dataSec = fsState->bootInfo.BPB_TotSec32 - (fsState->bootInfo.BPB_RsvdSecCnt + 
    (fsState->bootInfo.BPB_NumFATs * fsState->bootInfo.BPB_FATSz32) + fsState->bootInfo.rootDirSectors);

    // Calculate total clusters in data region
    fsState->bootInfo.total_clusters = fsState->bootInfo.dataSec / fsState->bootInfo.BPB_SecPerClus;

    // Find size of fat32.img
    fsState->bootInfo.image_size = lseek(file, 0, SEEK_END);

    // Calculate the number of entries in one FAT
    fsState->bootInfo.EntriesPerFAT = fsState->bootInfo.BPB_FATSz32 * fsState->bootInfo.BPB_BytsPerSec / 4;
}

void display_boot_sector_info(const FileSystemState *fsState)
{
    printf("\nRoot Cluster: %x\n", fsState->bootInfo.rootClusPosition);
    printf("Bytes Per Sector: %u\n", fsState->bootInfo.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", fsState->bootInfo.BPB_SecPerClus);
    printf("Total Clusters in Data Region: %u\n", fsState->bootInfo.total_clusters);
    printf("Entries Per FAT: %u\n", fsState->bootInfo.EntriesPerFAT);
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
         printf("Reached the end of the file or directory cluster chain.\n");
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

        if (strcmp(fsState->currentWorkingDir, "/") == 0)
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

                // Check for end of directory or deleted entry
                if ((unsigned char)entry.DIR_Name[0] == 0x00)
                {
                    printf("Debug: Reached end of directory entries\n");
                    return false;
                }
                if ((unsigned char)entry.DIR_Name[0] == 0xE5)
                {
                    continue; // Deleted entry
                }

                // Check if entry is a valid file
                if (strncmp(entry.DIR_Name, formattedFileName, 11) == 0 && !(entry.DIR_Attr & 0x10))
                {
                    // File found and it's not a directory
                    *firstCluster = ((uint32_t)entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;
                    return true;
                }
            }
        }

        // Move to next cluster
        fsState->currentCluster = get_next_cluster(fileDescriptor, fsState);
    }

    printf("Debug: File '%s' not found\n", formattedFileName);
    return false; // File not found
}

bool open_file(int fileDescriptor, FileSystemState *fsState, const char *filename, const char *mode)
{
    char formattedFilename[12];
    format_dir_name(filename, formattedFilename); // Format the filename to FAT32 8.3 format

    const char *basePath = "fat32.img"; // Set this to the directory where fat32.img is located
    char fullPath[256];

    if (strcmp(fsState->currentWorkingDir, "/") == 0)
    {
        snprintf(fullPath, sizeof(fullPath), "%s", basePath);
    }
    else
    {
        snprintf(fullPath, sizeof(fullPath), "%s%s", basePath, fsState->currentWorkingDir);
    }

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
    printf("Attempting to open file at path: %s\n", fullPath);

    uint32_t firstCluster;
    if (!find_file_in_cluster(fileDescriptor, fsState, formattedFilename, &firstCluster))
    {
        printf("Error: File '%s' does not exist in %s.\n", formattedFilename, fsState->currentWorkingDir);
        return false;
    }

    // Process opening the file with appropriate flags
    int flags = determine_open_flags(mode);
    int fd = open(fullPath, flags);
    if (fd == -1)
    {
        printf("Error: Invalid mode '%s'\n", mode);
        return false;
    }

    add_to_opened_files(fsState, formattedFilename, fullPath, firstCluster, fd, mode);

    printf("Opened File\n");
    // printf("Opened file '%s' with FD: %d\n", formattedFilename, fd);
    return true;
}

bool close_file(FileSystemState *fsState, const char *filename)
{
    char formattedFilename[12];
    format_dir_name(filename, formattedFilename); // Format filename to FAT32 8.3 format

    // Check if the file is in the openedFiles array
    bool fileFound = false;
    int fileIndex = -1;
    for (int i = 0; i < fsState->openedFilesCount; i++)
    {
        if (strcmp(fsState->openedFiles[i].filename, formattedFilename) == 0)
        {
            fileFound = true;
            fileIndex = i;
            break;
        }
    }

    // If the file was not found in the openedFiles array, print an error and return false
    if (!fileFound)
    {
        printf("Error: File '%s' is not opened or does not exist\n", formattedFilename);
        return false;
    }

    // Remove the file from the openedFiles array
    // This can be done by shifting all elements after the found file one position to the left
    for (int i = fileIndex; i < fsState->openedFilesCount - 1; i++)
    {
        fsState->openedFiles[i] = fsState->openedFiles[i + 1];
    }
    fsState->openedFilesCount--;

    printf("File '%s' closed successfully.\n", formattedFilename);
    return true;
}

bool custom_lseek(const char *filename, off_t offset, FileSystemState *fsState)
{
    // Check for a non-negative offset
    if (offset < 0)
    {
        printf("Error: Offset cannot be negative.\n");
        return false;
    }

    char formattedFilename[12];                   // 11 characters + 1 for null terminator
    format_dir_name(filename, formattedFilename); // Format the input filename

    // Find the opened file in FileSystemState
    for (int i = 0; i < fsState->openedFilesCount; i++)
    {
        if (strcmp(fsState->openedFiles[i].filename, formattedFilename) == 0)
        {
            // Set the offset
            fsState->openedFiles[i].offset = offset;
            return true;
        }
    }

    printf("Error: File '%s' not opened or does not exist.\n", filename);
    return false;
}

bool custom_read(const char *filename, size_t size, FileSystemState *fsState)
{
    printf("Made it here\n");
    char formattedFilename[12];                   // Buffer for formatted filename
    format_dir_name(filename, formattedFilename); // Format the input filename

    for (int i = 0; i < fsState->openedFilesCount; i++)
    {
        printf("in the loop\n");
        if (strcmp(fsState->openedFiles[i].filename, formattedFilename) == 0)
        {
            printf("File found\n");
            // Check if file is opened in read mode
            if (strstr(fsState->openedFiles[i].mode, "r") == NULL)
            {
                printf("Error: File '%s' is not opened in read mode.\n", formattedFilename);
                return false;
            }

            size_t fileSize;
            fileSize = findFileSizeBytes(i, fsState);
            printf("%zu\n", fileSize);
            if (size > fileSize)
                size = fileSize;

            // Allocate buffer for reading
            char *buffer = malloc(size + 1); // +1 for null terminator
            if (buffer == NULL)
            {
                printf("Error: Memory allocation failed.\n");
                return false;
            }

            off_t readOffset = (fsState->openedFiles[i].firstCluster - 2) * fsState->bootInfo.BPB_BytsPerSec;

            printf("2Filename: %s, FD: %d, Offset: %lld, Size: %zu\n", formattedFilename, fsState->openedFiles[i].file_descriptor, (long long)readOffset, size);
            // Read data from file
            ssize_t read_bytes = pread(fsState->openedFiles[i].file_descriptor, buffer, size, readOffset);
            if (read_bytes < 0)
            {
                perror("Error reading file");
                printf("Filename: %s, FD: %d, Offset: %lld, Size: %zu\n", formattedFilename, fsState->openedFiles[i].file_descriptor, (long long)readOffset, size);
                free(buffer);
                return false;
            }

            // Update the offset
            printf("1Filename: %s, FD: %d, Offset: %lld, Size: %zu\n", formattedFilename, fsState->openedFiles[i].file_descriptor, (long long)readOffset, size);
            // Print the read data
            buffer[read_bytes] = '\0'; // Null-terminate the buffer
            printf("%s\n", buffer);

            if (read_bytes < (ssize_t)size)
            {
                printf("Note: Read less than requested size. Possible end of file.\n");
            }

            fsState->openedFiles[i].offset += read_bytes;
            free(buffer);
            return true;
        }
    }

    printf("Error: File '%s' not opened or does not exist.\n", formattedFilename);
    return false;
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

void add_to_opened_files(FileSystemState *fsState, const char *filename, const char *path, uint32_t firstCluster, int fd, const char *mode)
{
    if (fsState->openedFilesCount >= 100)
    { // Check if max opened file limit is reached
        printf("Error: Maximum number of opened files reached\n");
        return;
    }

    int index = fsState->openedFilesCount;

    // Copy filename and ensure null termination
    strncpy(fsState->openedFiles[index].filename, filename, sizeof(fsState->openedFiles[index].filename) - 1);
    fsState->openedFiles[index].filename[sizeof(fsState->openedFiles[index].filename) - 1] = '\0';

    // Copy mode and ensure null termination
    strncpy(fsState->openedFiles[index].mode, mode, sizeof(fsState->openedFiles[index].mode) - 1);
    fsState->openedFiles[index].mode[sizeof(fsState->openedFiles[index].mode) - 1] = '\0';

    // Copy path and ensure null termination
    strncpy(fsState->openedFiles[index].path, path, sizeof(fsState->openedFiles[index].path) - 1);
    fsState->openedFiles[index].path[sizeof(fsState->openedFiles[index].path) - 1] = '\0';

    fsState->openedFiles[index].file_descriptor = fd;
    fsState->openedFiles[index].offset = 0;
    fsState->openedFiles[index].firstCluster = firstCluster;

    fsState->openedFilesCount++;
}

/*bool custom_append(const char *filename, char *str, FileSystemState *fsState)
{
    char formattedFilename[12];                   // Buffer for formatted filename
    format_dir_name(filename, formattedFilename); // Format the input filename

    for (int i = 0; i < fsState->openedFilesCount; i++)
    {
        if (strcmp(fsState->openedFiles[i].filename, formattedFilename) == 0)
        {
            // Check if file is opened in read mode
            if (strstr(fsState->openedFiles[i].mode, "w") == NULL)
            {
                printf("Error: File '%s' is not opened in write mode.\n", formattedFilename);
                free(str);
                return false;
            }

            // Check for quotation marks
            if (strlen(str) < 2 || (str[0] != '"' && str[strlen(str) - 1] != '"'))
            {
                printf("Error: [STRING] does not contain quotations.\n");
                free(str);
                return false;
            }
            else if (strlen(str) == 2 && str[0] != '"' && str[strlen(str) - 1] != '"')
            {
                printf("Error: [STRING] is empty.\n");
                free(str);
                return false;
            }

            memmove(str, str + 1, strlen(str) - 1);
            str[strlen(str) - 2] = '\0';
            str = realloc(str, strlen(str) + 1);
            if (str == NULL)       
            {
              printf("Error: Memory reallocation failed.\n");
              free(str); //if str == NULL how does it free?
              return false;
            }

            struct stat file_info;
            if (stat(fsState->openedFiles[i].path, &file_info) == -1)
            {
                printf("Error: Could not retrieve file size.\n");
                return false;
            }

            printf("%lld %s %zu %s %lld\n",(long long)fsState->openedFiles[i].offset, fsState->openedFiles[i].path, strlen(str) + 1, str, (long long)file_info.st_size);

            // Expand size of file
            if (fsState->openedFiles[i].offset + strlen(str) + 1 > file_info.st_size)
            {
                int trunc = ftruncate(fsState->openedFiles[i].file_descriptor, fsState->openedFiles[i].offset + strlen(str) + 1);
                if (trunc == -1) 
                {
                    printf("Error: Truncate Failed.\n");
                    free(str);
                    return false;
                }
            }

            // Add append flag
            int flags = fcntl(fsState->openedFiles[i].file_descriptor, F_GETFL);
            if (flags == -1) 
            {
                printf("Error getting file flags");
                free(str);
                return false;
            }
            flags |= O_APPEND; // Adding O_APPEND flag

            int result = fcntl(fsState->openedFiles[i].file_descriptor, F_SETFL, flags);
            if (result == -1) 
            {
                printf("Error setting file flags");
                free(str);
                return false;
            }

            // Write data to end of file
            ssize_t bytesWritten = write(fsState->openedFiles[i].file_descriptor, str, strlen(str) + 1);
            if (bytesWritten == -1)
            {
                printf("Error writing to the file");
                free(str);
                return false;
            }
            
            // Remove append flag
            flags = fcntl(fsState->openedFiles[i].file_descriptor, F_GETFL);
            if (flags == -1) 
            {
                printf("Error getting file flags");
                free(str);
                return false;
            }
            flags &= ~O_APPEND; // Adding O_APPEND flag

            result = fcntl(fsState->openedFiles[i].file_descriptor, F_SETFL, flags);
            if (result == -1) 
            {
                printf("Error setting file flags");
                free(str);
                return false;
            }

            // Update the offset
            fsState->openedFiles[i].offset += strlen(str); //null terminator does not count towards offset
            free(str);
            return true;
        }
    }
    printf("Error: File '%s' not opened or does not exist.\n", formattedFilename);
    return false;
}
*/
size_t findFileSizeBytes(int i, FileSystemState *fsState)
{
    printf("I'm broken\n");
    // Assuming the currentCluster is the cluster to check
    uint32_t tempCluster = fsState->currentCluster;
    fsState->currentCluster = fsState->openedFiles[i].firstCluster;
    int clusterCount = 0;
    // Check if the current cluster is the last cluster in the chain
    while (fsState->currentCluster < 0x0FFFFFF8)
    {
        printf("%u %d", fsState->currentCluster, clusterCount);
        fsState->currentCluster = get_next_cluster(fsState->openedFiles[i].file_descriptor, fsState);
        clusterCount++;
    }
    fsState->currentCluster = tempCluster; //restore regular current Cluster placement
    printf("jk\n");
    return clusterCount * (int)fsState->bootInfo.BPB_BytsPerSec;
}


void run_shell(const char *imageName, FileSystemState *fsState, int file)
{
    char command[100];
    while (1)
    {
        printf("[%s]%s> ", imageName, fsState->currentWorkingDir);
        scanf("%99s", command);

        if (strcmp(command, "exit") == 0)
        {
            //add a clean function later
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
            // if (!open_file(fsState, filename, mode))
            {
                printf("Failed to open file '%s'\n", filename);
            }
        }
        else if (strcmp(command, "close") == 0)
        {
            char filename[256];
            scanf("%s", filename);
            if (!close_file(fsState, filename))
            {
                printf("Error: Unable to close file '%s'\n", filename);
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
                printf("INDEX   FILENAME        MODE    OFFSET    PATH\n");
                for (int i = 0; i < fsState->openedFilesCount; i++)
                {
                    char displayMode[3];

                    // Copy mode without the '-' character
                    if (fsState->openedFiles[i].mode[0] == '-')
                    {
                        strncpy(displayMode, fsState->openedFiles[i].mode + 1, sizeof(displayMode) - 1);
                    }
                    else
                    {
                        strncpy(displayMode, fsState->openedFiles[i].mode, sizeof(displayMode));
                    }

                    displayMode[sizeof(displayMode) - 1] = '\0'; // Ensure null termination

                    printf("%-7d %-15s %-7s %-9ld %s\n",
                           i, fsState->openedFiles[i].filename, displayMode, fsState->openedFiles[i].offset, fsState->openedFiles[i].path);
                }
            }
        }
        else if (strcmp(command, "lseek") == 0) // Corrected the command name
        {
            char filename[256];
            off_t offset;
            scanf("%s %ld", filename, &offset);

            // Call the lseek function
            if (!custom_lseek(filename, offset, fsState))
            {
                printf("Error: Unable to set offset for file '%s'\n", filename);
            }
        }
        else if (strcmp(command, "read") == 0)
        {
            char filename[256];
            size_t size;
            scanf("%s %zu", filename, &size);
            printf("Hello\n");
            if (!custom_read(filename, size, fsState))
            {
                printf("Error: Unable to read file '%s'\n", filename);
            }
        }
        /*else if (strcmp(command, "append") == 0)
        {
            char filename[256];
            char *str = malloc(256);
            if (str == NULL)
            {
                printf("Error: Memory allocation failed.\n");
            }
            else
            {
                scanf("%s %255s", filename, str);
                if (!custom_append(filename, str, fsState))
                {
                   printf("Error: Unable to append to file '%s'\n", filename);
                   free(str);
                }
            }
        }*/
        else
        {
            printf("Unknown command: %s\n", command);
        }
    }
}
