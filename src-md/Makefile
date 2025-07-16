ifdef GENDEV
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

LDSCRIPTSDIR = $(ROOTDIR)/ldscripts

LIBPATH = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/12.1.0 -L$(ROOTDIR)/m68k-elf/m68k-elf/lib
INCPATH = -I$. -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include -I../liblzss

HWCCFLAGS = -m68000 -Wall -O1 -c -fomit-frame-pointer -fno-lto
CCFLAGS = -m68000 -Wall -Os -c -fomit-frame-pointer -flto -DUSE_SCD_IN_RAM
ASFLAGS = -m68000 --register-prefix-optional
ifdef ENABLE_SSF_MAPPER
HWCCFLAGS += -DENABLE_SSF_MAPPER
CCFLAGS += -DENABLE_SSF_MAPPER
ASFLAGS += --defsym ENABLE_SSF_MAPPER=1
endif
LINKFLAGS = -T $(LDSCRIPTSDIR)/mars-md.ld -Wl,-Map=output.map -nostdlib -Os -flto

PREFIX = $(ROOTDIR)/m68k-elf/bin/m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJC = $(PREFIX)objcopy

ASMZ80 = $(ROOTDIR)/bin/zasm
FLAGSZ80 = -bv2 -uwy

DD = dd
RM = rm -f

TARGET = m68k
LIBS = $(LIBPATH) -lc -lgcc -lnosys
OBJS = crt0.o everdrive.o main.o kos.o cd.o lzss.o megasd.o vgm.o zfiles.o scd.o scd_pcm.o scd_fs.o scd_roq.o

all: $(TARGET).bin

$(TARGET).bin: $(TARGET).elf
	$(OBJC) -O binary $< $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(LINKFLAGS) $(OBJS) $(LIBS) -o $(TARGET).elf

zfiles.o: z80_vgm.o80

%.o80: %.s80
	$(ASMZ80) $(FLAGSZ80) -o $@ $<

main.o: main.c
	$(CC) $(HWCCFLAGS) $(INCPATH) $< -o $@

megasd.o: megasd.c
	$(CC) $(HWCCFLAGS) $(INCPATH) $< -o $@

everdrive.o: everdrive.c
	$(CC) $(HWCCFLAGS) $(INCPATH) $< -o $@

cd.bin:
	make -C cd

cd.o: cd.s cd.bin

%.o: %.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: ../liblzss/%.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $(INCPATH) $< -o $@

clean:
	make clean -C cd
	$(RM) *.o *.o80 *.bin *.elf output.map

