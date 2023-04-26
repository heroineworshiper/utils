

// Hydrometer using the Skul AIM B board.
// Draw numbers on 2 LCD's.

// PIC Kit seems to reverse the PGC & PGD pins for the DS PIC.


// FICD
#pragma config ICS = PGD1               // ICD Communication Channel Select bits (Communicate on PGEC1 and PGED1)
#pragma config JTAGEN = OFF             // JTAG Enable bit (JTAG is disabled)

// FPOR
#pragma config BOREN = ON               //  (BOR is enabled)
#pragma config ALTI2C1 = ON             // Alternate I2C1 pins (I2C1 mapped to ASDA1/ASCL1 pins)
#pragma config ALTI2C2 = OFF            // Alternate I2C2 pins (I2C2 mapped to SDA2/SCL2 pins)
#pragma config WDTWIN = WIN25           // Watchdog Window Select bits (WDT Window is 25% of WDT period)

// FWDT
#pragma config WDTPOST = PS32768        // Watchdog Timer Postscaler bits (1:32,768)
#pragma config WDTPRE = PR128           // Watchdog Timer Prescaler bit (1:128)
#pragma config PLLKEN = OFF             // PLL Lock Enable bit (Clock switch will not wait for the PLL lock signal.)
#pragma config WINDIS = OFF             // Watchdog Timer Window Enable bit (Watchdog Timer in Non-Window mode)
#pragma config FWDTEN = ON              // Watchdog Timer Enable bit (Watchdog timer always enabled)

// FOSC
#pragma config POSCMD = HS              // Primary Oscillator Mode Select bits (HS Crystal Oscillator Mode)
#pragma config OSCIOFNC = OFF           // OSC2 Pin Function bit (OSC2 is clock output)
#pragma config IOL1WAY = ON             // Peripheral pin select configuration (Allow only one reconfiguration)
#pragma config FCKSM = CSECMD           // Clock Switching Mode bits (Clock switching is enabled,Fail-safe Clock Monitor is disabled)

// FOSCSEL
#pragma config FNOSC = PRI              // Oscillator Source Selection (Primary Oscillator (XT, HS, EC))
#pragma config PWMLOCK = ON             // PWM Lock Enable bit (Certain PWM registers may only be written after key sequence)
#pragma config IESO = OFF               // Two-speed Oscillator Start-up Enable bit (Start up with user-selected oscillator source)

// FGS
#pragma config GWRP = OFF               // General Segment Write-Protect bit (General Segment may be written)
#pragma config GCP = OFF                // General Segment Code-Protect bit (General Segment Code protect is Disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include "libpic30.h"

// build the master board
#define DO_MASTER


typedef unsigned char   UINT8;
typedef unsigned short  USHORT;
typedef unsigned short  UINT16;
typedef short  INT16;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
typedef unsigned long   UINT32;
typedef signed long     INT32;
typedef unsigned long long ULL;
#define STATIC_STRING static const __eds__ char
#define INTERRUPT_MACRO void __attribute__ ((__interrupt__, __auto_psv__, __shadow__)) 

#define BACKLIGHT_GPIO LATCbits.LATC11
#define RESET_WDT asm("Clrwdt\n");
#define HZ ((INT32)92)
#define CLAMP(x, y, z) ((x) = ((x) < (y) ? (y) : ((x) > (z) ? (z) : (x))))
#define CLIP(x, y, z) ((x) < (y) ? (y) : ((x) > (z) ? (z) : (x)))

#define DISPLAY_H 320
#define DISPLAY_W 240
#define START_CODE 0xe5

// AHT20 stuff
#define I2C_ADDRESS (0x38 << 1)
#define AHTX0_I2CADDR_DEFAULT 0x38   ///< AHT default i2c address
#define AHTX0_CMD_CALIBRATE 0xE1     ///< Calibration command
#define AHTX0_CMD_TRIGGER 0xAC       ///< Trigger reading command
#define AHTX0_CMD_SOFTRESET 0xBA     ///< Soft reset command
#define AHTX0_STATUS_BUSY 0x80       ///< Status bit for busy
#define AHTX0_STATUS_CALIBRATED 0x08 ///< Status bit for calibrated

#define CLK_LAT LATBbits.LATB7
#define DAT_LAT LATCbits.LATC10

#define CLK_PORT PORTBbits.RB7
#define DAT_PORT PORTCbits.RC10

#define CLK_TRIS TRISBbits.TRISB7
#define DAT_TRIS TRISCbits.TRISC10

typedef struct
{
    float h;
    float t;
} aht20_t;
aht20_t sensor;

uint8_t i2c_buffer[8];
#define I2C_OAR1_ADD0 ((uint32_t)0x00000001)
#define I2C_7BIT_ADD_WRITE(__ADDRESS__) ((uint8_t)((__ADDRESS__) & (~I2C_OAR1_ADD0)))
#define I2C_7BIT_ADD_READ(__ADDRESS__) ((uint8_t)((__ADDRESS__) | I2C_OAR1_ADD0))




typedef union 
{
	struct
	{
		unsigned triggered : 1;
        unsigned have_start : 1;
	};
	
	UINT16 value;
} flags_t;

flags_t flags;
volatile UINT8 tick;
volatile UINT8 delay_timer;
// data received
volatile UINT8 uart_in[8];
volatile UINT8 uart_size;



// LED command buffer
static __eds__ UINT8 buffer[32];




// Debugging *******************************************************************

