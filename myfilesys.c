#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "fs_struct.h"

int32_t* fattable = NULL;
struct Directory_entry directoryblock[16]; // 1024/64=dirperblock
struct Superblock super;
int disk_fd;

//int read_block(int fd, int blocknum, void* wblock){
/*
void writeback_fat(){
	int i;
	for(i = 0; i< super.fat_size; i++){
		write_block(disk_fd, i+1, (void*)fattable[i]);
	}
}
*/

//max path of 100
int find_path(const char *path, char *names[100]){
	char *token;
	char * temp;
	int count=0;
	strcpy(temp, path);
	while((token = strsep(&temp, "/")) != NULL){
		names[count] = token;
		count++;
	}
	return count;
}

//always starts at root directory
int find(const char* path, struct Directory_entry* de, int* offset){
	int i;
	int j;
	int start;
	void* rblock_rootdir = calloc(1024,1);
	int count;
	char * names[100];
	count = find_path(path, names);
	for(i = 0; i<count; i++){
		if(i == 0){
			read_block(disk_fd, super.root_block, (void*)directoryblock);
			//memcpy(directoryblock, rblock_rootdir, sizeof(directoryblock));
			//memcpy(de, rblock_rootdir, sizeof(struct Directory_entry));
			//assert(de->filename == ".");
		} else {
			read_block(disk_fd, de->start_block, (void*) directoryblock);
		}
		j = 0;
		//search 
		while(j<16){
			//found name
			if(!strcmp(directoryblock[j].filename, names[i])){
				//correct directory
				if(((directoryblock[j].flag & 1) == 1) && directoryblock[j].start_block != 0 ){
					start = de->start_block;
					memcpy(de, &directoryblock[j], sizeof(struct Directory_entry));
					break;
					//go to next level
				} else if ((directoryblock[j].flag & 1) == 0){
					//not a directory
					return -1;
				}

			}
			j++;
		}
		//checked all directory entries and couldnt find
		if(j == 16)
			return -1;
	}
	*offset = j;
	free(rblock_rootdir);
	return start;
}

//initiallize local block for super block, fat block and directory block
static void* my_init(struct fuse_conn_info* conn){
	int i;
	void* rblock_super = calloc(1024, 1);
	
	//fread(rblock, sizeof(rblock), 1, disk_fd);
	read_block(disk_fd, 0, rblock_super);
	memcpy(&super, rblock_super, sizeof(struct Superblock));
	//assert(super.magicnum == 0x2345beef);

	void* rblock_fat = calloc(1024, super.fat_size);

	for(i=0; i<super.fat_size; i++){
		read_block(disk_fd, i+1, (char*)rblock_fat+(i*1024));
	}

	memcpy(fattable, rblock_fat, 1024* super.fat_size);
	return NULL;
}

static int my_getattr(const char *path, struct stat *stbuf){
	void* dir_block = calloc(sizeof(struct Directory_entry),1);
	struct Directory_entry *de = dir_block;
	int where;
	int offset;
	where = find(path, de,&offset);
	if(where < 0){
		return -1;
	} else {
		stbuf->st_ctime= de->creation_time;
		stbuf->st_atime= de->access_time;
		stbuf->st_dev = 0;
		stbuf->st_ino = 0;
		stbuf->st_nlink = 1;
		stbuf->st_mtime = de->access_time;
		//stbuf->st_birthtime = 0;
		stbuf->st_size = super.total_blocks * 1024;
		stbuf->st_blksize = super.block_size;
		stbuf->st_blocks = super.total_blocks * 2;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		stbuf->st_mode = 0040777;
	}
	free(dir_block);
	return 0;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	//Make new directory block and replace it
	int x = 0;
	int i=1;
	int blockIs = find(path, directoryblock, &x);
	if(blockIs<0){
		return -1;
	}

	read_block(disk_fd, blockIs, (void*)directoryblock);
	
	//file is only 1 block long
	if(directoryblock[x].file_length <= 1024){
		read_block(disk_fd, directoryblock[x].start_block, (void*)buf);
		return directoryblock[x].file_length;

	} else {
		int next;
		next = fattable[directoryblock[x].start_block];
		read_block(disk_fd, directoryblock[x].start_block, (void*)buf);
		//fattable[directoryblock[x].start_block] = 0;
		//directoryblock[x].start_block = 0;
		//goes through fat chain 
		while(next != -2){
			int oldnext = next;
			next = fattable[next];
			read_block(disk_fd, fattable[oldnext], (void*)buf + (i*1024) );
			i++;
		}
		//clears -2
		read_block(disk_fd, fattable[next], (void*)buf + (i*1024) );

	}

	
	return 0;

}


