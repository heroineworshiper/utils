/*
 * F-35 NOZZLE CONTROLLER
 * Copyright (C) 2020-2021 Adam Williams <broadcast at earthling dot net>
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

// B0 - IR input
// B4 - power button
// AN0-AN5 - hall sensors
// D7 - LED
// D0-D5 - H bridges

// PORTC is dead on the recycled chip


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
#include "nozzle.h"

// control motors using console input.  Disable if the serial port is floating.
//#define TEST_MODE
// less accurate motor positioning but more debugging output
//#define VERBOSE

// use XY control instead of polar
//#define XY_CONTROL

#define ABS(x) ((x) < 0 ? (-(x)) : (x))

//#define CLOCKSPEED 13300000
#define CLOCKSPEED 22579000
#define LED_TRIS TRISDbits.TRISD7
#define LED_PORT PORTDbits.RD7

#define HBRIDGE_MASK (uint8_t)0b00111111
#define HBRIDGE_PORT PORTD
#define HBRIDGE_TRIS TRISD
#define MOTOR2_RIGHT (uint8_t)0b00000010
#define MOTOR2_LEFT  (uint8_t)0b00000001
#define MOTOR1_RIGHT (uint8_t)0b00010000
#define MOTOR1_LEFT  (uint8_t)0b00100000
#define MOTOR0_RIGHT (uint8_t)0b00001000
#define MOTOR0_LEFT  (uint8_t)0b00000100

// coast instead of brake
//#define COAST

#define BUTTON_PORT PORTBbits.RB4

uint8_t have_serial = 0;
uint8_t serial_in = 0;
// page 115
#define BAUD_CODE (CLOCKSPEED / 16 / 57600)
uint8_t interrupt_done = 0;

#define UART_BUFSIZE 64
uint8_t uart_buffer[UART_BUFSIZE];
uint8_t uart_size = 0;
uint8_t uart_position1 = 0;
uint8_t uart_position2 = 0;

// motors are armed
uint8_t armed = 0;
uint8_t led_counter = 0;
// mane timer fires at 10Hz
#define HZ 100
#define TIMER1_VALUE (-CLOCKSPEED / 4 / 8 / HZ)
#define LED_DELAY (HZ / 2)
#define LED_DELAY2 (HZ / 4)

uint8_t got_tick = 0;
uint8_t tick = 0;

// current state of the home operation & the motor tracking
uint8_t current_motor = 0;


// copy of H bridge pins written by the user
uint8_t motor_master = 0;
// ticks to wait for a motor to stop
#define MOTOR_DELAY (HZ / 2)



uint8_t current_adc = 0;
#define ADCON0_MASK 0b10000001
#define TOTAL_ADC 6
#define TOTAL_MOTORS 3
#define BOUNDARY0 1 // nearest motor
#define BOUNDARY1 2
#define BOUNDARY2 5 // farthest motor
#define ENCODER0 0 // nearest motor
#define ENCODER1 3
#define ENCODER2 4 // farthest motor
// amount above or below 0x80
#define SENSOR_THRESHOLD 0x20

typedef struct 
{
// analog value
    uint8_t analog;
// last detected direction
    uint8_t ns;
// add or subtract steps from position if an encoder
    int8_t step;
// absolute position if an encoder
    int8_t position;
} sensor_state_t;
sensor_state_t sensors[TOTAL_ADC];

typedef struct
{
// ADC of the sensors
    uint8_t boundary;
    uint8_t encoder;
// H bridge bitmasks of the 2 directions
    uint8_t dec_mask; // go towards boundary
    uint8_t inc_mask; // go away from boundary
    uint8_t total_unmask; // coasting
    uint8_t total_mask; // breaking

// ticks remaneing to sleep
    uint8_t timer;
    int8_t target_position;
// position range
    int8_t min;
    int8_t max;
// only move the motors if this is > 0
    uint8_t changed;
// use the brake instead of coasting.  Outlet is light enough to use braking.
    uint8_t brake;
} tracking_state_t;
tracking_state_t tracking_state[TOTAL_MOTORS];


// pitch of the nozzle in polar coordinates
int8_t nozzle_pitch = 0;
// user position of motor 0 to which the nozzle_pitch adds an offset to compensate
// for the nozzle bending sideways
int8_t nozzle_angle = 0;

#ifdef XY_CONTROL
// nozzle direction in XY coordinates
int16_t nozzle_x = 0;
int16_t nozzle_y = 0;

// neighboring nozzle positions in XY
#define NEIGHBORS 4
int16_t neighbor_x[NEIGHBORS];
int16_t neighbor_y[NEIGHBORS];
uint8_t good_neighbors[NEIGHBORS];
// relative angle/pitch of neighboring nozzle positions
const int8_t neighbor_pitch[] = {  0, 0, -1, 1 };
const int8_t neighbor_angle[] = { -1, 1,  0, 0 };
#endif // XY_CONTROL


typedef struct
{
    int8_t pitch;
    int8_t angle;
} preset_t;

// location in EEPROM of presets
#define DATA_EE_ADDR 0x00
#define PRESETS 4
#define PRESET_DELAY (HZ / 2)
uint8_t setting_preset = 0;
uint8_t current_preset = 0;
preset_t presets[PRESETS];
void (*preset_state)() = 0;
int8_t orig_nozzle_angle = 0;
uint8_t preset_delay = 0;




// IR code follows.  Improved version of heroineclock 2
typedef struct
{
    const uint8_t *data;
	const uint8_t size;
    const uint8_t value;
} ir_code_t;

// remote control codes
const uint8_t up_data[] = 
{
    0x30, 0x19, 0x03, 0x02, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0xdb, 0x30, 0x0b, 0x03 
};

const uint8_t down_data[] = 
{ 
    0x30, 0x19, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x02, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0xdb, 0x30, 0x0c, 0x03
};

const uint8_t left_data[] =  
{
    0x30, 0x19, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0xdb, 0x30, 0x0b, 0x03
};

const uint8_t right_data[] = 
{
    0x30, 0x19, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0xdb, 0x30, 0x0c, 0x03
};



// fast rewind button
const uint8_t fastrev_data[] =
{
    0x30, 0x19, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0xdb, 0x30, 0x0c, 0x03
};


const uint8_t fastfwd_data[] = 
{
    0x30, 0x19, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x02, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0xdb, 0x30, 0x0c, 0x03
};


const uint8_t button7_data[] = 
{
    0x30, 0x19, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x08, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0xdb, 0x30, 0x0c, 0x03
};

const uint8_t button8_data[] = 
{
    0x30, 0x19, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0xdb, 0x30, 0x0c, 0x03
};

const uint8_t button9_data[] = 
{
    0x30, 0x19, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0x03, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x02, 0x03, 0xdb, 0x30, 0x0c, 0x03
};

const uint8_t button0_data[] = 
{
    0x30, 0x19, 0x03, 0x03, 0x03, 0x09, 0x03, 0x02, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x02, 0x03, 0x02, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x03, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x09, 0x03, 0x03, 0x03, 0xdb, 0x30, 0x0c, 0x03
};


// remote control buttons to enums
#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3
#define POWER 4
#define SET_PRESET 5
#define PRESET1 6
#define PRESET2 7
#define PRESET3 8
#define PRESET4 9
#define BOGUS 10

const ir_code_t ir_codes[] = { 
    { button7_data, sizeof(button7_data), PRESET1 },
    { button8_data, sizeof(button8_data), PRESET2 },
    { button9_data, sizeof(button9_data), PRESET3 },
    { button0_data, sizeof(fastrev_data), PRESET4 },
    { up_data,    sizeof(up_data),      UP },
	{ down_data,  sizeof(down_data),  DOWN },
	{ left_data,  sizeof(left_data),  LEFT },
	{ right_data, sizeof(right_data), RIGHT },
    { fastfwd_data, sizeof(fastfwd_data), SET_PRESET },
    { fastrev_data, sizeof(fastrev_data), POWER },
};

#define IR_MARGIN 2
// At least 120ms as measured on the scope
#define IR_TIMEOUT 725
#define TOTAL_CODES (sizeof(ir_codes) / sizeof(ir_code_t))

// which codes have matched all bytes received so far
// 0 if matched all bytes  1 if failed
uint8_t ir_code_failed[TOTAL_CODES];
uint8_t ir_offset = 0;
// IR is transmitting repeats
uint8_t have_ir = 0;
// last IR code received
uint8_t ir_code = 0;
// delay before 1st repeat
uint8_t repeat_delay = 0;
#define REPEAT_DELAY (HZ / 2)
#define REPEAT_DELAY2 (HZ / 4)

// IR interrupt fired
volatile uint8_t got_ir_int = 0;
// time of last IR interrupt
volatile int16_t ir_time = 0;
// incremented for every TMR2 interrupt
uint16_t tmr2_high = 0;
// set in the interrupt handler when IR has timed out
uint8_t ir_timeout = 0;
uint8_t first_edge = 1;


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



#ifdef XY_CONTROL

// convert encoder positions to XY coordinates
void polar_to_xy(int16_t *x_out, int16_t *y_out, uint8_t pitch, uint8_t angle)
{
    int sin_value = sin_table[angle - ENCODER0_MIN];
    int cos_value = cos_table[angle - ENCODER0_MIN];
    *x_out = cos_value * pitch;
    *y_out = sin_value * pitch;
}

#endif // XY_CONTROL


// motor state machine

void motor_idle();
void motor_home1();
void (*motor_state)() = motor_idle;


void motor_tracking()
{
// only enough clockcycles to test 1 motor per iteration
    tracking_state_t *tracking = &tracking_state[current_motor];
    sensor_state_t *encoder = &sensors[tracking->encoder];

    current_motor++;
    if(current_motor >= TOTAL_MOTORS)
    {
        current_motor = 0;
    }

// motor moving
    if((motor_master & tracking->total_mask) == tracking->inc_mask)
    {
        if(encoder->position >= tracking->target_position)
        {
#ifdef COAST
            if(!tracking->brake)
            {
                motor_master &= tracking->total_unmask;
            }
            else
#endif
            {
                motor_master |= tracking->total_mask;
            }

            tracking->timer = MOTOR_DELAY;
        }
    }
    else
    if((motor_master & tracking->total_mask) == tracking->dec_mask)
    {
        if(encoder->position <= tracking->target_position)
        {
#ifdef COAST
            if(!tracking->brake)
            {
                motor_master &= tracking->total_unmask;
            }
            else
#endif
            {
                motor_master |= tracking->total_mask;
            }

            tracking->timer = MOTOR_DELAY;
        }
    }
    else
// waiting for next command
    if(/* tracking->timer == 0 && */ tracking->changed > 0)
    {
// print_text("changed=");
// print_number(tracking->changed);
// print_text("\n");
        if(encoder->position > tracking->target_position)
        {
            encoder->step = -1;
            motor_master &= tracking->total_unmask;
            motor_master |= tracking->dec_mask;
            tracking->changed--;
        }
        else
        if(encoder->position < tracking->target_position)
        {
            encoder->step = 1;
            motor_master &= tracking->total_unmask;
            motor_master |= tracking->inc_mask;
            tracking->changed--;
        }
        else
        {
// no difference in position.  Cancel the loop.
            tracking->changed--;
        }
    }
}

