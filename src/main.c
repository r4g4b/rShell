/*
 * rshell - a small unix shell.
 *
 * Supports running programs, pipes (|), redirections (<, >, >>)
 * and the builtins cd, pwd and exit. Words have to be separated
 * by spaces, so write "ls > out" and not "ls>out".
 */
#include "sys.h"
#include "str.h"

#define LINE_SIZE  1024
#define MAX_WORDS  128
#define MAX_ARGS   64
#define MAX_CMDS   16

struct cmd {
	char *argv[MAX_ARGS];
	char *in;	/* file after <, or 0 */
	char *out;	/* file after > or >>, or 0 */
	int append;	/* 1 if it was >> */
};

static char *get_env(char **envp, const char *name)
{
	int i, j;

	/* every entry looks like "NAME=value" */
	for (i = 0; envp[i]; i++) {
		for (j = 0; name[j] && envp[i][j] == name[j]; j++)
			;
		if (name[j] == '\0' && envp[i][j] == '=')
			return envp[i] + j + 1;
	}
	return 0;
}

static void set_sigint(unsigned long handler)
{
	struct k_sigaction sa;

	sa.handler = handler;
	sa.flags = 0;
	sa.restorer = 0;
	sa.mask = 0;
	sys_rt_sigaction(SIGINT, &sa, 0, sizeof(sa.mask));
}

/* read one line from stdin, returns -1 on end of file */
static int read_line(char *buf, int size)
{
	int n = 0;
	char c;

	while (n < size - 1) {
		if (sys_read(0, &c, 1) <= 0) {
			if (n == 0)
				return -1;
			break;
		}
		if (c == '\n')
			break;
		buf[n++] = c;
	}
	buf[n] = '\0';
	return n;
}

/* cut the line into words in place, returns the number of words */
static int split_words(char *line, char **words, int max)
{
	int n = 0;

	while (*line) {
		while (*line == ' ' || *line == '\t')
			*line++ = '\0';
		if (*line == '\0')
			break;
		if (n == max)
			break;
		words[n++] = line;
		while (*line && *line != ' ' && *line != '\t')
			line++;
	}
	return n;
}

/* fill cmds from the words, returns number of commands or -1 */
static int parse(char **words, int nwords, struct cmd *cmds)
{
	struct cmd *c;
	int ncmds = 0;
	int argc = 0;
	int i;

	c = &cmds[0];
	c->in = 0;
	c->out = 0;
	c->append = 0;

	for (i = 0; i < nwords; i++) {
		char *w = words[i];

		if (str_eq(w, "|")) {
			if (argc == 0 || ncmds + 1 == MAX_CMDS)
				return -1;
			c->argv[argc] = 0;
			ncmds++;
			c = &cmds[ncmds];
			c->in = 0;
			c->out = 0;
			c->append = 0;
			argc = 0;
		} else if (str_eq(w, "<")) {
			if (i + 1 == nwords)
				return -1;
			c->in = words[++i];
		} else if (str_eq(w, ">") || str_eq(w, ">>")) {
			if (i + 1 == nwords)
				return -1;
			c->append = (w[1] == '>');
			c->out = words[++i];
		} else {
			if (argc == MAX_ARGS - 1)
				return -1;
			c->argv[argc++] = w;
		}
	}
	if (argc == 0)
		return -1;
	c->argv[argc] = 0;
	return ncmds + 1;
}

/* only called in the child, so it's fine to exit on errors */
static void redirect(struct cmd *c)
{
	int fd;

	if (c->in) {
		fd = sys_open(c->in, O_RDONLY, 0);
		if (fd < 0) {
			print(2, "rshell: can't open ");
			print(2, c->in);
			print(2, "\n");
			sys_exit(1);
		}
		sys_dup2(fd, 0);
		sys_close(fd);
	}
	if (c->out) {
		int flags = O_WRONLY | O_CREAT;

		if (c->append)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;
		fd = sys_open(c->out, flags, 0644);
		if (fd < 0) {
			print(2, "rshell: can't open ");
			print(2, c->out);
			print(2, "\n");
			sys_exit(1);
		}
		sys_dup2(fd, 1);
		sys_close(fd);
	}
}