static int my_mkdir(const char *path, mode_t mode){
	
	int i,j;
	//void* fat_block = calloc(1024, 1);
	void* dir_block = calloc(sizeof(struct Directory_entry), 1);
	struct Directory_entry *de = dir_block;
	//struct Fatblock *fatblock = fat_block;


	int offset;
	int block_position = find(path, de, &offset); //get path from path function --to be written
	
	int free_index = -1;
	int fat_index = -1;
	//search for first available place in FAT
	for (i = 0; i < super.fat_size; i++){
		fat_index = i;
		for (j = 0; j < 256; j++){
			free_index = j*(i+1);
			if (fattable[free_index] == 0){
				fattable[free_index]=-2;
				break;
			}
		}
	}
	read_block(disk_fd, block_position, (void*)directoryblock);
	//search for free directory entry
	int empty_dir;
	for(i = 0; i<16; i++){
		if(directoryblock[i].start_block == 0){
			empty_dir = i;
			break;
		}	
	}
	//local changes
	char* name = strrchr(path,'/');
	strcpy(directoryblock[empty_dir].filename, name+1);
	directoryblock[empty_dir].creation_time = time(NULL);
	directoryblock[empty_dir].access_time = time(NULL);
	directoryblock[empty_dir].file_length = 0;
	directoryblock[empty_dir].start_block = free_index;
	directoryblock[empty_dir].flag |= 1;

	//write fattable back
	for(i = 0; i< super.fat_size; i++){
		write_block(disk_fd, i+1, (void*)fattable + (i*1024));
	}

	//writeback directory block
	write_block(disk_fd, block_position, (void*)directoryblock);
	
		
	return 0;
}


// create - create a new file
static int my_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
	int i,j;
	//void* fat_block = calloc(1024, 1);
	void* dir_block = calloc(sizeof(struct Directory_entry), 1);
	struct Directory_entry *de = dir_block;
	//struct Fatblock *fatblock = fat_block;


	int offset;
	int block_position = find(path, de, &offset); //get path from path function --to be written
	

	int free_index = -1;
	int fat_index = -1;
	//search for first available place in FAT
	for (i = 0; i < super.fat_size; i++){
		fat_index = i;
		for (j = 0; j < 256; j++){
			free_index = j*(i+1);
			if (fattable[free_index] == 0){
				fattable[free_index]=-2;
				break;
			}
		}
	}
	read_block(disk_fd, block_position, (void*)directoryblock);
	//search for free directory entry
	int empty_dir;
	for(i = 0; i<16; i++){
		if(directoryblock[i].start_block == 0){
			empty_dir = i;
			break;
		}	
	}
	//local changes
	char* name = strrchr(path,'/');
	strcpy(directoryblock[empty_dir].filename, name+1);
	directoryblock[empty_dir].creation_time = time(NULL);
	directoryblock[empty_dir].access_time = time(NULL);
	directoryblock[empty_dir].file_length = 0;
	directoryblock[empty_dir].start_block = free_index;
	directoryblock[empty_dir].flag &= -2; //zeros out first bit

	//write fattable back
	for(i = 0; i< super.fat_size; i++){
		write_block(disk_fd, i+1, (void*)fattable + (i*1024));
	}

	//writeback directory block
	write_block(disk_fd, block_position, (void*)directoryblock);
	

    return 0;
}


static int my_rmdir(const char *path){
	int x = 0;
	//offset stored in x
	struct Directory_entry* de = calloc(sizeof(struct Directory_entry),1);
	int blockIs = find(path, de, &x);

	//is not directory
	if((de->flag & 1)==0)
		return -1;

	if(de->file_length <= 128){
		void* rblock = calloc(1024, 1);
		read_block(disk_fd, blockIs, rblock);
		//de->file_length = 0; 
		memcpy(directoryblock, rblock, sizeof(directoryblock));
		directoryblock[x].file_length = 0;
		write_block(disk_fd, blockIs, (void*) directoryblock);
		return 0;
	}else{ 
		return -1;}

}

static int my_rename(const char *from, const char *to){
	void* dir_block = calloc(sizeof(struct Directory_entry),1);
    struct Directory_entry *de = dir_block;
	int where;
	int offset;
	void* rblock = calloc(1024,1);
	if(strcmp("/",from)){//check if they are trying to rename root
		return -1;
	}else if(strcmp(to,"/")){//dont rename it to root
		return -1;
	} else {
		where = find(from,de,&offset);
		if(where < 0){//source is there
		return where;
	}
		char* name = strrchr(to, '/');
		strcpy(directoryblock[offset].filename, name+1);
		read_block(disk_fd, offset, rblock);
        memcpy(directoryblock, rblock, sizeof(directoryblock));
        write_block(disk_fd, offset, (void*) directoryblock);
	}
        return 0;
}


//open a file and returns the files starting block 
static int my_open(const char *path, struct fuse_file_info *fi){
	
	int x = 0;
	void* block = calloc(1024, 1);
	struct Directory_entry* tmp = calloc(sizeof(struct Directory_entry),1);
	int blockIs = find(path, tmp, &x );

	if(blockIs<0){
		return -1;
	}
	return tmp->start_block;
}