// wait a while
void motor_home6()
{
    tracking_state_t *tracking = &tracking_state[current_motor];
    if(tracking->timer == 0)
    {
        uint8_t current_encoder = tracking->encoder;

#ifdef VERBOSE
//         print_text("motor=");
//         print_number(sensors[current_encoder].position);
//         print_text("\n");
#endif

        switch(current_motor)
        {
            case 2:
                current_motor = 1;
                motor_state = motor_home1;
                break;
            case 1:
                current_motor = 0;
                motor_state = motor_home1;
                break;
            case 0:
            default:
                current_motor = 0;
                motor_state = motor_tracking;
                break;
        }
    }
}

// count until home position
void motor_home5()
{
    tracking_state_t *tracking = &tracking_state[current_motor];
    uint8_t current_encoder = tracking->encoder;
    if(sensors[current_encoder].position >= 
        tracking_state[current_motor].target_position)
    {
#ifdef COAST
        if(!tracking->brake)
        {
            motor_master &= tracking->total_unmask;
        }
        else
#endif
        {
            motor_master |= tracking->total_mask;
        }

        tracking->timer = MOTOR_DELAY;
        motor_state = motor_home6;
    }
}

// go until boundary is detected again
void motor_home4()
{
    tracking_state_t *tracking = &tracking_state[current_motor];
    uint8_t current_boundary = tracking->boundary;
    uint8_t sensor_value = sensors[current_boundary].analog;
    if(ABS(sensor_value - 0x80) >= SENSOR_THRESHOLD)
    {
        uint8_t current_encoder = tracking->encoder;
        sensors[current_encoder].position = 0;
        sensors[current_encoder].step = 1;
        motor_state = motor_home5;
    }
}

