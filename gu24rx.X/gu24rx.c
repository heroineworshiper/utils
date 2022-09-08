/*
 * 433Mhz Receiver for remote controlled GU24 bulb or anything
 * requiring a receiver.
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

#define HZ 30

uint8_t serial_in;
uint8_t have_serial = 0;
uint8_t offset = 0;
uint8_t got_code = 0;
uint8_t pwm_buffer[4];
#define PWM_OFFSET (sizeof(key) - 4)
uint8_t radio_tick = 0;
// shorter increases responsiveness but also the chance of a dropped packet misfiring it
#define RADIO_TIMEOUT (HZ)
#define PERIOD 0x80
// 0 - (PERIOD + 1)
uint8_t stored_pwm = PERIOD + 1;
uint8_t received_pwm = PERIOD + 1;
uint8_t flash_tick = 0;
#define FLASH_TIMEOUT (HZ * 1)

#define FAN_LAT LATC2
#define FAN_TRIS TRISC2

void send_uart(uint8_t c)
{
// must use TRMT instead of TXIF
    while(!TRMT)
    {
    }
    TXREG = c;
//    delay();
}


void print_number(uint16_t number)
{
	if(number >= 10000) send_uart('0' + (number / 10000));
	if(number >= 1000) send_uart('0' + ((number / 1000) % 10));
	if(number >= 100) send_uart('0' + ((number / 100) % 10));
	if(number >= 10) send_uart('0' + ((number / 10) % 10));
	send_uart('0' + (number % 10));
}

void main()
{
// 2 Mhz internal clock
    OSCCON = 0b01100000;
// serial port
    TXSTA = 0b00100100;
    RCSTA = 0b10010000;
    BAUDCON = 0b00001000;
// 100kbaud at 2Mhz
    SPBRG = 4;
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

// enable PWM
    TRISC1 = 0;
    PWM4CON = 0b11000000;
    PWM4DCH = stored_pwm;
    PWM4DCL = 0;
    PR2 = PERIOD;
    T2CON = 0b00000101;

    FAN_TRIS = 0;
    FAN_LAT = 1;

// enable all interrupts
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

// start transmitting
    init_cc1101();
    cc1101_receiver();


    while(1)
    {
        asm("clrwdt");
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


//         if(have_serial)
//         {
//             have_serial = 0;
//             send_uart(serial_in);
//         }

        if(got_code)
        {
            got_code = 0;

            print_number(pwm_buffer[0]);
            send_uart('\n');
        }

        if(flash_tick >= FLASH_TIMEOUT &&
            stored_pwm != received_pwm)
        {
            stored_pwm = received_pwm;
// TODO
            send_uart('F');
            send_uart('L');
            send_uart('A');
            send_uart('S');
            send_uart('H');
            send_uart('E');
            send_uart('D');
            send_uart('\n');
        }
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
            if(flash_tick < FLASH_TIMEOUT)
                flash_tick++;
            interrupt_done = 0;

//            TRISC1 = 0;
//            LATC1 = !LATC1;
        }

// the radio
        if(PIR1bits.RCIF)
        {
            PIR1bits.RCIF = 0;
            serial_in = RCREG;
            have_serial = 1;
            interrupt_done = 0;


            if(offset < PWM_OFFSET)
            {
                if(key[offset] == serial_in)
                    offset++;
                else
                    offset = 0;
            }
            else
            {
                pwm_buffer[offset - PWM_OFFSET] = serial_in ^ key[offset];
                offset++;
                if(offset >= sizeof(key))
                {
                    uint8_t i;
                    for(i = 1; i < 4; i++)
                    {
                        if(pwm_buffer[i] != pwm_buffer[0])
                            break;
                    }

                    if(i >= 4)
                    {
                        got_code = 1;

// update the PWM duty cycle
                        received_pwm = pwm_buffer[0];
                        PWM4DCH = received_pwm;
                        if(received_pwm != stored_pwm)
                            flash_tick = 0;

// toggle the output if radio has been silent for a certain time
                        if(radio_tick >= RADIO_TIMEOUT)
                        {

                            if(PWM4EN)
                            {
// disable it
                                PWM4EN = 0;
                                LATC1 = 0;
                                FAN_LAT = 0;
                            }
                            else
                            {
// enable it
                                PWM4EN = 1;
                                FAN_LAT = 1;
                            }
                        }


                        radio_tick = 0;
                    }
                    offset = 0;
                }
            }
        }
    }
}























