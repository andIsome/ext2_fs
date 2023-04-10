#include "ext2.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "fs_util.h"

#define ROOT_DIR_INUM ((uint32_t)2)
#define INVALID_INUM  ((uint32_t)0)

#define MIN(a, b) ((a)<(b)?(a):(b))
#define MAX(a, b) ((a)>(b)?(a):(b))

typedef struct{
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t num_groups;
    uint32_t block_size;
    uint32_t inode_blocks_per_group;
    uint32_t inodes_per_block;
} ext2_layout;

typedef struct{
    char* str;
    uint32_t strlen;
} c_string;

typedef struct {
    ext2_super_block sb;
    ext2_layout layout;
    // Some drive info to be added
    // void* cached_blocks[10];
    //ext2_block_cache_descriptor bcd[10];

    ext2_block_group* bg_table;
    uint32_t bg_num_blocks;

    // ext2_inode* cached_inodes[50];
    // Cache bg and other stuff
    // Some cache book keeping to be added
    int is_mounted; // is mounted ?
    // drive_handle drive;
    int ro;
} ext2_fs;

#define C_STRING_INIT(str) (c_string){str, sizeof(str)-1}


// Only if the sparse_super feature flag is set
int group_contains_sb(uint32_t group_index){
    return group_index % 3 == 0
           || group_index % 5 == 0
           || group_index % 7 == 0;
}

uint32_t get_num_superblocks(uint32_t group_num, int is_sparse){
    uint32_t num_super_blocks = group_num / 3 + group_num / 5 + group_num / 7
                                - group_num / (3*5) - group_num / (3*7) - group_num / (5*7) + group_num / (3*5*7) + 1;
    return is_sparse ? num_super_blocks : group_num + 1;
}

// FIXME BADLY
void* get_block(void* memory, uint32_t block_num, ext2_layout layout){
    if(block_num<=1 || block_num > layout.blocks_per_group * layout.num_groups)
        return NULL;

    return memory + block_num * layout.block_size;
}

void* data_block_get(void* memory, uint32_t db_num, ext2_layout layout){
    // uint32_t group_num    = db_num / layout.blocks_per_group;
    // uint32_t local_db_num = db_num % layout.blocks_per_group;

    // ext2_block_group* block_group = table + group_num;

    return get_block(memory, db_num, layout);
}

ext2_inode* inode_get(void* memory, uint32_t inum, ext2_block_group* table, ext2_layout layout){
    if(inum == 0 || inum > layout.inodes_per_group * layout.num_groups){
        return NULL;
    }

    uint32_t group_num  = (inum - 1) / layout.inodes_per_group;
    uint32_t local_inum = (inum - 1) % layout.inodes_per_group;

    ext2_block_group* block_group = table + group_num;

    uint32_t inode_table_num   = local_inum / layout.inodes_per_block;
    uint32_t inode_table_index = local_inum % layout.inodes_per_block;

    ext2_inode* inode_table = get_block(memory, block_group->bg_inode_table + inode_table_num, layout);

    return (inode_table + inode_table_index);
}

uint16_t imode_create(uint16_t file_type, uint16_t exec_override, uint16_t acl){
    if(file_type & ~0xF000){
        printf("Invalid file type 0x%04X\n", file_type);
    }else if(exec_override & ~0x0E00){
        printf("Invalid exec override 0x%04X\n", exec_override);
    }else if(acl & ~0x01FF){
        printf("Invalid acl 0x%04X\n", acl);
    }else{
        return file_type | exec_override | acl;
    }
    return 0;
}

uint16_t imode_ftype(uint16_t imode, uint16_t ftype){
    return (imode & 0xF000) == ftype;
}

// --------------------- block (de)alloc ---------------------

uint32_t alloc_inode(void* memory, ext2_block_group* bg, ext2_layout layout){ //, uint32_t num_blocks_in_table
    for(int i=0;i<layout.num_groups;i++){

        void* bitmap = get_block(memory, (bg+i)->bg_inode_bitmap, layout);

        uint32_t local_inum = bitmap_alloc64(bitmap, layout.block_size);

        if(local_inum != ~((uint32_t)0)){
            uint32_t inum = i * layout.inodes_per_group + local_inum + 1;

            printf("Allocated inode: %u (local: %u)\n", inum, local_inum);

            (bg+i)->bg_free_inodes_count--;

            return inum;
        }

        printf("[%s] Inode bitmap of group %i is full!\n", __func__, i);
    }
    return ~((uint32_t)0);
}

uint32_t alloc_block(void* memory, ext2_block_group* bg, ext2_layout layout){
    for(int i=0;i<layout.num_groups;i++) {

        void *bitmap = get_block(memory, (bg + i)->bg_block_bitmap, layout);

        uint32_t local_bnum = bitmap_alloc64(bitmap, layout.block_size);

        if(local_bnum != ~((uint32_t)0)){
            uint32_t bnum = local_bnum;// + layout.blocks_per_group * i // FIXME HOW TO DO TRANSLATION

            printf("Allocated block: %u (local: %u)\n", bnum, local_bnum);

            (bg + i)->bg_free_blocks_count--;

            return bnum;
        }

        printf("[%s] Block bitmap of group %i is full!\n", __func__, i);
    }
    return ~((uint32_t)0);
}

int dealloc_inode(void* memory, uint32_t inum, ext2_block_group* bg_table, ext2_layout layout){
    uint32_t bg_num     = (inum - 1) / layout.inodes_per_group;
    uint32_t local_inum = (inum - 1) % layout.inodes_per_group;

    void* bitmap = get_block(memory, (bg_table + bg_num)->bg_inode_bitmap, layout);

    int ret = bitmap_free(bitmap, local_inum, layout.block_size);

    if(!ret){
        (bg_table + bg_num)->bg_free_inodes_count++;
    }else{
        printf("[%s:%i] ERROR corrupted?!\n", __func__, __LINE__);
    }

    printf("%s: Deallocated inode: %u (local: %u)\n", __func__, inum, local_inum);

    return ret;
}