// wait a while
void motor_home3()
{
    tracking_state_t *tracking = &tracking_state[current_motor];
    if(tracking->timer == 0)
    {
        motor_master &= tracking->total_unmask;
        motor_master |= tracking->inc_mask;
        motor_state = motor_home4;
    }
}

// detect motor boundary & overshoot
void motor_home2()
{
    tracking_state_t *tracking = &tracking_state[current_motor];
    uint8_t current_boundary = tracking->boundary;
    uint8_t sensor_value = sensors[current_boundary].analog;
    if(ABS(sensor_value - 0x80) >= SENSOR_THRESHOLD)
    {
// always coast
        motor_master &= tracking->total_unmask;
        motor_state = motor_home3;
        tracking->timer = MOTOR_DELAY;
    }
}

// go to home position
void motor_home1()
{
    tracking_state_t *tracking = &tracking_state[current_motor];
    uint8_t current_boundary = tracking->boundary;
    uint8_t sensor_value = sensors[current_boundary].analog;
// sensor is already on boundary.  Abort.
    if(ABS(sensor_value - 0x80) >= SENSOR_THRESHOLD)
    {
        motor_state = motor_idle;
    }
    else
// command motor to move to boundary
    {
        motor_master &= tracking->total_unmask;
        motor_master |= tracking->dec_mask;
        motor_state = motor_home2;
    }
}

void motor_idle()
{
}

void arm_motors()
{
    armed = 1;
    LED_PORT = 1;
    current_motor = 2;

    tracking_state[2].target_position = HOME2;
    tracking_state[2].changed = 0;

    tracking_state[1].target_position = HOME1;
    tracking_state[1].changed = 0;

    tracking_state[0].target_position = HOME0 + step_to_encoders[0 * TOTAL_MOTORS + 0];
    tracking_state[0].changed = 0;

    nozzle_pitch = 0;
    nozzle_angle = HOME0;
    motor_state = motor_home1;
}

void disarm_motors()
{
    armed = 0;
    HBRIDGE_PORT &= ~HBRIDGE_MASK;
    motor_state = motor_idle;
}

