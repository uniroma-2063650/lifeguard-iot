CXX=avr-g++
CC=avr-gcc
AS=avr-gcc
AVRDUDE=avrdude
AVROBJCOPY=avr-objcopy

CC_FLAGS_COMMON=\
	-O3\
	-funsigned-char\
	-funsigned-bitfields\
	-fshort-enums\
	-Wall\
	/opt/homebrew/opt/avr-gcc@14/avr/lib/avr6/libprintf_flt.a\
	-Wl,-u,vfprintf\
	-DF_CPU=16000000UL

TARGET=mega
AVRDUDE_PORT=/dev/tty.usbserial-10

ifeq ($(TARGET), mega)
	CC_FLAGS_COMMON += -mmcu=atmega2560
	AVRDUDE_FLAGS += -p m2560
	AVRDUDE_BAUDRATE = 115200
	AVRDUDE_BOOTLOADER = wiring
endif

ifeq ($(TARGET), uno)
	CC_FLAGS_COMMON += -mmcu=atmega328p
	AVRDUDE_FLAGS += -p m328p
	AVRDUDE_BAUDRATE = 115200
    AVRDUDE_BOOTLOADER = arduino
endif

CFLAGS+=$(CC_FLAGS_COMMON) --std=c17
CXXFLAGS+=$(CC_FLAGS_COMMON) --std=c++17
ASFLAGS+=$(CC_FLAGS_COMMON) -x assembler-with-cpp

AVRDUDE_FLAGS += -P $(AVRDUDE_PORT) -b $(AVRDUDE_BAUDRATE) -D -q -V -c $(AVRDUDE_BOOTLOADER)

.PHONY: all upload clean serial

all: $(BIN)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s $(HEADERS)
	$(AS) $(ASFLAGS) -c -o $@ $<

$(BIN): $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

upload: $(BIN)
	$(AVROBJCOPY) -O ihex -R .eeprom $(BIN) upload.hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:upload.hex:i
	rm upload.hex

clean:
	rm -f $(BIN) $(OBJS)

serial:
	node ../../utils/serial-reader/main.js $(AVRDUDE_PORT) 19600

.SECONDARY:	$(OBJS)
