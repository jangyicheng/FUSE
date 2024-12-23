/* main.c源码 */
#define _XOPEN_SOURCE 700

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "fuse.h"
#include "../include/ddriver.h"
#include <linux/fs.h>
#include "pwd.h"
#include "unistd.h"
#include "string.h"

#define DEMO_DEFAULT_PERM        0777


/* 超级块 */
struct demo_super
{
    int     driver_fd;  /* 模拟磁盘的fd */

    int     sz_io;      /* 磁盘IO大小，单位B */
    int     sz_disk;    /* 磁盘容量大小，单位B */
    int     sz_blks;    /* 逻辑块大小，单位B */
};

/* 目录项 */
struct demo_dentry
{
    char    fname[128];
}; 

struct demo_super super;

#define DEVICE_NAME "ddriver"

/* 挂载文件系统 */
static void* demo_mount(struct fuse_conn_info * conn_info){
    // 打开驱动
    char device_path[128] = {0};
    sprintf(device_path, "%s/" DEVICE_NAME, getpwuid(getuid())->pw_dir);
    super.driver_fd = ddriver_open(device_path);

    printf("super.driver_fd: %d\n", super.driver_fd);


    // /* 填充super信息 */
    // super.sz_io = 512/* TODO */;
    // super.sz_disk = 1048576/* TODO */;
    // super.sz_blks = 1024/* TODO */; 
    /* 填充super信息 */
    ddriver_ioctl(super.driver_fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io);    // 获取IO块大小
    ddriver_ioctl(super.driver_fd, IOC_REQ_DEVICE_SIZE, &super.sz_disk); // 获取磁盘容量
    super.sz_blks = 2 * super.sz_io; // 一个逻辑块是两个IO块

    return 0;
}

/* 卸载文件系统 */
static void demo_umount(void* p){
    // 关闭驱动
    ddriver_close(super.driver_fd);
}

/* 遍历目录 */
static int demo_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    // 此处任务一同学无需关注demo_readdir的传入参数，也不要使用到上述参数

    char filename[128]; // 待填充的

    /* 根据超级块的信息，从第500逻辑块读取一个dentry，ls将只固定显示这个文件名 */
    char io_buffer[super.sz_io]; 

    struct demo_dentry dentry; // 用于存放从磁盘块读取的目录项
    /* TODO: 计算磁盘偏移off，并根据磁盘偏移off调用ddriver_seek移动磁盘头到磁盘偏移off处 */
    int block_number = 500;                            // 第500逻辑块
    int disk_offset = block_number * super.sz_blks;    // 偏移量 = 逻辑块号 * 逻辑块大小

    /* TODO: 调用ddriver_read读出一个磁盘块到内存，512B */
    ddriver_seek(super.driver_fd, disk_offset, SEEK_SET); // 移动磁盘头到目标偏移

    ddriver_read(super.driver_fd, io_buffer, super.sz_io); // 从磁盘读取一个IO块大小的数据

    /* TODO: 使用memcpy拷贝上述512B的前sizeof(demo_dentry)字节构建一个demo_dentry结构 */
    memcpy(&dentry, io_buffer, sizeof(struct demo_dentry)); // 从内存数据中拷贝目录项数据

    /* TODO: 填充filename */
    strncpy(filename, dentry.fname, sizeof(dentry.fname));
    filename[sizeof(dentry.fname) - 1] = '\0'; // 确保filename以NULL结尾

    // 此处大家先不关注filler，已经帮同学写好，同学填充好filename即可
    return filler(buf, filename, NULL, 0);
}

/* 显示文件属性 */
static int demo_getattr(const char* path, struct stat *stbuf)
{
    if(strcmp(path, "/") == 0)
        stbuf->st_mode = DEMO_DEFAULT_PERM | S_IFDIR;            // 根目录是目录文件
    else
        stbuf->st_mode = DEMO_DEFAULT_PERM | S_IFREG/* TODO: 显示为普通文件 */;            // 该文件显示为普通文件
    return 0;
}

/* 根据任务1需求 只用实现前四个钩子函数即可完成ls操作 */
static struct fuse_operations ops = {
	.init = demo_mount,						          /* mount文件系统 */		
	.destroy = demo_umount,							  /* umount文件系统 */
	.getattr = demo_getattr,							  /* 获取文件属性 */
	.readdir = demo_readdir,							  /* 填充dentrys */
};

int main(int argc, char *argv[])
{
    int ret = 0;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    ret = fuse_main(args.argc, args.argv, &ops, NULL);
    fuse_opt_free_args(&args);
    return ret;
}
