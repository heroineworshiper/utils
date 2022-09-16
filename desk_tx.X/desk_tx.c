/*
 * IR Transmitter for remote controlled desk.
 *
 * Copyright (C) 2022 Adam Williams <broadcast at earthling dot net>
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

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection Bits (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = ON        // Watchdog Timer Enable (WDT enabled)
#pragma config PWRTE = ON      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = ON        // Internal/External Switchover Mode (Internal/External Switchover Mode is enabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LPBOR = OFF      // Low-Power Brown Out Reset (Low-Power BOR is disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>


#include <pic16f1508.h>
#include <stdint.h>
#include <string.h>
#include "desk_tx.h"



#define LED_LAT LATC3
#define LED_TRIS TRISC3
#define PWM_TRIS TRISC1


#define CODE_SIZE (KEYSIZE + 4)
static uint8_t code[CODE_SIZE];

#define UART_BUFSIZE 64
uint8_t uart_buffer[UART_BUFSIZE];
uint8_t uart_size = 0;
uint8_t uart_position1 = 0;
uint8_t uart_position2 = 0;



// send a UART char
void handle_uart()
{
// must use TRMT instead of TXIF on the 16F1508
    if(uart_size > 0 && TRMT)
    {
        TXREG = uart_buffer[uart_position2++];
		uart_size--;
		if(uart_position2 >= UART_BUFSIZE)
		{
			uart_position2 = 0;
		}
    }
}


void send_uart(uint8_t c)
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

static uint16_t number;
static int force;
void print_digit(uint16_t place)
{
	if(number >= place || force)
	{ 
		force = 1; 
		send_uart('0' + number / place); 
		number %= place; 
	}
}

void print_number(uint16_t number_arg)
{
	number = number_arg;
	force = 0;
	print_digit(10000000);
	print_digit(1000000);
	print_digit(100000);
	print_digit(10000);
	print_digit(1000);
	print_digit(100);
	print_digit(10);
	send_uart('0' + (number % 10)); 
	send_uart(' ');
}

void print_text(const uint8_t *s)
{
	while(*s != 0)
	{
		send_uart(*s);
		s++;
	}
}

void send_bit(uint8_t c)
{
    uint8_t i;
// RS232
// IR uses inverse logic so LED is off if bit is 1
//     if(c) 
//         PWM_TRIS = 1;
//     else
//         PWM_TRIS = 0;
// // 1000 baud
//     for(i = 0; i < 45; i++)
//     {
//         asm("nop");
//     }

    uint8_t delay = 15;
    if(!c)
        delay = 5;
// pulse
    PWM_TRIS = 0;
    for(i = 0; i < delay; i++)
    {
        asm("nop");
    }
    PWM_TRIS = 1;

// inactive period between pulses
    for(i = 0; i < 5; i++)
    {
        asm("nop");
    }
}

void send_ir(uint8_t c)
{
    uint8_t i;
// start bit
//    send_bit(0);
    for(i = 0; i < 8; i++)
    {
        send_bit((c & 1));
        c >>= 1;
    }
// stop bit
//    send_bit(1);
}

void main()
{
// 250khz internal clock
//    OSCCON = 0b00110000;
// 2 Mhz internal clock
    OSCCON = 0b01100000;
// serial port
    TXSTA = 0b00100100;
// disable receive
    RCSTA = 0b10000000;
    BAUDCON = 0b00001000;
// 100kbaud at 2Mhz
    SPBRG = 4;
// digital mode
    ANSELA = 0;
    ANSELB = 0;
// analog hall effect
    ANSELC = 0x00000100;
// enable pullups
    nWPUEN = 0;
    WPUA = 0xff;
    WPUB = 0xff;

// enable PWM4/C1
    T2CON = 0b00000100;
    PR2 = 12;
    PWM4DCH = 6;
    PWM4CON = 0b11000000;
// disable IR
    PWM_TRIS = 1;
// DEBUG enable IR
//    PWM_TRIS = 0;


// ADC
// AN6 input
    ADCON0 = 0b00011001;
    ADCON1 = 0b11100000;
    GO = 1;


    LED_TRIS = 0;
    LED_LAT = 1;

    print_text("Welcome to desk TX\n");

    uint8_t offset = 0;
    uint32_t analog_accum = 0;
    uint16_t analog_total = 0;
#define ANALOG_TOTAL 32
    while(1)
    {
        asm("clrwdt");
        handle_uart();

        if(ADIF)
        {
            uint16_t adc = ADRESH << 8;
            adc |= ADRESL;
            ADIF = 0;
            GO = 1;
            analog_accum += adc;
            analog_total++;
            if(analog_total >= ANALOG_TOTAL)
            {
// scale analog to 8 bits
                analog_accum /= ANALOG_TOTAL;
                analog_accum /= 4;

                print_number(analog_accum);
                print_number(RA0); // SET
                print_number(RA1); // PRESET 2
                print_number(RA2); // UP
                print_number(RA4); // PRESET 1
                print_number(RA5); // STOP
                print_number(RB4); // PRESET 4
                print_number(RB5); // DOWN
                print_number(RB6); // PRESET 3
                print_text("\n");

                uint8_t selection = 0;
                if(analog_accum >= 225)
                {
                    selection = 1;
                }
                else
                if(analog_accum >= 160)
                {
                    selection = 0;
                }
                else
                if(analog_accum > 80)
                {
                    selection = 2;
                }
                else
                {
                    selection = 3;
                }

// Convert only 1 button to a code
                uint8_t button = 0;
                if(!RA0) button = 0x1;
                else
                if(!RA1) button = 0x2;
                else
                if(!RA2) button = 0x4;
                else
                if(!RA4) button = 0x8;
                else
                if(!RA5) button = 0x10;
                else
                if(!RB4) button = 0x20;
                else
                if(!RB5) button = 0x40;
                else
                if(!RB6) button = 0x80;

// DEBUG
//selection = 0;
                uint8_t *key = keys + selection * KEYSIZE;
// compute new code
                uint8_t i;
                for(i = 0; i < KEYSIZE; i++)
                    code[i] = key[i];
// XOR the button with the 1st 4 bytes of the key
                for(i = 0; i < 4; i++)
                {
                    code[KEYSIZE + i] = button ^ key[i];
                }



// send it
                for(i = 0; i < CODE_SIZE; i++)
                {
                    send_ir(code[i]);
                }

                analog_accum = 0;
                analog_total = 0;
            }
        }
    }
}