// update the encoder positions based on polar positions
void update_motors()
{
#ifdef VERBOSE
    print_text("nozzle_pitch=");
    print_number(nozzle_pitch);
    print_text("nozzle_angle=");
    print_number(nozzle_angle);
    print_text("\n");
    flush_uart();
#endif

    if(nozzle_pitch > PITCH_STEPS)
    {
        nozzle_pitch = PITCH_STEPS;
    }
    if(nozzle_pitch < 0)
    {
        nozzle_pitch = 0;
    }

    if(nozzle_angle > ANGLE_STEPS)
    {
        nozzle_angle = ANGLE_STEPS;
    }
    if(nozzle_angle < 0)
    {
        nozzle_angle = 0;
    }


    tracking_state[0].target_position = nozzle_angle +
        step_to_encoders[nozzle_pitch * TOTAL_MOTORS + 0];
    tracking_state[1].target_position = 
        step_to_encoders[nozzle_pitch * TOTAL_MOTORS + 1];
    tracking_state[2].target_position = 
        step_to_encoders[nozzle_pitch * TOTAL_MOTORS + 2];

    if(tracking_state[0].target_position < tracking_state[0].min)
    {
        int8_t diff = tracking_state[0].min - tracking_state[0].target_position;
        tracking_state[0].target_position += diff;
// clamp radial position based on the nozzle step
        nozzle_angle += diff;
    }
    if(tracking_state[0].target_position > tracking_state[0].max)
    {
        int8_t diff = tracking_state[0].target_position - tracking_state[0].max;
        tracking_state[0].target_position -= diff;
// clamp radial position based on the nozzle step
        nozzle_angle -= diff;
    }


    if(tracking_state[0].changed < 0xff)
    {
        tracking_state[0].changed++;
    }
    if(tracking_state[1].changed < 0xff)
    {
        tracking_state[1].changed++;
    }
    if(tracking_state[2].changed < 0xff)
    {
        tracking_state[2].changed++;
    }

#ifdef VERBOSE
    print_number(nozzle_pitch);
    print_number(tracking_state[0].target_position);
    print_number(tracking_state[1].target_position);
    print_number(tracking_state[2].target_position);
    print_text("\n");
    flush_uart();
#endif

}

// return 1 if the motors are tracking
uint8_t motors_tracking()
{
    if(tracking_state[0].changed > 0 ||
        tracking_state[1].changed > 0 ||
        tracking_state[2].changed > 0)
    {
        return 1;
    }
    return 0;
}

void do_preset_pitch()
{
    if(!motors_tracking())
    {
// advance to pitch
        int8_t want_pitch = presets[current_preset].pitch;
        if(want_pitch > nozzle_pitch)
        {
            nozzle_pitch++;
            update_motors();
        }
        else
        if(want_pitch < nozzle_pitch)
        {
            nozzle_pitch--;
            update_motors();
        }
        else
        {
// done
            preset_state = 0;
        }
    }
}

void do_preset_delay2()
{
    if(preset_delay == 0)
    {
        preset_state = do_preset_pitch;
    }
}

void do_preset_angle()
{
    if(!motors_tracking())
    {
// advance to angle
        int8_t want_angle = presets[current_preset].angle;
        if(want_angle > nozzle_angle)
        {
            nozzle_angle++;
            update_motors();
        }
        else
        if(want_angle < nozzle_angle)
        {
            nozzle_angle--;
            update_motors();
        }
        else
        {
// go to the pitch
            preset_delay = PRESET_DELAY;
            preset_state = do_preset_delay2;
        }
    }
}

void do_preset_delay1()
{
    if(preset_delay == 0)
    {
        preset_state = do_preset_angle;
    }
}

void do_preset_center()
{
    if(!motors_tracking())
    {
// center it
        if(nozzle_pitch > 0)
        {
            nozzle_pitch--;
            update_motors();
        }
        else
        {
            preset_delay = PRESET_DELAY;
            preset_state = do_preset_delay1;
        }
    }
}

void do_preset_save2()
{
    if(preset_delay == 0)
    {
        nozzle_angle = orig_nozzle_angle;
        update_motors();
        preset_state = 0;
    }
}

void do_preset_save1()
{
    if(!motors_tracking())
    {
        preset_delay = PRESET_DELAY;
        preset_state = do_preset_save2;
    }
}

void read_presets()
{
    uint8_t i;
    uint8_t *ptr = (uint8_t*)presets;
    for(i = 0; i < sizeof(presets); i++)
    {
        EEADR = DATA_EE_ADDR + i;
        EECON1bits.EEPGD = 0;
        EECON1bits.RD = 1;
        *ptr++ = EEDATA;
    }
}

void write_presets()
{
    uint8_t i;
    uint8_t *ptr = (uint8_t*)presets;
// disable interrupts
    INTCONbits.GIE = 0;
    for(i = 0; i < sizeof(presets); i++)
    {
        PIR2bits.EEIF = 0;
        EEADR = DATA_EE_ADDR + i; // address
        EEDATA = *ptr; // value
        EECON1bits.EEPGD = 0; // data memory
        EECON1bits.WREN = 1; // enable writes
        EECON2 = 0x55;
        EECON2 = 0xaa;
        EECON1bits.WR = 1; // write the byte
        ptr++;
        
        while(!PIR2bits.EEIF)
        {
        }
    }

// disable writes
    EECON1bits.WREN = 0;
// enable interrupts
    INTCONbits.GIE = 1;
}

void handle_preset_button(int preset)
{
    if(setting_preset)
    {
        presets[preset].pitch = nozzle_pitch;
        presets[preset].angle = nozzle_angle;
        write_presets();
    
        setting_preset = 0;
// LED on
        LED_PORT = 1;

// wiggle the nozzle
        orig_nozzle_angle = nozzle_angle;
        if(nozzle_angle < ANGLE_STEPS)
        {
            nozzle_angle++;
        }
        else
        {
            nozzle_angle--;
        }
        update_motors();
        preset_state = do_preset_save1;
    }
    else
    {
        current_preset = preset;
// center it
        preset_state = do_preset_center;
    }
}

#ifdef XY_CONTROL

