ifdef $(GENDEV)
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

LDSCRIPTSDIR = $(ROOTDIR)/ldscripts

LIBPATH = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/4.6.2 -L$(ROOTDIR)/m68k-elf/m68k-elf/lib
INCPATH = -I$. -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include -I../liblzss

CCFLAGS = -m68000 -Wall -O1 -c -fomit-frame-pointer -DLZSS_BUF_SIZE=0x8000
ASFLAGS = -m68000 --register-prefix-optional
LINKFLAGS = -T $(LDSCRIPTSDIR)/mars-md.ld -Wl,-Map=output.map -nostdlib

PREFIX = $(ROOTDIR)/m68k-elf/bin/m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJC = $(PREFIX)objcopy

DD = dd
RM = rm -f

TARGET = m68k
LIBS = $(LIBPATH) -lc -lgcc -lnosys
OBJS = crt0.o font.o main.o kos.o cd.o lzss.o vgm.o vgm_player.o xvprintf.o

all: $(TARGET).bin

$(TARGET).bin: $(TARGET).elf
	$(OBJC) -O binary $< $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(LINKFLAGS) $(OBJS) $(LIBS) -o $(TARGET).elf

%.o: %.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: ../liblzss/%.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $(INCPATH) $< -o $@

clean:
	$(RM) *.o *.bin *.elf output.map
