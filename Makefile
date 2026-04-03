TARGET = Program
EIGHTTHREE = PROGRAM
CONFIG = 6502

.PHONY: all clean

all: build woz cf

build: $(TARGET).asm
	cl65 -t none -C $(CONFIG).cfg -l $(TARGET).lst -o $(TARGET).prg $(TARGET).asm 
	
view:
	hexdump -C $(TARGET).prg

woz:
	bin2woz -a 0x0800 $(TARGET).prg > $(TARGET).woz

cf:
	cffs create -s 128M $(TARGET).img
	cp $(TARGET).prg $(EIGHTTHREE).PRG
	cffs add $(TARGET).img $(EIGHTTHREE).PRG
	rm HELLOWLD.PRG

clean:
	rm $(TARGET).prg
	rm $(TARGET).woz
	rm $(TARGET).lst
	rm $(TARGET).img