static INT32 number;
static int force;
static __eds__ char *ptr;
#define DEBUG_BUFFER_SIZE 32
static __eds__ char debug_buffer[32];
static char printed_lf = 0;
#define TRACE trace(__FILE__, __FUNCTION__, __LINE__);
STATIC_STRING hex_table[] = 
{
	'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

#ifdef DO_MASTER
#include "font.c"
#else
#include "font2.c"
#endif

extern const __eds__ UINT8 digit0[];
extern const __eds__ UINT8 digit1[];
extern const __eds__ UINT8 digit2[];
extern const __eds__ UINT8 digit3[];
extern const __eds__ UINT8 digit4[];
extern const __eds__ UINT8 digit5[];
extern const __eds__ UINT8 digit6[];
extern const __eds__ UINT8 digit7[];
extern const __eds__ UINT8 digit8[];
extern const __eds__ UINT8 digit9[];
const __eds__ UINT8 *font[] = {
    digit0, 
    digit1, 
    digit2, 
    digit3, 
    digit4, 
    digit5, 
    digit6, 
    digit7, 
    digit8, 
    digit9
}; 

// A print which puts the string argument in flash memory
#define PRINT_TEXT(text) \
{ \
	STATIC_STRING text_[] = text; \
	print_text_eds(text_); \
}


void flush_uart1()
{
	while(!U1STAbits.TRMT) ;
}

// send character with blocking to the console
void send_uart1_char(char c)
{
// wait for room for 1 more character
	while(U1STAbits.UTXBF) ;

	U1TXREG = c;
}

// send characters with blocking
void send_uart1_eds(__eds__ char * text, int size)
{

	int i;
	for(i = 0; i < size; i++)
	{
// wait for room for 1 more character
		send_uart1_char(text[i]);
	}

}

void print_text_eds(__eds__ const char *text)
{
	__eds__ const char *ptr;

	int len = 0;
	for(ptr = text; *ptr != 0; ptr++)
	{
        if(*ptr == 0xa) printed_lf = 1;
		len++;
	}
	send_uart1_eds((__eds__ char*)text, len);
}
void print_digit(__eds__ char *dst, int maxlen, INT32 place)
{
	if(number >= place || force)
	{ 
		force = 1; 
		if(ptr - dst < maxlen - 1) 
		{ 
			*ptr++ = '0' + number / place; 
		} 
		number %= place; 
	}
}

int sprint_number(__eds__ char *dst, INT32 number_arg, int maxlen)
{
	number = number_arg;
	force = 0;
	ptr = dst;

	if(number < 0)
	{
		if(ptr - dst < maxlen - 1)
		{
			*ptr++ = '-';
		}
		number = -number;
	}

	print_digit(dst, maxlen, 10000000);
	print_digit(dst, maxlen, 1000000);
	print_digit(dst, maxlen, 100000);
	print_digit(dst, maxlen, 10000);
	print_digit(dst, maxlen, 1000);
	print_digit(dst, maxlen, 100);
	print_digit(dst, maxlen, 10);
	
	if(ptr - dst < maxlen - 1)
	{
		*ptr++ = '0' + (number % 10);
	}

	*ptr = 0;
	return ptr - dst;
}


int sprint_float(__eds__ char *dst, float number_arg, int maxlen)
{
	int len = 0;

	if(number_arg < 0)
	{
		if(len < maxlen - 1)
		{
			dst[len++] = '-';
		}
		number_arg = -number_arg;
	}
	
	INT32 int_part = (INT32)number_arg;
	float frac_part = number_arg - int_part;


	len += sprint_number(dst + len, int_part, maxlen);
	dst[len++] = '.';
	dst[len++] = '0' + (((INT32)(frac_part * (INT32)10)) % 10);
	dst[len++] = '0' + (((INT32)(frac_part * (INT32)100)) % 10);
	dst[len++] = '0' + (((INT32)(frac_part * (INT32)1000)) % 10);
	dst[len++] = '0' + (((INT32)(frac_part * (INT32)10000)) % 10);
	dst[len++] = '0' + (((INT32)(frac_part * (INT32)100000)) % 10);
	dst[len++] = '0' + (((INT32)(frac_part * (INT32)1000000)) % 10);
	dst[len] = 0;
	return len;
}
void print_number_nospace(INT32 number)
{
	int len;
	len = sprint_number(debug_buffer, number, DEBUG_BUFFER_SIZE);
	send_uart1_eds(debug_buffer, len);
}

void print_number(INT32  number)
{
	print_number_nospace(number);
	send_uart1_char(' ');
}

void print_float(float number_arg)
{
	int len;
	len = sprint_float(debug_buffer, number_arg, DEBUG_BUFFER_SIZE);
	send_uart1_eds(debug_buffer, len);
	send_uart1_char(' ');
}

void print_lf()
{
	send_uart1_char('\r');
	send_uart1_char('\n');
	printed_lf = 1;
}


void print_bin16(UINT16 number)
{
	send_uart1_char(number & (1 << 15) ? '1' : '0');
	send_uart1_char(number & (1 << 14) ? '1' : '0');
	send_uart1_char(number & (1 << 13) ? '1' : '0');
	send_uart1_char(number & (1 << 12) ? '1' : '0');
	send_uart1_char(number & (1 << 11) ? '1' : '0');
	send_uart1_char(number & (1 << 10) ? '1' : '0');
	send_uart1_char(number & (1 << 9) ? '1' : '0');
	send_uart1_char(number & (1 << 8) ? '1' : '0');
	send_uart1_char(number & (1 << 7) ? '1' : '0');
	send_uart1_char(number & (1 << 6) ? '1' : '0');
	send_uart1_char(number & (1 << 5) ? '1' : '0');
	send_uart1_char(number & (1 << 4) ? '1' : '0');
	send_uart1_char(number & (1 << 3) ? '1' : '0');
	send_uart1_char(number & (1 << 2) ? '1' : '0');
	send_uart1_char(number & (1 << 1) ? '1' : '0');
	send_uart1_char(number & (1 << 0) ? '1' : '0');
    send_uart1_char(' ');
}



void print_hex2(unsigned char number)
{
	send_uart1_char(hex_table[(number >> 4) & 0xf]);
	send_uart1_char(hex_table[number & 0xf]);
    send_uart1_char(' ');
}



void print_hex4(UINT16 number)
{
	send_uart1_char(hex_table[(number >> 12) & 0xf]);
	send_uart1_char(hex_table[(number >> 8) & 0xf]);
	send_uart1_char(hex_table[(number >> 4) & 0xf]);
	send_uart1_char(hex_table[number & 0xf]);
	send_uart1_char(' ');
}

void trace(__eds__ const char *file, __eds__ const char *function, int line)
{
	STATIC_STRING string1[] = ": ";
	STATIC_STRING string2[] = " ";
	STATIC_STRING string3[] = ": ";

// print a linefeed if none printed
	if(!printed_lf)
	{
		print_lf();
	}
	
	print_text_eds(file);

	print_text_eds(string1);
	print_text_eds(function);
	print_text_eds(string2);
	print_number_nospace(line);
	print_text_eds(string3);


// reset the flag
	printed_lf = 0;
    flush_uart1();
}



// Mane timer ******************************************************************

void DLY1000()
{
	asm(
        "    BCLR IFS0, #7\n"
        "    CLR  TMR2     ;TIMER 2 CLOCK = 20nS\n"
        "    MOV  #49980, W0\n"
        "    MOV  W0, PR2\n"
        "loop:  BTSS IFS0,	#7\n"
        "    BRA loop\n"
    );
}


void DLY100()
{
	asm(
        "    BCLR IFS0, #7\n"
        "    CLR  TMR2     ;TIMER 2 CLOCK = 20nS\n"
        "    MOV  #4998, W0\n"
        "    MOV  W0, PR2\n"
        "loop2:  BTSS IFS0,	#7\n"
        "    BRA loop2\n"
    );
}


// delay in milliseconds
void mdelay(UINT8 value)
{
    delay_timer = 0;
    while(delay_timer < value)
    {
    }
}




void setup_pic()
{
	ANSELA = 0x0200;
	ANSELB = 0x0000;
	ANSELC = 0x0000;
	ANSELD = 0x0000;
	ANSELE = 0x0000;
	ANSELF = 0x0000;
	ANSELG = 0x0000;
	LATA = 0x0000;
	LATB = 0x0000;
	LATC = 0x0000;
	LATD = 0x0000;
	LATE = 0x0000;
	LATF = 0x0000;
	LATG = 0x0000;
    TRISA = 0x0683;
    TRISB = 0x0087;
    TRISC = 0x3400;
    TRISD = 0x0020;
    TRISE = 0x0000;
    TRISF = 0x0001;
    TRISG = 0x01C0;
// backlight on
    BACKLIGHT_GPIO = 0;

// reset LCD
	LATEbits.LATE15 = 0;
// LCD data/command
	LATEbits.LATE12 = 0;
// W clock for LCD
	LATEbits.LATE13 = 1;
// R clock for LCD
	LATEbits.LATE14 = 1;


// pin mapping
// UART1 RX
    RPINR18 = 0x0060;
// UART1 TX
    RPOR9 = 0x0100;

// oscillator
    CLKDIV = 0x3002;
    PLLFBD = 0x0030;

// Parallel Master Data Port
    PMD1 = 0x0700;
    PMD2 = 0xFFFF;
    PMD3 = 0x03F6;
    PMD4 = 0x000C;
    PMD6 = 0x3F00;
    PMD7 = 0x0018;

//  UART 1: debug
    U1MODE = 0x8008;
// 19200 baud
//    U1BRG = 649;
// 115200 baud
    U1BRG = 107;
    U1STA = 0x1400;

#ifndef DO_MASTER
    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;
#endif // !DO_MASTER

// oscillator clock switching requires single byte writes
	asm(
		"MOV		#0x03,			W0\n" 
// OSCCONH (high byte) Unlock Sequence
		"MOV		#OSCCONH,		W1\n"
		"MOV		#0x78,			W2\n"
		"MOV		#0x9A,			W3\n"
		"MOV.B	W2,				[W1]\n" // WRITE 0x78
		"MOV.B	W3,				[W1]\n"	// WRITE 0x9A
// Set New Oscillator Selection
		"MOV.B	W0,				[W1]\n" // WRITE NEW OSCILLATOR SELECTION (PLL+HS)
// Place 0x01 in W0 for setting clock switch enabled bit
		"MOV		#0x01,			W0\n"
// OSCCONL (low byte) Unlock Sequence
		"MOV		#OSCCONL,		W1\n"
		"MOV		#0x46,			W2\n"
		"MOV		#0x57,			W3\n"
		"MOV.B	W2,				[W1]\n" // WRITE 0x46
		"MOV.B	W3,				[W1]\n" // WRITE 0x57
// CLOCK SWITCHING
		"MOV.B	W0,				[W1]\n" // SET OSWEN BIT "OSCCON,0" (PERFORM CLOCK SWITCHING)
	);

// wait for stable oscillator
	while(!OSCCONbits.LOCK) 
	{
		;
	}

// DSP engine
	CORCON = 0x0C01;

// Millisecond timer for synchronous DLY & SPI routines
	T2CON = 0xA000;
	IFS0bits.T2IF = 0;

// Tick counter for asynchronous timeouts
    T4CON = 0xA010; // clear config
	IEC1bits.T4IE = 1;



// enable interrupts
    INTCON2bits.GIE = 1;
}




// LCD driver ******************************************************************
// LCD write CMD's
#define LCD_CMD_EXIT_SLEEP 0x11
#define LCD_GAMMA_PRESET 0x26
#define LCD_CMD_DISPLAY_OFF 0x28
#define LCD_CMD_DISPLAY_ON 0x29
#define LCD_CMD_PWR_CTRL1 0xC0
#define LCD_CMD_PWR_CTRL2 0xC1
#define LCD_CMD_VCOM_CTRL1 0xC5
#define LCD_CMD_VCOM_CTRL2 0xC7
#define LCD_CMD_FRAME_RATE 0xB1
#define LCD_CMD_DISP_FUNC 0xB6
#define LCD_CMD_GAMMA_POS 0xE0
#define LCD_CMD_GAMMA_NEG 0xE1
#define LCD_CMD_COL_ADDY 0x2A
#define LCD_CMD_ROW_ADDY 0x2B
#define LCD_CMD_WRITE_GRAM 0x2C
#define LCD_CMD_SCROLLING 0x33
#define LCD_MADCTL 0x36
#define LCD_CMD_PIXEL 0x3A

// LCD read commands
#define LCD_VERSION 0x04
#define LCD_STATUS 0x09
#define LCD_ID1 0xda
#define LCD_ID2 0xdb
#define LCD_ID3 0xdc

#define SELECT_DATA \
	LATEbits.LATE12 = 1;

#define SELECT_CTL \
	LATEbits.LATE12 = 0;

#define WRITE_PULSE \
	LATEbits.LATE13 = 0; \
	asm("nop"); \
	LATEbits.LATE13 = 1;

#define READ_PULSE \
	LATEbits.LATE14 = 0; \
	asm("nop"); \
	LATEbits.LATE14 = 1;



// convert RGB to 16 bit little endian
#define COLOR_TO_PIXEL(r, g, b) \
	(((UINT16)((r) & 0xf8)) | \
		((UINT16)((g) & 0xe0) >> 5) | \
		((UINT16)((g) & 0x1c) << 11) | \
		((UINT16)((b) & 0xf8) << 5))

UINT16 current_pixel;
UINT16 fg;
UINT16 bg;
UINT16 VAL16;
UINT8 double_size = 0;
UINT8 VAL8;

// buffer a single line
__eds__ UINT16 line_buffer[DISPLAY_W];

// must write to the low byte of LATC, which is
// easiest to do in assembly
void lcd_cmd(unsigned char cmd)
{
    SELECT_CTL
	asm(
		"MOV.B WREG, LATC\n"  // write to low byte only
	);
	WRITE_PULSE
    SELECT_DATA
}

void lcd_read_mode()
{
	TRISCbits.TRISC0 = 1;
	TRISCbits.TRISC1 = 1;
	TRISCbits.TRISC2 = 1;
	TRISCbits.TRISC3 = 1;
	TRISCbits.TRISC4 = 1;
	TRISCbits.TRISC5 = 1;
	TRISCbits.TRISC6 = 1;
	TRISCbits.TRISC7 = 1;
}

void lcd_write_mode()
{
	TRISCbits.TRISC0 = 0;
	TRISCbits.TRISC1 = 0;
	TRISCbits.TRISC2 = 0;
	TRISCbits.TRISC3 = 0;
	TRISCbits.TRISC4 = 0;
	TRISCbits.TRISC5 = 0;
	TRISCbits.TRISC6 = 0;
	TRISCbits.TRISC7 = 0;
}

// ST7789S.pdf page 50
void lcd_write_byte(unsigned char data)
{
// easiest to write to low byte in assembly
	asm("MOV.B WREG, LATC\n");
	WRITE_PULSE
}

// ST7789S.pdf page 51
UINT8 lcd_read_byte()
{
	LATEbits.LATE14 = 0;
	UINT8 result = PORTC & 0xff;
	LATEbits.LATE14 = 1;
	return result;
}

void lcd_read_data(__eds__ UINT8 *data, int bytes)
{
	lcd_read_mode();
// 1st byte is always a dummy
	lcd_read_byte();
	int i;
	for(i = 0; i < bytes; i++)
	{
		data[i] = lcd_read_byte();
	}
	lcd_write_mode();
}

void lcd_write_data(const __eds__ char * data_ptr, int bytes)
{
	int i;
    for(i = 0; i < bytes; i++)
	{
        lcd_write_byte((UINT8)data_ptr[i]);
	}
}


void setup_lcd()
{
// RESET SIGNAL
    LATEbits.LATE15 = 0;
// 1ms
    DLY1000();
    LATEbits.LATE15 = 1;
// 1ms
    DLY1000();

    fg = COLOR_TO_PIXEL(0xff, 0xff, 0xff);
    bg = COLOR_TO_PIXEL(0, 0, 0);

	lcd_cmd(LCD_VERSION);
	lcd_read_data(buffer, 3);
// 240320CF ILI9341 controller
	if(buffer[2] == 0x85)
	{
        PRINT_TEXT("Got a 240320CF ILI9341\n");
    	lcd_cmd(LCD_CMD_EXIT_SLEEP);
    	mdelay(6);
    	lcd_cmd(LCD_CMD_DISPLAY_OFF);


    	lcd_cmd(LCD_CMD_PWR_CTRL1);
    	lcd_write_byte(0x26);
    	lcd_write_byte(0x04);


    	lcd_cmd(LCD_CMD_PWR_CTRL2);
    	lcd_write_byte(0x04);


    	lcd_cmd(LCD_CMD_VCOM_CTRL1);
    	lcd_write_byte(0x34);
    	lcd_write_byte(0x40);


    	lcd_cmd(LCD_CMD_FRAME_RATE);
    	lcd_write_byte(0x00);
    	lcd_write_byte(0x18);


    	lcd_cmd(LCD_CMD_DISP_FUNC);
    	STATIC_STRING disp_func_data[] = { 0x0A, 0xA2, 0x27, 0x00 };
    	lcd_write_data(disp_func_data, sizeof(disp_func_data));

    	lcd_cmd(LCD_MADCTL);
    	lcd_write_byte(0x60);

    	lcd_cmd(LCD_CMD_VCOM_CTRL2);
    	lcd_write_byte(0xC0);

    	lcd_cmd(LCD_CMD_PIXEL);
    	lcd_write_byte(0x55);//16 bit

// page 107
		lcd_cmd(LCD_GAMMA_PRESET);
		lcd_write_byte(0x02);

/*
 *     	lcd_cmd(LCD_CMD_GAMMA_POS);
 *     	STATIC_STRING gamma_pos_data[] = 
 * 		{
 * 			0x1F,0x1B,0x18,0x0B,0x0F,0x09,0x46,0xB5,
 * 			0x37,0x0A,0x0C,0x07,0x07,0x05,0x00,
 * 		};
 *     	lcd_write_data(gamma_pos_data, sizeof(gamma_pos_data));
 * 
 * 		lcd_cmd(LCD_CMD_GAMMA_NEG);
 *     	STATIC_STRING gamma_neg_data[] = 
 * 		{
 * 			0x00,0x24,0x27,0x04,0x10,0x06,0x39,0x74,
 * 			0x48,0x05,0x13,0x38,0x38,0x3A,0x1F
 * 		};
 *     	lcd_write_data(gamma_neg_data, sizeof(gamma_neg_data));
 */

    	lcd_cmd(LCD_CMD_COL_ADDY);	
    	STATIC_STRING col_addy_data[] = { 0x00,0x00,0x00,0xEF };
    	lcd_write_data(col_addy_data, sizeof(col_addy_data));

    	lcd_cmd(LCD_CMD_ROW_ADDY);
    	STATIC_STRING row_addy_data[] = { 0x00,0x00,0x01,0x3F };
    	lcd_write_data(row_addy_data, sizeof(row_addy_data));


    	lcd_cmd(LCD_CMD_DISPLAY_ON);
    	mdelay(24);

    	//;COLOR IS 16 BITS -----> R R R R R G G G G G G B B B B B
    	lcd_cmd(LCD_CMD_WRITE_GRAM);

    	lcd_cmd(LCD_CMD_SCROLLING);
    	STATIC_STRING scrolling_data[] = 
		{
	// TOP AREA in big endian
			0x00, 0x28, 
	// NUMBER OF SCROLLING LINES in big endian
			0x01, 0x18, 
	// BOTTOM AREA in big endian
			0x00, 0x00 
		};
    	lcd_write_data(scrolling_data, sizeof(scrolling_data));
	}
	else
// 240320SF ST7789S controller
	{
        PRINT_TEXT("Got a 240320SF ST7789S\n");
    	lcd_cmd(LCD_CMD_EXIT_SLEEP);
    	mdelay(6);
    	lcd_cmd(LCD_CMD_DISPLAY_OFF);


    	lcd_cmd(LCD_CMD_PWR_CTRL1);
    	lcd_write_byte(0x26);
    	lcd_write_byte(0x04);


    	lcd_cmd(LCD_CMD_PWR_CTRL2);
    	lcd_write_byte(0x04);


    	lcd_cmd(LCD_CMD_VCOM_CTRL1);
    	lcd_write_byte(0x34);
    	lcd_write_byte(0x40);


    	lcd_cmd(LCD_CMD_FRAME_RATE);
    	lcd_write_byte(0x00);
    	lcd_write_byte(0x18);


    	lcd_cmd(LCD_CMD_DISP_FUNC);
    	STATIC_STRING disp_func_data[] = { 0x0A,0xA2,0x27,0x00 };
    	lcd_write_data(disp_func_data, sizeof(disp_func_data));

    	lcd_cmd(LCD_MADCTL);
    	lcd_write_byte(0x08);

    	lcd_cmd(LCD_CMD_VCOM_CTRL2);
    	lcd_write_byte(0xC0);

    	lcd_cmd(LCD_CMD_PIXEL);
    	lcd_write_byte(0x55);//16 bit


    	lcd_cmd(LCD_CMD_GAMMA_POS);
    	STATIC_STRING gamma_pos_data[] = 
		{
			0x1F,0x1B,0x18,0x0B,0x0F,0x09,0x46,0xB5,
			0x37,0x0A,0x0C,0x07,0x07,0x05,0x00,
		};
    	lcd_write_data(gamma_pos_data, sizeof(gamma_pos_data));

		lcd_cmd(LCD_CMD_GAMMA_NEG);
    	STATIC_STRING gamma_neg_data[] = 
		{
			0x00,0x24,0x27,0x04,0x10,0x06,0x39,0x74,
			0x48,0x05,0x13,0x38,0x38,0x3A,0x1F
		};
    	lcd_write_data(gamma_neg_data, sizeof(gamma_neg_data));

    	lcd_cmd(LCD_CMD_COL_ADDY);	
    	STATIC_STRING col_addy_data[] = { 0x00,0x00,0x00,0xEF };
    	lcd_write_data(col_addy_data, sizeof(col_addy_data));

    	lcd_cmd(LCD_CMD_ROW_ADDY);
    	STATIC_STRING row_addy_data[] = { 0x00,0x00,0x01,0x3F };
    	lcd_write_data(row_addy_data, sizeof(row_addy_data));


    	lcd_cmd(LCD_CMD_DISPLAY_ON);
    	mdelay(24);

    	//;COLOR IS 16 BITS -----> R R R R R G G G G G G B B B B B
    	lcd_cmd(LCD_CMD_WRITE_GRAM);

    	lcd_cmd(LCD_CMD_SCROLLING);
    	STATIC_STRING scrolling_data[] = 
		{
	// TOP AREA in big endian
			0x00, 0x28, 
	// NUMBER OF SCROLLING LINES in big endian
			0x01, 0x18, 
	// BOTTOM AREA in big endian
			0x00, 0x00 
		};
    	lcd_write_data(scrolling_data, sizeof(scrolling_data));
	}
}




#define WRITE_CTL(x) \
    VAL16 = x; \
    asm( \
        "    MOV    _VAL16, W0\n" \
        "    BCLR	LATE,  #12   ;Set LCD Data/Control line to Control\n" \
        "    MOV.B	WREG,  LATC  ;Put LCD command on data lines\n" \
        "    BCLR	LATE,  #13   ;Set LCD Write Strobe low\n" \
        "    NOP\n" \
        "    BSET	LATE,  #13   ;Set LCD Write Strobe high\n" \
    ); \

#define WRITE_DATA(x) \
    VAL16 = x; \
    asm( \
        "    MOV    _VAL16, W0\n" \
        "    BSET	LATE,       #12         ;Set LCD Data/Control line to Data\n" \
		"	 SWAP	W0,                     ;Write high byte first\n" \
        "    MOV.B	WREG,       LATC       ;Put LCD data on data lines\n" \
        "    BCLR	LATE,       #13         ;Set LCD Write Strobe low\n" \
        "    NOP\n" \
        "    BSET	LATE,       #13         ;Set LCD Write Strobe high\n" \
        "    SWAP	W0,                     ;Now write low byte\n" \
        "    MOV.B	WREG,       LATC       ;Put LCD data on data lines\n" \
        "    BCLR	LATE,       #13         ;Set LCD Write Strobe low\n" \
        "    NOP\n" \
        "    BSET	LATE,       #13         ;Set LCD Write Strobe high\n" \
    );

void draw_pixel()
{
	asm(
		"MOV _current_pixel, W0\n"             // write low byte 1st
        "MOV.B		WREG,       LATC   \n"    //Put LCD data on data lines    
        "BCLR		LATE,       #13    \n"     //Set LCD Write Strobe low     
//        "NOP                          \n"                                     
        "BSET		LATE,       #13    \n"     //Set LCD Write Strobe high    
        "SWAP		W0,                 \n"    //Now write high byte           
        "MOV.B		WREG,       LATC   \n"    //Put LCD data on data lines    
        "BCLR		LATE,       #13    \n"     //Set LCD Write Strobe low     
//        "NOP                          \n"                                     
        "BSET		LATE,       #13     \n"    //Set LCD Write Strobe high    
	);
}

void set_window(int col0, int col1, int row0)
{
// set_window_asm
// column start/end command
    WRITE_CTL(0x2a);
    WRITE_DATA(col0);
    WRITE_DATA(col1);
// row start/end command
    WRITE_CTL(0x2b);
    WRITE_DATA(row0);
    WRITE_DATA(DISPLAY_H);
// write graphics RAM command
    WRITE_CTL(0x2c);
}

// Equivalent in speed to ASM
void clear_screen()
{
	set_window(0, DISPLAY_W - 1, 0);

	int i, j;
	current_pixel = bg;
	double_size = 0;
// Set LCD Data/Control line to Data
	SELECT_DATA
	for(i = 0; i < DISPLAY_H; i++)
	{
		for(j = 0; j < DISPLAY_W; j++)
		{
			draw_pixel();
		}
	}
}

// funky flash addressing on the DSPIC
// current address being read from flash in absolute bytes
static UINT32 flash_address;
// which byte of the temp are we reading
static UINT8 flash_counter;
// destination for reading from flash
static UINT8 flash_temp[4];
// convert virtual address to a flash address
// TODO: it's empirically derived.
#define VIRTUAL_TO_FLASH(x) (((UINT32)(x) & 0xff0000L) + \
	(((UINT32)(x) << 1) & 0xffffL))