int dealloc_block(void* memory, uint32_t bnum, ext2_block_group* bg_table, ext2_layout layout){
    uint32_t bg_num     = 0;    // FIXME
    uint32_t local_bnum = bnum; // FIXME

    void* bitmap = get_block(memory, (bg_table + bg_num)->bg_block_bitmap, layout);

    int ret = bitmap_free(bitmap,bnum,layout.block_size);

    if(!ret){
        (bg_table + bg_num)->bg_free_blocks_count++;
    }else{
        printf("[%s:%i] ERROR corrupted?!\n", __func__, __LINE__);
    }

    printf("%s: Deallocated inode: %u\n", __func__, bnum);
    return ret;
}


// -------------------- File resize operations -----------------------------


void add_data_blocks(void* memory, ext2_inode* inode, uint32_t num_blocks, int zero_out_page, ext2_block_group* bg, ext2_layout layout){
    // Used to index inode->i_block[] array
    uint32_t block_index = inode->i_blocks / (layout.block_size / 512);

    const uint32_t REF_PER_BLOCK = layout.block_size / sizeof(uint32_t);

    for(uint32_t i=0; i < num_blocks; i++){
        uint32_t block_num_to_add = alloc_block(memory,bg,layout);

        if(zero_out_page){
            void* block = get_block(memory, block_num_to_add, layout);
            memset(block, 0, layout.block_size);
        }

        if(block_index < 12){
            inode->i_block[block_index] = block_num_to_add;
        }else if(block_index-12 < REF_PER_BLOCK){
            // Check if the indirect block has already been allocated
            if(12 == (inode->i_blocks / (layout.block_size / 512))){
                inode->i_block[12] = alloc_block(memory,bg,layout);
            }
            uint32_t* indirect_block = get_block(memory, inode->i_block[12], layout);
            indirect_block[block_index-12] = block_num_to_add;
        }else {
            printf("ERR NOT IMPLEMENTED!!!\n");
            exit(-1);
        }
        // Register the size increase
        inode->i_blocks += layout.block_size / 512;
        block_index++;
    }
}

int remove_data_block(void* memory, ext2_inode* inode, uint32_t block_index, ext2_block_group *bg, ext2_layout layout){
    if(block_index < 12){
        uint32_t bnum_to_remove = inode->i_block[block_index];

        // See below same statement, but because this is not yet supported, exit before making any changes!
        if(inode->i_blocks / (layout.block_size / 512) > 12){
            printf("ERR: FIXME: %s %i\n", __func__, __LINE__);
            return -1;
        }

        // Unnecessary, it moves all 12 direct blocks one position up // FIXME
        memmove(inode->i_block + block_index, inode->i_block + block_index + 1, sizeof(uint32_t) * (12 - (block_index + 1)));

        dealloc_block(memory, bnum_to_remove, bg, layout);

        // Test if we have to move indirect references
        // if(inode->i_blocks / (layout.block_size / 512) > 12){
        //      Move indirect blocks
        // }

        // Update i_size if the block is within its 'boundary'
        if((block_index + 1) * layout.block_size >= inode->i_size) {
            inode->i_size -= layout.block_size;
        }
        inode->i_blocks -= layout.block_size / 512;
        return 0;
    }

    // FIXME implement indirect blocks
    printf("ERR: FIXME: %s\n", __func__);
    return -1;
}


// -------------------- Inode bnum lookup util ------------------------------


uint32_t get_bnum_from_inode(void* memory, const ext2_inode* inode, uint32_t index, ext2_layout layout){
    const uint32_t REF_PER_BLOCK = layout.block_size / sizeof(uint32_t);

    // Direct blocks
    if(index < 12){
        return inode->i_block[index];
    }

    index -= 12; // 'Reset/Realign' index

    // 1 Layer of indirection
    if(index < REF_PER_BLOCK){
        uint32_t* first_layer = data_block_get(memory, inode->i_block[12], layout);
        return first_layer[index];
    }

    index -= REF_PER_BLOCK; // 'Reset/Realign' index

    // 2 Layers of indirection
    if(index < REF_PER_BLOCK * REF_PER_BLOCK){
        uint32_t* first_layer = data_block_get(memory, inode->i_block[13], layout);
        uint32_t* second_layer = data_block_get(memory, first_layer[index / REF_PER_BLOCK], layout);
        return second_layer[index % REF_PER_BLOCK];
    }

    index -= REF_PER_BLOCK * REF_PER_BLOCK; // 'Reset/Realign' index

    // 3 Layers of indirection
    if(index < REF_PER_BLOCK * REF_PER_BLOCK * REF_PER_BLOCK){
        uint32_t* first_layer  = data_block_get(memory, inode->i_block[14], layout);
        uint32_t* second_layer = data_block_get(memory, first_layer[index / (REF_PER_BLOCK * REF_PER_BLOCK)], layout);
        uint32_t* third_layer  = data_block_get(memory, second_layer[(index / REF_PER_BLOCK) % REF_PER_BLOCK], layout);
        return third_layer[index % REF_PER_BLOCK];
    }

    printf("ERR: %s at %d\n", __FILE__, __LINE__);
    exit(-1);
}


// --------------------- file read/write ------------------------------------

