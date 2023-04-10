#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ext2.h"

#define BOOT_RECORD_SIZE 1024

uint8_t* disk_data = NULL;

// sudo mount -t ext2 -o loop -v ext2_filesystem_c.bin /mnt
// sudo umount -v  /mnt

int main(){
    ext2_setup_config config = {0, 10, 40, 1, 1};

    const size_t BLOCK_SIZE_BYTES = (1024 << config.block_size);

    size_t disk_size = BOOT_RECORD_SIZE +
                       (sizeof(ext2_super_block) + BLOCK_SIZE_BYTES) +
                       (BLOCK_SIZE_BYTES*2 + BLOCK_SIZE_BYTES*config.num_inodes_tables_per_group + BLOCK_SIZE_BYTES*config.num_blocks_per_group) * config.num_groups;

    printf("Trying to allocate %i bytes (%i KiB) of memory\n", (int)disk_size, (int)disk_size / 1024);

    disk_data = (uint8_t*) calloc(disk_size, 1);

    if(disk_data == NULL){
        printf("malloc failed!\n");
        return -1;
    }else{
        printf("Create disk with size: %zu Bytes (%zu KiB)\n", disk_size, disk_size/1024);
    }

    //ext2_super_block* super_block = (ext2_super_block*)(disk_data + BOOT_RECORD_SIZE);

    if(ext2_setup(config, disk_data, disk_size)!=0){
        printf("Failed to setup ext2!\n");
        return -1;
    }




    FILE* file;
    const char* fs_name = "ext2_filesystem.bin";
    file = fopen(fs_name, "w");

    if(file == NULL){
        printf("Failed to create file!\n");
        return -1;
    }

    size_t wrote = fwrite(disk_data, 1, disk_size, file);

    if(wrote != disk_size){
        printf("Failed to write disk_data (%zu)\n", wrote);
        fclose(file);
        return -1;
    }

    printf("Created file system!\n");
    fclose(file);
    return 0;
}