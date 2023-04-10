CC = gcc
CFLAGS = -fdiagnostics-color=always -O3 #-Wall -Werror -Wextra

EXT2_FS = ext2_filesystem.bin

all:
	$(CC) $(CFLAGS) main.c ext2.c fs_util.c

run:
	$(MAKE) all
	./a.out

clean:
	$(RM) $(EXT2_FS)

mount:
	sudo mount -t ext2 -o loop -v $(EXT2_FS) /mnt

unmount:
	sudo umount -v /mnt

remount:
	$(MAKE) run
	$(MAKE) unmount
	$(MAKE) mount
