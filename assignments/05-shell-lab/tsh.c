/* 
 * tsh —— 一个带作业控制（job control）的简易 shell
 * 
 * 金多虾 <jinduoxia666@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* 杂项常量 */
#define MAXLINE    1024   /* 一行命令的最大长度 */
#define MAXARGS     128   /* 一行命令里最多多少个参数 */
#define MAXJOBS      16   /* 同一时刻最多多少个作业 */
#define MAXJID    1<<16   /* 作业 ID 的上限 */

/* 作业状态 */
#define UNDEF 0 /* 未定义（空槽位） */
#define FG 1    /* 正在前台运行 */
#define BG 2    /* 正在后台运行 */
#define ST 3    /* 已停止 */

/* 
 * 作业状态：FG（前台）、BG（后台）、ST（停止）
 * 状态转换和触发它的动作：
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg 命令
 *     ST -> BG  : bg 命令
 *     BG -> FG  : fg 命令
 * 任何时刻最多只有 1 个作业处于 FG 状态。
 */

/* 全局变量 */
extern char **environ;      /* 由 libc 定义 */
char prompt[] = "tsh> ";    /* 命令行提示符（不要改） */
int verbose = 0;            /* 为真时打印额外的诊断信息 */
int nextjid = 1;            /* 下一个要分配的作业 ID */
char sbuf[MAXLINE];         /* 用来拼 sprintf 消息的缓冲区 */

struct job_t {              /* 作业结构体 */
    pid_t pid;              /* 作业的 PID */
    int jid;                /* 作业 ID [1, 2, ...] */
    int state;              /* UNDEF、BG、FG 或 ST */
    char cmdline[MAXLINE];  /* 命令行 */
};
struct job_t jobs[MAXJOBS]; /* 作业列表 */
/* 全局变量结束 */


/* 函数原型 */

/* 下面这些是要你自己实现的函数 */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* 下面这些是官方已经写好的辅助例程 */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main —— shell 的主例程
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* 是否打印提示符（默认打印） */

    /* 把 stderr 重定向到 stdout，这样驱动程序能从同一个管道上
     * 收到 shell 的全部输出 */
    dup2(1, 2);

    /* 解析 shell 自己的命令行参数 */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* 打印帮助信息 */
            usage();
	    break;
        case 'v':             /* 打印额外的诊断信息 */
            verbose = 1;
	    break;
        case 'p':             /* 不打印提示符 */
            emit_prompt = 0;  /* 自动测试时用得上 */
	    break;
	default:
            usage();
	}
    }

    /* 安装信号处理程序 */

    /* 这三个的处理程序要你自己实现 */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* 子进程终止或停止 */

    /* 这个给驱动程序留了一条干净地杀掉 shell 的路 */
    Signal(SIGQUIT, sigquit_handler); 

    /* 初始化作业列表 */
    initjobs(jobs);

    /* shell 的「读取—求值」主循环 */
    while (1) {

	/* 读一行命令 */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* 读到文件结束（ctrl-d） */
	    fflush(stdout);
	    exit(0);
	}

	/* 求值这行命令 */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* 控制流永远到不了这里 */
}
  
/* 
 * eval —— 对用户刚敲进来的这行命令求值
 * 
 * 如果用户敲的是内建命令（quit、jobs、bg、fg），就立刻执行它；否则
 * fork 出一个子进程，在子进程的上下文里运行这个作业。如果作业运行在
 * 前台，就等它结束再返回。注意：每个子进程都必须有自己独立的进程组
 * ID，这样我们在键盘上敲 ctrl-c（ctrl-z）时，内核才不会把 SIGINT
 * （SIGTSTP）也发给后台的子进程。
*/
void eval(char *cmdline) 
{
    char *argv[MAXARGS];       /* 传给 execve() 的参数列表 */

    parseline(cmdline, argv);
    if (argv[0] == NULL)       /* 空行直接忽略 */
        return;

    if (builtin_cmd(argv))     /* 内建命令在 shell 自己的进程里执行 */
        return;

    /* TODO(trace03)：fork 子进程 → setpgid(0,0) → execve 运行程序 */
}