void start_flash_read(UINT32 flash_address_)
{
	flash_address = flash_address_;
	flash_counter = 3;
}

// read a byte from flash
UINT8 read_flash_byte()
{
// time to read another 3 bytes
	if(flash_counter >= 3)
	{
		_memcpy_p2d24(flash_temp, flash_address / 2, 3);
		flash_address += 4;
		flash_counter = 0;
	}
	return flash_temp[flash_counter++];
}

// draw 0-9 using the current fg
void draw_digit(int n)
{
    const __eds__ UINT8 *src = font[n];
    UINT32 total = 0;
    UINT8 i;

    start_flash_read(VIRTUAL_TO_FLASH(src));


    while(total < (UINT32)DISPLAY_H * DISPLAY_W)
    {
        UINT8 code = read_flash_byte();
        UINT8 length = (code & 0x7f) + 1;

// print_hex2(code);
// print_number((code & 0x80));
// print_number(length);
// print_lf();

        if((code & 0x80))
        {
            current_pixel = fg;
        }
        else
        {
            current_pixel = bg;
        }

        for(i = 0; i < length; i++)
        {
            draw_pixel();
        }
        
        total += length;
    }
}

// mane HZ timer
INTERRUPT_MACRO _T4Interrupt()
{
	IFS1bits.T4IF = 0;

    tick++;
    delay_timer++;
//    BACKLIGHT_GPIO = !BACKLIGHT_GPIO;
}