size_t read(void* memory, const ext2_inode* inode, size_t size, void* buffer, size_t offset, ext2_layout layout){
    if(offset >= inode->i_size){
        return 0;
    }

    size_t to_read = MIN(inode->i_size - offset, size); // Make sure to not exceed file size
    size_t buffer_pos = 0;

    uint32_t curr_block_index = offset / layout.block_size;

    while(to_read > 0){
        uint32_t bnum = get_bnum_from_inode(memory, inode, curr_block_index, layout);
        uint32_t block_offset = offset % layout.block_size;
        void* block = data_block_get(memory, bnum, layout);

        size_t copied = MIN(layout.block_size-block_offset,to_read); // Make sure to not exceed block size
        memcpy(buffer + buffer_pos, block + block_offset, copied);

        to_read -= copied;
        buffer_pos += copied;
        offset += copied;
        curr_block_index++;
    }
    return buffer_pos;
}

size_t write(void* memory, ext2_inode* inode, size_t size, const void* buffer, size_t offset, ext2_layout layout, ext2_block_group* bg){
    if(imode_ftype(inode->i_mode, EXT2_S_IFDIR)) {printf("ERR: cannot write dir/lnk! %s\n", __func__ ); return 0;}

    uint32_t new_size = offset + size;
    // Check if file is big enough
    if(new_size > inode->i_blocks * 512) {

        // Pad the new size out to a multiple of a block_size
        uint32_t new_size_padded = pad_multiple_of(new_size, layout.block_size);

        // Calculate how many new blocks are needed
        uint32_t new_blocks = (new_size_padded / layout.block_size) - inode->i_blocks / (layout.block_size / 512);

        add_data_blocks(memory, inode, new_blocks, 0, bg, layout);
    }

    // (Not direct) index into the list of block in the inode
    uint32_t curr_block_index = offset / layout.block_size;
    uint32_t buff_pos = 0;
    size_t to_write = size;

    while(to_write > 0){
        // Get the data block number
        uint32_t bnum = get_bnum_from_inode(memory, inode, curr_block_index, layout);
        void* block = data_block_get(memory, bnum, layout);

        uint32_t block_offset = offset % layout.block_size;

        // Get the size to write on the current block
        size_t wrote = MIN(layout.block_size-block_offset,to_write);
        memcpy(block + block_offset,buffer+buff_pos,wrote);

        to_write -= wrote;
        offset   += wrote;
        buff_pos += wrote;
        curr_block_index++;
    }
    // Update the file size
    inode->i_size = MAX(new_size, inode->i_size);

    return buff_pos;
}


// ------------------------- Directory entry modifications ------------------------

ext2_dir_entry* get_next_entry(ext2_dir_entry* dir_entry){
    return (ext2_dir_entry*)((uint8_t*)dir_entry + dir_entry->rec_len);
}

const char* ft_strings[] = {
        "UNKNOWN",
        "REG_FILE",
        "DIR",
        "CHRDEV",
        "BLKDEV",
        "FIFO",
        "SOCK",
        "SYMLINK"
};

void dentry_add(void* memory, ext2_inode* inode, ext2_inode* entry_inode, uint32_t entry_inum, const char* name, uint8_t type, ext2_block_group* bg, ext2_layout layout){
    ext2_dir_entry dir_to_add;
    dir_to_add.inode = entry_inum;
    dir_to_add.file_type = type;
    dir_to_add.name_len = strlen(name);
    dir_to_add.rec_len = sizeof(ext2_dir_entry) + dir_to_add.name_len;

    // Add padding to be multiple of 4 bytes
    dir_to_add.rec_len = pad_multiple_of(dir_to_add.rec_len, 4);

    uint32_t dir_inum = ((void*)inode - get_block(memory, bg->bg_inode_table, layout))/sizeof(ext2_inode) + 1; // Hack // FIXME BADLY
    printf("Adding %s \"%s\"(%u) to inode: %u at block: %u\n",ft_strings[type], name,entry_inum,dir_inum, inode->i_block[0]);

    if(type == EXT2_FT_DIR)
        inode->i_links_count++;
    if(entry_inode)
        entry_inode->i_links_count++;

    uint32_t block_index = 0;

    // Write entry to block
    while(block_index < 12){ // FIXME later?

        if(block_index + 1 > inode->i_blocks / (layout.block_size / 512))
            add_data_blocks(memory, inode, 1, 1, bg, layout);

        uint32_t bnum = get_bnum_from_inode(memory, inode, block_index, layout);
        void* block = get_block(memory, bnum, layout);
        ext2_dir_entry* entry_it = (ext2_dir_entry*)block;

        // Test if block is empty
        if(block_index * layout.block_size >= inode->i_size) {
            dir_to_add.rec_len = layout.block_size;
            inode->i_size += layout.block_size;
            *entry_it = dir_to_add;
            memcpy(entry_it + 1, name, dir_to_add.name_len);
            return;
        }

        uint32_t block_offset = 0;
        while(block_offset<layout.block_size){

            uint32_t actual_entry_size = pad_multiple_of(entry_it->name_len + sizeof(ext2_dir_entry), 4);

            // Check if dir_entry uses up more space than needed
            if(entry_it->rec_len > actual_entry_size){

                // Calc unused space of entry_it
                uint32_t available = entry_it->rec_len - actual_entry_size;

                // Test if new entry can fit in the unused space
                if(available >= dir_to_add.rec_len){
                    uint32_t leftover = available - dir_to_add.rec_len;

                    entry_it->rec_len = actual_entry_size;
                    dir_to_add.rec_len += leftover;

                    ext2_dir_entry* new_entry = (ext2_dir_entry*) ((uint8_t*) entry_it + entry_it->rec_len);
                    *new_entry = dir_to_add;
                    memcpy(new_entry+1, name, dir_to_add.name_len);
                    return;
                }

            }
            block_offset += entry_it->rec_len;
            entry_it = (ext2_dir_entry*)((uint8_t*) entry_it + entry_it->rec_len);
        }
        block_index++;
    }
}

