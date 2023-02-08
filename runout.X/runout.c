
// PIC18LF1220 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1H
#pragma config OSC = INTIO2     // Oscillator Selection bits (Internal RC oscillator, CLKO function on RA6 and port function on RA7)
#pragma config FSCM = ON        // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor enabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal External Switchover mode enabled)

// CONFIG2L
#pragma config PWRT = ON        // Power-up Timer Enable bit (PWRT enabled)
#pragma config BOR = ON         // Brown-out Reset Enable bit (Brown-out Reset enabled)
#pragma config BORV = 27        // Brown-out Reset Voltage bits (VBOR set to 2.7V)

// CONFIG2H
#pragma config WDT = ON         // Watchdog Timer Enable bit (WDT enabled)
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits (1:32768)

// CONFIG3H
#pragma config MCLRE = ON       // MCLR Pin Enable bit (MCLR pin enabled, RA5 input pin disabled)

// CONFIG4L
#pragma config STVR = ON        // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = OFF        // Low-Voltage ICSP Enable bit (Low-Voltage ICSP disabled)

// CONFIG5L
#pragma config CP0 = OFF        // Code Protection bit (Block 0 (00200-0007FFh) not code-protected)
#pragma config CP1 = OFF        // Code Protection bit (Block 1 (000800-000FFFh) not code-protected)

// CONFIG5H
#pragma config CPB = OFF        // Boot Block Code Protection bit (Boot Block (000000-0001FFh) not code-protected)
#pragma config CPD = OFF        // Data EEPROM Code Protection bit (Data EEPROM not code-protected)

// CONFIG6L
#pragma config WRT0 = OFF       // Write Protection bit (Block 0 (00200-0007FFh) not write-protected)
#pragma config WRT1 = OFF       // Write Protection bit (Block 1 (000800-000FFFh) not write-protected)

// CONFIG6H
#pragma config WRTC = OFF       // Configuration Register Write Protection bit (Configuration registers (300000-3000FFh) not write-protected)
#pragma config WRTB = OFF       // Boot Block Write Protection bit (Boot Block (000000-0001FFh) not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM not write-protected)

// CONFIG7L
#pragma config EBTR0 = OFF      // Table Read Protection bit (Block 0 (00200-0007FFh) not protected from table reads executed in other blocks)
#pragma config EBTR1 = OFF      // Table Read Protection bit (Block 1 (000800-000FFFh) not protected from table reads executed in other blocks)

// CONFIG7H
#pragma config EBTRB = OFF      // Boot Block Table Read Protection bit (Boot Block (000000-0001FFh) not protected from table reads executed in other blocks)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include <pic18f1220.h>
#include <stdint.h>

uint8_t tick;
#define CLOCKSPEED 8000000
#define HZ 20
#define TIMER0_PERIOD (CLOCKSPEED / 4 / 4 / HZ - 1)
#define SPEAKER0 LATA0
#define SPEAKER1 LATA1
#define SPEAKER0_TRIS TRISA0
#define SPEAKER1_TRIS TRISA1
#define LED0 LATA6
#define LED1 LATA7
#define LED0_TRIS TRISA6
#define LED1_TRIS TRISA7
#define SENSOR_LAT LATA3
#define SENSOR_TRIS TRISA3
// sensor analog: AN4
#define THRESHOLD 128


//#define MIN_NOTE -(CLOCKSPEED / 4 / 440 / 2 - 1) 
//#define MAX_NOTE (MIN_NOTE / 2)
#define MIN_NOTE -2500
#define MAX_NOTE -1250
#define OCTAVE0 (MIN_NOTE - MIN_NOTE * 2)
#define OCTAVE1 (MAX_NOTE - MIN_NOTE)
#define OCTAVE2 (MAX_NOTE / 2 - MAX_NOTE)

#define LATENCY 58
#define FREQ_TO_PERIOD(f) (uint16_t)(-CLOCKSPEED / 4 / (f * 2) + LATENCY)