#ifndef DO_MASTER
INTERRUPT_MACRO _U1RXInterrupt()
{
    if(uart_size < sizeof(uart_in))
    {
        uart_in[uart_size++] = U1RXREG;
    }
    else
    {
        UINT8 c = U1RXREG;
    }

 	U1STAbits.URXDA = 0;
 	U1STAbits.OERR = 0;
 	IFS0bits.U1RXIF = 0;
}
#endif // !DO_MASTER


void hsv_to_rgb(UINT8 *r, UINT8 *g, UINT8 *b, float h, float s, float v)
{
    int i;
	float f, p, q, t;
    float r_f, g_f, b_f;
    if(s == 0) 
	{
        // achromatic (grey)
        r_f = g_f = b_f = v * 0xff;
        *r = CLIP(r_f, 0, 0xff);
        *g = CLIP(g_f, 0, 0xff);
        *b = CLIP(b_f, 0, 0xff);
        return;
    }

    h /= 60;                        // sector 0 to 5
    i = (int)h;
    f = h - i;                      // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch(i) 
	{
        case 0:
            r_f = v * 0xff;
            g_f = t * 0xff;
            b_f = p * 0xff;
            break;
        case 1:
            r_f = q * 0xff;
            g_f = v * 0xff;
            b_f = p * 0xff;
            break;
        case 2:
            r_f = p * 0xff;
            g_f = v * 0xff;
            b_f = t * 0xff;
            break;
        case 3:
            r_f = p * 0xff;
            g_f = q * 0xff;
            b_f = v * 0xff;
            break;
        case 4:
            r_f = t * 0xff;
            g_f = p * 0xff;
            b_f = v * 0xff;
            break;
        default:                // case 5:
            r_f = v * 0xff;
            g_f = p * 0xff;
            b_f = q * 0xff;
            break;
    }
    
    *r = CLIP(r_f, 0, 0xff);
    *g = CLIP(g_f, 0, 0xff);
    *b = CLIP(b_f, 0, 0xff);
    
	return;
}

