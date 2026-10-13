#ifndef SYS_H
#define SYS_H

/* implemented in syscalls.asm, they return -errno on failure */
long sys_read(int fd, void *buf, unsigned long count);
long sys_write(int fd, const void *buf, unsigned long count);
long sys_open(const char *path, int flags, int mode);
long sys_close(int fd);
long sys_rt_sigaction(int sig, void *act, void *oldact, unsigned long size);
long sys_pipe(int *fds);
long sys_dup2(int oldfd, int newfd);
long sys_fork(void);
long sys_execve(const char *path, char **argv, char **envp);
long sys_exit(int code);
long sys_wait4(int pid, int *status, int options, void *rusage);
long sys_getcwd(char *buf, unsigned long size);
long sys_chdir(const char *path);

/* values from the linux headers */
#define O_RDONLY  0
#define O_WRONLY  1
#define O_CREAT   0100
#define O_TRUNC   01000
#define O_APPEND  02000

#define SIGINT   2
#define SIG_DFL  0
#define SIG_IGN  1

/* the kernel's version of struct sigaction, not the libc one */
struct k_sigaction {
	unsigned long handler;
	unsigned long flags;
	unsigned long restorer;
	unsigned long mask;
};

#endif