// index to freq in CPU clocks
const uint16_t freqs[] = 
{
	FREQ_TO_PERIOD(130.81),
	FREQ_TO_PERIOD(138.59),
	FREQ_TO_PERIOD(146.83),
	FREQ_TO_PERIOD(155.56),
	FREQ_TO_PERIOD(164.81),
	FREQ_TO_PERIOD(174.61),
	FREQ_TO_PERIOD(185.00),
	FREQ_TO_PERIOD(196.00),
	FREQ_TO_PERIOD(207.65),
	FREQ_TO_PERIOD(220.00),
	FREQ_TO_PERIOD(233.08),
	FREQ_TO_PERIOD(246.94),

	FREQ_TO_PERIOD(261.63), // C0
	FREQ_TO_PERIOD(277.18), // _Db0
	FREQ_TO_PERIOD(293.66), // _D0
	FREQ_TO_PERIOD(311.13), // _Eb0
	FREQ_TO_PERIOD(329.63), // _E0
	FREQ_TO_PERIOD(349.23), // _F0
	FREQ_TO_PERIOD(369.99), // _Gb0
	FREQ_TO_PERIOD(392.00), // _G0
	FREQ_TO_PERIOD(415.30), // _Ab0
	FREQ_TO_PERIOD(440.00), // _A0
	FREQ_TO_PERIOD(466.16), // _Bb0
	FREQ_TO_PERIOD(493.88), // _B0

	FREQ_TO_PERIOD(523.251),
	FREQ_TO_PERIOD(554.365),
	FREQ_TO_PERIOD(587.330),
	FREQ_TO_PERIOD(622.254),
	FREQ_TO_PERIOD(659.255),
	FREQ_TO_PERIOD(698.456),
	FREQ_TO_PERIOD(739.989),
	FREQ_TO_PERIOD(783.991),
	FREQ_TO_PERIOD(830.609),
	FREQ_TO_PERIOD(880.000),
	FREQ_TO_PERIOD(932.328),
	FREQ_TO_PERIOD(987.767),

	FREQ_TO_PERIOD(1046.50),
	FREQ_TO_PERIOD(1108.73),
	FREQ_TO_PERIOD(1174.66),
	FREQ_TO_PERIOD(1244.51),
	FREQ_TO_PERIOD(1318.51),
	FREQ_TO_PERIOD(1396.91),
	FREQ_TO_PERIOD(1479.98),
	FREQ_TO_PERIOD(1567.98),
	FREQ_TO_PERIOD(1661.22),
	FREQ_TO_PERIOD(1760.00),
	FREQ_TO_PERIOD(1864.66),
	FREQ_TO_PERIOD(1975.53),

	FREQ_TO_PERIOD(2093.00)

};

// indexes for different notes
#define _C0 0
#define _Db0 1
#define _D0 2
#define _Eb0 3
#define _E0 4
#define _F0 5
#define _Gb0 6
#define _G0 7
#define _Ab0 8
#define _A0 9
#define _Bb0 10
#define _B0 11

#define _C1 12
#define _Db1 13
#define _D1 14
#define _Eb1 15
#define _E1 16
#define _F1 17
#define _Gb1 18
#define _G1 19
#define _Ab1 20
#define _A1 21
#define _Bb1 22
#define _B1 23

#define _C2 24
#define _Db2 25
#define _D2 26
#define _Eb2 27
#define _E2 28
#define _F2 29
#define _Gb2 30
#define _G2 31
#define _Ab2 32
#define _A2 33
#define _Bb2 34
#define _B2 35

#define _C3 36
#define _Db3 37
#define _D3 38
#define _Eb3 39
#define _E3 40
#define _F3 41
#define _Gb3 42
#define _G3 43
#define _Ab3 44
#define _A3 45
#define _Bb3 46
#define _B3 47

#define SONG_REST 0xfe
#define SONG_END 0xff

typedef struct
{
// ticks before next note
    uint8_t delay;   
// note index
    uint8_t freq_index;
} song_t;

#define DURATION (HZ / 5)
const song_t song[] =
{
    { DURATION, _G1 },
    { DURATION, _Gb1 },
    { DURATION, _G0 },
    { DURATION, _Db2 },
    { DURATION, _D2 },
    { DURATION, _Gb3 },
    { DURATION, _D3 },
//    { DURATION, SONG_REST },



//     { DURATION, _C0 },
//     { DURATION, _D0 },
//     { DURATION, _E0 },
//     { DURATION, _F0 },
//     { DURATION, _G0 },
//     { DURATION, _A0 },
//     { DURATION, _B0 },
//     { DURATION, _C1 },
//     { DURATION, _D1 },
//     { DURATION, _E1 },
//     { DURATION, _F1 },
//     { DURATION, _G1 },
//     { DURATION, _A1 },
//     { DURATION, _B1 },
//     { DURATION, _C2 },
//     { DURATION, _D2 },
//     { DURATION, _E2 },
//     { DURATION, _F2 },
//     { DURATION, _G2 },
//     { DURATION, _A2 },
//     { DURATION, _B2 },
//     { DURATION, _C3 },
    { 0, SONG_END },
};


#define SET_TIMER1(x) \
    TMR1L = ((x) & 0xff); \
    TMR1H = ((x) >> 8);

#define SET_TIMER0(x) \
    TMR0L = ((x) & 0xff); \
    TMR0H = ((x) >> 8);

typedef union 
{
	struct
	{
		unsigned interrupt_complete : 1;
        unsigned enable_song : 1;
	};
	
	unsigned char value;
} flags_t;

