/*
 * LED ANIMATION
 * Copyright (C) 2021 Adam Williams <broadcast at earthling dot net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */



// CONFIG
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = ON        // Watchdog Timer Enable bit (WDT enabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF        // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)


#include <pic16f877a.h>
#include <stdint.h>
#include <string.h>

// control motors using console input.  Disable if the serial port is floating.
//#define TEST_MODE
// less accurate motor positioning but more debugging output
//#define VERBOSE

// use XY control instead of polar
//#define XY_CONTROL

#define ABS(x) ((x) < 0 ? (-(x)) : (x))

#define CLOCKSPEED 4194000


//#define ENABLE_SERIAL

// 16f87.pdf page 115
#define BAUD_CODE (CLOCKSPEED / 16 / 9600)

#define UART_BUFSIZE 64
uint8_t uart_buffer[UART_BUFSIZE];
uint8_t uart_size = 0;
uint8_t uart_position1 = 0;
uint8_t uart_position2 = 0;

// tick/PWM timer fires at this speed
// the timer wraparounds per second must be a power of 2
// for the dissolve to be in seconds
#define HZ 256

uint16_t tick = 0;
uint16_t duty = 0;
// amount to left shift to get the number of ticks, since division is too slow
// HZ / 4 
#define DEFAULT_DISSOLVE 6
// minimum duty cycle to get an interrupt
#define MIN_DUTY 1
// maximum duty cycle to get an interrupt
#define MAX_DUTY 255
uint8_t current_line = 0;

uint8_t prev_line = 0;
uint16_t dissolve_ticks = 0;
uint16_t current_delay = 0;

// 16 LED strings
typedef union 
{
	struct
	{
		unsigned bit0 : 1; // grid fin 3 fully extended
		unsigned bit1 : 1; // grid fin 2
		unsigned bit2 : 1; // grid fin 1 retracted
		unsigned bit3 : 1; // landing leg 1 up
		unsigned bit4 : 1; // landing leg 3 + 2 + 1
		unsigned bit5 : 1; // landing leg 1 + 4
		unsigned bit6 : 1; // landing leg 3 + 2 + 1
		unsigned bit7 : 1; // landing leg 2
		unsigned bit8 : 1; // landing leg 3 + 2
		unsigned bit9 : 1; // landing leg 4 + 2
		unsigned bit10 : 1; // landing leg 3 + 2
		unsigned bit11 : 1; // landing leg 3
		unsigned bit12 : 1; // landing leg 4 + 3
		unsigned bit13 : 1; // landing leg 4 down
		unsigned bit14 : 1; // bottom flame
		unsigned bit15 : 1; // top flame
	};
	
	uint16_t value;
} bits_t;


typedef struct
{
    uint8_t porta;
    uint8_t portc;
    uint8_t portd;
    uint16_t ticks;
    uint8_t dissolve;
} sequence_t;

#define TO_PORTS(pins) \
    (((pins) & (1 << 12)) ? (1) : 0) | \
    (((pins) & (1 << 13)) ? (1 << 1) : 0) | \
    (((pins) & (1 << 14)) ? (1 << 2) : 0) | \
    (((pins) & (1 << 15)) ? (1 << 3) : 0), \
 \
    (((pins) & (1)) ? (1 << 3) : 0) | \
    (((pins) & (1 << 5)) ? (1 << 4) : 0) | \
    (((pins) & (1 << 6)) ? (1 << 5) : 0) | \
    (((pins) & (1 << 7)) ? (1 << 7) : 0), \
 \
    (((pins) & (1 << 1)) ? (1) : 0) | \
    (((pins) & (1 << 2)) ? (1 << 1) : 0) | \
    (((pins) & (1 << 3)) ? (1 << 2) : 0) | \
    (((pins) & (1 << 4)) ? (1 << 3) : 0) | \
    (((pins) & (1 << 8)) ? (1 << 4) : 0) | \
    (((pins) & (1 << 9)) ? (1 << 5) : 0) | \
    (((pins) & (1 << 10)) ? (1 << 6) : 0) | \
    (((pins) & (1 << 11)) ? (1 << 7) : 0)


// LED values & delays for each state.
// Translated into pin numbers going clockwise
// bit 15 is A3  bit 0 is C3

#define GRID_FIN_1 0b1000000000000000 // extended
#define GRID_FIN_2 0b0100000000000000
#define GRID_FIN_3 0b0010000000000000 // retracted

