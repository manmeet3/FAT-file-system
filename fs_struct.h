#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


struct Directory_entry {
         char filename[24]; //up to 24 bytes
         int64_t creation_time;
         int64_t access_time;
         uint32_t file_length;
         int32_t start_block;
         uint32_t flag;
         uint32_t unused;
};

struct Superblock {
         uint32_t magicnum; //0x2345beef
         uint32_t total_blocks; //N (passed in as param)
         uint32_t fat_size; // = k = (N / 256)
         uint32_t block_size; // 1024
         uint32_t root_block; // k + 1
};

struct Fatblock {
         int32_t fatentry[256];
};

//read and write block to disk image
int write_block(int fd, int blocknum, void* wblock);
int read_block(int fd, int blocknum, void* wblock);
