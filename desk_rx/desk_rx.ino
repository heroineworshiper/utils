/*
 * WIFI Receiver for remote controlled desk.
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



#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

extern "C" {
#include "osapi.h"
#include "os_type.h"
}

#define SSID "OVERPRICED APT"
#define PASSWORD "growamane"

#define CLOCKSPEED 80000000
#define PORT 1234
#define BUFSIZE 16
char incomingPacket[BUFSIZE];  // buffer for incoming packets

WiFiUDP Udp;
os_timer_t ticker;
#define HZ 20

// ticks between bits
#define UART_PERIOD (CLOCKSPEED / 9600)
// 10 ms UART timeout
#define UART_TIMEOUT (CLOCKSPEED / 100)

// GPIOS
#define CONNECTION_LED 2  // Connection LED.  On is 0
#define STATUS_LED 4 // status LED.  On is 0
#define UART_RX 5 // UART RX
// button outputs
#define DOWN_PIN 12
#define UP_PIN 13
#define RECALL_PIN 14
#define MODE_PIN 16

// button codes
#define PRESET1 0x08
#define PRESET2 0x02
#define PRESET3 0x80
#define PRESET4 0x20
#define FINE 0x01
//#define ABORT 0x10
#define SET 0x10
#define UP 0x04
#define DOWN 0x40
#define NO_BUTTON 0
// last button pressed
uint8_t button = NO_BUTTON;
// last ID
uint8_t id = 0xff;
uint8_t prev_id = 0xff;
// fine button pressed
uint8_t fine = 0;

int got_tick = 0;
// LED blinker
uint16_t blink_tick = 0;
// time since last button press
uint8_t button_tick = 0;
// time since fine mode started
uint8_t fine_tick = 0;

// ticks before releasing all buttons
#define BUTTON_TIMEOUT (HZ / 5)
// ticks for fine button press
#define FINE_TIMEOUT (HZ / 4)

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


typedef union 
{
	struct
	{
        unsigned have_code : 1;
        unsigned have_serial : 1;
// desired LED value from button state.  Overridden by led_state
        unsigned want_led : 1;
	};
	
	unsigned char value;
} flags_t;

flags_t flags;
uint8_t uart_counter;
uint8_t serial_in;



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
                    Serial.printf("SET PRESET\n");
                    preset_state = PRESET_IDLE;
                }
// moving to the preset
                else
                if(status_code[0] == 0x01 && 
                    status_code[1] == 0x01)
                {
                    Serial.printf("MOVING TO PRESET\n");
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
//Serial.printf("WAIT_MOVE %d\n", height);
// reached top
                    if(height >= MAX_HEIGHT)
                    {
                        Serial.printf("TOP REACHED\n");
                        preset_state = WAIT_TOP;
                    }
                    else
// reached bottom
                    if(height <= MIN_HEIGHT)
                    {
                        Serial.printf("BOTTOM REACHED\n");
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
                        Serial.printf("LEFT TOP\n");
                        preset_state = WAIT_MOVE;
                        preset_tick = 0;
                    }
                    else
                    if(preset_tick >= HEIGHT_WAIT_TIME)
                    {
                        Serial.printf("GOING UP AGAIN\n");
                        preset_state = HEIGHT_WAIT;
                        preset_tick = 0;
// press the up button
                        digitalWrite(UP_PIN, 0);
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
                        Serial.printf("LEFT BOTTOM\n");
                        preset_state = WAIT_MOVE;
                        preset_tick = 0;
                    }
                    else
                    if(preset_tick >= HEIGHT_WAIT_TIME)
                    {
                        Serial.printf("GOING DOWN AGAIN\n");
                        preset_state = HEIGHT_WAIT;
                        preset_tick = 0;
    // press the down button
                        digitalWrite(DOWN_PIN, 0);
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
                    Serial.printf("PRESET WORKAROUND DONE\n");
                    preset_state = PRESET_IDLE;
// release the buttons & quit
                    digitalWrite(UP_PIN, 1);
                    digitalWrite(DOWN_PIN, 1);
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


// button timed out or was invalid
void reset_buttons()
{
    digitalWrite(DOWN_PIN, 1);
    digitalWrite(UP_PIN, 1);
    digitalWrite(RECALL_PIN, 1);
    digitalWrite(MODE_PIN, 1);
    flags.want_led = 0;
    prev_id = id;
    fine_tick = 0;
    button = NO_BUTTON;
}



// timeout timer
void handle_tick(void* x)
{
//    digitalWrite(CONNECTION_LED,!(digitalRead(CONNECTION_LED)));
    got_tick = 1;
}


// start bit timeout
void ICACHE_RAM_ATTR start_bit_timeout()
{
    timer1_disable();
    status_size = 0;
}

// UART bit timer
void ICACHE_RAM_ATTR uart_int();
void ICACHE_RAM_ATTR timer1_int()
{
// schedule next bit
    timer1_write(UART_PERIOD);
//    digitalWrite(CONNECTION_LED,!(digitalRead(CONNECTION_LED)));
// desk outputs LSB 1st
    serial_in >>= 1;
    if(digitalRead(UART_RX))
        serial_in |= 0x80;
    uart_counter++;
    if(uart_counter >= 8)
    {
        flags.have_serial = 1;
// enable start bit detection & timeout
        timer1_attachInterrupt(start_bit_timeout);
        timer1_write(UART_TIMEOUT);
        attachInterrupt(digitalPinToInterrupt(UART_RX), uart_int, FALLING);
    }
}

// start bit detection
void ICACHE_RAM_ATTR uart_int()
{
    uart_counter = 0;
    serial_in = 0;
    detachInterrupt(digitalPinToInterrupt(UART_RX));
// enable bit reads
    timer1_attachInterrupt(timer1_int);
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
    timer1_write(UART_PERIOD * 3 / 2);
}

void do_connection()
{
    while(WiFi.status() != WL_CONNECTED)
    {
        Serial.printf("\nConnecting to %s ", SSID);
        WiFi.begin(SSID, PASSWORD);

        int tick = 0;
        flags.want_led = 1;
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
            digitalWrite(CONNECTION_LED, flags.want_led);
            flags.want_led = !flags.want_led;
            tick++;
// reset command might actually put it in bootloader
            if(tick >= 20) 
            {
                ESP.restart();
                break;
            }
        }
    }
    Serial.println(" connected");
    digitalWrite(CONNECTION_LED, 0);

    Udp.begin(PORT);
    Serial.printf("Now listening at IP %s, UDP port %d\n", 
        WiFi.localIP().toString().c_str(), 
        PORT);
}


void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println();
    pinMode(CONNECTION_LED, OUTPUT);
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(CONNECTION_LED, 0);
    digitalWrite(STATUS_LED, 1);
    pinMode(UART_RX, INPUT);


    pinMode(DOWN_PIN, OUTPUT);
    digitalWrite(DOWN_PIN, 1);
    pinMode(UP_PIN, OUTPUT);
    digitalWrite(UP_PIN, 1);
    pinMode(RECALL_PIN, OUTPUT);
    digitalWrite(RECALL_PIN, 1);
    pinMode(MODE_PIN, OUTPUT);
    digitalWrite(MODE_PIN, 1);

    do_connection();

    os_timer_setfn(&ticker, handle_tick, 0);
    os_timer_arm(&ticker, 1000 / HZ, 1);

//    timer1_attachInterrupt(timer1_int);
//    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
//    timer1_write(UART_PERIOD);

    flags.value = 0;
// enable start bit detection
    attachInterrupt(digitalPinToInterrupt(UART_RX), uart_int, FALLING);
}


void loop()
{
    if(WiFi.status() != WL_CONNECTED)
    {
        do_connection();
    }


    if(got_tick)
    {
        got_tick = 0;
        if(fine && 
            fine_tick < FINE_TIMEOUT &&
            (button == UP || button == DOWN))
            fine_tick++;
        
        if(button_tick < BUTTON_TIMEOUT)
            button_tick++;

        if(led_tick < LED_TIMEOUT)
            led_tick++;

        if(preset_tick < PRESET_TIMEOUT)
            preset_tick++;

        blink_tick++;
    }

    if(flags.have_serial)
    {
        flags.have_serial = 0;
        if(status_size < STATUS_SIZE)
            status_code[status_size++] = serial_in;

// handle a new packet
        if(status_size >= STATUS_SIZE)
        {
//             Serial.printf("loop %d: %02x %02x %02x %02x\n", 
//                 __LINE__,
//                 status_code[0],
//                 status_code[1],
//                 status_code[2],
//                 status_code[3]);

// waiting for a preset to set
            if(status_code[0] == 0x01 && 
                status_code[1] == 0x06 && 
                status_code[2] == 0x00 && 
                status_code[3] == 0x00)
            {
                led_state = LED_WAITING;
                led_tick = 0;
                Serial.printf("WAITING\n");
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
                Serial.printf("LOCKED\n");
            }


// preset bug workaround
            preset_bug();
            status_size = 0;
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

// turn off buttons in fine mode
    if(fine && 
        fine_tick >= FINE_TIMEOUT && 
        (button == UP || button == DOWN))
    {
        digitalWrite(DOWN_PIN, 1);
        digitalWrite(UP_PIN, 1);
    }

// button press overrides LED
    int packetSize = Udp.parsePacket();
    if (packetSize)
    {
// receive incoming UDP packets
        Serial.printf("loop %d: got %d bytes from %s ", 
            __LINE__,
            packetSize, 
            Udp.remoteIP().toString().c_str(), 
            Udp.remotePort());
        int len = Udp.read(incomingPacket, BUFSIZE);

// DEBUG light for all packets
//flags.want_led = 1;


// button code
        if(len == 2)
        {
            id = incomingPacket[0];
            button = incomingPacket[1];
            Serial.printf("ID=%02x BUTTON=%02x fine_tick=%d\n", 
                id, 
                button,
                fine_tick);
// extract the fine button
            fine = button & FINE;
            button &= ~FINE;



// abort height workaround on each button press
            preset_state = PRESET_IDLE;
            button_tick = 0;
            
            


            switch(button)
            {
                case UP:
                    digitalWrite(DOWN_PIN, 1);
                    digitalWrite(RECALL_PIN, 1);
                    digitalWrite(MODE_PIN, 1);
                    if(fine_tick < FINE_TIMEOUT)
                    {
                        digitalWrite(UP_PIN, 0);
                    }
                    flags.want_led = 1;
                    break;

                case DOWN:
                    digitalWrite(UP_PIN, 1);
                    digitalWrite(RECALL_PIN, 1);
                    digitalWrite(MODE_PIN, 1);
                    if(fine_tick < FINE_TIMEOUT)
                    {
                        digitalWrite(DOWN_PIN, 0);
                    }
                    flags.want_led = 1;
                    break;

                case SET:
                    if(!fine)
                    {
// reject preset button after it timed out unless its ID changed
                        if(id == prev_id) break;

                        digitalWrite(DOWN_PIN, 1);
                        digitalWrite(UP_PIN, 1);
                        digitalWrite(RECALL_PIN, 1);
                        digitalWrite(MODE_PIN, 0);
                        flags.want_led = 1;
                    }
                    break;
// Min height preset
                case PRESET1:
                    if(!fine)
                    {
// reject preset button after it timed out unless its ID changed
                        if(id == prev_id) break;

                        digitalWrite(DOWN_PIN, 1);
                        digitalWrite(UP_PIN, 1);
                        digitalWrite(RECALL_PIN, 0);
                        digitalWrite(MODE_PIN, 1);
                        flags.want_led = 1;
    // trigger the preset workaround
                        preset_state = WAIT_SAVED_PRESET;
                        preset_tick = 0;
                    }
                    break;

                case PRESET2:
                    if(!fine)
                    {
// reject preset button after it timed out unless its ID changed
                        if(id == prev_id) break;

                        digitalWrite(DOWN_PIN, 0);
                        digitalWrite(UP_PIN, 1);
                        digitalWrite(RECALL_PIN, 0);
                        digitalWrite(MODE_PIN, 1);
                        flags.want_led = 1;
    // trigger the preset workaround
                        preset_state = WAIT_SAVED_PRESET;
                        preset_tick = 0;
                    }
                    break;

                case PRESET3:
                    if(!fine)
                    {
// reject preset button after it timed out unless its ID changed
                        if(id == prev_id) break;

                        digitalWrite(DOWN_PIN, 1);
                        digitalWrite(UP_PIN, 0);
                        digitalWrite(RECALL_PIN, 0);
                        digitalWrite(MODE_PIN, 1);
                        flags.want_led = 1;
    // trigger the preset workaround
                        preset_state = WAIT_SAVED_PRESET;
                        preset_tick = 0;
                    }
                    break;

// Max height preset
                case PRESET4:
                    if(!fine)
                    {
// reject preset button after it timed out unless its ID changed
                        if(id == prev_id) break;

                        digitalWrite(DOWN_PIN, 0);
                        digitalWrite(UP_PIN, 0);
                        digitalWrite(RECALL_PIN, 1);
                        digitalWrite(MODE_PIN, 1);
                        flags.want_led = 1;
    // trigger the preset workaround
                        preset_state = WAIT_SAVED_PRESET;
                        preset_tick = 0;
                    }
                    break;

                default:
// reset all buttons
                    reset_buttons();
                    break;
            }
        }
    }

    if(button_tick == BUTTON_TIMEOUT)
    {
        Serial.printf("BUTTON_TIMEOUT\n");
        button_tick++;
        reset_buttons();
    }


// Active LED state has LED's highest priority
    switch(led_state)
    {
        case LED_WAITING:
// 2 HZ blink
            flags.want_led = (blink_tick % (HZ / 2)) < HZ / 4;
            break;

        case LED_LOCKED:
// 0.5 HZ blink
            flags.want_led = (blink_tick % (HZ * 2)) < HZ;
            break;
    }




// update the LED
    digitalWrite(STATUS_LED, !flags.want_led);
}





