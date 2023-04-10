#pragma once

#include <stdint.h>

#define PACKED __attribute__((packed))


enum imode_ff{
    EXT2_S_IFSOCK =  0xC000, //socket
    EXT2_S_IFLNK  =  0xA000, //symbolic link
    EXT2_S_IFREG  =  0x8000, //regular file
    EXT2_S_IFBLK  =  0x6000, //block device
    EXT2_S_IFDIR  =  0x4000, //directory
    EXT2_S_IFCHR  =  0x2000, //character device
    EXT2_S_IFIFO  =  0x1000, //fifo
};

enum imode_execution_override {
    EXT2_S_ISUID  =  0x0800, //Set process User ID
    EXT2_S_ISGID  =  0x0400, //Set process Group ID
    EXT2_S_ISVTX  =  0x0200, //sticky bit
};

enum imode_access_rights {
    EXT2_S_IRUSR  =  0x0100, //user read
    EXT2_S_IWUSR  =  0x0080, //user write
    EXT2_S_IXUSR  =  0x0040, //user execute
    EXT2_S_IRGRP  =  0x0020, //group read
    EXT2_S_IWGRP  =  0x0010, //group write
    EXT2_S_IXGRP  =  0x0008, //group execute
    EXT2_S_IROTH  =  0x0004, //others read
    EXT2_S_IWOTH  =  0x0002, //others write
    EXT2_S_IXOTH  =  0x0001, //others execute
};

enum s_creator_os {
    EXT2_OS_LINUX   = 0, //Linux
    EXT2_OS_HURD    = 1, //GNU HURD
    EXT2_OS_MASIX   = 2, //MASIX
    EXT2_OS_FREEBSD = 3, //FreeBSD
    EXT2_OS_LITES   = 4, //Lites
};

enum s_state {
    EXT2_VALID_FS = 1,
    EXT2_ERROR_FS = 2
};

enum s_errors {
    EXT2_ERRORS_CONTINUE    =  1,	//continue as if nothing happened
    EXT2_ERRORS_RO          =  2,	//remount read-only
    EXT2_ERRORS_PANIC       =  3,	//cause a kernel panic
};

enum s_rev_level {
    EXT2_GOOD_OLD_REV = 0, //Revision 0
    EXT2_DYNAMIC_REV  = 1, //Revision 1 with variable inode sizes, extended attributes, etc.
};


typedef struct __attribute__((packed)) {
    uint32_t	s_inodes_count;		    /* Inodes count */
    uint32_t	s_blocks_count;		    /* Blocks count */
    uint32_t	s_r_blocks_count;	    /* Reserved blocks count */
    uint32_t	s_free_blocks_count;	/* Free blocks count */

    uint32_t	s_free_inodes_count;	/* Free inodes count */
    uint32_t	s_first_data_block;	    /* First Data Block */ // First usable byte?
    uint32_t	s_log_block_size;	    /* Block size */
    uint32_t	s_log_frag_size;	    /* Fragment size */

    uint32_t	s_blocks_per_group;	    /* # Blocks per group */
    uint32_t	s_frags_per_group;	    /* # Fragments per group */
    uint32_t	s_inodes_per_group;	    /* # Inodes per group */
    uint32_t	s_mtime;		        /* Mount time */

    uint32_t	s_wtime;		        /* Write time */
    uint16_t	s_mnt_count;		    /* Mount count */
    uint16_t	s_max_mnt_count;	    /* Maximal mount count */
    uint16_t	s_magic;		        /* Magic signature */
    uint16_t	s_state;		        /* File system state */
    uint16_t	s_errors;		        /* Behaviour when detecting errors */
    uint16_t	s_minor_rev_level; 	    /* minor revision level */

    uint32_t	s_lastcheck;		    /* time of last check */
    uint32_t	s_checkinterval;	    /* max. time between checks */
    uint32_t	s_creator_os;		    /* OS */
    uint32_t	s_rev_level;		    /* Revision level */

    uint16_t	s_def_resuid;		    /* Default uid for reserved blocks */
    uint16_t	s_def_resgid;		    /* Default gid for reserved blocks */
    /*
     * These fields are for EXT2_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    uint32_t	s_first_ino; 		      /* First non-reserved inode */
    uint16_t    s_inode_size; 		      /* size of inode structure */
    uint16_t	s_block_group_nr; 	      /* block group # of this superblock */
    uint32_t	s_feature_compat; 	      /* compatible feature set */

    uint32_t	s_feature_incompat; 	  /* incompatible feature set */
    uint32_t	s_feature_ro_compat; 	  /* readonly-compatible feature set */
    uint8_t	    s_uuid[16];		          /* 128-bit uuid for volume */
    char	    s_volume_name[16]; 	      /* volume name */
    char	    s_last_mounted[64]; 	  /* directory where last mounted */
    uint32_t	s_algorithm_usage_bitmap; /* For compression */
    /*
     * Performance hints.  Directory preallocation should only
     * happen if the EXT2_COMPAT_PREALLOC flag is on.
     */
    uint8_t	    s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
    uint8_t	    s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
    uint16_t	s_padding1;
    /*
     * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
     */
    uint8_t	    s_journal_uuid[16];	/* uuid of journal superblock */
    uint32_t	s_journal_inum;		/* inode number of journal file */
    uint32_t	s_journal_dev;		/* device number of journal file */
    uint32_t	s_last_orphan;		/* start of list of inodes to delete */
    uint32_t	s_hash_seed[4];		/* HTREE hash seed */
    uint8_t	    s_def_hash_version;	/* Default hash version to use */
    uint8_t	    s_reserved_char_pad;
    uint16_t	s_reserved_word_pad;
    uint32_t	s_default_mount_opts;
    uint32_t	s_first_meta_bg; 	/* First metablock block group */
    uint32_t	s_reserved[190];	/* Padding to the end of the block */
} ext2_super_block;

