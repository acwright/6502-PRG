TARGET = Program
EIGHTTHREE = PROGRAM
CONFIG = 6502

# VDP=1 builds for an ACE with a 6502-PICOVDP and BIOS 2.x (6502-VDP.inc).
# The default builds for BIOS 1.x and the TMS9918A (6502.inc).
# ROM=path runs the build on that BIOS image instead of the bundled one.
VDP ?= 0
ifeq ($(VDP),1)
  OUT      = $(TARGET)-VDP
  ASFLAGS  = --asm-define VDP
  # --vdp picovdp needs 6502-EMULATOR 3.x or later
  RUNFLAGS = --vdp picovdp
else
  OUT      = $(TARGET)
  ASFLAGS  =
  RUNFLAGS =
endif
RUNFLAGS += $(if $(ROM),--rom $(ROM))

.PHONY: all build view run woz cf clean

all: build woz cf

build: $(TARGET).asm
	cl65 -t none $(ASFLAGS) -C $(CONFIG).cfg -l $(OUT).lst -o $(OUT).prg $(TARGET).asm

view:
	hexdump -C $(OUT).prg

run:
	6502 run $(RUNFLAGS) $(OUT).prg

woz:
	bin2woz -a 0x0800 $(OUT).prg > $(OUT).woz

cf:
	cffs create $(OUT).img --size 1M
	mkdir -p .cf
	cp -f $(OUT).prg .cf/$(EIGHTTHREE).PRG
	cffs add $(OUT).img .cf/$(EIGHTTHREE).PRG
	rm -rf .cf

clean:
	rm -rf .cf
	rm -f $(TARGET).prg $(TARGET).woz $(TARGET).lst $(TARGET).img
	rm -f $(TARGET)-VDP.prg $(TARGET)-VDP.woz $(TARGET)-VDP.lst $(TARGET)-VDP.img