void calculate_neighbors()
{
    uint8_t i;
    for(i = 0; i < NEIGHBORS; i++)
    {
        uint8_t pitch = nozzle_pitch + neighbor_pitch[i];
        uint8_t angle = nozzle_angle + neighbor_angle[i];
        if(pitch >= 0 &&
            pitch <= PITCH_STEPS &&
            angle >= ENCODER0_MIN &&
            angle <= ENCODER0_MAX)
        {
            polar_to_xy(&neighbor_x[i], 
                &neighbor_x[i], 
                nozzle_pitch + neighbor_pitch[i], 
                nozzle_angle + neighbor_angle[i]);
            good_neighbors[i] = 1;
        }
        else
        {
            good_neighbors[i] = 0;
        }
    }
}

#endif // XY_CONTROL


#define DO_XY_CONTROL(want_direction, reject_direction, store_direction) \
    calculate_neighbors(); \
 \
/* get neighbors in desired direction */ \
    int16_t distance; \
    uint8_t best_neighbor = 0xff; \
    for(i = 0; i < NEIGHBORS; i++) \
    { \
        if(good_neighbors[i]) \
        { \
            good_neighbors[i] = want_direction; \
        } \
    } \
 \
/* get closest neighbor in undesired direction */ \
    for(i = 0; i < NEIGHBORS; i++) \
    { \
        if(good_neighbors[i]) \
        { \
            int16_t new_distance = ABS(reject_direction); \
            if(best_neighbor == 0xff || \
                new_distance < distance) \
            { \
                distance = new_distance; \
                best_neighbor = i; \
            } \
        } \
    } \
 \
/* got one */ \
    if(best_neighbor != 0xff) \
    { \
        store_direction; \
        nozzle_pitch = nozzle_pitch + neighbor_pitch[best_neighbor]; \
        nozzle_angle = nozzle_angle + neighbor_angle[best_neighbor]; \
        update_motors(); \
    }


void handle_ir_code()
{
    uint8_t i;
	switch(ir_code)
	{
        case UP:
            setting_preset = 0;
            preset_state = 0;
            LED_PORT = !LED_PORT;



#ifdef XY_CONTROL
            DO_XY_CONTROL((neighbor_y[i] > nozzle_y),
                neighbor_x[i] - nozzle_x,
                nozzle_y = neighbor_y[best_neighbor])
            
#else // XY_CONTROL
            if(nozzle_pitch > 0)
            {
                nozzle_pitch--;
                update_motors();
            }
#endif // !XY_CONTROL
            break;

        case DOWN:
            setting_preset = 0;
            preset_state = 0;
            LED_PORT = !LED_PORT;

#ifdef XY_CONTROL
            DO_XY_CONTROL((neighbor_y[i] < nozzle_y),
                neighbor_x[i] - nozzle_x,
                nozzle_y = neighbor_y[best_neighbor])
            
#else // XY_CONTROL
            if(nozzle_pitch < PITCH_STEPS)
            {
                nozzle_pitch++;
                update_motors();
            }
#endif // !XY_CONTROL
            break;

        case LEFT:
            setting_preset = 0;
            preset_state = 0;
            LED_PORT = !LED_PORT;
#ifdef XY_CONTROL
            DO_XY_CONTROL((neighbor_x[i] < nozzle_x),
                neighbor_y[i] - nozzle_y,
                nozzle_x = neighbor_y[best_neighbor])
            
#else // XY_CONTROL
            if(nozzle_angle < tracking_state[0].max)
            {
                nozzle_angle++;
                update_motors();
            }
#endif // !XY_CONTROL
            break;

        case RIGHT:
            setting_preset = 0;
            preset_state = 0;
            LED_PORT = !LED_PORT;

#ifdef XY_CONTROL
            DO_XY_CONTROL((neighbor_x[i] > nozzle_x),
                neighbor_y[i] - nozzle_y,
                nozzle_x = neighbor_y[best_neighbor])
            
#else // XY_CONTROL
            if(nozzle_angle > tracking_state[0].min)
            {
                nozzle_angle--;
                update_motors();
            }
#endif // !XY_CONTROL
            break;
        
        case SET_PRESET:
            if(setting_preset)
            {
                setting_preset = 0;
            }
            else
            if(armed)
            {
                setting_preset = 1;
            }
            break;

        case PRESET1:
            handle_preset_button(0);
            break;

        case PRESET2:
            handle_preset_button(1);
            break;

        case PRESET3:
            handle_preset_button(2);
            break;

        case PRESET4:
            handle_preset_button(3);
            break;

        case POWER:
            setting_preset = 0;
            preset_state = 0;
            LED_PORT = !LED_PORT;
            if(!armed)
            {
                arm_motors();
            }
            else
            {
                disarm_motors();
            }
            break;
	}
}

