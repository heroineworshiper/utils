/*
 * 433Mhz Transmitter for remote controlled GU24 bulb or anything
 * requiring a transmitter.
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
#include "cc1101.h"
#include "gu24tx.h"

#define PWM 255
uint8_t serial_in;

#define HZ 30
#define LED_LAT LATB6
#define LED_TRIS TRISB6

uint8_t radio_tick = 0;
// transmit for this much time
// shorter decreases the chance of a dropped packet misfiring it
// but increases the chance of it never being received
#define RADIO_TIMEOUT (HZ)

static void delay()
{
    uint16_t i;
// 1 ms delay at 250khz clock
//    for(i = 0; i < 3; i++)
// 1 ms delay at 4Mhz clock
    for(i = 0; i < 70; i++)
    {
        asm("nop");
    }
}

void send_uart(uint8_t c)
{
    TXREG = c;
// must use TRMT instead of TXIF
    while(!TRMT)
    {
    }
//    delay();
}

void main()
{
// 250khz internal clock
//    OSCCON = 0b00110000;
// 2 Mhz internal clock
    OSCCON = 0b01100000;
// serial port
    TXSTA = 0b00100100;
    RCSTA = 0b10010000;
    BAUDCON = 0b00001000;
// 62.5 kbaud at 250Khz
//    SPBRG = 0;
// 100kbaud at 2Mhz
    SPBRG = 4;
// 57600baud at 4Mhz
//    SPBRGL = 17;
    PIR1bits.RCIF = 0;
    PIE1bits.RCIE = 1;
// digital mode
    ANSELA = 0;
    ANSELB = 0;
    ANSELC = 0;
// disable pullups
    WPUA = 0;

// tick timer
    T1CON = 0b01000001;
// enable tick interrupt
    TMR1IE = 1;
    
    LED_TRIS = 0;
    LED_LAT = 1;

// enable all interrupts
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

// start transmitting
    init_cc1101();
    cc1101_transmitter();

    uint8_t offset = 0;
    while(1)
    {
        asm("clrwdt");

        if(radio_tick < RADIO_TIMEOUT)
        {
            if(offset < sizeof(key) - 4)
            {
                send_uart(key[offset++]);
            }
            else
            if(offset < sizeof(key))
            {
                send_uart(PWM ^ key[offset++]);
            }

            if(offset >= sizeof(key))
                offset = 0;
        }
        else
        {
            LED_LAT = 0;
        }



//         send_uart('H');
//         send_uart('E');
//         send_uart('L');
//         send_uart('L');
//         send_uart('O');
//         send_uart(' ');
//         send_uart('W');
//         send_uart('O');
//         send_uart('R');
//         send_uart('L');
//         send_uart('D');
//         send_uart('\n');

    }
}

void interrupt isr()
{
    uint8_t interrupt_done = 0;
    while(!interrupt_done)
    {
        interrupt_done = 1;

// the tick
        if(TMR1IF)
        {
            TMR1IF = 0;
            if(radio_tick < RADIO_TIMEOUT)
                radio_tick++;
            interrupt_done = 0;

//            TRISC1 = 0;
//            LATC1 = !LATC1;
        }

        if(PIR1bits.RCIF)
        {
            PIR1bits.RCIF = 0;
            serial_in = RCREG;
            interrupt_done = 0;
        }
    }
}







