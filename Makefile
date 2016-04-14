MCU=atmega328p

p=/usr/local/CrossPack-AVR/bin/
CC=$(p)avr-gcc
CXX=$(p)avr-g++
OBJCOPY=$(p)avr-objcopy
SIZE=$(p)avr-size

COMMON_FLAGS=-Os -DF_CPU=16000000UL -mmcu=$(MCU)
CFLAGS=$(COMMON_FLAGS) --std=c99
CXXFLAGS=$(COMMON_FLAGS) --std=c++03

buggles.hex: buggles.elf
	${OBJCOPY} -O ihex -R .eeprom buggles.elf buggles.hex
	$(SIZE) --format=avr --mcu=$(MCU) buggles.elf

buggles.elf: buggles.o cc2500.o tinyspi.o
	${CXX} $(CXXFLAGS) -o buggles.elf buggles.o cc2500.o tinyspi.o

buggles.o: config.h cc2500.h sumd.h tinyspi.h

clean:
	rm buggles.{elf,hex,o} cc2500.o tinyspi.o

install: buggles.hex
	# avrdude -F -V -c usbtiny -p m328p -U flash:w:buggles.hex
	avrdude -v -p atmega328p -c arduino -P /dev/tty.SLAB_USBtoUART -b 57600 -D -U flash:w:buggles.hex:i

fuse:
	avrdude -v -p m328p -c usbtiny -b 57600 -D -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m -U efuse:w:0xff:m
