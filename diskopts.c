#include "fs_struct.h"
#include <fcntl.h>
#include <unistd.h>

int write_block(int fd, int blocknum, void* wblock){
	lseek(fd, blocknum*1024, SEEK_SET);
	return write(fd, wblock, 1024);
}

int read_block(int fd, int blocknum, void* rblock){
	lseek(fd, blocknum*1024, SEEK_SET);
	return read(fd, rblock, 1024);
}

