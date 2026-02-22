# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

# required files for cp1
#
SYSOBJS_CP1 = \
	heap0.o \
	device.o \
	see.o \
	string.o \
	assert.o \
	console.o \
	error.o \
	excp.o \
	intr.o \
	plic.o \
	io.o \
	start.o \
	trap.o \
	dev/rtc.o \
	dev/uart.o

# required files for cp2
#
SYSOBJS_CP2 = $(SYSOBJS_CP1) \
	dev/viorng.o \
	dev/virtio.o

# required files for cp3
#
SYSOBJS_CP3 = $(SYSOBJS_CP2) \
	thrasm.o \
	thread.o \
	timer.o

CFLAGS = -Wall -Werror=implicit-function-declaration
CFLAGS += -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.

# Uncomment for debug and trace messages
#
# CFLAGS += -DDEBUG -DTRACE # Everything!
# CFLAGS += -DHEAP_DEBUG -DHEAP_TRACE
# CFLAGS += -DTHREAD_DEBUG -DTHREAD_TRACE
# CFLAGS += -DTIMER_DEBUG -DTIMER_TRACE

ASFLAGS = -march=rv64imazicsr

LDFLAGS = -melf64lriscv

QEMUOPTS = -global virtio-mmio.force-legacy=false
QEMUOPTS += -machine virt -bios none -nographic
QEMUOPTS += -object rng-random,filename=/dev/urandom,id=rng0
QEMUOPTS += -device virtio-rng-device,rng=rng0
QEMUOPTS += -serial mon:stdio

# GENERAL TARGETS
#
all: mp2cp1.elf

clean:
	rm -rf *.o dev/*.o test/*.o demo/*.o *.elf

# CP0 TARGETS
#
mp2cp0.elf: $(SYSOBJS_CP1) demo/mp2cp0.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-mp2cp0: mp2cp0.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< 

debug-mp2cp0: mp2cp0.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -S -s

# CP1 TARGETS
#
mp2cp1.elf: $(SYSOBJS_CP1) obj/trek_mp2.o demo/mp2cp1.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-mp2cp1: mp2cp1.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty

debug-mp2cp1: mp2cp1.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty -S -s

# CP2 TARGETS
#
mp2cp2.elf: $(SYSOBJS_CP2) obj/trek_mp2.o demo/mp2cp2.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-mp2cp2: mp2cp2.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty

debug-mp2cp2: mp2cp2.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty -S -s

# CP3 TARGETS
#
mp2cp3.elf: $(SYSOBJS_CP3) obj/trek_mp2.o obj/rule30_mp2.o demo/mp2cp3.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

run-mp2cp3: mp2cp3.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty -serial pty

debug-mp2cp3: mp2cp3.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< -serial pty -serial pty -S -s


# TEST TARGETS
#
test-string.elf: $(SYSOBJS_CP3) $(GAMEOBJS) test/string.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-string: test-string.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $<

test-viorng.elf: $(SYSOBJS_CP3) $(GAMEOBJS) test/viorng.o
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test-viorng: test-viorng.elf
	$(QEMU) $(QEMUOPTS) -m 8M -kernel $< | python3 test/viorng.py
