#ifndef _TYPES_H_
#define _TYPES_H_


/******************************************************************************
* SECTION: Type def
*******************************************************************************/
typedef int          boolean;
typedef uint16_t     flag16;

typedef enum nfs_file_type {
    NFS_REG_FILE,
    NFS_DIR,
    NFS_SYM_LINK
} NFS_FILE_TYPE;
/****************************************************************************** 
* SECTION: Macro 
******************************************************************************/
#define MAX_NAME_LEN    128

#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define NFS_MAGIC_NUM           0x52415453  
#define NFS_SUPER_OFS           0
#define NFS_ROOT_INO            0

#define NFS_ERROR_NONE          0
#define NFS_ERROR_ACCESS        EACCES
#define NFS_ERROR_SEEK          ESPIPE     
#define NFS_ERROR_ISDIR         EISDIR
#define NFS_ERROR_NOSPACE       ENOSPC
#define NFS_ERROR_EXISTS        EEXIST
#define NFS_ERROR_NOTFOUND      ENOENT
#define NFS_ERROR_UNSUPPORTED   ENXIO
#define NFS_ERROR_IO            EIO     /* Error Input/Output */
#define NFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define NFS_MAX_FILE_NAME       128
#define NFS_INODE_PER_FILE      1
#define NFS_DATA_PER_FILE       6      //一个node对应的文件数
#define NFS_DEFAULT_PERM        0777

#define NFS_IOC_MAGIC           'N'
#define NFS_IOC_SEEK            _IO(NFS_IOC_MAGIC, 0)

#define NFS_FLAG_BUF_DIRTY      0x1
#define NFS_FLAG_BUF_OCCUPY     0x2

#define NFS_BLOCK_SIZE                   1024        /* Logical Block Size in bytes */
#define MAX_INO                          585
#define MAX_DATA                         3508
#define NFS_SUPER_BLKS                   1
#define NFS_MAP_INODE_BLKS               1
#define NFS_MAP_DATA_BLKS                1




/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define NFS_IO_SZ()                     (nfs_super.sz_io)
#define NFS_DISK_SZ()                   (nfs_super.sz_disk)
#define NFS_DRIVER()                    (nfs_super.driver_fd)


#define NFS_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define NFS_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

#define NFS_BLKS_SZ(blks)               ((blks) * NFS_BLOCK_SIZE)
#define NFS_ASSIGN_FNAME(pnfs_dentry, _fname) memcpy(pnfs_dentry->fname, _fname, strlen(_fname))

#define NFS_INO_OFS(ino)                (nfs_super.inode_offset + NFS_BLKS_SZ(ino))
#define NFS_DATA_OFS(ino)               (nfs_super.data_offset + NFS_BLKS_SZ(ino))

#define NFS_IS_DIR(pinode)              (pinode->dentry->ftype == NFS_DIR)
#define NFS_IS_REG(pinode)              (pinode->dentry->ftype == NFS_REG_FILE)
#define NFS_IS_SYM_LINK(pinode)         (pinode->dentry->ftype == NFS_SYM_LINK)

#define DENTRY_PER_BLOCK()              NFS_ROUND_DOWN((NFS_BLOCK_SIZE) / sizeof(struct nfs_dentry_d),1)

/****************************************************************************** 
* SECTION: FS Specific Structure - In-memory structure 
******************************************************************************/
struct nfs_dentry;
struct nfs_inode;
struct nfs_super;

struct custom_options {
    const char* device;  /* Device name (e.g., disk) */
};

struct nfs_inode {
    int                ino;                           /* 在inode位图中的下标 */
    int                size;                          /* 文件已占用空间 */
    char               target_path[NFS_MAX_FILE_NAME];/* store traget path when it is a symlink */
    int                dir_cnt;
    struct nfs_dentry* dentry;                        /* 指向该inode的dentry */
    struct nfs_dentry* dentrys;                       /* 所有目录项 */
    NFS_FILE_TYPE          ftype;                           // 文件类型（目录类型、普通文件类型）
    uint8_t           data[NFS_DATA_PER_FILE];     
    int               block_pointer[NFS_DATA_PER_FILE];          
};

struct nfs_dentry {
    int                ino;
    char               fname[NFS_MAX_FILE_NAME];
    NFS_FILE_TYPE      ftype;

    struct nfs_inode*  inode;                         /* 指向inode */
    struct nfs_dentry* parent;                        /* 父亲Inode的dentry */
    struct nfs_dentry* brother;                       /* 兄弟 */
};

struct nfs_super {
    int                driver_fd;
    uint32_t           magic_num;           // 幻数
    int                sz_io;
    int                sz_disk;
    int                sz_usage;
    
    int                max_ino;
    int                max_data;
    uint8_t*           map_inode;
    int                map_inode_blks;
    int                map_inode_offset;

    int                map_data_blks;
    uint8_t*           map_data;    
    int                map_data_blks_size;
    int                map_data_offset;  

    int                inode_offset;
    int                data_offset;

    boolean            is_mounted;
    struct nfs_dentry* root_dentry;
};

/****************************************************************************** 
* SECTION: FS Specific Structure - Disk structure (to be written to disk) 
******************************************************************************/
struct nfs_super_d {
    uint32_t           magic_num;
    uint32_t           sz_usage;
    

    uint32_t           map_inode_blks;
    uint32_t           map_inode_offset;
    uint32_t           data_offset;

    int                map_data_blks;
    int                map_data_offset;  
    int                inode_offset;
};

struct nfs_inode_d {
    uint32_t           ino;                           /* 在inode位图中的下标 */
    uint32_t           size;                          /* 文件已占用空间 */
    char               target_path[NFS_MAX_FILE_NAME];/* store traget path when it is a symlink */
    uint32_t           dir_cnt;
    NFS_FILE_TYPE      ftype;  

    int                block_pointer[NFS_DATA_PER_FILE];         
};

struct nfs_dentry_d {
    char               fname[NFS_MAX_FILE_NAME];
    NFS_FILE_TYPE      ftype;
    uint32_t           ino;                           /* 指向的ino号 */
};



/****************************************************************************** 
* SECTION: FS Specific Function 
******************************************************************************/

static inline struct nfs_dentry* new_dentry(char * fname, NFS_FILE_TYPE ftype) {
    struct nfs_dentry * dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    NFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;

    return dentry;                                            
}

#endif /* _TYPES_H_ */