void handle_ir2()
{
// uncomment this to capture the IR codes
// send binary to save serial port space & see if the length is consistent
// DEBUG
//print_byte(ir_time);
//return;



// search for the code
	uint8_t i;
// got complete code
	uint8_t got_it = 0;
// got part of a code
    uint8_t got_one = 0;
	for(i = 0; i < TOTAL_CODES && !got_it; i++)
//	for(i = 0; i < 2 && !got_it; i++)
	{
// code has matched all previous bytes
		if(!ir_code_failed[i])
		{
			const ir_code_t *code = &ir_codes[i];
			const uint8_t *data = code->data;

            if(ir_offset < code->size)
            {
// test latest byte
                int16_t data_value = data[ir_offset];
			    int16_t error = ABS(data_value - ir_time);

// reject code if latest byte doesn't match
			    if(error > IR_MARGIN)
			    {
// don't search this code anymore
				    ir_code_failed[i] = 1;
			    }
			    else
// all bytes so far matched the current code
			    {
// complete code was received
				    if(ir_offset >= code->size - 1)
				    {
					    have_ir = 1;
                        repeat_delay = REPEAT_DELAY;
					    ir_code = code->value;

                        got_it = 1;
				    }

				    got_one = 1;
			    }
            }
		}
	}

// process the code
    if(got_it)
    {
#ifdef VERBOSE
		print_text("IR code: ");
		print_number(ir_code);
        print_text("total codes=");
        print_number(TOTAL_CODES);
		print_byte('\n');
        flush_uart();
#endif

// handle the code
        handle_ir_code();
    }
    else
    if(got_one)
    {
// advance to next offset
    	ir_offset++;
    }
    else
    {
// prepare to receive the next code before the next release
  		ir_offset = 0;
		for(i = 0; i < TOTAL_CODES; i++)
		{
			ir_code_failed[i] = 0;
		}
    }
}

void handle_ir()
{
    uint8_t i;
// IR timed out
    if(ir_timeout && !first_edge)
    {
#ifdef VERBOSE
        print_text("IR released\n");
#endif

// reset the timer
    	INTCONbits.GIE = 0;
// use falling edge.  page 25
        OPTION_REGbits.INTEDG = 0;
		TMR2 = 0;
        PIR1bits.TMR2IF = 0;
    	INTCONbits.GIE = 1;

		ir_offset = 0;
		for(i = 0; i < TOTAL_CODES; i++)
		{
			ir_code_failed[i] = 0;
		}
        first_edge = 1;
		have_ir = 0;
        got_ir_int = 0;
        LED_PORT = 1;
    }

// repeat IR code after 1st delay & motors are all in braking mode
    if(have_ir && 
        repeat_delay == 0 &&
        ir_code != POWER &&
        (motor_master & HBRIDGE_MASK) == HBRIDGE_MASK)
    {
//print_text("IR repeat\n");
//        repeat_delay = REPEAT_DELAY2;
        handle_ir_code();
    }

	if(got_ir_int)
	{
        got_ir_int = 0;
// reverse edge
		OPTION_REGbits.INTEDG = !OPTION_REGbits.INTEDG;
    	if(first_edge)
    	{
        	first_edge = 0;
    	}
    	else
    	{
 			handle_ir2();
    	}
	}
}


void charge_adc();
void (*adc_state)() = charge_adc;

void capture_adc()
{
    if(PIR1bits.ADIF)
    {
        sensor_state_t *sensor = &sensors[current_adc];
        sensor->analog = ADRESH;
        if(sensor->ns == 0 && 
            sensor->analog >= 0x80 + SENSOR_THRESHOLD)
        {
            sensor->ns = 1;
            sensor->position += sensor->step;
        }
        else
        if(sensor->ns == 1 &&
            sensor->analog < 0x80 - SENSOR_THRESHOLD)
        {
            sensor->ns = 0;
            sensor->position += sensor->step;
        }

        current_adc++;
        if(current_adc >= TOTAL_ADC)
        {
            current_adc = 0;
            
//             if(uart_size == 0)
//             {
//                 print_hex2(sensor_values[0]);
//                 print_text(" ");
//                 print_hex2(sensor_values[1]);
//                 print_text(" ");
//                 print_hex2(sensor_values[2]);
//                 print_text(" ");
//                 print_hex2(sensor_values[3]);
//                 print_text(" ");
//                 print_hex2(sensor_values[4]);
//                 print_text(" ");
//                 print_hex2(sensor_values[5]);
//                 print_text("\n");
//             }
        }
        
        switch(current_adc)
        {
            case 0:
                ADCON0 = ADCON0_MASK | 0b00000000;
                break;
            case 1:
                ADCON0 = ADCON0_MASK | 0b00001000;
                break;
            case 2:
                ADCON0 = ADCON0_MASK | 0b00010000;
                break;
            case 3:
                ADCON0 = ADCON0_MASK | 0b00011000;
                break;
            case 4:
                ADCON0 = ADCON0_MASK | 0b00100000;
                break;
            case 5:
                ADCON0 = ADCON0_MASK | 0b00101000;
                break;
        }
        
        TMR0 = 0;
        INTCONbits.TMR0IF = 0;
        adc_state = charge_adc;
    }
}

void charge_adc()
{
    if(INTCONbits.TMR0IF)
    {
        INTCONbits.TMR0IF;
        PIR1bits.ADIF = 0;
        ADCON0bits.GO = 1;
        adc_state = capture_adc;
    }
}

#ifdef TEST_MODE

void menu()
{
    print_text("1/2 - motor 0 direct drive\n");
    flush_uart();
    print_text("3/4 - motor 1 direct drive\n");
    flush_uart();
    print_text("5/6 - motor 2 direct drive\n");
    flush_uart();
    print_text("q/a - motor 0 tracking (");
    print_number_nospace(tracking_state[0].target_position);
    print_text(")\n");
    flush_uart();
    print_text("w/s - motor 1 tracking (");
    print_number_nospace(tracking_state[1].target_position);
    print_text(")\n");
    flush_uart();
    print_text("e/d - motor 2 tracking (");
    print_number_nospace(tracking_state[2].target_position);
    print_text(")\n");
    print_text("r - right angle\n");
    print_text("t - straight\n");
    flush_uart();
    print_text("SPACE - stop\n");
    
}

