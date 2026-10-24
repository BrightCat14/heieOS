CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -O2 -Wall -Wextra -ffreestanding -nostdlib -fno-pie -marm -march=armv7-a
LDFLAGS = -T linker.ld -nostdlib

SCAN ?= 0
CFLAGS += -DSCAN_MODE=$(SCAN)

LOG_ADDR = 0x22000
LOG_SIZE = 0x2000

all: baremetal.bin

baremetal.elf: main.c log.h linker.ld
	$(CC) $(CFLAGS) main.c -o $@ $(LDFLAGS)

baremetal.bin: baremetal.elf
	$(OBJCOPY) -O binary $< $@

run: baremetal.bin
	sunxi-fel clear $(LOG_ADDR) $(LOG_SIZE) write 0x20000 baremetal.bin exec 0x20000

log:
	sunxi-fel read $(LOG_ADDR) $(LOG_SIZE) log.bin
	python3 readlog.py log.bin

clean:
	rm -f *.elf *.bin *.o

.PHONY: all run log clean