typedef struct __attribute__((packed)) {
    uint16_t	i_mode;		/* File mode */
    uint16_t	i_uid;		/* Low 16 bits of Owner Uid */
    uint32_t	i_size;		/* Size in bytes */
    uint32_t	i_atime;	/* Access time */
    uint32_t	i_ctime;	/* Creation time */

    uint32_t	i_mtime;	/* Modification time */
    uint32_t	i_dtime;	/* Deletion Time */
    uint16_t	i_gid;		/* Low 16 bits of Group Id */
    uint16_t	i_links_count;	/* Links count */
    uint32_t	i_blocks;	/* Blocks count */

    uint32_t	i_flags;	/* File flags */

    union {
        struct {
            uint32_t  l_i_reserved1;
        } linux1;
        struct {
            uint32_t  h_i_translator;
        } hurd1;
        struct {
            uint32_t  m_i_reserved1;
        } masix1;
    } osd1;
    /* OS dependent 1 */
    uint32_t	i_block[15];    /* Pointers to blocks */
    uint32_t	i_generation;	/* File version (for NFS) */
    uint32_t	i_file_acl;	    /* File ACL */
    uint32_t	i_dir_acl;	    /* Directory ACL */
    uint32_t	i_faddr;	    /* Fragment address */

    union {
        struct {
            uint8_t	    l_i_frag;	/* Fragment number */
            uint8_t	    l_i_fsize;	/* Fragment size */
            uint16_t	i_pad1;
            uint16_t	l_i_uid_high;	/* these 2 fields    */
            uint16_t	l_i_gid_high;	/* were reserved2[0] */
            uint32_t	l_i_reserved2;
        } linux2;
        struct {
            uint8_t	    h_i_frag;	/* Fragment number */
            uint8_t	    h_i_fsize;	/* Fragment size */
            uint16_t	h_i_mode_high;
            uint16_t	h_i_uid_high;
            uint16_t	h_i_gid_high;
            uint32_t	h_i_author;
        } hurd2;
        struct {
            uint8_t	    m_i_frag;	/* Fragment number */
            uint8_t	    m_i_fsize;	/* Fragment size */
            uint16_t	m_pad1;
            uint32_t	m_i_reserved2[2];
        } masix2;
    } osd2;				/* OS dependent 2 */
} ext2_inode;

typedef struct __attribute__((packed)){
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;

    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;

    uint32_t position; // block offset of this block group
    uint8_t has_superblock_backup;
    uint8_t  bg_reserved[7];
    //uint8_t	 bg_reserved[12];
} ext2_block_group;


enum EXT2_FT {
    EXT2_FT_UNKNOWN	 = 0,	//Unknown File Type
    EXT2_FT_REG_FILE = 1,	//Regular File
    EXT2_FT_DIR      = 2,	// Directory File
    EXT2_FT_CHRDEV   = 3,	//Character Device
    EXT2_FT_BLKDEV	 = 4,	//Block Device
    EXT2_FT_FIFO     = 5,	//Buffer File
    EXT2_FT_SOCK     = 6,	//Socket File
    EXT2_FT_SYMLINK  = 7,	//Symbolic Link
};

enum s_feature_compat{
    EXT2_FEATURE_COMPAT_DIR_PREALLOC  = 0x0001, //Block pre-allocation for new directories
    EXT2_FEATURE_COMPAT_IMAGIC_INODES = 0x0002, //
    EXT3_FEATURE_COMPAT_HAS_JOURNAL   = 0x0004, //An Ext3 journal exists
    EXT2_FEATURE_COMPAT_EXT_ATTR      = 0x0008, //Extended inode attributes are present
    EXT2_FEATURE_COMPAT_RESIZE_INO    = 0x0010, //Non-standard inode size used
    EXT2_FEATURE_COMPAT_DIR_INDEX     = 0x0020,	//Directory indexing (HTree)
};

enum s_feature_incompat {
    EXT2_FEATURE_INCOMPAT_COMPRESSION = 0x0001, // Disk/File compression is used
    EXT2_FEATURE_INCOMPAT_FILETYPE    = 0x0002, //
    EXT3_FEATURE_INCOMPAT_RECOVER     = 0x0004, //
    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV = 0x0008, //
    EXT2_FEATURE_INCOMPAT_META_BG     = 0x0010, //
};

enum s_feature_ro_compat {
    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x0001, // Sparse Superblock
    EXT2_FEATURE_RO_COMPAT_LARGE_FILE   = 0x0002, // Large file support, 64-bit file size
    EXT2_FEATURE_RO_COMPAT_BTREE_DIR    = 0x0004, // Binary tree sorted directory files
};


typedef struct __attribute__((packed)){
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
} ext2_dir_entry;


_Static_assert(sizeof(ext2_block_group) ==   32);

_Static_assert(sizeof(ext2_inode)       ==  128);

_Static_assert(sizeof(ext2_super_block) == 1024); // 1 KiB



typedef struct{
    int32_t block_size; // in num lshifts of 1024. e.g. 2 -> 1024 << 2 = 2048
    int32_t num_inodes_tables_per_group;
    int32_t num_blocks_per_group;
    int32_t num_groups;
    uint8_t sparse_superblock;
} ext2_setup_config;

int ext2_setup(ext2_setup_config config, void* memory, uint64_t size);

//void ext2_create(const char* path, const char* data, uint64_t size);