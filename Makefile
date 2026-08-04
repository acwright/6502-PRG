TARGET = Program
EIGHTTHREE = PROGRAM
CONFIG = 6502

.PHONY: all build view run woz cf clean

all: build woz cf

build: $(TARGET).asm
	cl65 -t none -C $(CONFIG).cfg -l $(TARGET).lst -o $(TARGET).prg $(TARGET).asm 
	
view:
	hexdump -C $(TARGET).prg

run:
	6502 run $(TARGET).prg

woz:
	bin2woz -a 0x0800 $(TARGET).prg > $(TARGET).woz

cf:
	cffs create $(TARGET).img --size 1M
	cp -f $(TARGET).prg $(EIGHTTHREE).PRG || true
	cffs add $(TARGET).img $(EIGHTTHREE).PRG

clean:
	rm -f $(TARGET).prg $(TARGET).woz $(TARGET).lst $(TARGET).img $(EIGHTTHREE).PRG
