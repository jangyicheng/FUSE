#include "../include/newfs.h"

extern struct nfs_super nfs_super;
extern struct custom_options nfs_options;

/**
 * @brief 获取文件名
 *
 * @param path
 * @return char*
 */
char *nfs_get_fname(const char *path)
{
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path
 * @return int
 */
int nfs_calc_lvl(const char *path)
{
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char *str = path;
    int lvl = 0;
    if (strcmp(path, "/") == 0)
    {
        return lvl;
    }
    while (*str != NULL)
    {
        if (*str == '/')
        {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 驱动读
 *
 * @param offset
 * @param out_content
 * @param size
 * @return int
 */
int nfs_driver_read(int offset, uint8_t *out_content, int size)
{
    int offset_aligned = NFS_ROUND_DOWN(offset, NFS_IO_SZ());
    int bias = offset - offset_aligned;
    int size_aligned = NFS_ROUND_UP((size + bias), NFS_IO_SZ());
    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;
    // lseek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(NFS_DRIVER(), cur, NFS_IO_SZ());
        ddriver_read(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 *
 * @param offset
 * @param in_content
 * @param size
 * @return int
 */
int nfs_driver_write(int offset, uint8_t *in_content, int size)
{
    int offset_aligned = NFS_ROUND_DOWN(offset, NFS_IO_SZ());
    int bias = offset - offset_aligned;
    int size_aligned = NFS_ROUND_UP((size + bias), NFS_IO_SZ());
    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;
    nfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);

    // lseek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(NFS_DRIVER(), cur, NFS_IO_SZ());
        ddriver_write(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();
    }

    free(temp_content);
    return NFS_ERROR_NONE;
}
/**
 * @brief 将denry插入到inode中，采用头插法
 *
 * @param inode
 * @param dentry
 * @return int
 */
int nfs_alloc_dentry(struct nfs_inode *inode, struct nfs_dentry *dentry)
{
    if (inode->dentrys == NULL)
    {
        inode->dentrys = dentry;
    }
    else
    {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }


    int byte_cursor  = 0;
    int bit_cursor  = 0;
    int data_cursor  = 0;
    boolean  is_find_free_entry = FALSE; 


    // 计算当前目录项是否达到了块的最大容量
    int index = inode->dir_cnt;
    int alloc = inode->dir_cnt %  DENTRY_PER_BLOCK(); // 当前目录项在块中的位置

    if(alloc == 0){
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_data_blks);
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((nfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                nfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            data_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }
     inode->block_pointer[index] = data_cursor;
    }
    

    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 *
 * @param inode 一个目录的索引结点
 * @param dentry 该目录下的一个目录项
 * @return int
 */
int nfs_drop_dentry(struct nfs_inode *inode, struct nfs_dentry *dentry)
{
    boolean is_find = FALSE;
    struct nfs_dentry *dentry_cursor;
    dentry_cursor = inode->dentrys;

    if (dentry_cursor == dentry)
    {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else
    {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry)
            {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find)
    {
        return -NFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}
/**
 * @brief 分配一个inode，占用位图
 *
 * @param dentry 该dentry指向分配的inode
 * @return nfs_inode
 */
struct nfs_inode *nfs_alloc_inode(struct nfs_dentry *dentry)
{
    // printf("allocate once!\n");
    struct nfs_inode *inode;
    int byte_cursor = 0;
    int bit_cursor = 0;
    int ino_cursor = 0;
    boolean is_find_free_entry = FALSE;
    /* 检查位图是否有空位 */
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            if ((nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0)
            {
                /* 当前ino_cursor位置空闲 */
                nfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry)
        {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == nfs_super.max_ino)
        return -NFS_ERROR_NOSPACE;

    inode = (struct nfs_inode *)malloc(sizeof(struct nfs_inode));
    inode->ino = ino_cursor;
    inode->size = 0;

    /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino = inode->ino;
    /* inode指回dentry */
    inode->dentry = dentry;

    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    for (int i =0;i<NFS_DATA_PER_FILE;i++){
        inode->block_pointer[i] = -1;
    }
    if (NFS_IS_REG(inode))
    {
        for (int i = 0; i < NFS_DATA_PER_FILE; i++)
        {
            inode->data[i] = (uint8_t *)malloc(NFS_BLOCK_SIZE);
        }
    }

    return inode;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 *
 * @param inode
 * @return int
 */
int nfs_sync_inode(struct nfs_inode *inode)
{
    struct nfs_inode_d inode_d;
    struct nfs_dentry *dentry_cursor;
    struct nfs_dentry_d dentry_d;
    int ino = inode->ino;
    inode_d.ino = ino;
    inode_d.size = inode->size;
    memcpy(inode_d.target_path, inode->target_path, NFS_MAX_FILE_NAME);
    inode_d.ftype = inode->dentry->ftype;
    inode_d.dir_cnt = inode->dir_cnt;
    int offset;

    for (int i = 0; i < NFS_DATA_PER_FILE; i++)
    {
        inode_d.block_pointer[i] = inode->block_pointer[i];
    }

    /* 先写inode本身 */
    if (nfs_driver_write(NFS_INO_OFS(ino), (uint8_t *)&inode_d,
                         sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE)
    {
        NFS_DBG("[%s] io error\n", __func__);
        return -NFS_ERROR_IO;
    }

// 判断 inode 是否为目录
if (NFS_IS_DIR(inode)) {
    struct nfs_dentry *dentry_cursor = inode->dentrys;  // 目录项游标
    int dir_cnt = inode->dir_cnt;                      // 剩余目录项计数
    int offset, block_end;

    // 使用 for 循环遍历每个块
    for (int block_index = 0; dir_cnt > 0 && block_index < NFS_DATA_PER_FILE; block_index++) {
        // 当前块的起始偏移和结束偏移
        offset = NFS_DATA_OFS(inode->block_pointer[block_index]);
        block_end = NFS_DATA_OFS(inode->block_pointer[block_index] + 1);

        // 遍历当前块中的目录项
        while (dir_cnt > 0 && (offset + sizeof(struct nfs_dentry_d)) < block_end) {
            if (dentry_cursor == NULL) {
                NFS_DBG("[%s] dentry cursor is NULL\n", __func__);
                return -NFS_ERROR_IO;
            }

            // 拷贝目录项数据到目标结构体
            memcpy(dentry_d.fname, dentry_cursor->fname, MAX_NAME_LEN);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;

            // 写入目录项数据到驱动
            if (nfs_driver_write(offset, (uint8_t *)&dentry_d, sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE) {
                return -NFS_ERROR_IO;
            }

            // 如果目录项有 inode，进行同步
            if (dentry_cursor->inode != NULL) {
                nfs_sync_inode(dentry_cursor->inode);
            }

            // 更新偏移量和剩余目录项计数
            offset += sizeof(struct nfs_dentry_d);
            dir_cnt--;
            dentry_cursor = dentry_cursor->brother;  // 移动到下一个目录项
        }
    }
}
    else if (NFS_IS_REG(inode))
    { /* 如果当前inode是文件，那么数据是文件内容，直接写即可 */
        for (int i = 0; i < NFS_DATA_PER_FILE; i++)
        {
            if(inode->block_pointer[i] != -1){
            if (nfs_driver_write(NFS_DATA_OFS(inode->block_pointer[i]), inode->data[i],
                                 NFS_BLOCK_SIZE) != NFS_ERROR_NONE)
            {
                NFS_DBG("[%s] io error\n", __func__);
                return -NFS_ERROR_IO;
            }}
        }
    }
    return NFS_ERROR_NONE;
}
/**
 * @brief 删除内存中的一个inode
 * Case 1: Reg File
 *
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 *
 *  1) Step 1. Erase Bitmap
 *  2) Step 2. Free Inode                      (Function of nfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 *
 *   Recursive
 * @param inode
 * @return int
 */
int nfs_drop_inode(struct nfs_inode *inode)
{
    struct nfs_dentry *dentry_cursor;
    struct nfs_dentry *dentry_to_free;
    struct nfs_inode *inode_cursor;

    int byte_cursor = 0;
    int bit_cursor = 0;
    int ino_cursor = 0;
    boolean is_find = FALSE;

    if (inode == nfs_super.root_dentry->inode)
    {
        return NFS_ERROR_INVAL;
    }

    if (NFS_IS_DIR(inode))
    {
        dentry_cursor = inode->dentrys;
        /* 递归向下drop */
        while (dentry_cursor)
        {
            inode_cursor = dentry_cursor->inode;
            nfs_drop_inode(inode_cursor);
            nfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }

        for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
             byte_cursor++) /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
            {
                if (ino_cursor == inode->ino)
                {
                    nfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                    is_find = TRUE;
                    break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE)
            {
                break;
            }
        }
    }
    else if (NFS_IS_REG(inode) || NFS_IS_SYM_LINK(inode))
    {
        for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
             byte_cursor++) /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
            {
                if (ino_cursor == inode->ino)
                {
                    nfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                    is_find = TRUE;
                    break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE)
            {
                break;
            }
        }
        if (inode->data)
            free(inode->data);
        free(inode);
    }
    return NFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct nfs_inode* 
 */
struct nfs_inode* nfs_read_inode(struct nfs_dentry* dentry, int ino) {
    struct nfs_inode* inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));
    struct nfs_inode_d inode_d;
    struct nfs_dentry* sub_dentry;
    struct nfs_dentry_d dentry_d;
    int dir_cnt = 0;
    int index = 0;   // 追加变量用于块号

    // 从磁盘中读取inode数据
    if (nfs_driver_read(NFS_INO_OFS(ino), (uint8_t *)&inode_d, sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return NULL;
    }

    // 更新内存中的inode数据
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;
    for (int i = 0; i < NFS_DATA_PER_FILE; i++) {
        inode->block_pointer[i] = inode_d.block_pointer[i];
    }

   // 判断 inode 是否为目录
if (NFS_IS_DIR(inode)) {
    struct nfs_dentry *dentry_cursor = inode->dentrys;  // 目录项游标
    int dir_cnt = inode_d.dir_cnt;                      // 剩余目录项计数
    int offset;
    
    // 逐块读取目录项
    for (int block_index = 0; dir_cnt > 0 && block_index < NFS_DATA_PER_FILE; block_index++) {
        // 当前块的起始偏移
        offset = NFS_DATA_OFS(inode->block_pointer[block_index]);

        // 读取当前块中的所有目录项
        while (dir_cnt > 0 && offset + sizeof(struct nfs_dentry_d) < NFS_DATA_OFS(inode->block_pointer[block_index] + 1)) {
            // 读取目录项数据
            if (nfs_driver_read(offset, (uint8_t *)&dentry_d, sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }

            // 创建并分配新目录项
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino = dentry_d.ino;

            // 将新目录项分配到 inode
            nfs_alloc_dentry(inode, sub_dentry);

            // 更新偏移量和剩余目录项计数
            offset += sizeof(struct nfs_dentry_d);
            dir_cnt--;
        }
    }
}

    // 判断inode是否为文件
    else if (NFS_IS_REG(inode)) {
        for (int i = 0; i < NFS_DATA_PER_FILE; i++) {
            inode->data[i] = (uint8_t *)malloc(NFS_BLOCK_SIZE);
            if(inode->block_pointer[i] != -1){
            if (nfs_driver_read(NFS_DATA_OFS(inode->block_pointer[i]), (uint8_t *)inode->data[i], NFS_BLOCK_SIZE) != NFS_ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }
            }
        }
    }

    return inode;
}

/**
 * @brief
 *
 * @param inode
 * @param dir [0...]
 * @return struct nfs_dentry*
 */
struct nfs_dentry *nfs_get_dentry(struct nfs_inode *inode, int dir)
{
    struct nfs_dentry *dentry_cursor = inode->dentrys;
    int cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt)
        {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief 查找文件或目录
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *
 *
 * 如果能查找到，返回该目录项
 * 如果查找不到，返回的是上一个有效的路径
 *
 * path: /a/b/c
 *      1) find /'s inode     lvl = 1
 *      2) find a's dentry
 *      3) find a's inode     lvl = 2
 *      4) find b's dentry    如果此时找不到了，is_find=FALSE且返回的是a的inode对应的dentry
 *
 * @param path
 * @return struct nfs_dentry*
 */
struct nfs_dentry *nfs_lookup(const char *path, boolean *is_find, boolean *is_root)
{
    struct nfs_dentry *dentry_cursor = nfs_super.root_dentry;
    struct nfs_dentry *dentry_ret = NULL;
    struct nfs_inode *inode;
    int total_lvl = nfs_calc_lvl(path);
    int lvl = 0;
    boolean is_hit;
    char *fname = NULL;
    char *path_cpy = (char *)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0)
    { /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = nfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");
    while (fname)
    {
        lvl++;
        if (dentry_cursor->inode == NULL)
        { /* Cache机制 */
            nfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (NFS_IS_REG(inode) && lvl < total_lvl)
        {
            NFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (NFS_IS_DIR(inode))
        {
            dentry_cursor = inode->dentrys;
            is_hit = FALSE;

            while (dentry_cursor) /* 遍历子目录项 */
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0)
                {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }

            if (!is_hit)
            {
                *is_find = FALSE;
                NFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl)
            {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/");
    }

    if (dentry_ret->inode == NULL)
    {
        dentry_ret->inode = nfs_read_inode(dentry_ret, dentry_ret->ino);
    }

    return dentry_ret;
}
/**
 * @brief 挂载nfs, Layout 如下
 *
 * Layout
 * | Super | Inode Map | Data |
 *
 * 2*IO_SZ = BLK_SZ
 *
 * 每个Inode占用一个Blk
 * @param options
 * @return int
 */
int nfs_mount(struct custom_options options)
{
    int ret = NFS_ERROR_NONE;
    int driver_fd;
    struct nfs_super_d nfs_super_d;
    struct nfs_dentry *root_dentry;
    struct nfs_inode *root_inode;

    int inode_num;
    int map_inode_blks;
    int inode_blks;
    int map_data_blks;

    int super_blks;
    boolean is_init = FALSE;

    nfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0)
    {
        return driver_fd;
    }

    nfs_super.driver_fd = driver_fd;
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_SIZE, &nfs_super.sz_disk);
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &nfs_super.sz_io);

    root_dentry = new_dentry("/", NFS_DIR); /* 根目录项每次挂载时新建 */

    if (nfs_driver_read(NFS_SUPER_OFS, (uint8_t *)(&nfs_super_d),
                        sizeof(struct nfs_super_d)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }
    /* 读取super */
    if (nfs_super_d.magic_num != NFS_MAGIC_NUM)
    {   /* 幻数不正确，初始化 */
        /* 估算各部分大小 */
        super_blks = NFS_SUPER_BLKS;
        map_inode_blks = NFS_MAP_INODE_BLKS;
        map_data_blks = NFS_MAP_DATA_BLKS;
        /* 布局layout */
        nfs_super.max_ino = MAX_INO;
        nfs_super.max_data = MAX_DATA;
        nfs_super_d.map_inode_blks = map_inode_blks;
        nfs_super_d.map_data_blks = map_data_blks;

        nfs_super_d.map_inode_offset = NFS_SUPER_OFS + NFS_BLKS_SZ(super_blks);
        nfs_super_d.map_data_offset = nfs_super_d.map_inode_offset + NFS_BLKS_SZ(map_inode_blks);
        nfs_super_d.inode_offset = nfs_super_d.map_data_offset + NFS_BLKS_SZ(map_data_blks);
        nfs_super_d.data_offset = nfs_super_d.inode_offset + NFS_BLKS_SZ(MAX_INO);

        nfs_super_d.sz_usage = 0;
        is_init = TRUE;
        nfs_super_d.magic_num = NFS_MAGIC_NUM;
    }
    nfs_super.sz_usage = nfs_super_d.sz_usage; /* 建立 in-memory 结构 */

    nfs_super.map_inode = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_inode_blks));
    nfs_super.map_inode_blks = nfs_super_d.map_inode_blks;
    nfs_super.map_inode_offset = nfs_super_d.map_inode_offset;

    nfs_super.map_data = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_data_blks));
    nfs_super.map_data_blks = nfs_super_d.map_data_blks;
    nfs_super.map_data_offset = nfs_super_d.map_data_offset;
    nfs_super.inode_offset = nfs_super_d.inode_offset;
    nfs_super.data_offset = nfs_super_d.data_offset;
    // nfs_dump_map();

    printf("\n--------------------------------------------------------------------------------\n\n");

    if (nfs_driver_read(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode),
                        NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }
    if (nfs_driver_read(nfs_super_d.map_data_offset, (uint8_t *)(nfs_super.map_data),
                        NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }
    if (is_init)
    { /* 分配根节点 */
        root_inode = nfs_alloc_inode(root_dentry);
        nfs_sync_inode(root_inode);
    }

    root_inode = nfs_read_inode(root_dentry, NFS_ROOT_INO); /* 读取根目录 */
    root_dentry->inode = root_inode;
    nfs_super.root_dentry = root_dentry;
    nfs_super.is_mounted = TRUE;

    // nfs_dump_map();
    return ret;
}
/**
 * @brief
 *
 * @return int
 */
int nfs_umount()
{
    struct nfs_super_d nfs_super_d;

    if (!nfs_super.is_mounted)
    {
        return NFS_ERROR_NONE;
    }

    nfs_sync_inode(nfs_super.root_dentry->inode); /* 从根节点向下刷写节点 */


    nfs_super_d.magic_num = NFS_MAGIC_NUM;
    nfs_super_d.map_inode_blks = nfs_super.map_inode_blks;
    nfs_super_d.map_inode_offset = nfs_super.map_inode_offset;
    nfs_super_d.sz_usage = nfs_super.sz_usage;

    nfs_super_d.inode_offset = nfs_super.inode_offset;
    nfs_super_d.map_data_blks = nfs_super.map_data_blks;
    nfs_super_d.map_data_offset = nfs_super.map_data_offset;
    nfs_super_d.data_offset = nfs_super.data_offset;



    if (nfs_driver_write(NFS_SUPER_OFS, (uint8_t *)&nfs_super_d,
                         sizeof(struct nfs_super_d)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }

    // nfs_print_map();
    // printf("%d",nfs_super.map_data_offset);

    if (nfs_driver_write(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode),
                         NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }
    if (nfs_driver_write(nfs_super_d.map_data_offset, (uint8_t *)(nfs_super.map_data),
                         NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }

    free(nfs_super.map_inode);
    free(nfs_super.map_data);
    ddriver_close(NFS_DRIVER());

    return NFS_ERROR_NONE;
}

void nfs_dump_map()
{
    int byte_cursor = 0;
    int bit_cursor = 0;

    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
         byte_cursor += 4)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\n");
    }
}


void nfs_print_map()
{
    int byte_cursor = 0;
    int bit_cursor = 0;

    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_data_blks);
         byte_cursor += 4)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_data[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_data[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (nfs_super.map_data[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\n");
    }
}

// void nfs_print_map()
// {
//     int byte_cursor = 0;
//     int bit_cursor = 0;

//     for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
//          byte_cursor += 4)
//     {
//         for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
//         {
//             printf("%d ", (nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);
//         }
//         printf("\t");

//         for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
//         {
//             printf("%d ", (nfs_super.map_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);
//         }
//         printf("\t");

//         for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
//         {
//             printf("%d ", (nfs_super.map_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);
//         }
//         printf("\t");

//         for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
//         {
//             printf("%d ", (nfs_super.map_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);
//         }
//         printf("\n");
//     }
// }