int dentry_remove(void* memory, ext2_inode* inode, const char* file_name, size_t file_name_len, ext2_block_group *bg, ext2_layout layout){
    printf("Removing dentry: %.*s\n", (int)file_name_len, file_name);
    uint32_t num_blocks = inode->i_size / layout.block_size;

    for(uint32_t i = 0; i < num_blocks; ++i){
        uint32_t bnum = get_bnum_from_inode(memory, inode, i, layout);
        ext2_dir_entry* dir_entry = get_block(memory, bnum, layout);
        ext2_dir_entry* last_dir_entry = dir_entry;
        uint32_t block_offset = 0;

        while(block_offset < layout.block_size){

            if(dir_entry->name_len == file_name_len && strncmp((const char*)(dir_entry+1), file_name, file_name_len)==0){
                printf("Found entry!\n");
                if(block_offset == 0){ // Check if it is the first element
                    // Check if there are other dir entries on the same block
                    if(dir_entry->rec_len != layout.block_size){
                        // Move the next dir entry to the current pos
                        ext2_dir_entry *next_dir = get_next_entry(dir_entry);
                        uint32_t next_dir_size = next_dir->name_len + sizeof(ext2_dir_entry);
                        next_dir->rec_len += dir_entry->rec_len;

                        memcpy(dir_entry, next_dir, next_dir_size);
                        return 0;

                    }else { // Block is empty, so remove it
                        return remove_data_block(memory, inode, i, bg, layout);
                    }
                }else{
                    printf("Extended the dir entry size before\n");
                    // Override the current dir entry
                    last_dir_entry->rec_len += dir_entry->rec_len;
                    return 0;
                }
            }

            block_offset += dir_entry->rec_len;

            last_dir_entry = dir_entry;
            dir_entry = get_next_entry(dir_entry);
        }
    }

    return -1;
}

int dentry_rename(void* memory, ext2_inode* inode, const c_string old_fname, const c_string new_fname, ext2_block_group *bg, ext2_layout layout){
    printf("Renaming %.*s to %.*s\n", old_fname.strlen, old_fname.str, new_fname.strlen, new_fname.str);
    uint32_t num_blocks = inode->i_size / layout.block_size;

    for(uint32_t i = 0; i < num_blocks; ++i){

        uint32_t bnum = get_bnum_from_inode(memory, inode, i, layout);
        ext2_dir_entry* dir_entry = get_block(memory, bnum, layout);

        uint32_t block_offset = 0;

        while(block_offset < layout.block_size){

            if(dir_entry->name_len == old_fname.strlen && strncmp((const char*)(dir_entry+1), old_fname.str, old_fname.strlen) == 0){
                uint32_t renamed_entry_size = pad_multiple_of(sizeof(ext2_dir_entry) + new_fname.strlen, 4);
                printf("Found dentry to rename\n");
                if(renamed_entry_size <= dir_entry->rec_len){
                    printf("The new name fits within the old entry\n");
                    memcpy(dir_entry+1, new_fname.str, new_fname.strlen);
                    dir_entry->name_len = new_fname.strlen;
                    // Do not update rec_len (look at if)
                }else{
                    printf("The new entry is to big!\n");
                    dentry_add(memory, inode, NULL, dir_entry->inode,new_fname.str,dir_entry->file_type,bg,layout);
                    dentry_remove(memory, inode, old_fname.str,old_fname.strlen,bg,layout);
                }
                return 0;
            }

            block_offset += dir_entry->rec_len;
            dir_entry = get_next_entry(dir_entry);
        }
    }

    return -1;
}

uint32_t dentry_lookup(void* memory, ext2_inode* inode, const c_string fname, ext2_block_group* bg, ext2_layout layout){
    printf("Searching in directory for %.*s (%u)\n", fname.strlen, fname.str, fname.strlen);

    if(!imode_ftype(inode->i_mode, EXT2_S_IFDIR)){
        return INVALID_INUM;
    }

    uint32_t num_blocks = inode->i_size / layout.block_size;

    for(uint32_t i = 0; i < num_blocks; ++i){

        uint32_t bnum = get_bnum_from_inode(memory, inode, i, layout);
        ext2_dir_entry* dir_entry = get_block(memory, bnum, layout);

        uint32_t block_offset = 0;

        while(block_offset < layout.block_size){

            printf("Found entry: %.*s (strlen: %u, inum:%u)\n", dir_entry->name_len, (char*)(dir_entry+1), dir_entry->name_len, dir_entry->inode);

            if(dir_entry->name_len == fname.strlen && strncmp((char*)(dir_entry+1), fname.str, fname.strlen) == 0){
                printf("Found matching entry! %.*s with inum: %u\n",dir_entry->name_len, (char*)(dir_entry+1), dir_entry->inode);
                return dir_entry->inode;
            }


            block_offset += dir_entry->rec_len;
            dir_entry = get_next_entry(dir_entry);
        }
    }

    printf("Nothing found!\n");
    return INVALID_INUM;
}

// ------------------------- File util functions ---------------------------

// TO be added more
// uint32_t get_hardlink_count(void* memory, uint32_t inum, ext2_block_group* bg, ext2_layout layout){
//     ext2_inode* inode = inode_get(memory, inum, bg, layout);
//     return inode->i_links_count;
// }

// -------------------------- setup stuff ----------------------------------------


void setup_inode(ext2_inode* inode, uint16_t imode){
    memset((void*)inode, 0, sizeof(ext2_inode));

    uint32_t time_now = (uint32_t)time(NULL);
    inode->i_atime = time_now;
    inode->i_ctime = time_now;
    inode->i_mtime = time_now;

    inode->i_mode = imode;
}

void setup_block_bitmap(void* bitmap, uint32_t block_count){
    ((uint8_t*)bitmap)[0] = 0xFF;
    ((uint8_t*)bitmap)[1] = 0x7F;

    uint32_t off = 0;
    for(;off<block_count/8;off++){}

    for(;off<1024;off++){
        ((uint8_t*)bitmap)[off] = 0xFF;
    }
}

