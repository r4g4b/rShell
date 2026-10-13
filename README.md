# rShell

A small Unix shell written in C, with the system calls written in assembly.

I built this for learning. I wanted to see what a shell actually does
under the hood, and the answer is: not that much. Once you get down to the
low level, things are very simple. A shell is mostly a loop that reads a
line, splits it into words, calls `fork`, moves some file descriptors
around with `dup2`, and calls `execve`. That's it. Everything else is
convenience built on top of that.

## What it can do

- run programs, using `$PATH` to find them (`ls -l`, `/bin/echo hi`)
- pipes, as many as you want: `cat file | grep foo | wc -l`
- redirections: `<`, `>` and `>>`
- builtins: `cd`, `pwd`, `exit [code]`
- Ctrl-C kills the running program, not the shell
- Ctrl-D exits

It doesn't do quotes, variables, globbing (`*.c`), job control or `&&`.
Words must be separated by spaces, so write `ls > out`, not `ls>out`.
I kept it like that on purpose so the code stays easy to read.

## No libc

The binary is not linked against any libc, neither dynamic nor static.
There's no `printf`, no `malloc`, no `fork()` wrapper from glibc. The
program talks to the kernel directly.

- [src/syscalls.asm](src/syscalls.asm) has the entry point `_start` and a
  small wrapper for each system call the shell needs (`read`, `write`,
  `open`, `close`, `pipe`, `dup2`, `fork`, `execve`, `wait4`, `chdir`,
  `getcwd`, `exit`, `rt_sigaction`). Each wrapper just puts the syscall
  number in `rax` and runs the `syscall` instruction.
- [src/str.c](src/str.c) has the few string helpers that libc would
  normally give us.
- [src/main.c](src/main.c) is the shell itself.

The assembly is written for and assembled with
[flat assembler (fasm)](https://flatassembler.net/). fasm outputs the
object file directly and doesn't pull in any C runtime, which fits the
idea of the project. The final link is done with plain `ld`, so you can
check that nothing else gets in.

Only the system calls are in assembly here, but the whole project could
be written in pure assembly the same way, since all the real work is
done by these syscalls anyway. I wrote the rest in C to save time.

## Building

You need Linux on x86_64, `fasm`, `gcc` and `ld` (binutils).

```
sudo apt install fasm gcc make
make
./rshell
```

To confirm there's no libc in it:

```
$ readelf -d rshell
There is no dynamic section in this file.
```

## Example

```
rshell$ pwd
/home/me/rShell
rshell$ ls /etc | grep conf | wc -l
42
rshell$ echo hello > out.txt
rshell$ echo again >> out.txt
rshell$ cat < out.txt
hello
again
rshell$ cd /tmp
rshell$ exit
```

## How a command runs

For `cat f.txt | wc -l` the shell does roughly this:

1. `pipe()` gives two file descriptors, one to read and one to write.
2. `fork()` the first child. It does `dup2(write_end, 1)` so its stdout
   goes into the pipe, then `execve("/bin/cat", ...)`.
3. `fork()` the second child. It does `dup2(read_end, 0)` so its stdin
   comes from the pipe, then `execve("/usr/bin/wc", ...)`.
4. The parent closes its copies of both ends (otherwise `wc` never sees
   end of file) and waits for both children with `wait4()`.

Redirections are the same idea: `open()` the file and `dup2()` it over
fd 0 or 1 before `execve`.
