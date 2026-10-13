; syscalls.asm - program entry point and system call wrappers.
;
; There is no libc in this program, so this file is the only place
; that talks to the kernel. Each wrapper follows the normal C calling
; convention (args in rdi, rsi, rdx, rcx) and the kernel wants the
; syscall number in rax and the 4th argument in r10 instead of rcx.
;
; Assemble with: fasm syscalls.asm syscalls.o

format ELF64

public _start
extrn main

section '.text' executable

; The kernel starts us here with the stack looking like:
;   [rsp]      argc
;   [rsp+8]    argv[0] ... argv[argc-1], NULL
;   after it   envp[0] ... NULL
_start:
        xor     rbp, rbp
        mov     rdi, [rsp]              ; argc
        lea     rsi, [rsp+8]            ; argv
        lea     rdx, [rsi+rdi*8+8]      ; envp = argv + argc + 1
        and     rsp, -16                ; align stack before the call
        call    main
        mov     edi, eax                ; main's return value
        mov     eax, 60                 ; exit
        syscall

macro syscall_wrapper name, number
{
        public name
name:
        mov     eax, number
        mov     r10, rcx
        syscall
        ret
}

; numbers are from arch/x86/entry/syscalls/syscall_64.tbl
syscall_wrapper sys_read,          0
syscall_wrapper sys_write,         1
syscall_wrapper sys_open,          2
syscall_wrapper sys_close,         3
syscall_wrapper sys_rt_sigaction,  13
syscall_wrapper sys_pipe,          22
syscall_wrapper sys_dup2,          33
syscall_wrapper sys_fork,          57
syscall_wrapper sys_execve,        59
syscall_wrapper sys_exit,          60
syscall_wrapper sys_wait4,         61
syscall_wrapper sys_getcwd,        79
syscall_wrapper sys_chdir,         80

section '.note.GNU-stack'
