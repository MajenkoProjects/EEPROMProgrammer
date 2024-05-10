#include <CLI.h>

#define ADDR(X) (0x100 | (X))
#define DATA(X) (0x200 | (X))
#define CE 0x300
#define OE 0x301
#define WE 0x302
#define NC 0xFFF

const uint32_t SST39SF020A[32] = {
	NC,
	ADDR(16),
	ADDR(15),
	ADDR(12),
	ADDR(7),
	ADDR(6),
	ADDR(5),
	ADDR(4),
	ADDR(3),
	ADDR(2),
	ADDR(1),
	ADDR(0),
	DATA(0),
	DATA(1),
	DATA(2),
	NC,
	DATA(3),	
	DATA(4),
	DATA(5),
	DATA(6),
	DATA(7),
	CE,
	ADDR(10),
	OE,
	ADDR(11),
	ADDR(9),
	ADDR(8),
	ADDR(13),
	ADDR(14),
	ADDR(17),
	WE,
	NC
};

#define PIN_BASE 22

const uint32_t *chip = SST39SF020A;

static inline uint8_t h2d(unsigned char hex) {
    if(hex > 0x39) hex -= 7; // adjust for hex letters upper or lower case
    return(hex & 0xf);
}

void myDigitalWrite(int pin, int val) {
	switch (pin) {
		case 24:
			digitalWrite(2, val);
			break;
		case 26:
			digitalWrite(3, val);
			break;
		case 27:
			digitalWrite(4, val);
			break;
		case 50:
			digitalWrite(5, val);
			break;
		case 51:
			digitalWrite(6, val);
			break;
		default:
			digitalWrite(pin, val);
	}
}

void myPinMode(int pin, int val) {
	switch (pin) {
		case 24:
			pinMode(2, val);
			break;
		case 26:
			pinMode(3, val);
			break;
		case 27:
			pinMode(4, val);
			break;
		case 50:
			pinMode(5, val);
			break;
		case 51:
			pinMode(6, val);
			break;
		default:
			pinMode(pin, val);
	}
}


void config_pins() {
	for (int pin = 0; pin < 32; pin++) {
		switch (chip[pin] & 0xF00) {
			case 0x100: // Address
				myPinMode(PIN_BASE + pin, OUTPUT);
				myDigitalWrite(PIN_BASE + pin, LOW);
				break;
			case 0x200: // Data
				myPinMode(PIN_BASE + pin, INPUT);
				break;
			case 0x300: // Control
				myPinMode(PIN_BASE + pin, OUTPUT);
				myDigitalWrite(PIN_BASE + pin, HIGH);
				break;
		}
	}
}

void set_address(uint32_t address) {
	for (int pin = 0; pin < 32; pin++) {
		switch (chip[pin] & 0xF00) {
			case 0x100: // Address
				myPinMode(PIN_BASE + pin, OUTPUT);
				myDigitalWrite(PIN_BASE + pin, (address & (1UL << (chip[pin] & 0xFFUL))) ? HIGH : LOW);
				break;
		}
	}
}

void set_pin(int func, int level) {
	for (int pin = 0; pin < 32; pin++) {
		if (chip[pin] == func) {
			myPinMode(PIN_BASE + pin, OUTPUT);
			myDigitalWrite(PIN_BASE + pin, level);
		}
	}
}

void set_read_mode() {
	for (int pin = 0; pin < 32; pin++) {
		switch (chip[pin] & 0xF00) {
			case 0x200: // Data
				myPinMode(PIN_BASE + pin, INPUT);
				break;
		}
	}
}

void set_write_mode() {
	for (int pin = 0; pin < 32; pin++) {
		switch (chip[pin] & 0xF00) {
			case 0x200: // Data
				myPinMode(PIN_BASE + pin, OUTPUT);
				break;
		}
	}
}

uint8_t read_byte() {
	uint8_t val = 0;
	for (int pin = 0; pin < 32; pin++) {
		switch (chip[pin] & 0xF00) {
			case 0x200: // Data
				val |= (digitalRead(PIN_BASE + pin) << (chip[pin] & 0xFF));
				break;
		}
	}
	return val;
}

void write_byte(uint8_t val) {
	for (int pin = 0; pin < 32; pin++) {
		switch (chip[pin] & 0xF00) {
			case 0x200: // Data
				myDigitalWrite(PIN_BASE + pin, (val & (1 << (chip[pin] & 0xFF))) ? HIGH : LOW);
				break;
		}
	}
}

int read(int address) {
	set_address(address);
	set_read_mode();
	set_pin(WE, HIGH);
	set_pin(CE, LOW);
	set_pin(OE, LOW);
	uint8_t val = read_byte();
	set_pin(OE, HIGH);
	set_pin(CE, HIGH);
	return val;
}

void write(int address, uint8_t val) {
	set_address(address);
	set_write_mode();
	write_byte(val);
	set_pin(CE, LOW);
	set_pin(OE, HIGH);
	set_pin(WE, LOW);
	delayMicroseconds(5);
	set_pin(WE, HIGH);
	set_pin(CE, HIGH);
}

void erase_chip() {
	write(0x5555, 0xAA);
	write(0x2AAA, 0x55);
	write(0x5555, 0x80);
	write(0x5555, 0xAA);
	write(0x2AAA, 0x55);
	write(0x5555, 0x10);
	delay(100);
}

void write_byte(int address, uint8_t val) {
	write(0x5555, 0xAA);
	write(0x2AAA, 0x55);
	write(0x5555, 0xA0);
	write(address, val);
	delayMicroseconds(20);
}

void dump_chip() {
	for (int addr = 0; addr < 0x40000; addr += 16) {
		Serial.printf("%06X:", addr);
		for (int sub = 0; sub < 16; sub++) {
			Serial.printf(" %02X", read(addr + sub));
		}
		Serial.println();
	}
}

