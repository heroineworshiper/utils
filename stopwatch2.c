/*
 * Stopwatch 2
 * Copyright (C) 2017  Adam Williams <broadcast at earthling dot net>
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





#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>



// big stopwatch
// *******  *******  ::  *******  *******  ::  *******  *******  ::  *******


#define WIDTH 75
#define HEIGHT 10
#define DIGIT_W 7
#define DIGIT_H 10
#define COLON_W 2
#define ESC 0x1b

typedef struct
{
	char *lines[DIGIT_H];
} digit_t;

const char *colon[DIGIT_H] = 
{
	"  ",
	"  ",
	"**",
	"**",
	"  ",
	"  ",
	"**",
	"**",
	"  ",
	"  ",
};

const digit_t digit0 = 
{
	"*******",
	"*******",
	"**   **",
	"**   **",
	"**   **",
	"**   **",
	"**   **",
	"**   **",
	"*******",
	"*******",
};

const digit_t digit1 = 
{
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
	"   **  ",
};

const digit_t digit2 = 
{
	"*******",
	"*******",
	"     **",
	"     **",
	"*******",
	"*******",
	"**     ",
	"**     ",
	"*******",
	"*******",
};

const digit_t digit3 = 
{
	"*******",
	"*******",
	"     **",
	"     **",
	"*******",
	"*******",
	"     **",
	"     **",
	"*******",
	"*******",
};

const digit_t digit4 = 
{
	"**   **",
	"**   **",
	"**   **",
	"**   **",
	"*******",
	"*******",
	"     **",
	"     **",
	"     **",
	"     **",
};

const digit_t digit5 = 
{
	"*******",
	"*******",
	"**     ",
	"**     ",
	"*******",
	"*******",
	"     **",
	"     **",
	"*******",
	"*******",
};

const digit_t digit6 = 
{
	"*******",
	"*******",
	"**     ",
	"**     ",
	"*******",
	"*******",
	"**   **",
	"**   **",
	"*******",
	"*******",
};

const digit_t digit7 = 
{
	"*******",
	"*******",
	"     **",
	"     **",
	"     **",
	"     **",
	"     **",
	"     **",
	"     **",
	"     **",
};

const digit_t digit8 = 
{
	"*******",
	"*******",
	"**   **",
	"**   **",
	"*******",
	"*******",
	"**   **",
	"**   **",
	"*******",
	"*******",
};

const digit_t digit9 = 
{
	"*******",
	"*******",
	"**   **",
	"**   **",
	"*******",
	"*******",
	"     **",
	"     **",
	"*******",
	"*******",
};

const digit_t *digits[] = 
{
    &digit0, 
    &digit1, 
    &digit2, 
    &digit3, 
    &digit4, 
    &digit5, 
    &digit6, 
    &digit7, 
    &digit8, 
    &digit9
};


char bitmap[WIDTH * HEIGHT];
char prev_bitmap[WIDTH * HEIGHT];

void draw_digit(int x, int y, const digit_t *ptr)
{
    int i, j;
    for(i = 0; i < DIGIT_H; i++)
    {
        for(j = 0; j < DIGIT_W; j++)
        {
            if(ptr->lines[i][j] != ' ')
            {
                bitmap[(y + i) * WIDTH + x + j] = 1;
            }
            else
            {
                bitmap[(y + i) * WIDTH + x + j] = 0;
            }
        }
    }
}

void draw_colon(int x, int y)
{
    int i, j;
    for(i = 0; i < DIGIT_H; i++)
    {
        for(j = 0; j < COLON_W; j++)
        {
            if(colon[i][j] != ' ')
            {
                bitmap[(y + i) * WIDTH + x + j] = 1;
            }
            else
            {
                bitmap[(y + i) * WIDTH + x + j] = 0;
            }
        }
    }
}

void draw_bitmap(int hours, int minutes, int seconds, int hseconds)
{
    int tseconds = hseconds / 10;
    draw_digit(0, 0, digits[hours / 10]);
    draw_digit(9, 0, digits[hours % 10]);
    draw_digit(22, 0, digits[minutes / 10]);
    draw_digit(31, 0, digits[minutes % 10]);
    draw_digit(44, 0, digits[seconds / 10]);
    draw_digit(53, 0, digits[seconds % 10]);
    draw_digit(66, 0, digits[tseconds]);
    draw_colon(18, 0);
    draw_colon(40, 0);
    draw_colon(62, 0);
}

void draw_console(int init, int all, int final)
{
    int i, j;
    int is_inv = 0;


    // cursor up
	if(!init)
	{
        putc(ESC, stdout);
        if(final)
		{
			printf("[%dA", HEIGHT + 1);
		}
		else
		{
			printf("[%dA", HEIGHT);
		}
	}

    for(i = 0; i < HEIGHT; i++)
    {
        int skip = 0;
//            putc(ESC, stdout);
//            printf("[%dC", HEIGHT / 2 - i / 2);

        for(j = 0; j < WIDTH; j++)
        {
            if(init || 
				all || 
				final ||
				bitmap[i * WIDTH + j] != prev_bitmap[i * WIDTH + j])
            {
                // cursor right
                if(skip > 0)
                {
                    putc(ESC, stdout);
                    printf("[%dC", skip);
                }

                if(bitmap[i * WIDTH + j])
                {
                    if(!is_inv)
                    {
                        putc(ESC, stdout); 
                        printf("[7m");
                        is_inv = 1;
                    }

//                     if(final)
// 					{
// 						printf("*");
// 					}
// 					else
					{
						printf(" ");
					}
                }
                else
                {
                    if(is_inv)
                    {
                        putc(ESC, stdout);
                        printf("[0m");
                        is_inv = 0;
                    }
					
//                     if(final)
// 					{
// 						printf("-");
// 					}
// 					else
					{
						printf(" ");
					}
                }

                skip = 0;
            }
            else
            {
                skip++;
            }
        }

        // cursor down
		if(init)
		{
			printf("\n");
		}
		else
		{
        	printf("\r");
        	putc(ESC, stdout);
        	printf("[1B");
		}
    }

    if(is_inv)
    {
        putc(ESC, stdout);
        printf("[0m");
    }
	

	
    memcpy(prev_bitmap, bitmap, WIDTH * HEIGHT);
    fflush(stdout);
}



int main()
{
    bzero(bitmap, WIDTH * HEIGHT);
    bzero(prev_bitmap, WIDTH * HEIGHT);
    draw_bitmap(0, 0, 0, 0);
	struct timeval current_time;
	struct timeval start_time;
	struct timeval select_time;


	while(1)
	{
		fd_set fds;

        draw_console(1, 0, 0);
		printf("Press Enter to stop.\r");

		gettimeofday(&start_time, 0);
		int prev_seconds = 0;
		while(1)
		{
			usleep(100000);
			gettimeofday(&current_time, 0);
			int64_t useconds = (current_time.tv_sec * 1000000 + current_time.tv_usec) -
				(start_time.tv_sec * 1000000 + start_time.tv_usec);
			int hours = useconds / ((int64_t)60 * 60 * 1000000);
			int minutes = (useconds / ((int64_t)60 * 1000000)) % 60;
			int seconds = (useconds / (int64_t)1000000) % 60;
			int hseconds = (useconds / 10000) % 100;
			
			
            draw_bitmap(hours, minutes, seconds, hseconds);
			
			
			if(seconds != prev_seconds)
			{
	            draw_console(0, 1, 0);
				prev_seconds = seconds;
			}
			else
			{
	            draw_console(0, 0, 0);
			}
			
			
//			printf("%02d:%02d:%02d:%02d\r",
//				hours, 
//				minutes,
//				seconds,
//				hseconds);
//			fflush(stdout);


			FD_ZERO(&fds);
			FD_SET(0, &fds);
			select_time.tv_sec = 0;
			select_time.tv_usec = 0;
			int got_stop = select(1, &fds, 0, 0, &select_time);
			if(got_stop)
			{
                draw_console(0, 0, 1);
				getc(stdin);
				break;
			}
		}


		printf("Press Enter to start.");
// erase to end of line
		putc(ESC, stdout);
		printf("[K\r");
		getc(stdin);
	}
}