void handle_menu()
{
    if(have_serial)
    {
        have_serial = 0;
        switch(serial_in)
        {
            case '1':
                armed = 1;
                motor_master &= ~MOTOR0_RIGHT;
                motor_master |= MOTOR0_LEFT;
                menu();
                break;
            case '2':
                armed = 1;
                motor_master &= ~MOTOR0_LEFT;
                motor_master |= MOTOR0_RIGHT;
                menu();
                break;
            case '3':
                armed = 1;
                motor_master &= ~MOTOR1_RIGHT;
                motor_master |= MOTOR1_LEFT;
                menu();
                break;
            case '4':
                armed = 1;
                motor_master &= ~MOTOR1_LEFT;
                motor_master |= MOTOR1_RIGHT;
                menu();
                break;
            case '5':
                armed = 1;
                motor_master &= ~MOTOR2_RIGHT;
                motor_master |= MOTOR2_LEFT;
                menu();
                break;
            case '6':
                armed = 1;
                motor_master &= ~MOTOR2_LEFT;
                motor_master |= MOTOR2_RIGHT;
                menu();
                break;

            case 'q':
                tracking_state[0].target_position++;
                if(tracking_state[0].changed < 0xff)
                {
                    tracking_state[0].changed++;
                }
                menu();
                break;

            case 'a':
                tracking_state[0].target_position--;
                if(tracking_state[0].changed < 0xff)
                {
                    tracking_state[0].changed++;
                }
                menu();
                break;

            case 'w':
                tracking_state[1].target_position++;
                if(tracking_state[1].changed < 0xff)
                {
                    tracking_state[1].changed++;
                }
                menu();
                break;

            case 's':
                tracking_state[1].target_position--;
                if(tracking_state[1].changed < 0xff)
                {
                    tracking_state[1].changed++;
                }
                menu();
                break;

            case 'e':
                tracking_state[2].target_position++;
                if(tracking_state[2].changed < 0xff)
                {
                    tracking_state[2].changed++;
                }
                menu();
                break;

            case 'd':
                tracking_state[2].target_position--;
                if(tracking_state[2].changed < 0xff)
                {
                    tracking_state[2].changed++;
                }
                menu();
                break;

            case 'r':
                tracking_state[2].target_position = tracking_state[2].max;
                if(tracking_state[2].changed < 0xff)
                {
                    tracking_state[2].changed++;
                }
                tracking_state[1].target_position = tracking_state[1].max;
                if(tracking_state[1].changed < 0xff)
                {
                    tracking_state[1].changed++;
                }
                tracking_state[0].target_position = tracking_state[0].max;
                if(tracking_state[0].changed < 0xff)
                {
                    tracking_state[0].changed++;
                }
                menu();
                break;

            case 't':
                tracking_state[2].target_position = tracking_state[2].min;
                if(tracking_state[2].changed < 0xff)
                {
                    tracking_state[2].changed++;
                }
                tracking_state[1].target_position = tracking_state[1].min;
                if(tracking_state[1].changed < 0xff)
                {
                    tracking_state[1].changed++;
                }
                tracking_state[0].target_position = HOME0;
                if(tracking_state[0].changed < 0xff)
                {
                    tracking_state[0].changed++;
                }
                menu();
                break;

            case ' ':
                disarm_motors();
                menu();
                break;
                
            case '\n':
            {
                print_text("ir_offset=");
                print_number(ir_offset);
                uint8_t i;
                for(i = 0; i < TOTAL_CODES; i++)
                {
                    print_number(ir_code_failed[i]);
                }
                print_text("\n");
                flush_uart();
                break;
            }
        }
    }
}
#endif // TEST_MODE

// void handle_pwm()
// {
//     if((tick % 3) == 0)
//     {
//         uint8_t prev_port = HBRIDGE_PORT & ~HBRIDGE_MASK;
//         HBRIDGE_PORT = prev_port | motor_master;
//     }
//     else
//     {
//         HBRIDGE_PORT &= motor_master ^ 0xff;
//     }
// }

void interrupt isr()
{
    interrupt_done = 0;
    while(!interrupt_done)
    {
        interrupt_done = 1;

// mane timer
        if(PIR1bits.TMR1IF)
        {
            PIR1bits.TMR1IF = 0;
            TMR1 = TIMER1_VALUE;
            got_tick = 1;
            uint8_t prev_port = HBRIDGE_PORT & ~HBRIDGE_MASK;
            HBRIDGE_PORT = prev_port | motor_master;
            interrupt_done = 0;
        }

// UART receive
        if(PIR1bits.RCIF)
        {
            PIR1bits.RCIF = 0;
            serial_in = RCREG;
            have_serial = 1;
			interrupt_done = 0;
        }

// manage the IR timer
        if(PIR1bits.TMR2IF)
        {
            PIR1bits.TMR2IF = 0;
            tmr2_high++;
            if(tmr2_high > IR_TIMEOUT)
            {
                ir_timeout = 1;
            }
            interrupt_done = 0;
        }

// IR interrupt
		if(INTCONbits.INTF)
		{
			INTCONbits.INTF = 0;
// reset the timer
			TMR2 = 0;
            PIR1bits.TMR2IF = 0;
// copy the software timer value
			ir_time = tmr2_high;
            tmr2_high = 0;
            ir_timeout = 0;

			got_ir_int = 1;
			interrupt_done = 0;
		}
    }
}