uint8_t checksum(int *cs, uint8_t b) {
	*cs += b;
	return ((256 - (*cs & 0xFF)) & 0xFF);
}

void ihex_send(CLIClient *dev, uint32_t addr, uint8_t type, const uint8_t *buf, uint8_t len) {
	int cs = 0;
	dev->print(":");
	uint8_t csout = 0;

	dev->printf("%02X", len);
	csout = checksum(&cs, len);
	dev->printf("%02X", (addr >> 8) & 0xFF);
	csout = checksum(&cs, (addr >> 8) & 0xFF);
	dev->printf("%02X", addr & 0xFF);
	csout = checksum(&cs, addr & 0xFF);
	dev->printf("%02X", type);
	csout = checksum(&cs, type);

	for (int i = 0; i < len; i++) {
		dev->printf("%02X", buf[i]);
		csout = checksum(&cs, buf[i]);
	}
	dev->printf("%02X", csout);
	dev->println();
}

void ihex_id(CLIClient *dev) {
    uint8_t tbuf[2];

    write(0x5555, 0xAA);
    write(0x2AAA, 0x55);
    write(0x5555, 0x90);
    tbuf[0] = read(0); // Manufacturer
    tbuf[1] = read(1); // Product
    write(0x5555, 0xAA);
    write(0x2AAA, 0x55);
    write(0x5555, 0xF0);

    ihex_send(dev, 0, 0x0d, tbuf, 2);
}


CLI_COMMAND(dump_ihex) {
	uint8_t tbuf[16];
	ihex_id(dev);
	for (uint32_t addr = 0; addr < 0x40000; addr += 16) {
		if ((addr & 0x0000FFFFUL) == 0) {
			tbuf[0] = addr >> 24;
			tbuf[1] = addr >> 16;
			ihex_send(dev, 0, 4, tbuf, 2);
		}
		for (int sub = 0; sub < 16; sub++) {
			tbuf[sub] = read(addr + sub);
		}
		ihex_send(dev, addr, 0, tbuf, 16);
	}
	ihex_send(dev, 0, 1, NULL, 0);
	return 0;
}

CLI_COMMAND(write_ihex) {

	uint32_t address_offset = 0;
	char buffer[100] = {0};
	uint8_t data[50];
	int pos = 0;
	uint8_t cscount = 0;

	ihex_id(dev);

	while (true) {
		if (dev->available()) {
			char c = dev->read();
			switch (c) {
				case '\r':
					// Process the line

					if (strcasecmp(buffer, ":00000001FF") == 0) {
						return 0;
					}
					pos = 0;
					if ((buffer[0] == ':') && (strlen(buffer) > 10)) {
						int dlen = (h2d(buffer[1]) << 4) | h2d(buffer[2]);
						for (int i = 0; i < dlen + 5; i++) {
							data[i] = (h2d(buffer[1 + (i*2)]) << 4) | h2d(buffer[1 + (i*2) +1]);
							cscount += data[i];
						}

						if (cscount != 0) {
							ihex_send(dev, 0, 0xFF, (const uint8_t *)"\x02", 1);
							break;
						}

						uint32_t addr = (data[1] << 8) | data[2];
						int type = data[3];

						switch (type) {
							case 0: // Data
								for (int i = 0; i < dlen; i++) {
									write_byte((address_offset + addr) + i, data[4 + i]);
								}
								break;
							case 2: // Segment
								address_offset = ((data[4] << 8) | data[5]) * 16;
								break;
							case 4: // Offset
								address_offset = (data[4] << 24) | (data[5] << 16);
								break;
						}

						ihex_send(dev, 0, 0xFF, (const uint8_t *)"\x00", 1);
					} else {
						dev->println(":010000FF01FF");
						ihex_send(dev, 0, 0xFF, (const uint8_t *)"\x01", 1);
					}

					break;
				case '\n':
					break;
				default:
					if (pos < 98) {
						buffer[pos++] = c;
						buffer[pos] = 0;
					}
					break;
			}
		}

	}
	return 0;
}

CLI_COMMAND(erase) {
	dev->println("Erasing now");
	erase_chip();
	return 0;
}

CLI_COMMAND(readByte) {
	if (argc != 2) {
		dev->println("Usage: read <address>");
		return 10;
	}
	int addr = strtoul(argv[1], NULL, 0);
	int val = read(addr);
	dev->printf("%06X: %02X\r\n", addr, val);
	return 0;
}

CLI_COMMAND(writeByte) {
	if (argc != 3) {
		dev->println("Usage: write <address> <value>");
		return 10;
	}
	int addr = strtoul(argv[1], NULL, 0);
	int val = strtoul(argv[2], NULL, 0);
	write_byte(addr, val);
	return 0;
}

CLI_COMMAND(id) {
	write(0x5555, 0xAA);
	write(0x2AAA, 0x55);
	write(0x5555, 0x90);
	uint8_t mfg = read(0);
	uint8_t prod = read(1);
	write(0x5555, 0xAA);
	write(0x2AAA, 0x55);
	write(0x5555, 0xF0);
	dev->printf("Manufacturer: %02X, Product: %02X", mfg, prod);
	dev->println();
	return 0;
}

void setup() {
	Serial.begin(1000000);

	config_pins();

    CLI.setDefaultPrompt("> ");

    CLI.addCommand("erase", erase);
	CLI.addCommand("read", readByte);
	CLI.addCommand("write", writeByte);
	CLI.addCommand("dumpihex", dump_ihex);
	CLI.addCommand("writeihex", write_ihex);
	CLI.addCommand("id", id);

    CLI.addClient(Serial);
}

void loop() {
	CLI.process();
}
