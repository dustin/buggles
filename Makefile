MCU=atmega328p

p=/usr/local/CrossPack-AVR/bin/
CC=$(p)avr-gcc
CXX=$(p)avr-g++
OBJCOPY=$(p)avr-objcopy
SIZE=$(p)avr-size

CPUSPEED_atmega328p=16000000UL
CPUSPEED_attiny85=8000000UL

COMMON_FLAGS=-Os -DF_CPU=$(CPUSPEED_$(MCU)) -mmcu=$(MCU)
CFLAGS=$(COMMON_FLAGS) --std=c99
CXXFLAGS=$(COMMON_FLAGS) --std=c++03
ASFLAGS=$(COMMON_FLAGS)

attiny85_OBJS=BasicSerial3.o
OBJS=buggles.o cc2500.o tinyspi.o serial.o $($(MCU)_OBJS)

ifeq ($MCU, attiny85)
	OBJS+=BasicSerial3.o
endif

buggles.hex: buggles.elf
	${OBJCOPY} -O ihex -R .eeprom buggles.elf buggles.hex
	$(SIZE) --format=avr --mcu=$(MCU) buggles.elf

buggles.elf: $(OBJS)
	${CXX} $(CXXFLAGS) -o $@ $(OBJS)

buggles.o: config.h cc2500.h sumd.h tinyspi.h serial.h

serial.o: config.h serial.h serial.c BasicSerial3.h

clean:
	rm buggles.{elf,hex} *.o

install: buggles.hex
	# avrdude -F -V -c usbtiny -p m328p -U flash:w:buggles.hex
	avrdude -v -p atmega328p -c arduino -P /dev/tty.SLAB_USBtoUART -b 57600 -D -U flash:w:buggles.hex:i

fuse:
	avrdude -v -p m328p -c usbtiny -b 57600 -D -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m -U efuse:w:0xff:m