/* 
 * parseline —— 解析命令行，构造 argv 数组
 * 
 * 单引号括起来的内容算作一个参数。用户要的是后台（BG）作业就返回真，
 * 要的是前台（FG）作业就返回假。
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* 命令行的本地副本 */
    char *buf = array;          /* 遍历命令行的指针 */
    char *delim;                /* 指向第一个空格分隔符 */
    int argc;                   /* 参数个数 */
    int bg;                     /* 是不是后台作业？ */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* 把结尾的 '\n' 换成空格 */
    while (*buf && (*buf == ' ')) /* 跳过开头的空格 */
	buf++;

    /* 逐个切出参数，填进 argv */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* 跳过空格 */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* 空行，直接忽略 */
	return 1;

    /* 命令行以 & 结尾的话，这个作业要放到后台跑 */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd —— 如果用户敲的是内建命令，就立刻执行它
 *    是内建命令返回 1，不是返回 0。
 */
int builtin_cmd(char **argv) 
{
    if (!strcmp(argv[0], "quit"))  /* quit：直接结束 shell */
        exit(0);

    return 0;     /* 不是内建命令 */
}

/* 
 * do_bgfg —— 执行内建的 bg 和 fg 命令
 */
void do_bgfg(char **argv) 
{
    return;
}

/* 
 * waitfg —— 阻塞，直到进程 pid 不再是前台进程为止
 */
void waitfg(pid_t pid)
{
    return;
}

/*****************
 * 信号处理程序
 *****************/

/* 
 * sigchld_handler —— 只要有子作业终止（变成僵死进程），或者因为收到
 *     SIGSTOP / SIGTSTP 而停止，内核就会给 shell 发一个 SIGCHLD。
 *     这个处理程序要把当前所有能回收的僵死子进程都回收掉，但**不能**
 *     去等那些还在运行的子进程结束。
 */
void sigchld_handler(int sig) 
{
    return;
}

/* 
 * sigint_handler —— 用户在键盘上敲 ctrl-c 时，内核会给 shell 发一个
 *    SIGINT。捕获它，然后转发给前台作业。
 */
void sigint_handler(int sig) 
{
    return;
}

/*
 * sigtstp_handler —— 用户在键盘上敲 ctrl-z 时，内核会给 shell 发一个
 *     SIGTSTP。捕获它，再给前台作业发一个 SIGTSTP 把它挂起。
 */
void sigtstp_handler(int sig) 
{
    return;
}

/*********************
 * 信号处理程序结束
 *********************/

/***********************************************
 * 操作作业列表的辅助例程（官方已写好）
 **********************************************/

/* clearjob —— 清空一个作业结构体的各个字段 */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs —— 初始化作业列表 */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid —— 返回已分配的最大作业 ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob —— 往作业列表里加一个作业 */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob —— 从作业列表里删掉 PID 等于 pid 的作业 */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid —— 返回当前前台作业的 PID，没有前台作业则返回 0 */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid —— 按 PID 在作业列表里找一个作业 */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid —— 按 JID 在作业列表里找一个作业 */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid —— 把进程 ID 映射成作业 ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs —— 打印作业列表 */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * 作业列表辅助例程结束
 ******************************/


/***********************
 * 其它辅助例程（官方已写好）
 ***********************/

/*
 * usage —— 打印帮助信息
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error —— Unix 风格的报错例程（打印 errno 对应的消息后退出）
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error —— 应用风格的报错例程（只打印消息后退出）
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal —— sigaction 的包装函数
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* 处理期间只阻塞当前这种信号 */
    action.sa_flags = SA_RESTART; /* 尽量重启被中断的系统调用 */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler —— 驱动程序可以给子 shell 发 SIGQUIT，
 *    让它干净地退出。
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



