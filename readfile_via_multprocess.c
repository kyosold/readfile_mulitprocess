#include "readfile_via_multprocess.h"

volatile int current_child_num;


int init_for_rindex(struct file_info_t *file_info_st)
{
	file_info_st->file_rindex_fd = 0;
	file_info_st->file_rindex_offset_ptr = NULL;
	file_info_st->rindex_offset = 0;

	struct tm *tmp;
	struct stat st;
	
	char now_str[1024] = {0};
	time_t cur = time(NULL);
	
	tmp = localtime(&cur);
	strftime(now_str, sizeof(now_str) - 1, "%Y%m%d", tmp);
	
	file_info_st->file_rindex_fd = open(OFFSET_FILE, O_RDWR| O_NDELAY | O_CREAT, 0600);
	if (file_info_st->file_rindex_fd == -1) {
		printf("open file:%s fail:%m\n", OFFSET_FILE);
		return 1;
	}
	
	fcntl(file_info_st->file_rindex_fd, F_SETFD, 1);	// 禁止与子进程共享

	if (fstat(file_info_st->file_rindex_fd, &st) == -1) {
		printf("fstat file:%s fail:%m\n", OFFSET_FILE);
		close(file_info_st->file_rindex_fd);
		return 1;
	}
	
	if (st.st_size < 1) {
		ftruncate(file_info_st->file_rindex_fd, 1024);
	}
	
	file_info_st->file_rindex_offset_ptr = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, file_info_st->file_rindex_fd, 0);
	if (file_info_st->file_rindex_offset_ptr == MAP_FAILED) {
		printf("mmap file:%s fail:%m\n", OFFSET_FILE);
		close(file_info_st->file_rindex_fd);
		return 1;
	}
	
	if (st.st_size > 0) {
		file_info_st->rindex_offset = atoll(file_info_st->file_rindex_offset_ptr);
	}
	
	return 0;
}

// 判断文件最后一次修改时间 为昨天的话退出
int is_exit(char *rfile)
{
	struct stat st = {0};
	if (stat(rfile, &st) == -1) {
		printf("stat file:%s fail:%m\n", rfile); 
		return 0;
	}
	if (time(NULL) > (st.st_mtime + (3600 * 24))) {
		return 1;
	}
	
	return  0;
}

void process_via_child(char *buf, int max_child_num)
{
	while (current_child_num > max_child_num) {
		printf("no child free, wait 5 second");
		sleep(5);
	}
	
	while ((pid = fork()) == -1) {
		printf("fork fail:%m\n");
		sleep(5);
	}
	
	current_child_num++;
	
	if (pid == 0) {
		printf("child[%u]:%s\n", getpid(), buf);
		
		// do something ...
		
		_exit(0);
	}
}

void sigchld()
{
    pid_t pid;
    int wstat;
    while ((pid = waitpid(-1, &wstat, WNOHANG)) > 0) {
        current_child_num--;
    }   
}


void sig_catch(int sig, void (*f) () )
{
    struct sigaction sa;
    sa.sa_handler = f;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, (struct sigaction *) 0);
}


void usage(char *prog)
{
	printf("%s -c[child num] -f[file for read]\n", prog);
}

int main(int argc, char **argv)
{
	int max_child_num = 0;
	char rfile[1024] = {0};
	
	// process command line
	int ch;
	const char *args = "c:f:h";
	while ((ch = getopt(argc, argv, args)) != -1) {
		switch (ch) {
			case 'c':
				max_child_num = atoi(optarg);
				break;
			case 'f':
				snprintf(rfile, sizeof(rfile), "%s", optarg);
				break;
			case 'h':
			default:
				usage(argv[0]);
				exit(0);
		} 
	}
	
	current_child_num = 0;
	
	// 捕抓子进程退出信号
    sig_catch(SIGCHLD, sigchld);
	
	// 初始化，读取文件索引内容
	struct file_info_t file_info_st;
	if (init_for_rindex(&file_info_st) != 0) {
		printf("init_for_rindex fail\n");
		return 1;
	}
	
	// 打开文件
	char buf[4096] = {0};
	FILE *fp = fopen(rfile, "r");
	if (!fp) {
		printf("open file:%s fail:%m\n", rfile);
		if (file_info_st.file_rindex_fd) 
			close(file_info_st.file_rindex_fd);
		return 1;
	}
	
	fcntl(fileno(fp), F_SETFD, 1);		// 禁止子进程共享此文件描述符
	
	// 如果偏移量大于0，则偏移文件到上次读取的地方
	if (file_info_st.rindex_offset > 0) {
		if (lseek(fileno(fp), file_info_st.rindex_offset, SEEK_SET) == -1) {
			printf("lseek fail:%m\n");
			fclose(fp);
			if (file_info_st.file_rindex_fd) 
				close(file_info_st.file_rindex_fd);
			return 1;
		}
	}
	
	// 循环读取文件，读一行创建一个进程去执行
	while (1) {
		if (!fgets(buf, sizeof(buf) - 1, fp)) {
			// 读取失败
			if (is_exit(rfile)) {
				printf("rfile:%s was expire, bye!\n", rfile);
				goto bye;
			}
			
			sleep(5);
			continue;
		}
		
		if (*buf == '\0') 
			continue;
		
		// 处理
		process_via_child(buf, max_child_num);
		
		// 保存当前读取的偏移量
		//file_info_st.rindex_offset = lseek(fileno(fp), 0, SEEK_CUR);
		file_info_st.rindex_offset = ftell(fp);
		if (file_info_st.rindex_offset > 0) {
			snprintf(file_info_st.file_rindex_offset_ptr, 1023, "%ld", file_info_st.rindex_offset);
		}
	}
	
bye:
	fclose(fp);
	if (file_info_st.file_rindex_fd) 
		close(file_info_st.file_rindex_fd);

	return 0;
}





