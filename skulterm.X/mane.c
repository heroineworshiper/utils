

// Terminal output using the Skul AIM B board.

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


typedef unsigned char   UINT8;
typedef unsigned short  USHORT;
typedef unsigned short  UINT16;
typedef short  INT16;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
typedef unsigned long   UINT32;
typedef signed long     INT32;
typedef unsigned long long ULL;

#include "font.c"

extern const __eds__ UINT8 font[];

#define STATIC_STRING static const __eds__ char
#define INTERRUPT_MACRO void __attribute__ ((__interrupt__, __auto_psv__, __shadow__)) 

#define BACKLIGHT_GPIO LATCbits.LATC11
#define RESET_WDT asm("Clrwdt\n");
#define HZ ((INT32)92)

#define DISPLAY_H 320
#define DISPLAY_W 240
#define TEXT_W 40
#define TEXT_H 30

typedef union 
{
	struct
	{
	};
	
	UINT16 value;
} flags_t;

volatile flags_t flags;
volatile UINT8 delay_timer;


//#define PORT1_COLOR COLOR_TO_PIXEL(0, 0x88, 0xff)
#define PORT1_COLOR COLOR_TO_PIXEL(0xff, 0xff, 0xff)
#define PORT2_COLOR COLOR_TO_PIXEL(0xaa, 0xff, 0x66)
//#define BG COLOR_TO_PIXEL(0, 0, 0xaa)
#define BG COLOR_TO_PIXEL(0, 0, 0)


// LCD command buffer
static __eds__ UINT8 buffer[32];

// RAM copy of the font
// starting ASCII character
#define FONT_START 33
// total characters
#define FONT_SIZE 94
#define GLYPH_W 8
#define GLYPH_H 8
static __eds__ UINT8 font_ram[FONT_SIZE * GLYPH_H];


// how many serial ports to read
//#define PORTS 2
#define PORTS 1



// port FIFOs
#define PORT_BUFSIZE 1024
static __eds__ UINT8 port_buffers[PORTS][PORT_BUFSIZE];
static UINT16 port_bufsize[PORTS];
static UINT16 port_in[PORTS];
static UINT16 port_out[PORTS];
static UINT8 esc_code[PORTS][8];
static UINT8 esc_size[PORTS];

// screen RAM
static __eds__ UINT8 screen[TEXT_W * TEXT_H];
// starting row of the 2nd serial port
#define PORT2_ROW (TEXT_H / PORTS)
// rows per port
#define PORT_ROWS (TEXT_H / PORTS)
// position of the 2 serial port cursors
// 0 - PORT_ROWS
UINT8 cursor_x[PORTS];
UINT8 cursor_y[PORTS];

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
	ANSELA = 0x0203;
	ANSELB = 0x0083;
	ANSELC = 0x0400;
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


// pin mapping
    RPINR19 = 0x22; // UART2 RX
    RPINR27 = 0x4577; // UART3 RX

// pin mapping
// UART1 RX
    RPINR18 = 0x0060;
// UART1 TX
    RPOR9 = 0x0100;

// capture the uarts
    U3MODE = 0x8008;
    U3BRG = 107;
    U3STA = 0x1400;
    U2MODE = 0x8008;
    U2BRG = 107;
    U2STA = 0x1400;

//  UART 1: debug
    U1MODE = 0x8008;
// 19200 baud
//    U1BRG = 649;
// 115200 baud
    U1BRG = 107;
    U1STA = 0x1400;

    IFS1bits.U2RXIF = 0;
    IEC1bits.U2RXIE = 1;
    IFS5bits.U3RXIF = 0;
    IEC5bits.U3RXIE = 1;
    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;



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
UINT16 VAL16;
UINT8 double_size = 0;
UINT8 VAL8;

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


	lcd_cmd(LCD_VERSION);
	lcd_read_data(buffer, 3);