#define FLAME_DOWN 0b0000000000000011
#define FLAME_UP   0b0000000000000001

#define LEG_1      0b0001110000000000 // up
#define LEG_2      0b0000101111100000
#define LEG_3      0b0000101010111000
#define LEG_4      0b0000010001001100 // down

sequence_t sequence[] = 
{
// basic animation
    { TO_PORTS(GRID_FIN_3 | LEG_1), HZ * 2, DEFAULT_DISSOLVE },

    { TO_PORTS(GRID_FIN_2 | LEG_1), HZ, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_1), HZ, DEFAULT_DISSOLVE },
    
    { TO_PORTS(GRID_FIN_1 | LEG_1 | FLAME_DOWN), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_1 | FLAME_UP), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_1 | FLAME_DOWN), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_1 | FLAME_UP), HZ / 2 , DEFAULT_DISSOLVE},
    { TO_PORTS(GRID_FIN_1 | LEG_2 | FLAME_DOWN), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_2 | FLAME_UP), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_3 | FLAME_DOWN), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_3 | FLAME_UP), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_4 | FLAME_DOWN), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_4 | FLAME_UP), HZ / 2, DEFAULT_DISSOLVE },
    { TO_PORTS(GRID_FIN_1 | LEG_4 | FLAME_DOWN), HZ / 2, DEFAULT_DISSOLVE },

    { TO_PORTS(GRID_FIN_1 | LEG_4), HZ * 4, 9 },

//      { GRID_FIN_3 | LEG_4, HZ * 2, 9 },
//      { GRID_FIN_3 | LEG_1, HZ * 2, 9 },

// cycle through all the strings
//    { 0b1111111111111111, HZ, 0 },
//    { 0b1000000000000000, HZ },
//    { 0b0100000000000000, HZ },
//    { 0b0010000000000000, HZ },
//    { 0b0001000000000000, HZ },
//    { 0b0000100000000000, HZ },
//    { 0b0000010000000000, HZ },
//    { 0b0000001000000000, HZ },
//    { 0b0000000100000000, HZ },
//    { 0b0000000010000000, HZ },
//    { 0b0000000001000000, HZ },
//    { 0b0000000000100000, HZ },
//    { 0b0000000000010000, HZ },
//    { 0b0000000000001000, HZ },
//    { 0b0000000000000100, HZ },
//    { 0b0000000000000010, HZ },
//    { 0b0000000000000001, HZ },
};

#define SEQUENCE_SIZE (sizeof(sequence) / sizeof(sequence_t))

void show_leds();
void (*led_state)() = show_leds;



void print_byte(uint8_t c)
{
	if(uart_size < UART_BUFSIZE)
	{
		uart_buffer[uart_position1++] = c;
		uart_size++;
		if(uart_position1 >= UART_BUFSIZE)
		{
			uart_position1 = 0;
		}
	}
}

void print_text(const uint8_t *s)
{
	while(*s != 0)
	{
		print_byte(*s);
		s++;
	}
}


void print_number_nospace(uint16_t number)
{
	if(number >= 10000) print_byte('0' + (number / 10000));
	if(number >= 1000) print_byte('0' + ((number / 1000) % 10));
	if(number >= 100) print_byte('0' + ((number / 100) % 10));
	if(number >= 10) print_byte('0' + ((number / 10) % 10));
	print_byte('0' + (number % 10));
}

void print_number2(uint8_t number)
{
	print_byte('0' + ((number / 10) % 10));
	print_byte('0' + (number % 10));
}

void print_number(uint16_t number)
{
    print_number_nospace(number);
   	print_byte(' ');
}

void print_number_signed(int16_t number)
{
    if(number < 0)
    {
        print_byte('-');
        print_number_nospace(-number);
    }
    else
    {
        print_number_nospace(number);
    }
   	print_byte(' ');
}