void setup_inode_bitmap(void* bitmap, uint32_t inode_count){
    ((uint8_t*)bitmap)[0] = 0xFF;
    ((uint8_t*)bitmap)[1] = 0x03;

    uint32_t off = 0;
    for(;off<inode_count/8;off++){} // FIXME

    for(;off<1024;off++){
        ((uint8_t*)bitmap)[off] = 0xFF;
    }
}


// -------------------------- ext2 basic interface ----------------------------------------

uint32_t ext2_create(void* memory, uint32_t p_inum, const char* name, uint8_t type, ext2_block_group* bg, ext2_layout layout){
    printf("\nCreating file: %s\n", name);

    uint32_t    inum = alloc_inode(memory,bg,layout);
    ext2_inode* p_inode = inode_get(memory, p_inum, bg, layout);
    ext2_inode* this_inode = inode_get(memory, inum, bg, layout);

    setup_inode(this_inode, 0x81FF); // FIXME hardcoded
    this_inode->i_links_count = 0;

    dentry_add(memory, p_inode, this_inode, inum, name, type, bg, layout);

    return inum;
}

uint32_t ext2_mkdir(void* memory, uint32_t p_inum, const char* name, ext2_block_group* bg, ext2_layout layout){
    printf("\nCreating directory: %s\n", name);
    uint32_t    inum = alloc_inode(memory,bg,layout);
    ext2_inode* p_inode = inode_get(memory, p_inum, bg, layout);
    ext2_inode* this_inode = inode_get(memory, inum, bg, layout);


    setup_inode(this_inode, 0x41FF); // 0xB6ED
    uint32_t block_no = alloc_block(memory,bg,layout);
    this_inode->i_block[0] = block_no;
    this_inode->i_blocks = layout.block_size / 512;
    this_inode->i_links_count = 0;

    dentry_add(memory, p_inode, NULL, inum, name, EXT2_FT_DIR, bg, layout); // ,inum

    dentry_add(memory, this_inode, NULL, inum, ".", EXT2_FT_DIR, bg, layout);
    dentry_add(memory, this_inode, NULL, p_inum, "..", EXT2_FT_DIR, bg, layout);

    bg->bg_used_dirs_count++;
    return inum;
}

int ext2_symlink(void* memory, uint32_t p_inum, c_string fname, c_string link_name, ext2_block_group* bg, ext2_layout layout){
    printf("\nCreating symlink named %s -> %s\n", link_name.str, fname.str);
    uint32_t inum = ext2_create(memory, p_inum, link_name.str, EXT2_FT_SYMLINK, bg, layout);
    ext2_inode* inode = inode_get(memory, inum, bg, layout);

    inode->i_mode = imode_create(EXT2_S_IFLNK, 0x0E00, 0x01FF);

    write(memory, inode, fname.strlen, fname.str, 0, layout, bg);

    return 0;
}

int ext2_link(void* memory, uint32_t p_inum, uint32_t link_inum, c_string link_name, ext2_block_group* bg, ext2_layout layout){
    printf("\nCreating hardlink %s to %i\n", link_name.str, link_inum);
    ext2_inode* p_inode = inode_get(memory, p_inum, bg, layout);
    ext2_inode* link_inode = inode_get(memory, link_inum, bg, layout);

    if(!imode_ftype(p_inode->i_mode, EXT2_S_IFDIR)) {
        printf("ERR: %s at %i\n", __func__, __LINE__);
        return -1;
    }

    if(imode_ftype(link_inode->i_mode, EXT2_S_IFLNK)){
        printf("[%s] Cant link a soft link!\n", __func__);
        return -1;
    }

    dentry_add(memory, p_inode, link_inode, link_inum, link_name.str, EXT2_FT_REG_FILE, bg, layout);
    return 0;
}

int ext2_unlink(void* memory, uint32_t inum, ext2_block_group *block_group_table, ext2_layout layout) {
    ext2_inode *inode = inode_get(memory, inum, block_group_table, layout);

    if(!(imode_ftype(inode->i_mode, EXT2_S_IFREG) || imode_ftype(inode->i_mode, EXT2_S_IFLNK)))
        { printf("ERR: %s\n", __func__ ); return -1; }

    if(inode->i_links_count != 0) // Avoid overflow
        inode->i_links_count--;

    if(inode->i_links_count == 0){

        uint32_t num_blocks = inode->i_blocks / (layout.block_size / 512);

        for(uint32_t i = 0; i < num_blocks;++i){ // FIXME delete indirect blocks!!
            uint32_t bnum = get_bnum_from_inode(memory, inode, i, layout);
            dealloc_block(memory, bnum, block_group_table, layout);
        }

        inode->i_blocks = 0;
        inode->i_size = 0;
        inode->i_dtime = (uint32_t)time(NULL);

        dealloc_inode(memory, inum, block_group_table, layout);
        return 0;
    }
}

