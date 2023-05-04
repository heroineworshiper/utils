/*
 * RF Receiver for remote controlled desk.
 *
 * Copyright (C) 2022-2023 Adam Williams <broadcast at earthling dot net>
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

// the desk code
#define SELF 2


// button outputs
#define DOWN_LAT LATC5
#define UP_LAT LATC4
#define RECALL_LAT LATC3
#define MODE_LAT LATC6

#define DOWN_TRIS TRISC5
#define UP_TRIS TRISC4
#define RECALL_TRIS TRISC3
#define MODE_TRIS TRISC6


#define LED_LAT LATC0
#define LED_TRIS TRISC0
// ticks per second
#define HZ 977
#define RF_PORT RA2

typedef union 
{
	struct
	{
		unsigned interrupt_complete : 1;
		unsigned have_serial : 1;
        unsigned update_button : 1;
// desired LED value from button state.  Overridden by led_state
        unsigned want_led : 1;
	};
	
	unsigned char value;
} flags_t;

flags_t flags;

uint8_t serial_in;
// LED blinker
uint16_t blink_tick = 0;
// time since last byte
uint8_t serial_tick = 0;
// incremented value
uint8_t serial_tick2 = 0;

// time between status packets
#define SERIAL_THRESHOLD 10



// button codes
#define PRESET1 0x08
#define PRESET2 0x02
#define PRESET3 0x80
#define PRESET4 0x20
#define ABORT 0x10
#define SET 0x01
#define UP 0x06
#define DOWN 0x40
// #define PRESET1 7
// #define PRESET2 1
// #define PRESET3 3
// #define PRESET4 8
// #define ABORT 4
// #define SET 0
// #define UP 2
// #define DOWN 6
#define NO_BUTTON 0xff
// current button pressed
uint8_t button = NO_BUTTON;
#define TOTAL_BUTTONS 8


uint16_t code;
uint8_t code_counts[TOTAL_BUTTONS];
// time since last code count
uint8_t code_tick = 0;
// ticks to accumulate code counts
#define CODE_TICKS (HZ / 5)
// code count required for a button press
#define CODE_THRESHOLD 3

// ticks before releasing all buttons.  Not used
#define BUTTON_TIMEOUT 200

// buffer for the status code
#define STATUS_SIZE 4
uint8_t status_code[STATUS_SIZE];
uint8_t status_size = 0;

// preset bug workaround
#define PRESET_IDLE 0
// wait for confirmation of a saved preset
#define WAIT_SAVED_PRESET 1
// wait for the boundary height
#define WAIT_MOVE 2
// wait for the stop
#define WAIT_TOP 3
#define WAIT_BOTTOM 4
// release the button
#define HEIGHT_WAIT 5

// wait for maximum or minimum height preset
uint8_t preset_state = PRESET_IDLE;
uint16_t preset_tick = 0;
// timeout for all height states
#define PRESET_TIMEOUT (2 * HZ)
// delay before pressing up button
#define HEIGHT_WAIT_TIME (1 * HZ)
// maximum height reported when desk preset stops
#define MAX_HEIGHT 0x01e8
// minimum height reported when desk preset stops
#define MIN_HEIGHT 0x00e2

#define LED_IDLE 0
#define LED_WAITING 1
#define LED_LOCKED 2
uint8_t led_state = LED_IDLE;
// time since last LED state update
uint16_t led_tick = 0;
// LED to IDLE after this much time without a status packet
#define LED_TIMEOUT (HZ / 4)


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

void print_hex2(uint8_t v)
{
    const uint8_t hex_table[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f'  
    };
    
    send_uart(hex_table[v >> 4]);
    send_uart(hex_table[v & 0xf]);
//    send_uart(' ');
}

void print_hex4(uint16_t v)
{
    const uint8_t hex_table[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f'  
    };
    
    send_uart(hex_table[(v >> 12) & 0xf]);
    send_uart(hex_table[(v >> 8) & 0xf]);
    send_uart(hex_table[(v >> 4) & 0xf]);
    send_uart(hex_table[v & 0xf]);
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

void reset_buttons()
{
    DOWN_TRIS = 1;
    UP_TRIS = 1;
    RECALL_TRIS = 1;
    MODE_TRIS = 1;
    flags.want_led = 0;
}


void preset_bug()
{
    if(preset_tick < PRESET_TIMEOUT)
    {
        switch(preset_state)
        {
            case WAIT_SAVED_PRESET:
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x06 && 
                    status_code[2] == 0x01 && 
                    status_code[3] == 0x00)
                {
// preset was a set command.  Ignore it
                    print_text("SET PRESET\n");
                    preset_state = PRESET_IDLE;
                }
// moving to the preset
                else
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x01)
                {
                    print_text("MOVING TO PRESET\n");
                    preset_state = WAIT_MOVE;
                    preset_tick = 0;
                }
                break;

            case WAIT_MOVE:
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x01)
                {
                    uint16_t height = (status_code[2] << 8) |
                        status_code[3];
// reached top
                    if(height >= MAX_HEIGHT)
                    {
                        print_text("TOP REACHED\n");
                        preset_state = WAIT_TOP;
                    }
                    else
// reached bottom
                    if(height <= MIN_HEIGHT)
                    {
                        print_text("BOTTOM REACHED\n");
                        preset_state = WAIT_BOTTOM;
                    }

                    preset_tick = 0;
                }
                else
                {
// unknown packet.  Abort
                    preset_state = PRESET_IDLE;
                }
                break;

// wait after reaching preset height
            case WAIT_TOP:
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x01)
                {
                    uint16_t height = (status_code[2] << 8) |
                        status_code[3];
                    if(height < MAX_HEIGHT)
                    {
                        print_text("LEFT TOP\n");
                        preset_state = WAIT_MOVE;
                        preset_tick = 0;
                    }
                    else
                    if(preset_tick >= HEIGHT_WAIT_TIME)
                    {
                        print_text("GOING UP AGAIN\n");
                        preset_state = HEIGHT_WAIT;
                        preset_tick = 0;
// press the up button
                        UP_TRIS = 0;
                    }
                }
                else
                {
// unknown packet.  Abort
                    preset_state = PRESET_IDLE;
                }
                break;

// wait after reaching preset height
            case WAIT_BOTTOM:
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x01)
                {
                    uint16_t height = (status_code[2] << 8) |
                        status_code[3];
                    if(height > MIN_HEIGHT)
                    {
                        print_text("LEFT BOTTOM\n");
                        preset_state = WAIT_MOVE;
                        preset_tick = 0;
                    }
                    else
                    if(preset_tick >= HEIGHT_WAIT_TIME)
                    {
                        print_text("GOING DOWN AGAIN\n");
                        preset_state = HEIGHT_WAIT;
                        preset_tick = 0;
    // press the down button
                        DOWN_TRIS = 0;
                    }
                }
                else
                {
// unknown packet.  Abort
                    preset_state = PRESET_IDLE;
                }
                break;

// wait after pressing button
            case HEIGHT_WAIT:
                if(preset_tick >= HEIGHT_WAIT_TIME)
                {
                    print_text("PRESET WORKAROUND DONE\n");
                    preset_state = PRESET_IDLE;
// release the buttons & quit
                    UP_TRIS = 1;
                    DOWN_TRIS = 1;
                }
                break;
        }
    }
    else
    {
// timed out
        preset_state = PRESET_IDLE;
    }
}


void main()
{
// 16 Mhz internal clock to handle the UART, IR reception, LED
    OSCCON = 0b01111000;

    LED_TRIS = 0;
    LED_LAT = 0;
    flags.want_led = 0;

    DOWN_TRIS = 1;
    UP_TRIS = 1;
    RECALL_TRIS = 1;
    MODE_TRIS = 1;
    DOWN_LAT = 0;
    UP_LAT = 0;
    RECALL_LAT = 0;
    MODE_LAT = 0;
    

// serial port
    TXSTA = 0b00100100;
// enable receive
    RCSTA = 0b10010000;
    BAUDCON = 0b00001000;
// 9600 baud at 16Mhz
#define BAUD_CODE 416
// 19200 baud for debugging
//#define BAUD_CODE 208
    SPBRGH = BAUD_CODE >> 8;
    SPBRGL = BAUD_CODE & 0xff;
    RCIE = 1;

// tick timer
    OPTION_REG = 0b10000011;
    TMR0IE = 1;

// digital mode
    ANSELA = 0;
    ANSELB = 0;
    ANSELC = 0;


// RF interrupt
    IOCIE = 1;
// detect both edges of RF pin
    IOCAP2 = 1;
    IOCAN2 = 1;
// RF timer
    T1CON = 0b00000001;
    TMR1 = -BIT_PERIOD;
    TMR1IF = 0;
    TMR1IE = 1;

    flags.value = 0;

// enable interrupts
    GIE = 1;
    PEIE = 1;

    print_text("\n\n\n\nWelcome to desk RX\n");

    while(1)
    {
        asm("clrwdt");
        handle_uart();

        if(flags.have_serial)
        {
            flags.have_serial = 0;
// start of a new packet
            if(serial_tick >= SERIAL_THRESHOLD)
                status_size = 0;

            if(status_size < STATUS_SIZE)
                status_code[status_size++] = serial_in;

// handle a new packet
            if(status_size >= STATUS_SIZE)
            {
// waiting for a preset to set
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x06 && 
                    status_code[2] == 0x00 && 
                    status_code[3] == 0x00)
                {
                    led_state = LED_WAITING;
                    led_tick = 0;
                    print_text("WAITING\n");
                }
                else
// panel locked
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x02 && 
                    status_code[2] == 0x00 && 
                    status_code[3] == 0x80)
                {
                    led_state = LED_LOCKED;
                    led_tick = 0;
                    print_text("LOCKED\n");
                }


// preset bug workaround
                preset_bug();
            }
        }

// turn off the LED if no button is being pressed
// Inactive LED state has the lowest priority
        if(led_tick == LED_TIMEOUT)
        {
            led_tick++;
            led_state = LED_IDLE;
// end the flashing
            flags.want_led = 0;
        }

// IR code overrides inactive LED
        if(flags.update_button)
        {
            flags.update_button = 0;

// abort height workaround on each button press
            preset_state = PRESET_IDLE;

// print_text("BUTTON ");
// print_hex2(button);
// print_text("\n");
            switch(button)
            {
                case UP:
                    DOWN_TRIS = 1;
                    UP_TRIS = 0;
                    RECALL_TRIS = 1;
                    MODE_TRIS = 1;
                    flags.want_led = 1;
                    break;
                case DOWN:
                    DOWN_TRIS = 0;
                    UP_TRIS = 1;
                    RECALL_TRIS = 1;
                    MODE_TRIS = 1;
                    flags.want_led = 1;
                    break;
                case SET:
                    DOWN_TRIS = 1;
                    UP_TRIS = 1;
                    RECALL_TRIS = 1;
                    MODE_TRIS = 0;
                    flags.want_led = 1;
                    break;
                case ABORT:
                    DOWN_TRIS = 1;
                    UP_TRIS = 1;
                    RECALL_TRIS = 1;
                    MODE_TRIS = 0;
                    flags.want_led = 1;
                    break;
// Min height preset
                case PRESET1:
                    DOWN_TRIS = 1;
                    UP_TRIS = 1;
                    RECALL_TRIS = 0;
                    MODE_TRIS = 1;
                    flags.want_led = 1;
// trigger the preset workaround
                    preset_state = WAIT_SAVED_PRESET;
                    preset_tick = 0;
                    break;

                case PRESET2:
                    DOWN_TRIS = 0;
                    UP_TRIS = 1;
                    RECALL_TRIS = 0;
                    MODE_TRIS = 1;
                    flags.want_led = 1;
// trigger the preset workaround
                    preset_state = WAIT_SAVED_PRESET;
                    preset_tick = 0;
                    break;

                case PRESET3:
                    DOWN_TRIS = 1;
                    UP_TRIS = 0;
                    RECALL_TRIS = 0;
                    MODE_TRIS = 1;
                    flags.want_led = 1;
// trigger the preset workaround
                    preset_state = WAIT_SAVED_PRESET;
                    preset_tick = 0;
                    break;
// Max height preset
                case PRESET4:
                    DOWN_TRIS = 0;
                    UP_TRIS = 0;
                    RECALL_TRIS = 1;
                    MODE_TRIS = 1;
                    flags.want_led = 1;
// trigger the preset workaround
                    preset_state = WAIT_SAVED_PRESET;
                    preset_tick = 0;
                    break;

                default:
// reset all buttons
                    reset_buttons();
                    break;
            }
        }


// Active LED state has LED's highest priority
        switch(led_state)
        {
            case LED_WAITING:
                flags.want_led = (blink_tick & 0x1ff) < 0x100;
                break;
            
            case LED_LOCKED:
                flags.want_led = (blink_tick & 0x7ff) < 0x400;
                break;
        }

// update the LED
        LED_LAT = flags.want_led;
    }
}


void capture_bit()
{
// shift next bit in
    code <<= 1;
    code |= RF_PORT;

// fill the code counts
    if(code == BUTTON_CODE(SELF, PRESET1)) 
    {
        code_counts[0]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, PRESET2))
    {
        code_counts[1]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, PRESET3))
    {
        code_counts[2]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, PRESET4))
    {
        code_counts[3]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, ABORT))
    {
        code_counts[4]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, SET))
    {
        code_counts[5]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, UP)){
        code_counts[6]++;
        code = 0;
    }
    else
    if(code == BUTTON_CODE(SELF, DOWN))
    {
        code_counts[7]++;
        code = 0;
    }

// if(code == KEY)
// {
//     print_text("*");
// LED_LAT = 1;
// }
// else
// {
// //    print_text("-");
// LED_LAT = 0;
// }

}


void interrupt isr()
{
    flags.interrupt_complete = 0;
	while(!flags.interrupt_complete)
	{
		flags.interrupt_complete = 1;

        if(TMR0IF)
        {
            flags.interrupt_complete = 0;
            TMR0IF = 0;
            blink_tick++;
            code_tick++;
            if(code_tick >= CODE_TICKS)
            {
                code_tick = 0;
                flags.update_button = 1;
                if(code_counts[0] >= CODE_THRESHOLD) button = PRESET1;
                else
                if(code_counts[1] >= CODE_THRESHOLD) button = PRESET2;
                else
                if(code_counts[2] >= CODE_THRESHOLD) button = PRESET3;
                else
                if(code_counts[3] >= CODE_THRESHOLD) button = PRESET4;
                else
                if(code_counts[4] >= CODE_THRESHOLD) button = ABORT;
                else
                if(code_counts[5] >= CODE_THRESHOLD) button = SET;
                else
                if(code_counts[6] >= CODE_THRESHOLD) button = UP;
                else
                if(code_counts[7] >= CODE_THRESHOLD) button = DOWN;
                else
                    button = NO_BUTTON;

// DEBUG print code counts
print_number(code_counts[0]);
print_number(code_counts[1]);
print_number(code_counts[2]);
print_number(code_counts[3]);
print_number(code_counts[4]);
print_number(code_counts[5]);
print_number(code_counts[6]);
print_number(code_counts[7]);
print_hex4(code);
print_text("\n");
                code_counts[0] = 0;
                code_counts[1] = 0;
                code_counts[2] = 0;
                code_counts[3] = 0;
                code_counts[4] = 0;
                code_counts[5] = 0;
                code_counts[6] = 0;
                code_counts[7] = 0;
            }

            if(led_tick < LED_TIMEOUT)
                led_tick++;

            if(serial_tick2 < 0xff)
                serial_tick2++;

            if(preset_tick < PRESET_TIMEOUT)
                preset_tick++;

// DEBUG test tick frequency
//            LED_LAT = !LED_LAT;
        }
        
        if(RCIF)
        {
            flags.interrupt_complete = 0;
            serial_in = RCREG;
            RCIF = 0;
            serial_tick = serial_tick2;
            serial_tick2 = 0;
            flags.have_serial = 1;
        }


// RF pin changed
        if(IOCAF2)
        {
            flags.interrupt_complete = 0;
// reset the alarm to land in the middle of the next bit
// Rising edge lags.  Falling edge leads.
            if(RF_PORT)
                TMR1 = -BIT_PERIOD;
            else
                TMR1 = -BIT_PERIOD * 3 / 2;
            TMR1IF = 0;
            IOCAF2 = 0;

            capture_bit();
        }


// alarm expired
        if(TMR1IF)
        {
            flags.interrupt_complete = 0;
            TMR1 = -BIT_PERIOD;
            TMR1IF = 0;

            capture_bit();

        }
    }
}







