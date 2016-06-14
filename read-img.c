
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fs_struct.h"

int main(int argc, char **argv)
{
    FILE *fp = fopen(argv[1], "r");
    char buf[1024];

    if (fp == NULL){
		perror("bad image");
		exit(1);
	}
    
    struct Superblock super;
    fread(buf, sizeof(buf), 1, fp);
    memcpy(&super, buf, sizeof(super));

    printf("Superblock:\n");
    printf(" magic:                     %x\n", super.magicnum);
    printf(" block size:                %d\n", super.block_size);
    printf(" file system size (blocks): %d\n", super.total_blocks);
    printf(" FAT length (blocks):       %d\n", super.fat_size);
    printf(" Root directory start:      %d\n", super.root_block);

#define IS_FILE 1
#define IS_DIR 2
    int32_t *fat = calloc(1024, super.fat_size);
    char *map = calloc(super.total_blocks, 1);
    map[super.root_block] = IS_DIR;
    
    fread(fat, 1024, super.fat_size, fp);

    int i, blk = super.root_block;
    printf("FAT:\n");
    for (i = 0; i < super.total_blocks; i++) {
	if (fat[i]!= 0)
	    printf("[%d]: %d %s\n", i, fat[i],
		   fat[i]==-2 ? "EOF" : "-");
    }
    printf("\n");
    
    while (!feof(fp) && blk < super.total_blocks) {
	fread(buf, 1, 1024, fp);
	if (map[blk] != 0) {
	    if (map[blk] == IS_DIR) {
		struct Directory_entry *de = (void*)buf;
		printf("Block %d: directory\n", blk);
		for (i = 0; i < 16; i++)
		    if (de[i].start_block!=0) {
			printf("   [%d] %s %d %s %d\n", i, de[i].filename,
			       de[i].start_block, ((de[i].flag&1)==1) ? "dir " : "file",
			       de[i].file_length);
			map[de[i].start_block] = ((de[i].flag&1) ==1) ? IS_DIR : IS_FILE;
		    }
	    }
	    else {
		printf("Block %d: data\n", blk);
		for (i = 0; i < 16; i++)
		    if (!isprint(buf[i]))
			buf[i] = ' ';
		printf("   '%.16s'...\n", buf);
	    }
	    if (fat[blk]!=-2)
		map[fat[blk]] = map[blk];
	}
	blk++;
    }
    fclose(fp);
    return 0;
}
