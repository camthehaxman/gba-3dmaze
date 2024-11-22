OPTIMIZE := -O3
#SANITIZE := -fsanitize=address
#PROFILE := -pg -fno-inline

CFLAGS  := $(shell sdl2-config --cflags) -Wall -g $(OPTIMIZE) $(SANITIZE) $(PROFILE)
LDFLAGS := $(SANITIZE) $(PROFILE) $(shell sdl2-config --libs) -lm
BITMAPS := brick.bmp castle.bmp cover8.bmp wood24.bmp
OBJECTS := 3dmaze.o sineTable.o $(BITMAPS:.bmp=.o)

GBA_PREFIX  := $(DEVKITARM)/bin/arm-none-eabi-
GBA_CC      := $(GBA_PREFIX)gcc
GBA_LD      := $(GBA_PREFIX)ld
GBA_OBJCOPY := $(GBA_PREFIX)objcopy
GBA_CFLAGS  := -I$(DEVKITPRO)/libgba/include -mcpu=arm7tdmi -mtune=arm7tdmi -mthumb -mthumb-interwork $(OPTIMIZE) -DNDEBUG
GBA_LDFLAGS := -L$(DEVKITPRO)/libgba/lib -lgba -lm
GBAFIX      := $(DEVKITPRO)/tools/bin/gbafix

default: 3dmaze 3dmaze.gba

# Host (PC) version
3dmaze: $(OBJECTS) sdl.o

# GBA version
3dmaze.elf: $(OBJECTS:.o=.gba.o) gba.gba.o
	$(GBA_CC) -specs=gba.specs $^ $(GBA_LDFLAGS) -Xlinker -Map=$(@:.elf=.map) -o $@
	nm $@ | cut -d' ' -f1,3 | sort | sed -e '/ __.*\(start\|end\)/d' > $(@:.elf=.sym)

profile: 3dmaze
	$(RM) gmon.out
	./3dmaze
	gprof 3dmaze gmon.out

clean:
	$(RM) 3dmaze *.gba *.elf *.o *.sym *.map gmon.out sineTable.c

%.c: %.bmp image_convert.py
	./image_convert.py $< $(basename $<) > $@

sineTable.c: make_sin_table.py
	./make_sin_table.py > $@

%.gba.o: %.c
	$(GBA_CC) $(GBA_CFLAGS) -c -o $@ $<

%.gba: %.elf
	$(GBA_OBJCOPY) -O binary $< $@
