AVR_DIR := /root/arduino-1.8.15/hardware/tools/avr/bin/
AVR_GCC := $(AVR_DIR)avr-gcc
AVR_OBJCOPY := $(AVR_DIR)avr-objcopy -j .text -j .data -O ihex
AVR_DUDE := avrdude -v -patmega328p -cstk500v1 -P/dev/ttyACM0 -b115200
AVR_CFLAGS := -O2 -mmcu=atmega328p
AVR_LFLAGS := -O2 -mmcu=atmega328p -Wl,--section-start=.text=0x0000 -nostdlib

GUICAST := ../hvirtual/guicast/x86_64/

atmega_pwm: atmega_pwm.c
	$(AVR_GCC) $(AVR_CFLAGS) -o atmega_pwm.o atmega_pwm.c
	$(AVR_GCC) $(AVR_LFLAGS) -o atmega_pwm.elf atmega_pwm.o
	$(AVR_OBJCOPY) atmega_pwm.elf atmega_pwm.hex




# program standalone atmega328 fuses
atmega_fuse:
	$(AVR_DUDE) -e -Ulock:w:0x3F:m -Uefuse:w:0x05:m -Uhfuse:w:0xDA:m -Ulfuse:w:0xE2:m 

# program atmega_pwm.hex
atmega_pwm_isp: atmega_pwm.hex
	$(AVR_DUDE) -Uflash:w:atmega_pwm.hex:i -Ulock:w:0x0F:m


grapher: grapher.C $(GUICAST)/libguicast.a $(GUICAST)/libcmodel.a
	g++ -O2 -g -o grapher grapher.C \
		-I../hvirtual/guicast \
		$(GUICAST)/libguicast.a \
		$(GUICAST)/libcmodel.a \
		-lX11 \
		-lXext \
		-lXft \
		-lXv \
		-lGL \
		-lpthread \
		-lpng \
		-lm \


