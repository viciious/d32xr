ifdef GENDEV
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

WAD ?= doom32x.wad
BINSIZE ?= 210

LDSCRIPTSDIR = $(ROOTDIR)/ldscripts

LIBPATH = -L$(ROOTDIR)/sh-elf/lib -L$(ROOTDIR)/sh-elf/lib/gcc/sh-elf/4.6.2 -L$(ROOTDIR)/sh-elf/sh-elf/lib
INCPATH = -I. -I$(ROOTDIR)/sh-elf/include -I$(ROOTDIR)/sh-elf/sh-elf/include -I./liblzss

CCFLAGS = -c -std=c11 -m2 -mb
CCFLAGS += -Wall -Wextra -pedantic -Wno-unused-parameter -Wimplicit-fallthrough=0 -Wno-missing-field-initializers -Wnonnull
CCFLAGS += -D__32X__ -DMARS
ifndef ENABLE_DMA_SOUND
CCFLAGS += -DDISABLE_DMA_SOUND
endif
CCFLAGS += -fomit-frame-pointer
LDFLAGS = -T mars-ssf.ld -Wl,-Map=output.map -nostdlib -Wl,--gc-sections,--sort-section=alignment --specs=nosys.specs
ASFLAGS = --big
ASFLAGS += --defsym WADBASE=$(BINSIZE)
ifdef ENABLE_FIRE_ANIMATION
CCFLAGS += -DENABLE_FIRE_ANIMATION
endif
ifdef DISABLE_CDFS
CCFLAGS += -DDISABLE_CDFS
endif
ifdef ENABLE_SSF_MAPPER
HWCCFLAGS += -DENABLE_SSF_MAPPER
CCFLAGS += -DENABLE_SSF_MAPPER
ASFLAGS += --defsym ENABLE_SSF_MAPPER=1
endif
ifdef USE_SMALL_LUMPS
CCFLAGS += -DUSE_SMALL_LUMPS
endif
ifdef MIPLEVELS
CCFLAGS += -DMIPLEVELS=$(MIPLEVELS)
endif

debug: CCFLAGS += -g -ggdb

release: CCFLAGS += -ffast-math -funroll-loops -fno-align-loops -fno-align-jumps -fno-align-labels
release: CCFLAGS += -fno-common -ffunction-sections -fdata-sections -flto=auto

release: LDFLAGS += -Os -flto=auto

MARSHWCFLAGS := $(CCFLAGS)
MARSHWCFLAGS += -O1 -fno-lto

ROQCCFLAGS := $(CCFLAGS)
ROQCCFLAGS += -O2

CCFLAGS += -Os

PREFIX = $(ROOTDIR)/sh-elf/bin/sh-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
OBJC = $(PREFIX)objcopy

DD = dd
RM = rm -f

TARGET ?= D32XR
LIBS = $(LIBPATH) -lc -lgcc -lgcc-Os-4-200 -lnosys
OBJS = \
	crt0.o \
	f_main.o \
	f_wipe.o \
	in_main.o \
	am_main.o \
	st_main.o \
	m_main.o \
	o_main.o \
	comnjag.o \
	vsprintf.o \
	d_main.o \
	g_game.o \
	info.o \
	p_ceilng.o \
	p_doors.o \
	p_enemy.o \
	p_floor.o \
	p_inter.o \
	p_lights.o \
	p_map.o \
	p_maputl.o \
	p_mobj.o \
	p_plats.o \
	p_pspr.o \
	p_setup.o \
	p_spec.o \
	p_switch.o \
	p_telept.o \
	p_tick.o \
	p_base.o \
	p_user.o \
	p_sight.o \
	p_shoot.o \
	p_move.o \
	p_change.o \
	p_slide.o \
	r_main.o \
	r_data.o \
	r_phase1.o \
	r_phase2.o \
	r_phase3.o \
	r_phase5.o \
	r_phase6.o \
	r_phase7.o \
	r_phase8.o \
	r_phase9.o \
	marssound.o \
	sounds.o \
	tables.o \
	w_wad.o \
	z_zone.o \
	marshw.o \
	marsonly.o \
	marsnew.o \
	marsdraw.o \
	marssave.o \
	wadbase.o \
	comnnew.o \
	d_mapinfo.o \
	sh2_fixed.o \
	sh2_draw.o \
	sh2_drawlow.o \
	sh2_mixer.o \
	r_cache.o \
	m_fire.o \
	lzss.o \
	gs_main.o \
	roq_read.o \
	marsroq.o \
	mars_newrb.o

release: $(TARGET).32x

debug: $(TARGET).32x

all: release

m68k.bin:
	make -C src-md

$(TARGET).32x: $(TARGET).elf
	$(OBJC) -O binary $< temp2.bin
	$(DD) if=temp2.bin of=temp.bin bs=$(BINSIZE)K conv=sync
	rm -f temp3.bin
	cat temp.bin $(WAD) >>temp3.bin
	$(DD) if=temp3.bin of=$@ bs=512K conv=sync

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET).elf

crt0.o: crt0.s m68k.bin

marshw.o: marshw.c
	$(CC) $(MARSHWCFLAGS) $(INCPATH) $< -o $@

roq_read.o: roq_read.c
	$(CC) $(ROQCCFLAGS) $(INCPATH) $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: liblzss/%.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $(INCPATH) $< -o $@

clean:
	make clean -C src-md
	$(RM) *.o mr8k.bin $(TARGET).32x $(TARGET).elf output.map temp.bin temp2.bin

optwad:
	mkdir -p iso
	find iso -type f -name "*.WAD" -exec ./wadptr -nomerge -nosquash -nostack -c {} \;

iso: $(TARGET).32x
	mkdir -p iso
	genisoimage -sysid "SEGA SEGACD" -volid "DOOM CD32X FUSION" -full-iso9660-filenames -l -o $(TARGET).iso iso
