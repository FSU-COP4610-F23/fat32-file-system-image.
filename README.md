# project-3-group-5

<div style="margint: 200px;">

## Purpose
The purpose of this project is to familiarize you with basic file-system design and implementation. You 
will need to understand various aspects of the FAT32 file system, such as cluster-based storage, FAT 
tables, sectors, and directory structure.

## Project Prompt
For this project, you will design and implement a simple, user-space, shell-like utility that is capable of 
interpreting a FAT32 file system image. The program must understand the basic commands to 
manipulate the given file system image, must not corrupt the file system image, and should be robust. 
You may not reuse kernel file system code and you may not copy code from other file system utilities.
You are tasked with writing a program that supports the following file system commands to a FAT32 
image file. For good modular coding design, try to implement each command in one or more separate 
functions (e.g., for write you may have several shared lookup functions, an update directory entry 
function, and an update cluster function). Your program will take the image file path name as an 
argument and will read and write to it according to the different commands. You can check the validity
of the image file by mounting it with the loop back option and using tools like Hexedit. You will also 
need to handle various errors. When you encounter an error, you should print a descriptive message 
(e.g., when cd’ing to a nonexistent file you can do something like “Directory not found: foo”). Further, 
your program must continue running and the state of the system should remain unchanged as if the 
command was never called (i.e., don’t corrupt the file system with invalid data).


## Group Members
- **Jasmine Masopeh**: jdm21e@fsu.edu
- **Angela Fields**: js19@fsu.edu
- **JuanCarlos Alguera**: ab19@fsu.edu

## Division of Labor

### Part 1: Mount the Image File [5 points]
  **Responsibilities**: \
  The user will need to mount the image file through command line arguments:

  ./filesys [FAT32 ISO] -> [1pt]

  You should read the image file and implement the correct structure to store the format of FAT32 and 
  how to navigate it.
  You should close out the program and return an error message if the file does not exist.
  The user will then be greeted with a standard shell (like project 1) that will accept user input. 
  Your terminal should look like this:

  [NAME_OF_IMAGE]/[PATH_IN_IMAGE]/>

  The following commands will need to be implemented:

  info -> [3 pts]\
  Parses the boot sector. Prints the field name and corresponding value for each entry, one per
  line (e.g., Bytes Per Sector: 512).
  The fields you need to print out are:\
  * position of root cluster
  * bytes per sector
  * sectors per cluster
  * total # of clusters in data region
  * '#' of entries in one fat
  * size of image (in bytes)\

  exit -> [1 pt]\
  &nbsp;&nbsp;&nbsp;Safely closes the program and frees up any allocated resources.

- **Assigned to**: JuanCarlos Alguera, Jasmine Masopeh

### Part 2: Navigation [10 points]
**Responsibilities**:
You will need to create these commands that will allow the user to navigate the image.

cd [DIRNAME] -> [5 pts]
* Changes the current working directory to [DIRNAME].
    * Your code will need to maintain the current working directory state.
Print an error if [DIRNAME] does not exist or is not a directory.

ls -> [5 pts]
* Print the name filed for the directories within the current working directory including the “.” And
“..” directories.
    * For simplicity, you may print each of the directory entries on separate lines.

**Assigned to**: Angela Fields, Jasmine Masopeh

### Part 4: Read [20 points]
**Responsibilities**:
You will need to create commands that will read from opened files. Create a structure that stores which files are opened.

open [FILENAME] [FLAGS] -> [4 pts]
* Opens a file named [FILENAME] in the current working directory. A file can only be read from or written to if it is opened first. You will need to maintain some data structure of opened files and add [FILENAME] to it when open is called. [FLAGS] is a flag and is only valid if it is one of the following (do not miss the  
‘-‘ character for the flag):

  * -r - read-only.
  * -w - write-only.
  * -rw - read and write.
  * -wr - write and read.

* Initialize the offset of the file at 0. Can be stored in the open file data structure along with other info.
* Print an error if the file is already opened, if the file does not exist, or an invalid mode is used.

close [FILENAME] -> [2 pts]
&nbsp;Closes a file [FILENAME] in current working directory.
&nbsp;&nbsp; Needs to remove the file entry from the open file data structure.Print an error if the file is not opened, or if the file does not exist in current working directory.

lsof -> [2 pts]
&nbsp;Lists all opened files.
&nbsp;&nbsp;Needs to list the index, file name, mode, offset, and path for every opened file.
If no files are opened, notify the user.

lseek [FILENAME] [OFFSET] -> [2 pts] 
Set the offset (in bytes) of file [FILENAME] in current working directory for further reading or 
writing.
Store the value of [OFFSET] (in memory) and relate it to the file [FILENAME].
Print an error if file is not opened or does not exist.
Print an error if [OFFSET] is larger than the size of the file.

read [FILENAME] [SIZE] -> [10 pts]
Read the data from a file in the current working directory with the name [FILENAME], and print it out.
Start reading from the file’s stored offset and stop after reading [SIZE] bytes.
If the offset + [SIZE] is larger than the size of the file, just read until end of file.
Update the offset of the file to offset + [SIZE] (or to the size of the file if you reached the 
end of the file).
Print an error if [FILENAME] does not exist, if [FILENAME] is a directory, or if the file is not
opened for reading.
**Assigned to**: Jasmine Masopeh, Juancarlos Alguera

## File Listing
```
project-3-group-5/
├── filesys.c
├── fat32.img
├── Makefile
└── README.md

```
# How to Compile & Execute

### Requirements
- **Compiler**: e.g., `gcc` for C/C++, `rustc` for Rust.
- **Dependencies**: List any libraries or frameworks necessary (rust only).


### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in ...
### Execution
```bash
.\filesys fat32.img
```

## Bugs
- Should be none, any bugs caught in testing were fixed


## Considerations
- If there are any bugs we didn't catch, it's due to us moving on to study for finals after the project got cut down. We appreciate the project parts being cut down so we could focus more on the final for this class. <3 
</>