const char hex_table[] = 
{
	'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

void print_hex2(uint8_t number)
{
	print_byte(hex_table[(number >> 4) & 0xf]);
	print_byte(hex_table[number & 0xf]);
}

void print_bin(uint8_t number)
{
	print_byte((number & 0x80) ? '1' : '0');
	print_byte((number & 0x40) ? '1' : '0');
	print_byte((number & 0x20) ? '1' : '0');
	print_byte((number & 0x10) ? '1' : '0');
	print_byte((number & 0x8) ? '1' : '0');
	print_byte((number & 0x4) ? '1' : '0');
	print_byte((number & 0x2) ? '1' : '0');
	print_byte((number & 0x1) ? '1' : '0');
}


// send a UART char
void handle_uart()
{
    if(uart_size > 0 && PIR1bits.TXIF)
    {
        PIR1bits.TXIF = 0;
        TXREG = uart_buffer[uart_position2++];
		uart_size--;
		if(uart_position2 >= UART_BUFSIZE)
		{
			uart_position2 = 0;
		}
    }
}

void flush_uart()
{
    while(uart_size > 0)
    {
        asm("clrwdt");
        handle_uart();
    }
}


// copy sequence bits to pins
#define write_leds(step) \
{ \
    PORTA = sequence[step].porta; \
    PORTC = sequence[step].portc; \
    PORTD = sequence[step].portd; \
}

void dissolve1();

// write previous state
void dissolve2()
{
// go to always on
// dissolve out previous state
    write_leds(prev_line);


    TMR2 = duty;
    led_state = dissolve1;
}

// write next state
void dissolve1()
{
    uint8_t dissolve = sequence[prev_line].dissolve;
    uint16_t tick2 = tick / 2;
    if(tick2 >= dissolve_ticks)
    {
        tick = 0;
        led_state = show_leds;
//	    print_text("dissolve1 1\n");
    }
    else
    {
// dissolve into next state
        duty = (((uint32_t)tick2) << 8) >> dissolve;
        if(duty > MAX_DUTY)
        {
            duty = MAX_DUTY;
        }
        if(duty < MIN_DUTY)
        {
            duty = MIN_DUTY;
        }

        TMR2 = -duty;
        led_state = dissolve2;
    }

    write_leds(current_line);
}

void show_leds()
{
// next state
    if(tick >= current_delay)
    {
        prev_line = current_line;
        dissolve_ticks = 1 << sequence[current_line].dissolve;
        current_line++;
        if(current_line >= SEQUENCE_SIZE)
        {
            current_line = 0;
        }

        tick = 0;
        uint16_t next_dissolve_ticks = 1 << sequence[current_line].dissolve;
        current_delay = sequence[current_line].ticks - next_dissolve_ticks;
//	    print_text("show_leds 1\n");
        led_state = dissolve1;
    }
}



void main()
{
	uint8_t i;

#ifdef ENABLE_SERIAL
    SPBRG = BAUD_CODE;
// 16f87.pdf page 113
    TXSTA = 0b00100100;
// 16f87.pdf page 114 receive disabled
    RCSTA = 0b10000000;
// must disable both directions to use C7
//    RCSTA = 0b00000000;
#endif


// digital mode page 130
    ADCON1 = 0b01000110;


// PWM/tick timer 16:1 prescale page 63
    T2CON = 0b00000111;
    PR2 = 0xff;
    PIR1bits.TMR2IF = 0;

#ifdef ENABLE_SERIAL

	print_text("\n\n\n\nWelcome to LED animation\n");
    flush_uart();
    uint8_t i;
    for(i = 0; i < SEQUENCE_SIZE; i++)
    {
        print_bin(sequence[i].porta);
        print_text(" ");
        print_bin(sequence[i].portc);
        print_text(" ");
        print_bin(sequence[i].portd);
        print_text("\n");
        flush_uart();
    }
#endif


// turn on LEDs
    TRISAbits.TRISA3 = 0;
    TRISAbits.TRISA2 = 0;
    TRISAbits.TRISA1 = 0;
    TRISAbits.TRISA0 = 0;

    TRISDbits.TRISD7 = 0;
    TRISDbits.TRISD6 = 0;
    TRISDbits.TRISD5 = 0;
    TRISDbits.TRISD4 = 0;
    TRISCbits.TRISC7 = 0;

    TRISCbits.TRISC5 = 0;
    TRISCbits.TRISC4 = 0;
    TRISDbits.TRISD3 = 0;
    TRISDbits.TRISD2 = 0;
    TRISDbits.TRISD1 = 0;
    TRISDbits.TRISD0 = 0;
    TRISCbits.TRISC3 = 0;


// write starting LEDs
    write_leds(0);

    while(1)
    {
        if(PIR1bits.TMR2IF)
        {
            PIR1bits.TMR2IF = 0;
            tick++;
            led_state();

            asm("clrwdt");
        }
    }

}




