BOARD=chipKIT:pic32:mega_pic32
SKETCH=EEPROMProgrammer
PORT?=/dev/ttyUSB0

SUBDIR=$(subst :,.,$(BOARD))
BIN=bin/$(SKETCH).ino.hex

$(BIN): $(SKETCH).ino
	arduino-cli compile --fqbn $(BOARD) --output-dir bin --build-path build

install: $(BIN)
	arduino-cli upload --fqbn $(BOARD) -p $(PORT) --input-dir bin

clean:
	rm -rf build bin

watch:
	echo ${SKETCH}.ino | entr -c -s 'make'