#ifdef DO_MASTER
#define I2C_DELAY DLY100();

void set_data(uint8_t value)
{
    DAT_TRIS = value;
    I2C_DELAY
}

void set_clock(uint8_t value)
{
    CLK_TRIS = value;
    I2C_DELAY
}

uint8_t get_data()
{
    return DAT_PORT;
}

uint8_t get_clock()
{
    return CLK_PORT;
}

void i2c_write(uint8_t value)
{
    uint8_t i = 0;
    for(i = 0; i < 8; i++)
    {
        if((value & 0x80))
        {
            set_data(1);
        }
        else
        {
            set_data(0);
        }
        set_clock(1);
        set_clock(0);
        value <<= 1;
    }

// read ACK
    set_clock(1);
// wait for clock to rise
    while(!get_clock())
    {
        ;
    }
    uint8_t ack = get_data();
    set_clock(0);
}

void i2c_read(uint8_t bytes)
{
    uint8_t i, j;
    for(i = 0; i < bytes; i++)
    {
        uint8_t value = 0;

/* data must rise before clock to read the byte */
        set_data(1);

        for(j = 0; j < 8; j++)
        {
            value <<= 1;
            set_clock(1);
            while(!get_clock())
            {
            }
            
            value |= get_data();
            set_clock(0);
        }
        
        i2c_buffer[i] = value;
        
// write ACK
        if(i >= bytes - 1)
        {
            set_data(1);
        }
        else
        {
            set_data(0);
        }

// pulse clock
        set_clock(1);
        set_clock(0);
    }
}