// 240320CF ILI9341 controller
	if(buffer[2] == 0x85)
	{
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

static inline void draw_pixel()
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
	current_pixel = BG;
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


INTERRUPT_MACRO _T4Interrupt()
{
	IFS1bits.T4IF = 0;
    

    delay_timer++;
//    BACKLIGHT_GPIO = !BACKLIGHT_GPIO;
}

// start new line for a uart.  Scroll if necessary
void new_line(int top_row, int port)
{
// rewind & advance a line
    cursor_x[port] = 0;
    cursor_y[port]++;

    if(cursor_y[port] >= PORT_ROWS)
    {
        UINT16 i;
// scroll the screen
        __eds__ UINT8 *dst = &screen[(UINT16)top_row * TEXT_W];
        __eds__ UINT8 *src = dst + TEXT_W;
        for(i = 0; i < (PORT_ROWS - 1) * TEXT_W; i++)
        {
            *dst++ = *src++;
        }

// blank the last row
        UINT8 x;
        dst = &screen[(UINT16)(top_row + PORT_ROWS - 1) * TEXT_W];
        for(x = 0; x < TEXT_W; x++)
        {
            *dst++ = 0;
        }
        cursor_y[port] = PORT_ROWS - 1;
    }
}


void handle_char(UINT8 c, UINT8 port, UINT8 top_row)
{
//print_number(c);
//print_lf();
// begin escape code
    if(c == 27)
    {
        esc_code[port][0] = c;
        esc_size[port] = 1;
    }
    else
// continue escape code
    if(esc_size[port] == 1)
    {
        if(c == 91)
        {
            esc_code[port][1] = c;
            esc_size[port]++;
        }
        else
        {
            esc_size[port] = 0;
        }
    }
    else
// continue escape code
    if(esc_size[port] == 2)
    {
        switch(c)
        {
            case 49:
            case 50:
            case 51:
            case 52:
                esc_code[port][2] = c;
                esc_size[port]++;
                break;
// up
            case 65:
                if(cursor_y[port] > 0)
                {
                    cursor_y[port]--;
                }
                else
                if(cursor_x[port] > 0)
                {
                    cursor_x[port] = 0;
                }
                esc_size[port] = 0;
                break;
// down
            case 66:
                if(cursor_y[port] < PORT_ROWS - 1)
                {
                    cursor_y[port]++;
                }
                else
                if(cursor_x[port] < TEXT_W - 1)
                {
                    cursor_x[port] = TEXT_W - 1;
                }
                esc_size[port] = 0;
                break;
// left
            case 68:
                if(cursor_x[port] == 0)
                {
                    if(cursor_y[port] > 0)
                    {
                        cursor_y[port]--;
                        cursor_x[port] = TEXT_W - 1;
                    }
                }
                else
                {
                    cursor_x[port]--;
                }
                esc_size[port] = 0;
                break;
// right
            case 67:
                if(cursor_x[port] >= TEXT_W - 1)
                {
                    if(cursor_y[port] < PORT_ROWS - 1)
                    {
                        cursor_y[port]++;
                        cursor_x[port] = 0;
                    }
                }
                else
                {
                    cursor_x[port]++;
                }
                esc_size[port] = 0;
                break;
            default:
                esc_size[port] = 0;
                break;
        }
    }
    else
    if(esc_size[port] == 3)
    {
        if(esc_code[port][2] == 49 && c == 126)
        {
// home
            cursor_x[port] = 0;
        }
        else
        if(esc_code[port][2] == 50 && c == 126)
        {
// insert
// Shift line right.  Obviously a more complicated behavior is needed, 
// but this is a toy.
            UINT16 i;
            for(i = TEXT_W - 1; i > cursor_x[port]; i--)
            {
                UINT16 dst = (UINT16)(cursor_y[port] + top_row) * TEXT_W + i;
                screen[dst] = screen[dst - 1];
            }
            screen[(UINT16)(cursor_y[port] + top_row) * TEXT_W + cursor_x[port]] = 0;
        }
        else
        if(esc_code[port][2] == 51 && c == 126)
        {
// delete.  
// Shift line left.
            UINT16 i;
            for(i = cursor_x[port]; i < TEXT_W - 1; i++)
            {
                UINT16 dst = (UINT16)(cursor_y[port] + top_row) * TEXT_W + i;
                screen[dst] = screen[dst + 1];
            }
            screen[(UINT16)(cursor_y[port] + top_row) * TEXT_W + TEXT_W - 1] = 0;
        }
        else
        if(esc_code[port][2] == 52 && c == 126)
        {
// end
            cursor_x[port] = TEXT_W - 1;
        }
        esc_size[port] = 0;
    }
    else
    if(c == 8)
    {
// backspace
        if(cursor_x[port] == 0)
        {
            if(cursor_y[port] > 0)
            {
                cursor_y[port]--;
                cursor_x[port] = TEXT_W - 1;
                screen[(UINT16)(cursor_y[port] + top_row) * TEXT_W + cursor_x[port]] = 0;
            }
        }
        else
        if(cursor_x[port] > 0)
        {
            cursor_x[port]--;
            screen[(UINT16)(cursor_y[port] + top_row) * TEXT_W + cursor_x[port]] = 0;
// Shift line left.
            UINT16 i;
            for(i = cursor_x[port]; i < TEXT_W - 1; i++)
            {
                UINT16 dst = (UINT16)(cursor_y[port] + top_row) * TEXT_W + i;
                screen[dst] = screen[dst + 1];
            }
            screen[(UINT16)(cursor_y[port] + top_row) * TEXT_W + TEXT_W - 1] = 0;
        }

    }
    else
    if(c == '\n')
    {
        new_line(top_row, port);
    }
    else
    if(c != '\r')
    {
// append the character
        if(cursor_x[port] >= TEXT_W)
        {
            new_line(top_row, port);
        }

        screen[(UINT16)(cursor_y[port] + top_row) * TEXT_W + cursor_x[port]] = c;
        cursor_x[port]++;
    }
}

void drain_uart(UINT8 port, UINT8 top_row)
{
    UINT16 bufsize = port_bufsize[port];
    UINT16 i;
    if(bufsize > 0)
    {
        for(i = 0; i < bufsize; i++)
        {
            handle_char(port_buffers[port][port_out[port]], port, top_row);
            port_out[port]++;
            if(port_out[port] >= PORT_BUFSIZE)
            {
                port_out[port] = 0;
            }
        }

        INTCON2bits.GIE = 0;
        port_bufsize[port] -= bufsize;
        INTCON2bits.GIE = 1;
    }
}


void handle_uart_int(UINT8 c, UINT8 port)
{
    if(port_bufsize[port] < PORT_BUFSIZE)
    {
        port_buffers[port][port_in[port]] = c;
        port_in[port]++;
        port_bufsize[port]++;
        if(port_in[port] >= PORT_BUFSIZE)
        {
            port_in[port] = 0;
        }
        
    }
}

// capture the debug uart instead of UART3
INTERRUPT_MACRO _U1RXInterrupt()
{
    UINT8 c = U1RXREG;
 	U1STAbits.URXDA = 0;
 	U1STAbits.OERR = 0;
 	IFS0bits.U1RXIF = 0;
    handle_uart_int(c, 0);
}

INTERRUPT_MACRO _U2RXInterrupt()
{
    UINT8 c = U2RXREG;
 	U2STAbits.URXDA = 0;
 	U2STAbits.OERR = 0;
 	IFS1bits.U2RXIF = 0;
#if (PORTS == 2)
    handle_uart_int(c, 1);
#endif
}

INTERRUPT_MACRO _U3RXInterrupt()
{
    UINT8 c = U3RXREG;
 	U3STAbits.URXDA = 0;
 	U3STAbits.OERR = 0;
 	IFS5bits.U3RXIF = 0;
//    handle_uart_int(c, 0);
}


void draw_screen()
{
// draw rotated screen
    UINT8 text_x;
    UINT8 text_y;
    UINT8 i;
    UINT8 j;

//     for(i = 0; i < 255; i++)
//     {
//         print_hex2(screen[i]);
//     }


    for(text_x = 0; text_x < TEXT_W; text_x++)
//    for(text_x = 0; text_x < 1; text_x++)
    {
// draw 8 LCD rows per text x
        for(i = 0; i < GLYPH_H; i++)
//        for(i = 0; i < 2; i++)
        {
// unsigned comparison with -1
            for(text_y = TEXT_H - 1; text_y < 0xff; text_y--)
            {
                UINT8 is_cursor = 0;
                if((cursor_x[0] == text_x && cursor_y[0] == text_y)
#if (PORTS == 2)
                 || 
                (cursor_x[1] == text_x && cursor_y[1] == text_y - PORT2_ROW)
#endif
                )
                {
                    is_cursor = 1;
                }

                UINT8 c = screen[(UINT16)text_y * TEXT_W + text_x];
                UINT16 fg;
                UINT16 bg = BG;
                if(text_y >= PORT2_ROW)
                {
                    fg = PORT2_COLOR;
                }
                else
                {
                    fg = PORT1_COLOR;
                }

                if(is_cursor)
                {
                    bg = fg;
                    fg = BG;
                }


// draw 8 LCD columns per text y
                if(c >= FONT_START && c < FONT_START + FONT_SIZE)
                {
                    UINT8 mask = 0x80;
                    UINT8 src = font_ram[(UINT16)(c - FONT_START) * GLYPH_H + i];

                    for(j = 0; j < GLYPH_W; j++)
                    {
                        if((src & mask) > 0)
                        {
                            current_pixel = fg;
                        }
                        else
                        {
                            current_pixel = bg;
                        }
// no need to buffer since it's not compositing
                        draw_pixel();
                        mask >>= 1;
                    }
                }
                else
                {
                    current_pixel = bg;
                    for(j = 0; j < GLYPH_W; j++)
                    {
                        draw_pixel();
                    }
                }
            }
        }
    }
}

int main()
{
    setup_pic();


    PRINT_TEXT("\n\n\n\nWelcome to SKUL TERM\n");
    setup_lcd();
    clear_screen();

// copy the font to RAM for faster drawing
    UINT16 i;
    start_flash_read(VIRTUAL_TO_FLASH(font));
    for(i = 0; i < FONT_SIZE * GLYPH_H; i++)
    {
        UINT8 c = read_flash_byte();
        font_ram[i] = c;
    }

    __eds__ UINT8 *ptr = screen;
    for(i = 0; i < TEXT_W * TEXT_H; i++)
    {
        *ptr++ = 0;
    }

    for(i = 0; i < PORTS; i++)
    {
        port_bufsize[i] = 0;
        port_in[i] = 0;
        port_out[i] = 0;
        cursor_x[i] = 0;
        cursor_y[i] = 0;
        esc_size[i] = 0;
    }

// 	COLOR_TO_PIXEL(current_pixel, 255, 0, 0);
//     for(i = 0; i < 100; i++)
//     {
//         draw_pixel();
//     }

// print the greeting
    screen[0] = 'W';
    screen[1] = 'E';
    screen[2] = 'L';
    screen[3] = 'C';
    screen[4] = 'O';
    screen[5] = 'M';
    screen[6] = 'E';
    screen[7] = ' ';
    screen[8] = 'T';
    screen[9] = 'O';
    screen[10] = ' ';
    screen[11] = 'S';
    screen[12] = 'K';
    screen[13] = 'U';
    screen[14] = 'L';
    screen[15] = ' ';
    screen[16] = 'T';
    screen[17] = 'E';
    screen[18] = 'R';
    screen[19] = 'M';
    
    UINT8 x = 8;
    UINT8 y = 8;
    UINT16 offset = y * TEXT_W + x;
    screen[offset + 2] = '-';
    screen[offset + 3] = '-';
    screen[offset + 4] = '-';
    screen[offset + 5] = '-';
    screen[offset + 6] = '-';
    offset += TEXT_W;
    screen[offset + 1] = '/';
    screen[offset + 7] = '\\';
    offset += TEXT_W;
    screen[offset + 0] = '|';
    screen[offset + 2] = '(';
    screen[offset + 3] = ')';
    screen[offset + 5] = '(';
    screen[offset + 6] = ')';
    screen[offset + 8] = '|';
    offset += TEXT_W;
    screen[offset + 1] = '\\';
    screen[offset + 4] = 'U';
    screen[offset + 7] = '/';
    offset += TEXT_W;
    screen[offset + 2] = '|';
    screen[offset + 3] = '|';
    screen[offset + 4] = '|';
    screen[offset + 5] = '|';
    screen[offset + 6] = '|';
    offset += TEXT_W;
    screen[offset + 2] = '|';
    screen[offset + 3] = '|';
    screen[offset + 4] = '|';
    screen[offset + 5] = '|';
    screen[offset + 6] = '|';

    
    cursor_x[0] = 0;
    cursor_y[0] = 1;

    draw_screen();

    while(1)
    {
		RESET_WDT

// drain the UART buffers & redraw
        if(port_bufsize[0] > 0 
#if (PORTS == 2)
        || port_bufsize[1] > 0
#endif
        )
        {
            drain_uart(0, 0);
#if (PORTS == 2)
            drain_uart(1, PORT2_ROW);
#endif
            draw_screen();
        }

//        mdelay(100);
//        BACKLIGHT_GPIO = !BACKLIGHT_GPIO;

    }

    return 0;
}