int ext2_rmdir(void* memory, uint32_t inum, ext2_block_group *bg, ext2_layout layout){
    ext2_inode *inode = inode_get(memory, inum, bg, layout);

    if(!imode_ftype(inode->i_mode, EXT2_S_IFDIR)) { printf("ERR: %s\n", __func__ ); return -1; }

    if(inode->i_links_count != 0)  // Avoid overflow
        inode->i_links_count--;

    if(inode->i_links_count - 2 == 0){ // FIXME take into account children dirs pointing up

        uint32_t num_blocks = inode->i_blocks / (layout.block_size / 512);

        // Iterate over all blocks that contain the directory entries
        for(uint32_t i = 0; i < num_blocks; ++i){

            uint32_t bnum = get_bnum_from_inode(memory, inode, i, layout);
            ext2_dir_entry* dir_entry = get_block(memory, bnum, layout);
            uint32_t block_offset = 0;

            // unlink all entries
            while(block_offset < layout.block_size){

                const char* file_name = (const char*)(dir_entry + 1);
                size_t file_name_len = dir_entry->name_len;

                if(file_name_len == 0 || dir_entry->rec_len < sizeof(ext2_dir_entry)){
                    printf("ERR: invalid dir entry %s %i\n", __func__, __LINE__);
                    return -1;
                }

                if(!((file_name_len==1 && file_name[0] == '.')
                     || (file_name_len==2 && file_name[0] == '.' && file_name[1] == '.'))){

                    if(dir_entry->file_type == EXT2_FT_REG_FILE || dir_entry->file_type == EXT2_FT_SYMLINK){
                        ext2_unlink(memory, dir_entry->inode, bg, layout);
                    }else if(dir_entry->file_type == EXT2_FT_DIR){
                        ext2_rmdir(memory, dir_entry->inode, bg, layout);
                    }else{
                        printf("ERR: not supported file type del %s\n", __func__);
                        return -1;
                    }
                }

                dir_entry->inode = 0;

                block_offset += dir_entry->rec_len;
                dir_entry = get_next_entry(dir_entry);
            }

            // Deallocate the data block containing the entries, as they are all cleared
            dealloc_block(memory, bnum, bg, layout);
        }

    }

    inode->i_blocks = 0;
    inode->i_size = 0;
    inode->i_dtime = (uint32_t)time(NULL);
    // Remove the directory inode
    dealloc_inode(memory, inum, bg, layout);
    return 0;
}

int ext2_rename(void* memory, uint32_t inum_where, c_string fname, c_string new_fname, ext2_block_group* bg, ext2_layout layout){
    ext2_inode* inode = inode_get(memory, inum_where, bg, layout);

    return dentry_rename(memory, inode, fname, new_fname, bg, layout);
}

// FIXME make me work with paths that aren't correct like looking up entries in a file that isn't a dir or with symlinks !!!

uint32_t ext2_lookup(void* memory, uint32_t inum_where, c_string path, ext2_block_group* table, ext2_layout layout){
    printf("\next2_lookup: %s\n", path.str);
    if(path.strlen == 0){
        return INVALID_INUM;
    }

    const char* current = path.str + 1; // Skip the first '/'
    uint32_t current_size = 1;
    uint32_t inum = path.str[0] == '/' ? ROOT_DIR_INUM : inum_where;
    uint32_t inum_before = inum;


    while (current_size < path.strlen && inum != INVALID_INUM) {
        const char* end = current;
        inum_before = inum;

        // Find the end of the current element
        while (*end != '/' && current_size < path.strlen) {
            end++;
            current_size++;
        }

        c_string string = { .str=(char*)current, .strlen=(end - current)};

        if(string.strlen == 0) return INVALID_INUM;
        if(string.strlen == 1) continue; // Skip the two consecutive '/' or skip the last '/'

        printf("Looking up: %.*s (%u)\n", string.strlen, string.str, string.strlen);

        ext2_inode* inode = inode_get(memory, inum, table, layout);
        if(imode_ftype(inode->i_mode, EXT2_S_IFLNK)) {

            //printf("Got symlink!\n");
            printf("Symlink not lookup not yet supported!\n"); // FIXME
            return inum;
            //void *data_block = data_block_get(memory, inode->i_block[0], layout);
            //inum = ext2_lookup(memory, inum_before,
            //                   (c_string) {data_block, MIN(inode->i_size, layout.block_size)},
            //                   table, layout);
        }else{
            inum = dentry_lookup(memory, inode, string, table, layout);
        }


        current = end + 1;
        current_size++;
    }

    //ext2_inode* inode = inode_get(memory, inum, table, layout);
    //if(imode_ftype(inode->i_mode, EXT2_S_IFLNK)) {
    //    printf("Got symlink!\n");
    //    void *data_block = data_block_get(memory, inode->i_block[0], layout);
    //    inum = ext2_lookup(memory, inum_before,
    //                       (c_string) {data_block, MIN(inode->i_size, layout.block_size)},
    //                       table, layout);
    //}
    return inum;
}
























/* TODO:
 * 1. Fix so that file operations can work with 'working directories' to save perf & time
 * 2. Fix bookkeeping of link count and number of used data blocks & inodes
 * 2. Make it work with > 1 block group
 * 3. Add local to global and vice versa translation of block index and inode index
 * 4. Add abstraction for different drives
 * 5. Add vfs abstraction (a maybe)
 *
 * LAST: add unit tests
 * */


// int vfs_create(struct inode *, struct dentry *, umode_t, bool);
// int vfs_mkdir(struct inode *, struct dentry *, umode_t);
// int vfs_mknod(struct inode *, struct dentry *, umode_t, dev_t);
// int vfs_symlink(struct inode *, struct dentry *, const char *);
// int vfs_link(struct dentry *, struct inode *, struct dentry *, struct inode **);
// int vfs_rmdir(struct inode *, struct dentry *);
// int vfs_unlink(struct inode *, struct dentry *, struct inode **);
// int vfs_rename(struct inode *, struct dentry *, struct inode *, struct dentry *, struct inode **, unsigned int);
// int vfs_whiteout(struct inode *, struct dentry *);







// ----------------------Setup stuff--------------------------