void i2c_start()
{
	set_clock(1);
	set_data(1);
    set_data(0); 
	set_clock(0);
}

void i2c_stop()
{
    set_data(0);
    set_clock(1);
    set_data(1);
}



void i2c_read_device(unsigned char reg, uint8_t bytes)
{
	uint8_t i;
	for(i = 0; i < bytes; i++)
	{
		i2c_buffer[i] = 0xff;
	}

    i2c_start();
// write device address & reg
    i2c_write(I2C_7BIT_ADD_WRITE(I2C_ADDRESS));
    i2c_write(reg);

    i2c_start();
    i2c_write(I2C_7BIT_ADD_READ(I2C_ADDRESS));
    i2c_read(bytes);
    i2c_stop();
}

void i2c_read_device2(uint8_t bytes)
{
	uint8_t i;
	for(i = 0; i < bytes; i++)
	{
		i2c_buffer[i] = 0xff;
	}

    i2c_start();

    i2c_write(I2C_7BIT_ADD_READ(I2C_ADDRESS));
    i2c_read(bytes);

    i2c_stop();
}


void i2c_write_device(unsigned char reg, unsigned char value)
{
// start
    i2c_start();

// write device address
    i2c_write(I2C_7BIT_ADD_WRITE(I2C_ADDRESS));
    i2c_write(reg);
    i2c_write(value);
    
    i2c_stop();
}

