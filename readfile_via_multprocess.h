#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <getopt.h>

#define OFFSET_FILE	"./read.offset"

struct file_info_t {
	int file_rindex_fd;	// 保存被读取文件当前偏移量的文件描述符
	char *file_rindex_offset_ptr;	// 保存偏移量，用于共享内存到文件
	off_t rindex_offset;	// 保存偏移量
};



int init_for_rindex(struct file_info_t *file_info_st);
int is_exit(char *rfile);
void process_via_child(char *buf, int max_child_num);