void main()
{
	uint8_t i;
    LED_PORT = 1;
    LED_TRIS = 0;

    HBRIDGE_PORT &= ~HBRIDGE_MASK;
    HBRIDGE_TRIS &= ~HBRIDGE_MASK;

    have_serial = 0;
    SPBRG = BAUD_CODE;
// page 113
    TXSTA = 0b00100100;
// page 114
    RCSTA = 0b10010000;
    PIE1bits.RCIE = 1;


// enable IR interrupt page 26
    INTCONbits.INTE = 1;
    INTCONbits.INTF = 0;
// use falling edge.  page 25
    OPTION_REGbits.INTEDG = 0;
	ir_offset = 0;
    first_edge = 1;

// IR timer. 4x prescale. page 63
    T2CON = 0b00000101;
// timer period
    PR2 = 0xff;
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 1;



// enable mane timer.  8:1 prescale  
    T1CON = 0b00110001;
    TMR1 = TIMER1_VALUE;

    memset(sensors, 0, sizeof(sensor_state_t) * TOTAL_ADC);

// enable ADC page 129
    ADCON0 = ADCON0_MASK;
// page 130
    ADCON1 = 0b01001001;
// ADC timer page 56
    OPTION_REGbits.T0CS = 0;
    TMR0 = 0;
    INTCONbits.TMR0IF = 0;

    read_presets();

// enable all interrupts page 26
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

	print_text("\n\n\n\nWelcome to F-35 nozzle\n");


// initialize motor tables
    tracking_state[2].min = ENCODER2_MIN;
    tracking_state[2].max = ENCODER2_MAX;
    tracking_state[2].boundary = BOUNDARY2;
    tracking_state[2].encoder = ENCODER2;
    tracking_state[2].dec_mask = MOTOR2_RIGHT;
    tracking_state[2].inc_mask = MOTOR2_LEFT;
    tracking_state[2].total_unmask = ~(MOTOR2_LEFT | MOTOR2_RIGHT);
    tracking_state[2].total_mask = MOTOR2_LEFT | MOTOR2_RIGHT;
    tracking_state[2].brake = 1;

    tracking_state[1].min = ENCODER1_MIN;
    tracking_state[1].max = ENCODER1_MAX;
    tracking_state[1].boundary = BOUNDARY1;
    tracking_state[1].encoder = ENCODER1;
    tracking_state[1].dec_mask = MOTOR1_LEFT;
    tracking_state[1].inc_mask = MOTOR1_RIGHT;
    tracking_state[1].total_unmask = ~(MOTOR1_LEFT | MOTOR1_RIGHT);
    tracking_state[1].total_mask = MOTOR1_LEFT | MOTOR1_RIGHT;

    tracking_state[0].min = ENCODER0_MIN;
    tracking_state[0].max = ENCODER0_MAX;
    tracking_state[0].boundary = BOUNDARY0;
    tracking_state[0].encoder = ENCODER0;
    tracking_state[0].dec_mask = MOTOR0_RIGHT;
    tracking_state[0].inc_mask = MOTOR0_LEFT;
    tracking_state[0].total_unmask = ~(MOTOR0_LEFT | MOTOR0_RIGHT);
    tracking_state[0].total_mask = MOTOR0_LEFT | MOTOR0_RIGHT;

#ifdef XY_CONTROL
// starting coordinate
    polar_to_xy(&nozzle_x, &nozzle_y, nozzle_pitch, nozzle_angle);
#endif // XY_CONTROL




#ifdef TEST_MODE
    menu();
#endif // TEST_MODE


    while(1)
    {
        asm("clrwdt");

// turn off all motors
        if(!armed)
        {
            motor_master &= ~HBRIDGE_MASK;
        }
        else
        {
//            LED_PORT = 1;
        }

// send a UART char
        handle_uart();

// receive UART char
#ifdef TEST_MODE
        handle_menu();
#endif // TEST_MODE

// mane timer fired
        if(got_tick)
        {
            got_tick = 0;
            for(i = 0; i < TOTAL_MOTORS; i++)
            {
                tracking_state_t *tracking = &tracking_state[i];
                if(tracking->timer > 0)
                {
                    tracking->timer--;
                }
            }
            tick++;
            if(repeat_delay > 0)
            {
                repeat_delay--;
            }
            if(preset_delay > 0)
            {
                preset_delay--;
            }

// flash LED if disarmed or setting a preset
            if(!armed)
            {
                led_counter++;
                if(led_counter >= LED_DELAY)
                {
                    led_counter = 0;
                    LED_PORT = !LED_PORT;
                }
            }
            else
            if(setting_preset)
            {
                led_counter++;
                if(led_counter >= LED_DELAY2)
                {
                    led_counter = 0;
                    LED_PORT = !LED_PORT;
                }
            }
        }


        handle_ir();



// probe hall effect sensors
        adc_state();


        motor_state();

        if(preset_state != 0)
        {
            preset_state();
        }
    }

}