void i2c_write_device2(uint8_t len)
{
// start
    i2c_start();

// write device address
    i2c_write(I2C_7BIT_ADD_WRITE(I2C_ADDRESS));
    uint8_t i;
    for(i = 0; i < len; i++)
    {
        i2c_write(i2c_buffer[i]);
    }
    
    i2c_stop();
}

uint8_t aht20_status()
{
    i2c_read_device2(1);
// TRACE
// print_number(i2c_buffer[0]);
// print_lf();
// flush_uart();
    return i2c_buffer[0];
}


void read_aht20(aht20_t *ptr)
{
    i2c_buffer[0] = AHTX0_CMD_TRIGGER;
    i2c_buffer[1] = 0x33;
    i2c_buffer[2] = 0x00;
    i2c_write_device2(3);

// poll just the status
    while(aht20_status() & AHTX0_STATUS_BUSY)
    {
        ;
    }

// read status + 5 bytes of data without the CRC
    i2c_read_device2(6);
    int32_t x = i2c_buffer[1];
    x <<= 8;
    x |= i2c_buffer[2];
    x <<= 4;
    x |= i2c_buffer[3] >> 4;
    ptr->h = (float)(x * 100) / 0x100000;

    x = i2c_buffer[3] & 0x0f;
    x <<= 8;
    x |= i2c_buffer[4];
    x <<= 8;
    x |= i2c_buffer[5];
    ptr->t = (float)(x * 200) / 0x100000 - 50;
}

