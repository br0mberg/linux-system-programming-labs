CC=gcc
CFLAGS=-m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
LDFLAGS=-m32 -nostdlib -Wl,-T,linker.ld -Wl,--build-id=none -no-pie

all: iso

kernel.bin: boot.o kernel.o
	$(CC) $(LDFLAGS) -o $@ $^

boot.o: boot.s
	as --32 -o $@ $<

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<

iso: kernel.bin grub.cfg
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	cp grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o kernel.iso iso >/dev/null 2>&1 || grub-mkrescue -o kernel.iso iso

run: iso
	qemu-system-i386 -cdrom kernel.iso

clean:
	rm -rf iso kernel.iso *.o kernel.bin