void ext2_setup_super_block(ext2_super_block* super_block, const ext2_setup_config config){
    for(uint64_t i=0;i<sizeof(ext2_super_block);i++)
        *((uint8_t*)super_block + i) = 0;

    super_block->s_log_block_size   = 0;//config.block_size; // 1024 << [1] = 2 KiB
    super_block->s_first_data_block = super_block->s_log_block_size == 0 ? 1 : 0;

    super_block->s_first_ino = 11;

    super_block->s_inode_size = sizeof(ext2_inode);
    super_block->s_inodes_per_group = (1024/sizeof(ext2_inode))*config.num_inodes_tables_per_group;
    super_block->s_blocks_per_group = config.num_blocks_per_group;
    super_block->s_blocks_count = config.num_groups * config.num_blocks_per_group;
    super_block->s_inodes_count = config.num_groups * super_block->s_inodes_per_group;

    super_block->s_free_blocks_count = super_block->s_blocks_count;
    super_block->s_free_inodes_count = super_block->s_inodes_count - 10;

    super_block->s_log_frag_size = 0; // Verified valid value
    super_block->s_frags_per_group = super_block->s_blocks_per_group;//0; // = blocks_per_group ??

    super_block->s_r_blocks_count = 0; // Verified valid value
    super_block->s_block_group_nr = 0; // Verified valid value

    super_block->s_mtime = (uint32_t)time(NULL);
    super_block->s_wtime = super_block->s_mtime;
    super_block->s_lastcheck = super_block->s_mtime;
    super_block->s_checkinterval = (uint32_t)4294967295;
    super_block->s_rev_level = EXT2_DYNAMIC_REV; // Verified

    super_block->s_def_resgid = 0; // Verified valid value
    super_block->s_def_resuid = 0; // Verified valid value

    super_block->s_feature_compat    = 0; // 0x38
    super_block->s_feature_incompat  = EXT2_FEATURE_INCOMPAT_FILETYPE;
    super_block->s_feature_ro_compat = EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;

    ((uint64_t*)super_block->s_uuid)[0] = 0x2f0d4a2c533f9693;
    ((uint64_t*)super_block->s_uuid)[1] = 0xc925f725e4d1c1e0;


    ((uint64_t*)super_block->s_volume_name)[0] = 0x6c706d6932747865; // ext2impl
    ((uint64_t*)super_block->s_volume_name)[1] = 0x0000000000000000;

    super_block->s_last_mounted[0] = 0;

    super_block->s_algorithm_usage_bitmap = 0;
    super_block->s_prealloc_blocks = 0;
    super_block->s_prealloc_dir_blocks = 0;

    super_block->s_def_hash_version = 0;
    super_block->s_hash_seed[0] = 0;
    super_block->s_hash_seed[0] = 1;
    super_block->s_hash_seed[0] = 2;
    super_block->s_hash_seed[0] = 3;

    super_block->s_mnt_count = 0;
    super_block->s_max_mnt_count = 0xFFFF;
    super_block->s_state  = EXT2_VALID_FS;
    super_block->s_errors = EXT2_ERRORS_RO;

    super_block->s_creator_os = EXT2_OS_LINUX;

    super_block->s_journal_uuid[0] = 0;
    super_block->s_journal_dev = 0;
    super_block->s_journal_inum = 0;
    super_block->s_last_orphan = 0;

    super_block->s_magic = 0xEF53;
}

//typedef struct{
//    uint32_t bg_block_bitmap;
//    uint32_t bg_inode_bitmap;
//    uint32_t bg_inode_table;
//    uint16_t bg_free_blocks_count;
//    uint16_t bg_free_inodes_count;
//    uint16_t bg_used_dirs_count;
//    uint16_t bg_pad;
//    uint8_t  bg_reserved[12];
//} ext2_block_group;

// !!!! May not work for block size > 1 KiB
void ext2_setup_block_group(ext2_block_group* block_group, const ext2_setup_config config){
    const uint32_t BLOCKS_PER_GROUP = config.num_blocks_per_group - 15;
    const uint32_t BLOCK_SIZE = 1024 << config.block_size;
    const uint32_t INODES_PER_GROUP = (BLOCK_SIZE/sizeof(ext2_inode))*config.num_inodes_tables_per_group - 10;
    const uint32_t NUM_INODE_BLOCKS_PER_GROUP = config.num_inodes_tables_per_group;

    uint32_t offset_counter = 1;
    if(config.block_size > 0) printf("WARNING: Block size > 1 KiB is may not work [%s]", __func__);

    for(int i=0;i<config.num_groups;i++){
        block_group[i].has_superblock_backup = 0;
        block_group[i].position = offset_counter;

        if(!config.sparse_superblock || group_contains_sb(i)){
            offset_counter+=2;
            block_group[i].has_superblock_backup = 1;
        }

        block_group[i].bg_block_bitmap = offset_counter;
        block_group[i].bg_inode_bitmap = offset_counter + 1;
        block_group[i].bg_inode_table  = offset_counter + 2;
        block_group[i].bg_free_blocks_count = BLOCKS_PER_GROUP;
        block_group[i].bg_free_inodes_count = INODES_PER_GROUP;
        block_group[i].bg_used_dirs_count = 0;

        block_group[i].bg_pad = 0;
        for(int x=0;x<7;x++){
            block_group[i].bg_reserved[x] = 0;
        }

        // 2 = 1 blockbitmap + 1 inodebitmap
        offset_counter += 2 + NUM_INODE_BLOCKS_PER_GROUP + BLOCKS_PER_GROUP;
    }
}

void create_root_dir(void* memory, ext2_block_group* bg, ext2_layout layout){
    ext2_inode* root_inode = inode_get(memory, ROOT_DIR_INUM, bg, layout);

    setup_inode(root_inode, 0x41FF);

    dentry_add(memory, root_inode, NULL, ROOT_DIR_INUM, ".", EXT2_FT_DIR, bg, layout);
    dentry_add(memory, root_inode, NULL, ROOT_DIR_INUM, "..", EXT2_FT_DIR, bg, layout);

    bg->bg_used_dirs_count++;
}

