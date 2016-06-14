#include "fs_struct.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


int main(int argc, char **argv){
	int i;
	void* wblock = calloc(1024, 1);
	int fd = 0;
	if(argc<3){
		printf("Usage: ./createdisk filename size(in blocks)\n");
		exit(1);
	}

	
	char* file = argv[1]; //file name for disk
	int num_blocks = strtol(argv[2], &argv[2], 0); //number of blocks

	if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0)
	    perror("error creating file"), exit(1);
	for (int i = 0; i < num_blocks; i++)
	    if (write(fd, wblock, 1024) < 0) //zero out file
		perror("error writing file"), exit(1);
	lseek(fd, 0, SEEK_SET);


	
// initialize superblock
    struct Superblock* super = wblock;
    super->magicnum = 0x2345BEEF;
    super->total_blocks = num_blocks;//strtol(argv[2], &argv[2], 0); //N

    if (super->total_blocks % 1024 != 0){
            super->fat_size = (super->total_blocks / 256) + 1;
    } else {
            super->fat_size = (super->total_blocks / 256); //k
    }
    super->block_size = 1024; //1024
    super->root_block = super->fat_size + 1; //k + 1
    printf("total_blocks: %d\n", super->total_blocks);
    printf("fat_size: %d\n", super->fat_size);

    int fat_size = super->fat_size;
    int block_size = super->block_size;
    int root_block = fat_size + 1;

    write_block(fd, 0, wblock);

// initialize FAT
	int dna = fat_size + 1; //do not allocate (superblock and fat stored there)
	struct Fatblock *fb = wblock;
	memset(wblock, 0, block_size);
	
	for (int i = 0; i < fat_size + 1; i++){
		
		for (int j = 0; j < 256; j++){
			
			if (j < dna){
				fb->fatentry[j] = -1;	
			} else{
				fb->fatentry[j] = 0;
			}
		}
		dna -= 256; //subtract 256 so only the first fat block has unused placeholders
		
		write_block(fd, 1+i, wblock);
	}

// initialize root directory
	struct Directory_entry* de = wblock;
	memset(wblock, 0, block_size);
	de[0].flag = 1;
	de[0].file_length = 64; // only current directory
	char name[24] = "/";
	strcpy(de[0].filename, name );
	de[0].start_block = fat_size+1;
	write_block(fd, 1+fat_size, wblock);


//testing


//end testing
	
	close(fd);
    return 0;
}
