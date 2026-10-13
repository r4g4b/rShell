# Everything is linked with plain ld, no libc at all.

CC     = gcc
CFLAGS = -Wall -O1 -ffreestanding -fno-builtin -fno-stack-protector \
         -fno-pie -fno-asynchronous-unwind-tables -nostdlib

OBJS = build/syscalls.o build/str.o build/main.o

rshell: $(OBJS)
	ld -static -o $@ $(OBJS)

build/syscalls.o: src/syscalls.asm | build
	fasm $< $@

build/%.o: src/%.c src/sys.h src/str.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build rshell

.PHONY: clean
