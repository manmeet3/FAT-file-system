all: createdisk myfilesys readimg

createdisk: diskopts
	cc -o createdisk createdisk.c diskopts.o

diskopts: 
	cc -c diskopts.c

readimg:
	cc -o read-img read-img.c

myfilesys: 
	cc -D_FILE_OFFSET_BITS=64 -I/usr/local/include/fuse  -pthread -L/usr/local/lib -lfuse -lrt diskopts.o myfilesys.c -o myfilesys

clean:
	rm *.o
	rm createdisk
	rm myfilesys
	rm read-img