static int my_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi){
	
	int x = 0;
	void* block = calloc(1024, 1);
	struct Directory_entry* tmp = calloc(sizeof(struct Directory_entry),1);
	int blockIs = find(path, tmp, &x );

	int i=1;
	if(blockIs<0){
		return -1;
	}

	read_block(disk_fd, blockIs, (void*)directoryblock);
	
	//file is only 1 block long
	if(directoryblock[x].file_length <= 1024){
		read_block(disk_fd, directoryblock[x].start_block, (void*)buf);
		return directoryblock[x].file_length;

	} else {
		int next;
		next = fattable[directoryblock[x].start_block];
		read_block(disk_fd, directoryblock[x].start_block, (void*)buf);
		//fattable[directoryblock[x].start_block] = 0;
		//directoryblock[x].start_block = 0;
		//goes through fat chain 
		while(next != -2){
			int oldnext = next;
			next = fattable[next];
			read_block(disk_fd, fattable[oldnext], (void*)buf + (i*1024) );
			i++;
		}
		//clears -2
		read_block(disk_fd, fattable[next], (void*)buf + (i*1024) );

	}


	return directoryblock[x].file_length;
}


static int my_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi){
/*
	int x = 0;
	int blockIs = find(path, directoryblock, &x);
	if(blockIs<0){return blockIs;}
	int bytesRead;
	if(blockIs==0){return bytesRead = write_block(disk_fd, blockIs, buf);}
	*/

	int x = 0;
	void* block = calloc(1024, 1);
	struct Directory_entry* tmp = calloc(sizeof(struct Directory_entry),1);
	int blockIs = find(path, tmp, &x );


	int i=1;
	int bytesLeft=size;
	if(blockIs<0){
		return -1;
	}

	read_block(disk_fd, blockIs, (void*)directoryblock);
		
	//file is only 1 block long
	if(size <= 1024){
		//read_block(disk_fd, directoryblock[x].start_block, (void*)buf);
		return write_block(disk_fd, directoryblock[x].start_block, (void*)buf);

	} else {
		int next;
		int temp;
		next = fattable[directoryblock[x].start_block];
		//temp = write_block(disk_fd, directoryblock[x].start_block, (void*)buf);
		//fattable[directoryblock[x].start_block] = 0;
		//directoryblock[x].start_block = 0;
		//goes through fat chain 
		//bytesLeft -= temp;
		while(bytesLeft > 0){
			int oldnext = next;
			next = fattable[next];
			//read_block(disk_fd, fattable[oldnext], (void*)buf + (i*1024) );
			temp = write_block(disk_fd, fattable[oldnext], (void*)buf + (i*1024) );
			bytesLeft -= temp;
			i++;
		}
		//clears -2
		//read_block(disk_fd, fattable[next], (void*)buf + (i*1024) );
	}
	return 0;
}


// unlink - delete a file
static int my_unlink(const char *path)
{
	int x = 0;
	void* block = calloc(1024, 1);
	struct Directory_entry* tmp = calloc(sizeof(struct Directory_entry),1);
	int blockIs = find(path, tmp, &x );

	//if found entry is not a file
	if((tmp->flag & 1) == 1){
		return -1;
	}

	read_block(disk_fd, blockIs, (void*)directoryblock);
	
	//file is a directory
	if((directoryblock[x].flag & 1)==1){
		return -1;
	}
	//file is only 1 block long
	if(directoryblock[x].file_length <= 1024){
		directoryblock[x].start_block = 0;
		fattable[directoryblock[x].start_block] = 0;
	} else {
		int next;
		next = fattable[directoryblock[x].start_block];
		fattable[directoryblock[x].start_block] = 0;
		directoryblock[x].start_block = 0;
		//goes through fat chain and frees them
		while(next != -2){
			int oldnext = next;
			next = fattable[next];
			fattable[oldnext] = 0;
		}
		//clears -2
		fattable[next] = 0;
	}
	//update fat table	
	int i;
	for(i = 0; i< super.fat_size; i++){
		write_block(disk_fd, i+1, (void*)fattable + (i*1024));
	}

	//update directory block
	write_block(disk_fd, blockIs, (void*) directoryblock);
	
	return 0;
}


static struct fuse_operations my_oper = {
	.getattr	= my_getattr,
	//.readdir	= my_readdir,
	//.mkdir		= my_mkdir,
	.rmdir		= my_rmdir,
	.rename		= my_rename,
	//.open		= my_open,
	//.read		= my_read,
	//.write		= my_write,
	.unlink		= my_unlink,
	.create		= my_create
};

int main(int argc, char *argv[]){
	//char* file;
	//strcpy(file, argv[1]);
	
	if ((disk_fd = open("mydisk.img", O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0){
		perror("couldnt open file");	
		return -1;
	}
	return fuse_main(argc, argv, &my_oper, NULL);
}
