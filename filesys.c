#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Define the structure for the boot sector information
typedef struct {
    unsigned int root_cluster;
    unsigned int bytes_per_sector;
    unsigned int sectors_per_cluster;
    unsigned int total_clusters;
    unsigned int num_FAT_entries;
    unsigned int image_size;
} boot_sector_info;

typedef struct {
  char filename[100]; 
  char mode[3]; 
  FILE *fileptr; 
  unsigned int offset;
}opened_file; 

opened_file opened_files[100]; 
int opened_files_count = 0; 

bool is_file_opened(const char *filename);
void open_file(const char *filename, const char *mode); 
void close_file(const char *filename); 
void list_open_files();
void seek_file(const char *filename, unsigned int offset); 
void read_file(const char *filename, unsigned int size); 

//  Used for part-1
void parse_boot_sector(FILE *file, boot_sector_info *info);
void display_boot_sector_info(const boot_sector_info *info);
void run_shell(const char *imageName, boot_sector_info *info);

int main(int argc, char *argv[]) {
    if (argc != 2) 
    {
        printf("Usage: ./filesys [FAT32 ISO]\n"); // if there are not enough arguments
        return 1;
    }

    const char *imagePath = argv[1];
    FILE *file = fopen(imagePath, "rb");
    if (file == NULL) {
        perror("Error opening image file");
        return 1;
    }

    boot_sector_info bootInfo;
    parse_boot_sector(file, &bootInfo);

    run_shell(imagePath, &bootInfo);

    fclose(file);
    return 0;
}

bool is_file_opened(const char *filename)
{
  for (int i = 0; i < opened_files_count; i++)
  {
    if (strcmp(opened_files[i].filename, filename) == 0)
    {
      return true;
    }
  }
  return false;
}

void open_file(const char *filename, const char * mode)
{
  char fopen_mode[3]; 

  if (is_file_opened(filename))
  {
    printf("Error: File is already opened!"); 
    return; 
  }

  if (strcmp(mode, "-r") != 0 && strcmp(mode, "-w") != 0 && 
      strcmp(mode, "-rw") != 0 && strcmp(mode, "-wr") != 0)
  {
    printf("Error: Invalid mode.\n");
    return;
  }

  if(strcmp(mode, "-r") == 0)
    strcpy (fopen_mode, "r"); 
  else if (strcmp(mode, "-w") == 0)
    strcpy(fopen_mode, "w"); 
  else 
    strcpy(fopen_mode, "r+");  // -rw and -wr

  FILE  *file = fopen(filename, fopen_mode); 
  if (file == NULL)
  {
    perror("Error opening file"); 
    return;
  }

   // Add the file to the opened_files array
  strcpy(opened_files[opened_files_count].filename, filename);
  strcpy(opened_files[opened_files_count].mode, mode);
  opened_files[opened_files_count].fileptr = file;
  opened_files[opened_files_count].offset = 0;  // Initialize offset to 0
  opened_files_count++;
  printf("File opened successfully.\n");
}

void parse_boot_sector(FILE *file, boot_sector_info *info) {
    // Read and parse the boot sector
    // These are fake values, we will need to change these
    info->root_cluster = 0;  // Example value
    info->bytes_per_sector = 512;  // Example value
    info->sectors_per_cluster = 8;  // Example value
    info->total_clusters = 10000;  // Example value
    info->num_FAT_entries = 2000;  // Example value
    info->image_size = 5120000;  // Example value

}

void display_boot_sector_info(const boot_sector_info *info) {
    printf("Root Cluster: %u\n", info->root_cluster);
    printf("Bytes Per Sector: %u\n", info->bytes_per_sector);
    printf("Sectors Per Cluster: %u\n", info->sectors_per_cluster);
    printf("Total Clusters: %u\n", info->total_clusters);
    printf("Number of FAT Entries: %u\n", info->num_FAT_entries);
    printf("Image Size: %u bytes\n", info->image_size);
}

void run_shell(const char *imageName, boot_sector_info *info) {
    char command[100];
    while (1) {
        printf("[%s]/>\n", imageName);
        scanf("%s", command);

        if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "info") == 0) {
            display_boot_sector_info(info);
        } else {
            printf("Unknown command: %s\n", command);
        }
    }
}


