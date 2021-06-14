/*
 * RPN2
 * Copyright (C) 2005-2018 Adam Williams <broadcast at earthling dot net>
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


// Using VT100 formatting codes


#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


#define ROW_SIZE 1024
#define STACK_ROWS 256
#define VIEW_ROWS 10
#define STRLEN 1024
#define ESC 0x1b

// the command line
char command_line[ROW_SIZE];
// the insertion point's horizontal position
int x_offset = 0;

// scrolling up the stack
int rewind_row = -1;


// Overwrite the screen if it has already been printed
int screen_printed = 0;
// number of rows in the last printing, including the function key row
int screen_height = 0;
// number of rows from the top to the cursor
int y_offset = 0;
// top row of the stack shown
int top_row = -1;

char **stack;
char **undo_stack;
char **temp_stack;
enum
{
	BASE_NONE,
	BASE_HEX,
	BASE_BINARY,
	BASE_TIME,
};

enum
{
	ANGLE_RAD,
	ANGLE_DEG
};


int current_base = BASE_HEX;
int current_angle = ANGLE_RAD;


char hex_table[] = 
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8',
	'9', 'A', 'B', 'C', 'D', 'E', 'F'
};

// Units
#define UNITS_NONE 0

#define UNITS_LB 1
#define UNITS_OZ 2
#define UNITS_KG 3
#define UNITS_MI 4
#define UNITS_YD 5
#define UNITS_FT 6
#define UNITS_IN 7
#define UNITS_KM 8
#define UNITS_MM 9
#define UNITS_CM 10
#define UNITS_M  11
#define UNITS_C  12
#define UNITS_F  13
#define UNITS_G  14

// 2 letter units must come before 1 letter units because they're longer
const char *unit_titles[] = 
{
	"",
	"_lb",
	"_oz",
	"_kg",
	"_mi",
	"_yd",
	"_ft",
	"_in",
	"_km",
	"_mm", 
	"_cm",
	"_m",
	"_c",
	"_f",
	"_g"
};

typedef struct
{
	int from_units;
	int to_units;
	double factor;
	double offset;
} conversion_t;


conversion_t conversions[] = 
{
	{ UNITS_LB, UNITS_OZ, 16.0, 0.0 },
	{ UNITS_KG, UNITS_LB, 2.2046, 0.0 },
	{ UNITS_KG, UNITS_G, 1000.0, 0.0 },
	{ UNITS_MI, UNITS_YD, 1760, 0.0 },
	{ UNITS_YD, UNITS_FT, 3, 0.0 },
	{ UNITS_FT, UNITS_IN, 12, 0.0 },
	{ UNITS_M,  UNITS_FT, 3.2808, 0.0 },
	{ UNITS_KM, UNITS_M, 1000, 0.0 },
	{ UNITS_M,  UNITS_CM, 100, 0.0 },
	{ UNITS_CM, UNITS_MM, 10, 0.0 },
	{ UNITS_C,  UNITS_F, 9.0 / 5.0, 32.0 }
};

#define TOTAL_CONVERSIONS (sizeof(conversions) / sizeof(conversion_t))

// Replica of conversion table with the current value in
// the positions of the different units
typedef struct 
{
	double from_value;
	double to_value;
	int from_valid;
	int to_valid;
} current_conversion_t;

current_conversion_t current_conversions[TOTAL_CONVERSIONS];


typedef struct
{
	double value;
	int units;
	int base;
} number_t;

number_t to_number(char *string);
void to_string(char *string, number_t number);


// print terminal codes
void cursor_left(int number)
{
	if(number > 0)
	{
		putc(ESC, stdout);
		printf("[%dD", number);
	}
}

void cursor_right(int number)
{
	if(number > 0)
	{
		putc(ESC, stdout);
		printf("[%dC", number);
	}
}

void cursor_up(int number)
{
	if(number > 0)
	{
		putc(ESC, stdout);
		printf("[%dA", number);
	}
}

void cursor_down(int number)
{
	if(number > 0)
	{
		putc(ESC, stdout);
		printf("[%dB", number);
	}
}

void reverse_video()
{
	putc(ESC, stdout);
	printf("[7m");
}

void reset_style()
{
	putc(ESC, stdout);
	printf("[m");
}

void save_position()
{
	putc(ESC, stdout);
	printf("7");
}

void load_position()
{
	putc(ESC, stdout);
	printf("8");
}

void erase_to_end()
{
	putc(ESC, stdout);
	printf("[K");
}

void erase_line()
{
	putc(ESC, stdout);
	printf("[2K");
}

void copy_stack(char **dst, char **src)
{
	int i;
	for(i = 0; i < STACK_ROWS; i++)
	{
		strcpy(dst[i], src[i]);
	}
}

int stack_size(int visible)
{
	int max = VIEW_ROWS;
	if(!visible)
	{
		max = STACK_ROWS;
	}

	int total_rows = 0;
	int i;
	for(i = 0 ; i < max; i++)
	{
		if(strlen(stack[i])) 
		{
			total_rows++;
		}
		else
		{
			break;
		}
	}
	return total_rows;
}

void print_help()
{
// overwrite the last printing
	if(screen_printed)
	{
		cursor_up(y_offset);
	}
	screen_printed = 0;

	erase_to_end();
	printf("Simple RPN calculator by Adam Williams\n");
	erase_to_end();
	printf("OPERATORS: + - / * %% & | ^ -= 1/ << >>\n");
	erase_to_end();
	printf("FUNCTIONS: pow sqrt sin cos tan asin acos atan exp log log10\n");
	erase_to_end();
	printf("CONSTANTS: pi e\n");
	erase_to_end();
	printf("STACK: c - clear all | d - clear row | s - swap | u - undo | Enter - copy\n");
	erase_to_end();
	printf("BASE: rb - real to base | br - base to real\n");
	erase_to_end();
	printf("UNITS: _c _f | _lb _oz | _kg _g | _mi _yd _ft _in | _km _m _cm _mm\n");
}

void print_function(char *key, char *text, int checked)
{
	int offset = 0;
	reverse_video();
	printf("%s", key);
	reset_style();
	
	printf(" ");
//	if(checked)
//	{
		reverse_video();
//	}
	
	if(checked)
	{
		printf("*");
		offset += 1;
	}
	printf("%s", text);
	if(checked)
	{
		printf("*");
		offset += 1;
	}
	
//	if(checked)
//	{
		reset_style();
//	}
	
	offset = strlen(key) + strlen(text) + 1;
	int i;
	for(i = 0; i < 12 - offset; i++)
	{
		printf(" ");
	}
}

void print_screen(int flush_it)
{
	int i;
	int total_rows = stack_size(0);
	int visible_rows = stack_size(1);

// overwrite the last printing
	if(screen_printed)
	{
		cursor_up(y_offset);
	}
	printf("\r");

	screen_printed = 1;
	screen_height = visible_rows + 1;

// the function keys
	erase_to_end();
	if(rewind_row >= 0)
	{
		print_function("F1", "ECHO", 0);
		print_function("F2", "PICK", 0);
		print_function("F3", "DROP", 0);
		print_function("F4", "DROP N", 0);
	}
	else
	{
		print_function("F1", "HELP", 0);
		print_function("F2", "HEX", current_base == BASE_HEX);
		print_function("F3", "BIN", current_base == BASE_BINARY);
		print_function("F4", "TIME", current_base == BASE_TIME);
		print_function("F5", "RAD", current_angle == ANGLE_RAD);
		print_function("F6", "DEG", current_angle == ANGLE_DEG);
	}
	
	printf("\n");


// the stack
	if(total_rows > visible_rows && rewind_row >= 0)
	{
		if(rewind_row > top_row)
		{
			top_row = rewind_row;
		}
		else
		if(rewind_row <= top_row - visible_rows)
		{
			top_row = rewind_row + visible_rows - 1;
		}
	}
	else
	{
		top_row = visible_rows - 1;
	}

	for(i = top_row; i >= top_row - visible_rows + 1; i--)
	{
		
		if(i == rewind_row)
		{
			reverse_video();
		}
		printf("%s", stack[i]);
		if(i == rewind_row)
		{
			reset_style();
		}
		erase_to_end();
		printf("\n");
	}

// the command line
	printf("%s", command_line);
	erase_to_end();

// move cursor to the highlighted line
	if(rewind_row >= 0)
	{
		y_offset = top_row - rewind_row + 1;
		cursor_up(screen_height - y_offset);
		printf("\r");
		cursor_right(x_offset);
	}
	else
	{
// move cursor to the command line
		y_offset = screen_height;
		cursor_left(strlen(command_line) - x_offset);
	}
	
	if(flush_it) 
	{
		fflush(stdout);
	}
}

void clear_screen()
{
	cursor_up(y_offset - 1);
	int i;
	for(i = 0; i < screen_height; i++)
	{
		erase_line();
		if(i < screen_height - 1)
		{
			printf("\n");
		}
	}
	y_offset = screen_height;
}

// cursor up & reprint 1 stack line
void dump_stack_line(int row, int highlight_it)
{
	if(row < VIEW_ROWS)
	{
// cursor up to the line
		cursor_up(row + 1);
		erase_to_end();
		if(highlight_it)
		{
			reverse_video();
		}
		printf("\r%s", stack[row]);
		if(highlight_it)
		{
			reset_style();
		}
		cursor_down(row + 1);
	}
}



int is_hex(char *ptr)
{
	int got_it = 1;
	int j;
	while(*ptr != 0 && got_it)
	{
		got_it = 0;
		for(j = 0; j < sizeof(hex_table) && !got_it; j++)
		{
			if(toupper(*ptr) == hex_table[j])
			{
				got_it = 1;
			}
		}
		
		ptr++;
	}

	if(!got_it)
	{
		printf("*** Not a hex number\n");
		return 0;
	}
	
	return 1;
}

int is_bin(char *ptr)
{
	while(*ptr != 0)
	{
		if(*ptr != '0' && *ptr != '1' && *ptr != ' ')
		{
			printf("*** Not a binary number\n");
			return 0;
		}
		ptr++;
	}
	
	return 1;
}

void reformat_binary(char *string)
{
	int counter = 0;
	int i, j;
//printf("reformat_binary %d %s\n", __LINE__, string);

// remove spaces
	for(i = 0; i < strlen(string); i++)
	{
		if(string[i] == ' ')
		{
			for(j = i; j < strlen(string); j++)
			{
				string[j] = string[j + 1];
			}
			string[j] = 0;
		}
	}

// pad to 4 bits
	const int bits_per_block = 8;
	int increment = bits_per_block;
	int bits = strlen(string) - 2;
	int padding = increment - (bits % increment);
//printf("reformat_binary %d padding=%d\n", __LINE__, padding);
	if(padding < increment && padding > 0)
	{
		int len = strlen(string);
		string[len + padding] = 0;
		for(i = len - 1; i >= 2; i--)
		{
			string[i + padding] = string[i];
		}

		for(i = 2; i < 2 + padding; i++)
		{
			string[i] = '0';
		}
	}

// add spaces
	for(i = strlen(string) - 1; i >= 2; i--)
	{
		if(counter == bits_per_block)
		{
// insert a space after 4 digits
			if(string[i] != ' ')
			{
				string[strlen(string) + 1] = 0;
				for(j = strlen(string) - 1; j > i; j--)
				{
					string[j + 1] = string[j];
				}
				string[i + 1] = ' ';
			}
			
			counter = 1;
		}
		else
		{
			counter++;
		}
	}


//printf("reformat_binary %d %s\n", __LINE__, string);
}


void push_stack(char *string)
{
	int i, j;
	if(string[0])
	{
// check the base
// 		if(string[0] == '#')
// 		{
// 			if(current_base == BASE_TIME)
// 			{
// 				printf("*** Not a time\n");
// 				return;
// 			}
// 			else
// 			if(current_base == BASE_BINARY)
// 			{
// 				if(!is_bin(string + 1))
// 				{
// 					return;
// 				}
// 			}
// 			else
// 			if(current_base == BASE_HEX)
// 			{
// 				if(!is_hex(string + 1))
// 				{
// 					return;
// 				}
// 			}
// 		}
// 		else
		if(string[1] &&
			string[0] == '0' &&
			string[1] == 'b')
		{
			reformat_binary(string);
//printf("push_stack %d %s\n", __LINE__, string);
			if(!is_bin(string + 2)) return;
		}
		else
		if(string[1] &&
			string[0] == '0' &&
			(string[1] == 'x' ||
			string[1] == 'h'))
		{
			if(!is_hex(string + 2)) return;
		}
		else
// Throw out unknown bases
		if(string[1] &&
			string[0] == '0' &&
			isalpha(string[1]))
		{
			printf("*** Unrecognized format\n");
			return;
		}
		
		for(i = STACK_ROWS - 1; i >= 1; i--)
		{
			strcpy(stack[i], stack[i - 1]);
		}
		strcpy(stack[0], string);
	}
	
	
//	print_screen();
}

// Remove the lowest item and put it in the string.
void pop_stack(char *string)
{
	int i;
	strcpy(string, stack[0]);
	for(i = 0; i < STACK_ROWS - 1; i++)
	{
		strcpy(stack[i], stack[i + 1]);
	}
	stack[STACK_ROWS - 1][0] = 0;
}

void replicate_row()
{
	int i;
	for(i = STACK_ROWS - 1; i >= 1; i--)
	{
		strcpy(stack[i], stack[i - 1]);
	}
	print_screen(1);
}

void delete_row()
{
	int i;
	for(i = 0; i < STACK_ROWS - 1; i++)
	{
		strcpy(stack[i], stack[i + 1]);
	}
	stack[STACK_ROWS - 1][0] = 0;
	erase_line();
	print_screen(1);
}

void swap_row()
{
	char *temp;
	temp = stack[0];
	stack[0] = stack[1];
	stack[1] = temp;
	print_screen(1);
}

void clear_stack(char **stack)
{
	int i;
	int visible_rows = stack_size(1);
	for(i = 0; i < STACK_ROWS; i++)
	{
		stack[i][0] = 0;
	}
	erase_line();
	for(i = 0; i < visible_rows; i++)
	{
		cursor_up(1);
		erase_line();
	}
	cursor_up(1);
	screen_printed = 0;
	print_screen(1);
}

// return the offset if it's a #, 0x, or 0b
// return 0 if it's decimal
int base_offset(char *string)
{
	int len = strlen(string);
//	if(len > 0 && string[0] == '#') return 1;
	if(len > 1 && string[0] == '0' && string[1] == 'x') return 2;
	if(len > 1 && string[0] == '0' && string[1] == 'h') return 2;
	if(len > 1 && string[0] == '0' && string[1] == 'b') return 2;
	return 0;
}


int64_t read_binary(char *string)
{
	int i;
	int64_t result = 0;
	int len = strlen(string);
	int offset = base_offset(string);

	for(i = offset; i < len; i++)
	{
		if(string[i] != ' ')
		{
			result <<= 1;
			if(string[i] == '1')
				result |= 1;
		}
	}
	return result;
}

int64_t read_hex(char *string)
{
	int i, j;
	int64_t result = 0;
	int len = strlen(string);
	int offset = base_offset(string);
	
	for(i = offset; i < len; i++)
	{
		result <<= 4;
		for(j = 0; j < 0x10; j++)
		{
			if(toupper(string[i]) == hex_table[j])
			{
				result |= j;
				j = 0x10;
			}
		}
	}
	return result;
}


void write_binary(char *string, int64_t number)
{
	uint64_t i;
	int got_it = 0;
	
	
	sprintf(string, "0b");
	for(i = 0x8000000000000000LL; i > 0; i >>= 1)
	{
		if(number & i)
		{
			strcat(string, "1");
			got_it = 1;
		}
		else
		if(got_it)
		{
			strcat(string, "0");
		}
	}

	if(!got_it)
		strcat(string, "0");

//printf("write_binary %d %s\n", __LINE__, string);

// Add formatting spaces
	reformat_binary(string);
//printf("write_binary %d %s\n", __LINE__, string);
}



void to_hex()
{
	int i;
	for(i = 0; i < STACK_ROWS; i++)
	{
		number_t number = to_number(stack[i]);
		if(number.base != BASE_NONE && 
			number.base != BASE_HEX)
		{
			number.base = BASE_HEX;
			to_string(stack[i], number);
		}
	}

	current_base = BASE_HEX;
	print_screen(1);
}

void to_bin()
{
	int i;
	for(i = 0; i < STACK_ROWS; i++)
	{
		number_t number = to_number(stack[i]);
		if(number.base != BASE_NONE && 
			number.base != BASE_BINARY)
		{
			number.base = BASE_BINARY;
			to_string(stack[i], number);
		}
	}

	current_base = BASE_BINARY;
	print_screen(1);
}

void to_time()
{
	int i;
	for(i = 0; i < STACK_ROWS; i++)
	{
		number_t number = to_number(stack[i]);
		if(number.base != BASE_NONE && 
			number.base != BASE_TIME)
		{
			number.base = BASE_TIME;
			to_string(stack[i], number);
		}
	}


	current_base = BASE_TIME;
	print_screen(1);
}

void to_rad()
{
	current_angle = ANGLE_RAD;
	print_screen(1);
}

void to_deg()
{
	current_angle = ANGLE_DEG;
	print_screen(1);
}

// Convert stack line into a number with units
number_t to_number(char *string)
{
	int len = strlen(string);
	char string2[ROW_SIZE];
	number_t result;
	result.units = UNITS_NONE;
	result.base = BASE_NONE;

// extract units
	char *ptr = strrchr(string, '_');
//printf("to_number %d string=%s ptr=%s\n", __LINE__, string, ptr);
	if(ptr)
	{
		int i;
		for(i = 0; i < sizeof(unit_titles) / sizeof(char*); i++)
		{
			if(!strcmp(ptr, unit_titles[i]))
			{
				result.units = i;
//printf("to_number %d units=%d\n", __LINE__, i);
				break;
			}
		}
	}

// extract time
	ptr = strrchr(string, ':');
//printf("to_number %d %s\n", __LINE__, ptr);
	if(ptr)
	{
// Get seconds
		double seconds = atof(ptr + 1);
		double minutes = 0;
		double hours = 0;

		char *ptr2 = ptr - 1;
		while(ptr2 >= string)
		{
			if(*ptr2 == ':' || ptr2 == string)
			{
				char *ptr3 = ptr2;
				if(*ptr3 == ':') ptr3++;
				memcpy(string2, ptr3, ptr - ptr3);
				string2[ptr - ptr3] = 0;
//				printf("to_number %d %s\n", __LINE__, string2);
				minutes = atof(string2);
				break;
			}
			
			ptr2--;
		}
		
		if(ptr2 && ptr2 > string)
		{
			memcpy(string2, string, ptr2 - string);
			string2[ptr2 - string] = 0;
//			printf("to_number %d %s\n", __LINE__, string2);
			hours = atof(string2);
		}
		
// 		printf("to_number %d hours=%f minutes=%f seconds=%f\n", 
// 			__LINE__, 
// 			hours,
// 			minutes,
// 			seconds);
		result.value = hours * 3600 +
			minutes * 60 +
			seconds;
		result.base = BASE_TIME;
		return result;
	}


// extract base
// 	if(string[0] == '#')
// 	{
// 		switch(current_base)
// 		{
// 			case BASE_HEX:
// 				result.base = BASE_HEX;
// 				result.value = (double)read_hex(string);
// 				break;
// 				
// 			case BASE_BINARY:
// 				result.base = BASE_BINARY;
// 				result.value = (double)read_binary(string + 1);
// 				break;
// 		}
// 	}
	
	if(len > 2 && string[0] == '0' && (string[1] == 'x' || string[1] == 'h'))
	{
		result.base = BASE_HEX;
		result.value = (double)read_hex(string);
	}

	if(len > 2 && string[0] == '0' && string[1] == 'b')
	{
		result.base = BASE_BINARY;
		result.value = (double)read_binary(string + 2);
	}

// return now
	if(result.base != BASE_NONE) return result;

// Remove commas
	char *in = string;
	char *out = string2;
	while(*in)
	{
		if(*in != ',') *out++ = *in;
		in++;
	}
	*out++ = 0;

// Convert exponents to scientific notation
	


// atof can't handle large numbers
	if(labs(atoll(string2)) > 0x7fffffff)
	{
		result.value = (double)atoll(string2);
	}
	else
	{
		result.value = atof(string2);
	}
	
	return result;
}


number_t pop_number()
{
	char string[ROW_SIZE];
	int i, j;
	

	pop_stack(string);
	erase_line();
	return to_number(string);
}

// Convert number to stack line
void to_string(char *string, number_t number)
{
	int has_fraction = (floor(number.value) != number.value);

	switch(number.base)
	{
		case BASE_NONE:
// Number has no fraction part
			if(!has_fraction)
			{
				sprintf(string, "%ld", (int64_t)number.value);
			}
			else
			{
				int i;
				if(fabs(number.value) > 0.00001)
					sprintf(string, "%f", number.value);
				else
					sprintf(string, "%.16e", number.value);
// Truncate fraction part 
				for(i = strlen(string) - 1; i > 0; i--)
				{
					if(string[i] != '0')
					{
						string[i + 1] = 0;
						break;
					}
				}
			}
			break;
		case BASE_HEX:
			sprintf(string, "0x%lx", (int64_t)number.value);
			break;
		case BASE_BINARY:
			write_binary(string, (int64_t)number.value);
			break;
		case BASE_TIME:
		{
			char string2[STRLEN];
			int force = 0;
			string[0] = 0;

	// hours
			if(number.value > 60 * 60)
			{
				sprintf(string, "%d:", (int)number.value / 60 / 60);
				force = 1;
			}

// minutes
			if(number.value > 60 || force)
			{
				if(force)
				{
					sprintf(string2, "%02d:", ((int)number.value / 60) % 60);
				}
				else
				{
					sprintf(string2, "%d:", (int)number.value / 60);
				}
				strcat(string, string2);
				force = 1;
			}

// seconds
			if(force)
			{
				sprintf(string2, "%02d", (int)number.value % 60);
				strcat(string, string2);
				
				if(has_fraction)
				{
					sprintf(string2, "%f", number.value - (int)number.value);
					strcat(string, string2 + 1);
				}
			}
			else
			{
				sprintf(string2, "%f", number.value);
				strcat(string, string2);
			}
		}
	}

	strcat(string, unit_titles[number.units]);
	
//	printf("to_string %d: string=%s\n", __LINE__, string);
}

// arg1 is most recent slot
void promote_units(number_t *output, number_t arg1, number_t arg2)
{
	if(arg1.units == UNITS_NONE) 
		output->units = arg2.units;
	else
		output->units = arg1.units;

	if(arg1.base == BASE_NONE)
		output->base = arg2.base;
	else
		output->base = arg1.base;
}

void push_number(number_t number)
{
	char string[ROW_SIZE];
	to_string(string, number);
	push_stack(string);
	print_screen(1);
}

void convert_units(int to_units)
{
// Get units of current item
	char prev_item[ROW_SIZE];
	int i, j;
	int total_rows = stack_size(0);
	pop_stack(prev_item);

// Does previous item have units?
	char *ptr = strrchr(prev_item, '_');

// Add dimension to item
	if(!ptr)
	{
		strcat(prev_item, unit_titles[to_units]);
	}
	else
// Convert item's dimension
	{
// Fill in every value in the table until the output units are found
		bzero(current_conversions, sizeof(current_conversions));
		int done = 0;

// Get the number
		number_t from_number;
		from_number = to_number(prev_item);

//printf("convert_units %d %d %d\n", __LINE__, to_units, from_number.units);
// First fill in every matching table entry with the number
		for(i = 0; i < TOTAL_CONVERSIONS; i++)
		{
			conversion_t *conversion = &conversions[i];
			if(conversion->from_units ==  from_number.units)
			{
				current_conversions[i].from_value = from_number.value;
				current_conversions[i].from_valid = 1;
			}

			if(conversion->to_units == from_number.units)
			{
				current_conversions[i].to_value = from_number.value;
				current_conversions[i].to_valid = 1;
			}
		}

// Iterate through the table until the output is found
		while(!done)
		{
			int conversion_done = 0;
// Propogate incomplete conversions to all slots
			for(i = 0; i < TOTAL_CONVERSIONS; i++)
			{
// Find an incomplete conversion
				current_conversion_t *current_conversion = &current_conversions[i];
				conversion_t *conversion = &conversions[i];

				number_t propogate;
				propogate.value = 0;
				propogate.units = UNITS_NONE;
				propogate.base = BASE_NONE;
				if(current_conversion->from_valid && !current_conversion->to_valid)
				{
					current_conversion->to_value = current_conversion->from_value * 
						conversion->factor +
						conversion->offset;
					current_conversion->to_valid = 1;
					propogate.units = conversion->to_units;
					propogate.value = current_conversion->to_value;
				}
				else
				if(current_conversion->to_valid && !current_conversion->from_valid)
				{
					current_conversion->from_value = (current_conversion->to_value -
						conversion->offset) / 
						conversion->factor;
					current_conversion->from_valid = 1;
					propogate.units = conversion->from_units;
					propogate.value = current_conversion->from_value;
				}

// Propogate the new value to matching slots
				if(propogate.units != UNITS_NONE)
				{
// If the units match, quit
					if(propogate.units == to_units)
					{
						done = 1;
						to_string(prev_item, propogate);
						break;
					}

					for(i = 0; i < TOTAL_CONVERSIONS; i++)
					{
						conversion_t *conversion = &conversions[i];
						if(conversion->from_units ==  propogate.units)
						{
							current_conversions[i].from_value = propogate.value;
							current_conversions[i].from_valid = 1;
							conversion_done = 1;
						}

						if(conversion->to_units == propogate.units)
						{
							current_conversions[i].to_value = propogate.value;
							current_conversions[i].to_valid = 1;
							conversion_done = 1;
						}
					}
				}
			}
			if(!conversion_done && !done) break;
		}

		if(!done)
		{
			printf("*** Unsupported unit conversion %s to %s\n",
				unit_titles[from_number.units], 
				unit_titles[to_units]);
		}
	}

// Replace item
	push_stack(prev_item);
// Failed to replace item
	if(total_rows > stack_size(0))
	{
		copy_stack(stack, undo_stack);
	}
	print_screen(1);
}


void process_line(char *string)
{
	int i;
//	fgets(string, ROW_SIZE, stdin);

// remove trailing garbage
	char *ptr = string + strlen(string) - 1;
	while(ptr >= string && (*ptr == 0xa || *ptr == ' '))
		*ptr-- = 0;

// remove leading garbage
	while(string[0] != 0 && string[0] ==' ')
	{
		memcpy(string, string + 1, strlen(string + 1) + 1);
	}

// detect invalid characters
	if(string[0] == '#')
	{
		printf("*** # notation is not supported.  Use 0x or 0b instead.\r");
	}
	else
	{

// Detect conversion command
		int is_units = 0;
		int to_units = UNITS_NONE;
		if(string[0] == '_')
		{
			for(i = 0; i < TOTAL_CONVERSIONS; i++)
			{
				if(!strcmp(string, unit_titles[conversions[i].from_units]))
				{
					is_units = 1;
					to_units = conversions[i].from_units;
					break;
				}
				else
				if(!strcmp(string, unit_titles[conversions[i].to_units]))
				{
					is_units = 1;
					to_units = conversions[i].to_units;
					break;
				}
			}
		}

// Detect command with trailing arithmetic operator
		if(strcmp(string, "1/"))
		{
// If a trailing arithmetic operator exists, split the command into 2 lines.
			for(ptr = string + strlen(string) - 1; ptr >= string; ptr--)
			{
				if(*ptr != '+' &&
					*ptr != '-' &&
					*ptr != '/' &&
					*ptr != '*' &&
					*ptr != '%' &&
					*ptr != '&' &&
					*ptr != '|' &&
					*ptr != '^' &&
					*ptr != '<' &&
					*ptr != '>')
				{
					ptr++;
					if(!strcmp(ptr, "+") ||
						!strcmp(ptr, "-") ||
						!strcmp(ptr, "/") ||
						!strcmp(ptr, "*") ||
						!strcmp(ptr, "%") ||
						!strcmp(ptr, "&") ||
						!strcmp(ptr, "|") ||
						!strcmp(ptr, "^") ||
						!strcmp(ptr, "<<") ||
						!strcmp(ptr, ">>"))
					{
						char temp[ROW_SIZE];
						strcpy(temp, string);
						temp[ptr - string] = 0;
						copy_stack(undo_stack, stack);

// These are constants which may be followed by an operator
// Expand temp
						if(!strcmp(temp, "pi"))
						{
							number_t number;
							number.units = UNITS_NONE;
							number.base = BASE_NONE;
							number.value = M_PI;
							push_number(number);
						}
						else
						if(!strcmp(temp, "e"))
						{
							number_t number;
							number.units = UNITS_NONE;
							number.base = BASE_NONE;
							number.value = exp(1);
							push_number(number);
						}
						else
						{
							push_stack(temp);
						}

						strcpy(string, ptr);
//printf("main %d temp=%s string=%s\n", __LINE__, temp, string);
					}
					break;
				}
			}
		}

// replicate row
		if(!string[0]) 
		{
			copy_stack(undo_stack, stack);
			replicate_row();
		}
		else
		if(is_units)
		{
			copy_stack(undo_stack, stack);
			convert_units(to_units);
		}
		else
		if(!strcmp(string, "-="))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = -number.value;
			push_number(number);
		}
		else
		if(!strcmp(string, "1/"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = 1.0 / number.value;
			push_number(number);
		}
		else
		if(!strcmp(string, "sqrt"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = sqrt(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "sin"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			if(current_angle == ANGLE_DEG)
				number.value = number.value * 2.0 * M_PI / 360;
			number.value = sin(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "cos"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			if(current_angle == ANGLE_DEG)
				number.value = number.value * 2.0 * M_PI / 360;
			number.value = cos(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "tan"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			if(current_angle == ANGLE_DEG)
				number.value = number.value * 2.0 * M_PI / 360;
			number.value = tan(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "asin"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = asin(number.value);
			if(current_angle == ANGLE_DEG)
				number.value = number.value * 360 / 2.0 / M_PI;
			push_number(number);
		}
		else
		if(!strcmp(string, "acos"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = acos(number.value);
			if(current_angle == ANGLE_DEG)
				number.value = number.value * 360 / 2.0 / M_PI;
			push_number(number);
		}
		else
		if(!strcmp(string, "atan"))
		{
			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = atan(number.value);
			if(current_angle == ANGLE_DEG)
				number.value = number.value * 360 / 2.0 / M_PI;
			push_number(number);
		}
		else
		if(!strcmp(string, "exp"))
		{

			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = exp(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "log"))
		{

			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = log(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "log10"))
		{

			copy_stack(undo_stack, stack);
			number_t number = pop_number();
			number.value = log10(number.value);
			push_number(number);
		}
		else
		if(!strcmp(string, "pi"))
		{
			copy_stack(undo_stack, stack);
			number_t number;
			number.value = M_PI;
			number.units = UNITS_NONE;
			number.base = BASE_NONE;
			push_number(number);
		}
		else
		if(!strcmp(string, "e"))
		{
			copy_stack(undo_stack, stack);
			number_t number;
			number.value = exp(1);
			number.units = UNITS_NONE;
			number.base = BASE_NONE;
			push_number(number);
		}
		else
		if(!strcmp(string, "br"))
		{

			number_t number = pop_number();
// printf("main %d %f %d %d\n", __LINE__, number.value, number.units, number.base);
			number.base = BASE_NONE;
			push_number(number);
		}
		else
		if(!strcmp(string, "rb"))
		{
			number_t number = pop_number();
			number.base = current_base;
			push_number(number);
		}
		else
		if(!strcmp(string, "rad"))
		{
			if(current_angle != ANGLE_RAD)
			{
				to_rad();
			}
			else
				printf("*** Already in radian mode\n");
		}
		else
		if(!strcmp(string, "deg"))
		{
			if(current_angle != ANGLE_DEG)
			{
				to_deg();
			}
			else
				printf("*** Already in degree mode\n");
		}
		else
		if(!strcmp(string, "h"))
		{
			to_hex();
		}
		else
		if(!strcmp(string, "b"))
		{
			to_bin();
		}
		else
		if(!strcmp(string, ":"))
		{
			to_time();
		}
		else
		if(!strcmp(string, "pow"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = pow(arg2.value, arg1.value);
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "+"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = arg2.value + arg1.value;
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "-"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = arg2.value - arg1.value;
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "/"))
		{
			copy_stack(undo_stack, stack);


			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = arg2.value / arg1.value;
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "*"))
		{
			copy_stack(undo_stack, stack);


			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = arg2.value * arg1.value;
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "%"))
		{
			copy_stack(undo_stack, stack);


			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = (int64_t)arg2.value % (int64_t)arg1.value;
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "&"))
		{
			copy_stack(undo_stack, stack);


			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = (double)(((int64_t)arg1.value) & ((int64_t)arg2.value));
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "|"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = (double)(((int64_t)arg1.value) | ((int64_t)arg2.value));
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "^"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = (double)(((int64_t)arg1.value) ^ ((int64_t)arg2.value));
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "<<"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = (double)(((int64_t)arg2.value) << ((int64_t)arg1.value));
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, ">>"))
		{
			copy_stack(undo_stack, stack);

			number_t arg1 = pop_number();
			number_t arg2 = pop_number();
			number_t result;
			result.value = (double)(((int64_t)arg2.value) >> ((int64_t)arg1.value));
// printf("main %d arg2=%d arg1=%d result=%d\n",
// __LINE__,
// (int)arg2.value,
// (int)arg1.value,
// (int)result.value);
			promote_units(&result, arg1, arg2);
			push_number(result);
		}
		else
		if(!strcmp(string, "c"))
		{
			copy_stack(undo_stack, stack);
			clear_stack(stack);
		}
		else
		if(!strcmp(string, "d"))
		{
			copy_stack(undo_stack, stack);
			delete_row(stack);
		}
		else
		if(!strcmp(string, "s"))
		{
			copy_stack(undo_stack, stack);
			swap_row(stack);
		}
		else
		if(!strcmp(string, "u"))
		{
			copy_stack(temp_stack, stack);
			copy_stack(stack, undo_stack);
			copy_stack(undo_stack, temp_stack);
			clear_screen();
			print_screen(1);
		}
		else
		if(!strcmp(string, "help") || 
			!strcmp(string, "he") ||
			!strcmp(string, "?"))
		{
			print_help();
			print_screen(1);
		}
		else
		{
			copy_stack(undo_stack, stack);
			push_stack(string);
			print_screen(1);
		}
	}

}

// get the line being edited
char* working_line()
{
	if(rewind_row >= 0)
	{
		return stack[rewind_row];
	}
	else
	{
		return command_line;
	}
}

// insert c at the current offset
void insert_char(char c)
{
	int i;
	char *string = working_line();
	int len = strlen(string);

	if(len < ROW_SIZE - 1)
	{
		for(i = len + 1; i >= x_offset; i--)
		{
			string[i] = string[i - 1];
		}
		string[x_offset] = c;
		len++;
		x_offset++;

		if(rewind_row >= 0)
		{
			reverse_video();
		}
		printf("%s", string + x_offset - 1);
		if(rewind_row >= 0)
		{
			reset_style();
		}

		if(x_offset >= len - 1)
		{
			cursor_left(len - 1 - x_offset);
		}
		else
		{
			cursor_left(len - x_offset);
		}
	}
	fflush(stdout);
}

void delete_char(int after)
{
	int i;
	char *string = working_line();
	int len = strlen(string);

	if(after)
	{
		if(len > x_offset)
		{
			for(i = x_offset; i < len; i++)
			{
				string[i] = string[i + 1];
			}
			if(rewind_row >= 0)
			{
				reverse_video();
			}
			printf("%s", string + x_offset);
			if(rewind_row >= 0)
			{
				reset_style();
			}
			printf(" ");
			cursor_left(len - x_offset);
		}
	}
	else
	if(x_offset > 0)
	{
		x_offset--;
		for(i = x_offset; i < len; i++)
		{
			string[i] = string[i + 1];
		}
		
		cursor_left(1);
		if(rewind_row >= 0)
		{
			reverse_video();
		}
		printf("%s", string + x_offset);
		if(rewind_row >= 0)
		{
			reset_style();
		}
		printf(" ");
		cursor_left(len - x_offset);
	}
	fflush(stdout);
}

void function1()
{
	if(rewind_row < 0)
	{
		print_help();
		print_screen(1);
	}
	else
	{
		if(rewind_row >= 0)
		{
// echo it to the insertion point
			char *src = stack[rewind_row];
			int src_len = strlen(src);
			int dst_len = strlen(command_line);
			int i;
			if(src_len + dst_len < ROW_SIZE - 1)
			{
				for(i = dst_len + src_len + 1; 
					i >= x_offset + src_len - 1; 
					i--)
				{
					command_line[i] = command_line[i - src_len];
				}
				
				strcpy(command_line, src);
				
				
				print_screen(1);
			}
		}
	}
}

void function2()
{
	if(rewind_row < 0)
	{
		to_hex();
	}
	else
	{
// push it on a new stack line
		copy_stack(undo_stack, stack);

		int i;
		for(i = STACK_ROWS - 1; i >= 1; i--)
		{
			strcpy(stack[i], stack[i - 1]);
		}
		strcpy(stack[0], stack[rewind_row + 1]);
		x_offset = strlen(working_line());
		print_screen(1);
	}
}

void function3()
{
	if(rewind_row < 0)
	{
		to_bin();
	}
	else
	{
// delete the stack line
		copy_stack(undo_stack, stack);
		int i;
		for(i = rewind_row; i < STACK_ROWS - 1; i++)
		{
			strcpy(stack[i], stack[i + 1]);
		}
		rewind_row--;
		stack[STACK_ROWS - 1][0] = 0;
		x_offset = strlen(working_line());
// clear the command line to erase the deleted line
		erase_line();
		print_screen(1);
	}
}

void function4()
{
	if(rewind_row < 0)
	{
		to_time();
	}
	else
	{
// delete all below the rewind line
		copy_stack(undo_stack, stack);

		int i;
		for(i = 0; i < STACK_ROWS - rewind_row - 1; i++)
		{
			strcpy(stack[i], stack[i + rewind_row + 1]);
		}
		for(i = STACK_ROWS - rewind_row - 1; i < STACK_ROWS; i++)
		{
			stack[i][0] = 0;
		}

// clear the entire screen + command line to erase the deleted lines
		clear_screen();
		rewind_row = -1;
		x_offset = strlen(working_line());
		print_screen(1);
	}
}

void function5()
{
	if(rewind_row < 0)
	{
		to_rad();
	}
	else
	{
	}
}

void function6()
{
	if(rewind_row < 0)
	{
		to_deg();
	}
	else
	{
	}
}


int main(int argc, char *argv[])
{
	int i;
	int out_fd = fileno(stdout);
	int in_fd = fileno(stdin);

	struct termios info;
	tcgetattr(in_fd, &info);
	info.c_lflag &= ~ICANON;
	info.c_lflag &= ~ECHO;
	tcsetattr(in_fd, TCSANOW, &info);


	stack = calloc(STACK_ROWS, sizeof(char*));
	undo_stack = calloc(STACK_ROWS, sizeof(char*));
	temp_stack = calloc(STACK_ROWS, sizeof(char*));

	for(i = 0; i < STACK_ROWS; i++)
	{
		stack[i] = calloc(1, ROW_SIZE);
		undo_stack[i] = calloc(1, ROW_SIZE);
		temp_stack[i] = calloc(1, ROW_SIZE);
	}

	print_screen(1);
	
	int escape_state = 0;
#define ESCAPE_NONE 0
#define ESCAPE_START 1
#define ESCAPE_ARROW 2
#define ESCAPE_FUNCTION14 3
#define ESCAPE_FUNCTION58 4
#define ESCAPE_ESC 5
	while(1)
	{
		char c = 0;

		if(escape_state == ESCAPE_START)
		{
// wait for a code with timeout
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(in_fd, &rfds);
			FD_SET(0, &rfds);
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;
			int result = select(in_fd + 1, 
				&rfds, 
				0, 
				0, 
				&timeout);

//printf("main %d result=%d c=%d\n", __LINE__, result, c);
			if(result >= 0 && FD_ISSET(in_fd, &rfds))
			{
// next character in a code
				read(in_fd, &c, 1);
			}
			else
			{
// got bare escape key
				escape_state = ESCAPE_ESC;
			}
		}
		else
		{
			read(in_fd, &c, 1);
		}
//printf("main %d c=%d\n", __LINE__, c);
//continue;

		if(escape_state == ESCAPE_ESC)
		{
			// exit from rewind mode
			if(rewind_row > 0)
			{
				rewind_row = -1;
				x_offset = strlen(working_line());
				print_screen(1);
			}
			escape_state = ESCAPE_NONE;

		}
		else
		if(escape_state == ESCAPE_START)
		{
//printf("main %d\n", __LINE__);
			if(c == 91)
			{
				escape_state = ESCAPE_ARROW;
			}
			else
			if(c == 79)
			{
				escape_state = ESCAPE_FUNCTION14;
			}
			else
			if(c == 27)
			{
// repeated ESC character
			}
			else
			{
				escape_state = ESCAPE_NONE;
			}
		}
		else
		if(escape_state == ESCAPE_FUNCTION14)
		{
			escape_state = ESCAPE_NONE;
			switch(c)
			{
// F1
				case 80:
					function1();
					break;
// F2
				case 81:
					function2();
					break;

// F3
				case 82:
					function3();
					break;

// F4
				case 83:
					function4();
					break;
			}
		}
		else
		if(escape_state == ESCAPE_FUNCTION58)
		{
			escape_state = ESCAPE_NONE;
			switch(c)
			{
// F5
				case 53:
					function5();
					break;
				
// F6
				case 55:
					function6();
					break;

// F7
				case 56:
					break;

				case 57:
// F8
					break;
			}
		}
		else
		if(escape_state == ESCAPE_ARROW)
		{
			escape_state = ESCAPE_NONE;
			switch(c)
			{
				case 68:
// left
					if(x_offset > 0)
					{
						cursor_left(1);
						x_offset--;
						fflush(stdout);
					}
					break;
				
				case 67:
				{
// right
					int len = strlen(working_line());

					if(x_offset < len)
					{
						cursor_right(1);
						x_offset++;
						fflush(stdout);
					}
					break;
				}
				
				case 65:
// up
					if(rewind_row < STACK_ROWS - 1 && 
						stack[rewind_row + 1][0] != 0)
					{
// rewind a line
						rewind_row++;
						x_offset = strlen(stack[rewind_row]);
						print_screen(1);
					}
					break;

				case 66:
// down
					if(rewind_row > 0)
					{
// go to the next stack line
						rewind_row--;
						x_offset = strlen(stack[rewind_row]);
						print_screen(1);
					}
					else
					if(rewind_row == 0)
					{
// exit from rewind mode
						rewind_row = -1;
						x_offset = strlen(command_line);
						print_screen(1);
					}
					break;

				case 49:
					escape_state = ESCAPE_FUNCTION58;
					break;

// delete
				case 51:
					delete_char(1);
					break;
			}
		}
		else
// Instant unit conversion doesn't work because of 'ft' vs 'f'
// want to confirm commands with Enter to prevent typos
		switch(c)
		{
			case 27:
				escape_state = ESCAPE_START;
				break;

			case 126:
				break;

// backspace
			case 127:
				delete_char(0);
				break;

			case '\n':
// exit from rewind mode
				if(rewind_row >= 0)
				{
					rewind_row = -1;
					x_offset = strlen(working_line());
					print_screen(1);
				}
				else
				{

// submit the command line
					char string2[ROW_SIZE];
					strcpy(string2, command_line);
					command_line[0] = 0;
					x_offset = 0;
					process_line(string2);
				}
				break;

			default:
				insert_char(c);
				break;
		}

	}
}


