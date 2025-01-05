ifdef $(GENDEV)
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

CC = $(ROOTDIR)/m68k-elf/bin/m68k-elf-gcc
AS = $(ROOTDIR)/m68k-elf/bin/m68k-elf-as
OBJC = $(ROOTDIR)/m68k-elf/bin/m68k-elf-objcopy
RM = rm -f

OPTION =
INCS = -I. -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include
CCFLAGS = $(ROOTDIR) -m68000 -Wall -Os -c -fomit-frame-pointer -ffunction-sections -fdata-sections -flto
ASFLAGS = -m68000 --register-prefix-optional
LIBS = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/12.1.0 -L$(ROOTDIR)/m68k-elf/m68k-elf/lib -lc -lgcc -lnosys
LINKFLAGS = -T cd-mode1.ld -Wl,-Map=output.map -nostdlib --specs=nosys.specs -Wl,--gc-sections -Os -flto

OBJS = crt.o pcm.o pcm-io.o adpcm.o adpcm_ima.o s_buffers.o s_channels.o s_main.o s_sources.o s_cd.o s_spcm.o cdfh.o stream.o
ifdef $(ADPCM_ENABLE_SB4)
OBJS += adpcm_sb4.o
endif

all: cd.bin

cd.bin: cd.elf
	$(OBJC) -O binary $< cd.bin

cd.elf: $(OBJS)
	$(CC) $(LINKFLAGS) $(OBJS) $(LIBS) -o cd.elf

%.o: %.c
	$(CC) $(CCFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	$(RM) *.o cd.bin *.elf *.map