flags_t flags;
uint16_t buzzer_period;
uint8_t song_tick = 0;
uint8_t song_offset = 0;
uint8_t current_delay = 0;

uint16_t sensor_accum = 0;
uint8_t sensor_count = 0;

#define UART_BUFSIZE 64
uint8_t uart_buffer[UART_BUFSIZE];
uint8_t uart_size = 0;
uint8_t uart_position1 = 0;
uint8_t uart_position2 = 0;

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


int main(int argc, char** argv)
{
    OSCCON = 0b01110011;
    flags.value = 0;


// 115200 baud
    SPBRG = CLOCKSPEED / 4 / 115200 - 1;
// page 134
    TXSTA = 0b00100100;
// page 135
    RCSTA = 0b10000000;
// page 136
    BAUDCTL = 0b00001000;

    print_text("\n\n\n\nWelcome to runout sensor\n");

// digital mode = 1
    ADCON1 = 0b11101111;
    ADCON0 = 0b00010001;
    ADCON2 = 0b10111110;
    GO = 1;

// buzzer pins
    SPEAKER0_TRIS = 0;
    SPEAKER1_TRIS = 0;
    SPEAKER0 = 1;
    SPEAKER1 = 0;
    
    SENSOR_TRIS = 0;
    SENSOR_LAT = 1;
    
    LED0_TRIS = 1;
    LED1_TRIS = 1;
    LED0 = 1;
    LED1 = 0;

// mane timer
    T0CON = 0b10000001;
    SET_TIMER0(-TIMER0_PERIOD);
    TMR0IF = 0;
    TMR0IE = 1;

// song state
    song_tick = 0;
    song_offset = 0;
    current_delay = song[song_offset].delay;
    buzzer_period = freqs[song[song_offset].freq_index];

// buzzer timer
    T1CON = 0b10000001;
    TMR1IF = 0;
    TMR1IE = 1;
    SET_TIMER1(buzzer_period);

    PEIE = 1;
    GIE = 1;

    while(tick < HZ / 4)
    {
        ClrWdt();
    }

// print the frequency table
//     uint8_t i;
//     for(i = 0; i < sizeof(freqs) / sizeof(uint16_t); i++)
//     {
//         flush_uart();
//         print_text("i=");
//         print_number(i);
//         print_number(CLOCKSPEED / 4 / (-(freqs[i] - LATENCY)));
//         print_text("\n");
//     }

    while(1)
    {
        ClrWdt();
        handle_uart();
        
        if(ADIF)
        {
            ADIF = 0;
            sensor_accum += ADRES;
            GO = 1;
            
            sensor_count++;
            if(sensor_count >= 64)
            {
                sensor_accum /= 256;
                if(sensor_accum >= THRESHOLD)
                {
                    flags.enable_song = 1;
                    LED0_TRIS = 0;
                    LED1_TRIS = 0;
                }
                else
                {
                    flags.enable_song = 0;
                    LED0_TRIS = 1;
                    LED1_TRIS = 1;
                }
                
                print_number(sensor_accum);
                print_text("\n");
                sensor_accum = 0;
                sensor_count = 0;
//                SENSOR_LAT = !SENSOR_LAT;
            }
        }
    }
}


void __interrupt(low_priority) isr1()
{
}

void __interrupt(high_priority) isr()
{
    flags.interrupt_complete = 0;
	while(!flags.interrupt_complete)
	{
		flags.interrupt_complete = 1;

// mane timer
        if(TMR0IF)
        {
            flags.interrupt_complete = 0;
            TMR0IF = 0;
            SET_TIMER0(-TIMER0_PERIOD);
            tick++;

            if((tick & 0x01))
            {
                LED0 = !LED0;
                LED1 = !LED1;
            }

// next note
            if(flags.enable_song)
            {
                song_tick++;
                if(song_tick >= current_delay)
                {
                    song_offset++;
                    if(song[song_offset].freq_index == SONG_END)
                        song_offset = 0;
                    song_tick = 0;
                    current_delay = song[song_offset].delay;
                    if(song[song_offset].freq_index == SONG_REST)
                    {
                        buzzer_period = _C0;
                        SPEAKER0_TRIS = 1;
                        SPEAKER1_TRIS = 1;
                    }
                    else
                    {
                        buzzer_period = freqs[song[song_offset].freq_index];

                        print_number(-buzzer_period);
                        print_text("\n");

                        SPEAKER0_TRIS = 0;
                        SPEAKER1_TRIS = 0;
                    }
                }
            }
        }
        
        if(TMR1IF)
        {
            flags.interrupt_complete = 0;
            TMR1IF = 0;
            SET_TIMER1(buzzer_period);
            
            if(flags.enable_song)
            {
                SPEAKER0 = !SPEAKER0;
                SPEAKER1 = !SPEAKER1;
            }
        }
    }
}