void init_aht20(aht20_t *ptr)
{
    i2c_buffer[0] = AHTX0_CMD_SOFTRESET;
    i2c_write_device2(1);

    i2c_buffer[0] = AHTX0_CMD_CALIBRATE;
    i2c_buffer[1] = 0x08;
    i2c_buffer[2] = 0x00;
    i2c_write_device2(3);

    while(aht20_status() & AHTX0_STATUS_BUSY)
    {
        ;
    }
}
#endif // DO_MASTER


int main()
{
    setup_pic();


    PRINT_TEXT("\n\n\n\nWelcome to hydrometer\n");
    setup_lcd();
    clear_screen();
#ifdef DO_MASTER
    PRINT_TEXT("Initializing sensor\n");
    init_aht20(&sensor);
    PRINT_TEXT("Mane loop\n");
#endif // DO_MASTER

// 	COLOR_TO_PIXEL(current_pixel, 255, 0, 0);
//     for(i = 0; i < 100; i++)
//     {
//         draw_pixel();
//     }


    float h1 = 0;
    float h2 = 180;
    UINT8 humidity = 0;
    while(1)
    {
		RESET_WDT

#ifdef DO_MASTER
// read sensor
        if(tick >= HZ)
        {
            tick = 0;
            read_aht20(&sensor);
//             PRINT_TEXT("TEMP=");
//             print_float(sensor.t);
//             PRINT_TEXT("HUMIDITY=");
//             print_float(sensor.h);
//             print_lf();
            humidity = (UINT8)sensor.h;

// send the code to the slave
            debug_buffer[0] = START_CODE;
            debug_buffer[1] = humidity;
            send_uart1_eds(debug_buffer, 2);
        }
#else // DO_MASTER

        IEC0bits.U1RXIE = 0;
        UINT8 i;
        if(uart_size > 0)
        {
//            PRINT_TEXT("uart_size=");
//            print_number(uart_size);
//            print_lf();
            for(i = 0; i < uart_size; i++)
            {
                if(flags.have_start)
                {
                    humidity = uart_in[i];
//                    PRINT_TEXT("HUMIDITY=");
//                    print_number(humidity);
//                    print_lf();
                    flags.have_start = 0;
                }
                else
                if(uart_in[i] == START_CODE)
                {
                    flags.have_start = 1;
                }
            }
            uart_size = 0;
        }
        IEC0bits.U1RXIE = 1;

#endif // !DO_MASTER

        UINT8 r1, g1, b1;
        hsv_to_rgb(&r1, &g1, &b1, h1, 1.0, 1.0);
        h1 += 5;
        if(h1 > 360)
        {
            h1 -= 360;
        }
        UINT8 r2, g2, b2;
        hsv_to_rgb(&r2, &g2, &b2, h2, 1.0, 1.0);
        h2 += 5;
        if(h2 > 360)
        {
            h2 -= 360;
        }

        fg = COLOR_TO_PIXEL(r1, g1, b1);
        bg = COLOR_TO_PIXEL(r2, g2, b2);
#ifdef DO_MASTER
        draw_digit(humidity / 10);
#else
        draw_digit(humidity % 10);
#endif
    }

    return 0;
}