void ext2_inode_to_string(ext2_inode* inode, ext2_layout layout){
    printf("Inode data at %p:\n", inode);
    printf("\ti_mode: 0x%02X\n", inode->i_mode);
    printf("\ti_uid: %i\n", inode->i_uid);
    printf("\ti_size: %i\n", inode->i_size);
    printf("\ti_atime: %i\n", inode->i_atime);
    printf("\ti_ctime: %i\n", inode->i_ctime);
    printf("\ti_mtime: %i\n", inode->i_mtime);
    printf("\ti_dtime: %i\n", inode->i_dtime);
    printf("\ti_gid: %i\n", inode->i_gid);
    printf("\ti_links_count: %i\n", inode->i_links_count);
    printf("\ti_blocks: %i (Num blocks: %i)\n", inode->i_blocks, inode->i_blocks / (layout.block_size / 512));
    printf("\ti_block 0=%i 1=%i 2=%i 3=%i 4=%i 5=%i 6=%i 7=%i 8=%i 9=%i 10=%i 11=%i 12=%i 13=%i 14=%i\n",
           inode->i_block[0], inode->i_block[1], inode->i_block[2], inode->i_block[3], inode->i_block[4],
           inode->i_block[5], inode->i_block[6], inode->i_block[7], inode->i_block[8], inode->i_block[9],
           inode->i_block[10], inode->i_block[11], inode->i_block[12], inode->i_block[13], inode->i_block[14]);
    printf("\ti_flags: %i\n", inode->i_flags);
    printf("\ti_generation: %i\n", inode->i_generation);
    printf("\ti_file_acl: %i\n", inode->i_file_acl);
    printf("\ti_dir_acl: %i\n", inode->i_dir_acl);
    printf("\ti_faddr: %i\n", inode->i_faddr);
}

int ext2_setup(ext2_setup_config config, void* memory, uint64_t size){
    if((1024<<config.block_size) % sizeof(ext2_inode)!=0){
        return -1;
    }

    ext2_layout layout;
    layout.block_size = 1024<<config.block_size;
    layout.blocks_per_group = config.num_blocks_per_group;
    layout.num_groups = config.num_groups;
    layout.inodes_per_block = layout.block_size/sizeof(ext2_inode);
    layout.inode_blocks_per_group = config.num_inodes_tables_per_group;
    layout.inodes_per_group = layout.inode_blocks_per_group * layout.inodes_per_block;

    ext2_super_block* super_block = (ext2_super_block*)((uint8_t*)memory + 1024);
    ext2_block_group* block_group = (ext2_block_group*)get_block(memory, 2, layout);
    uint8_t*    block_bitmap =               get_block(memory, 3, layout);
    uint8_t*    inode_bitmap =               get_block(memory, 4, layout);
    ext2_inode* inode_table  = (ext2_inode*) get_block(memory, 5, layout);

    ext2_setup_super_block(super_block, config);
    ext2_setup_block_group(block_group, config);

    setup_block_bitmap(block_bitmap, config.num_blocks_per_group);
    setup_inode_bitmap(inode_bitmap, layout.inodes_per_group);


    create_root_dir(memory, block_group, layout);






    ext2_mkdir(memory, ROOT_DIR_INUM, "etc", block_group, layout);
    ext2_mkdir(memory, ROOT_DIR_INUM, "home",block_group, layout);
    ext2_mkdir(memory, ROOT_DIR_INUM, "usr", block_group, layout);
    ext2_mkdir(memory, ROOT_DIR_INUM, "bin", block_group, layout);
    ext2_mkdir(memory, ROOT_DIR_INUM, "sys", block_group, layout);

    uint32_t test_txt_inum = ext2_create(memory, ROOT_DIR_INUM, "test.txt", EXT2_FT_REG_FILE, block_group, layout);

    ext2_link(memory, ROOT_DIR_INUM, test_txt_inum, C_STRING_INIT("test_hardlink.txt"), block_group, layout);

    printf("\n");
    printf("test.txt lookup says that the file has a inum of: %i\n",
                  dentry_lookup(memory, inode_get(memory,ROOT_DIR_INUM,block_group,layout), C_STRING_INIT("test.txt"), block_group, layout));

    printf("Lookup: /etc/../etc/../etc/: inum: %i\n", ext2_lookup(memory,ROOT_DIR_INUM, C_STRING_INIT("/etc/../etc/../etc/"), block_group, layout));

    ext2_rename(memory,ROOT_DIR_INUM, C_STRING_INIT("test.txt"), C_STRING_INIT("test_renamed.txt"),block_group, layout);

    ext2_symlink(memory,ROOT_DIR_INUM, C_STRING_INIT("test.txt"), C_STRING_INIT("test_symlink.txt"), block_group, layout);
    ext2_symlink(memory,ROOT_DIR_INUM, C_STRING_INIT("test_renamed.txt"), C_STRING_INIT("test_renamed_symlink.txt"), block_group, layout);

    printf("test_renamed_symlink.txt says that the file has a inum of: %i\n",
           ext2_lookup(memory, ROOT_DIR_INUM, C_STRING_INIT("/test_renamed_symlink.txt"), block_group, layout));

    const char test_str2[] = "This is a 128byte long text and it is perfectly distinguishable why you might ask 0000"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0001"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0002"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0003"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0004"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0005"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0006"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0007"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0008"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0009"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0010"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0011"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0012"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0013"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0014"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0015"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0016"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0017"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0018"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0019"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0020"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0021"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0022"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0023"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0024"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0025"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0026"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0027"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0028"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0029"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0030"
                             "well its just a fact... Ten more bytes\n\n\n\n"
                             "This is a 128byte long text and it is perfectly distinguishable why you might ask 0031"
                             "well its just a fact... Ten more bytes\n\n\n\n";


    uint32_t t_inum = ext2_lookup(memory, ROOT_DIR_INUM, C_STRING_INIT("/test_renamed.txt"), block_group, layout);

    printf("\n");

    if(t_inum) {
        ext2_inode *inode = inode_get(memory, t_inum, block_group, layout);

        write(memory, inode, sizeof(test_str2) - 1, test_str2, 8, layout, block_group);
        ext2_inode_to_string(inode, layout);
    }

    return 0;
}