/*
 * execve only returns if it failed. If the name has no '/' we try
 * every directory in $PATH until one works.
 */
static void exec_program(char **argv, char **envp)
{
	char path[512];
	char *dirs;
	char *p;
	int i;

	if (str_has(argv[0], '/')) {
		sys_execve(argv[0], argv, envp);
		return;
	}

	dirs = get_env(envp, "PATH");
	if (!dirs)
		dirs = "/bin:/usr/bin";

	while (*dirs) {
		i = 0;
		while (*dirs && *dirs != ':' && i < 400)
			path[i++] = *dirs++;
		if (*dirs == ':')
			dirs++;
		if (i == 0)
			path[i++] = '.';
		path[i++] = '/';
		for (p = argv[0]; *p && i < 510; p++)
			path[i++] = *p;
		path[i] = '\0';
		sys_execve(path, argv, envp);
	}
}

static void run(struct cmd *cmds, int ncmds, char **envp)
{
	int pids[MAX_CMDS];
	int fds[2];
	int prev = -1;	/* read end of the pipe from the last command */
	int status;
	int i, j;

	for (i = 0; i < ncmds; i++) {
		int last = (i == ncmds - 1);

		if (!last && sys_pipe(fds) < 0) {
			print(2, "rshell: pipe failed\n");
			break;
		}

		pids[i] = sys_fork();
		if (pids[i] < 0) {
			print(2, "rshell: fork failed\n");
			if (!last) {
				sys_close(fds[0]);
				sys_close(fds[1]);
			}
			break;
		}

		if (pids[i] == 0) {
			/* child */
			set_sigint(SIG_DFL);
			if (prev != -1) {
				sys_dup2(prev, 0);
				sys_close(prev);
			}
			if (!last) {
				sys_close(fds[0]);
				sys_dup2(fds[1], 1);
				sys_close(fds[1]);
			}
			redirect(&cmds[i]);
			exec_program(cmds[i].argv, envp);
			print(2, "rshell: command not found: ");
			print(2, cmds[i].argv[0]);
			print(2, "\n");
			sys_exit(127);
		}

		/* parent: close what the children use now */
		if (prev != -1)
			sys_close(prev);
		prev = -1;
		if (!last) {
			sys_close(fds[1]);
			prev = fds[0];
		}
	}

	if (prev != -1)
		sys_close(prev);

	for (j = 0; j < i; j++)
		sys_wait4(pids[j], &status, 0, 0);
}

/* returns 1 if argv was a builtin and we already ran it */
static int builtin(char **argv, char **envp)
{
	char buf[512];

	if (str_eq(argv[0], "exit")) {
		sys_exit(argv[1] ? str_to_int(argv[1]) : 0);
	}

	if (str_eq(argv[0], "cd")) {
		char *dir = argv[1];

		if (!dir)
			dir = get_env(envp, "HOME");
		if (!dir)
			dir = "/";
		if (sys_chdir(dir) < 0) {
			print(2, "cd: can't cd to ");
			print(2, dir);
			print(2, "\n");
		}
		return 1;
	}

	if (str_eq(argv[0], "pwd")) {
		if (sys_getcwd(buf, sizeof(buf)) < 0) {
			print(2, "pwd: failed\n");
		} else {
			print(1, buf);
			print(1, "\n");
		}
		return 1;
	}

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	char line[LINE_SIZE];
	char *words[MAX_WORDS];
	struct cmd cmds[MAX_CMDS];
	int nwords, ncmds;

	/* ctrl-c should kill the running program, not the shell */
	set_sigint(SIG_IGN);

	for (;;) {
		print(1, "rshell$ ");
		if (read_line(line, sizeof(line)) < 0)
			break;

		nwords = split_words(line, words, MAX_WORDS);
		if (nwords == 0)
			continue;

		ncmds = parse(words, nwords, cmds);
		if (ncmds < 0) {
			print(2, "rshell: syntax error\n");
			continue;
		}

		if (ncmds == 1 && builtin(cmds[0].argv, envp))
			continue;

		run(cmds, ncmds, envp);
	}

	print(1, "\n");
	return 0;
}
