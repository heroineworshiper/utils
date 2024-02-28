/*
 * Fake terminal
 * Copyright (C) 2017-2024  Adam Williams <broadcast at earthling dot net>
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



// Serial terminal with custom baud

// cross compile: ./compile.sh terminal.c terminal
// gcc -O2 -o terminal terminal.c

// Hit ctrl-c a certain number of times to quit the terminal

// sample XBee commands:
// atbd13900
// atbd6


//#define TEST_MPU9150
//#define TEST_GYRO
//#define TEST_RADIO
//#define TEST_ADC
//#define TEST_IR
//#define ADD_NEWLINES



#define LOG_FILE "terminal.cap"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#ifndef __clang__
#include <linux/serial.h>
#endif 


// delay between characters required for programming bluetooth
int output_delay = 1000;
int send_sigint = 1;
// quit the terminal after a certain number of sigint's
int sigint_count = 0;
#define MAX_SIGINTS 5
int hex_output = 0;
int ascii_output = 1;
int local_echo = 0;
int send_cr = 1;

void quit()
{
    printf("quit %d: Giving up & going to a movie\n", __LINE__);
// reset the console
	struct termios info;
	tcgetattr(fileno(stdin), &info);
	info.c_lflag |= ICANON;
	info.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &info);
    exit(0);
}

// Copy of pic chksum routine
static uint16_t get_chksum(unsigned char *buffer, int size)
{
	int i;
	uint16_t result = 0;

	size /= 2;
	for(i = 0; i < size; i++)
	{
		uint16_t prev_result = result;
// Not sure if word aligned
		uint16_t value = (buffer[0]) | (buffer[1] << 8);
		result += value;
// Carry bit
		if(result < prev_result) result++;
		buffer += 2;
	}

	uint16_t result2 = (result & 0xff) << 8;
	result2 |= (result & 0xff00) >> 8;
	return result2;
}

// Returns the FD of the serial port
static int init_serial(char *path, int baud, int custom_baud)
{
	struct termios term;

	printf("init_serial %d: opening %s\n", __LINE__, path);

// Initialize serial port
	int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
	if(fd < 0)
	{
		printf("init_serial %d: path=%s: %s\n", __LINE__, path, strerror(errno));
		return -1;
	}
	
	if (tcgetattr(fd, &term))
	{
		printf("init_serial %d: path=%s %s\n", __LINE__, path, strerror(errno));
		close(fd);
		return -1;
	}


#ifndef __clang__
// Try to set kernel to custom baud and low latency
	if(custom_baud)
	{
		struct serial_struct serial_struct;
		if(ioctl(fd, TIOCGSERIAL, &serial_struct) < 0)
		{
			printf("init_serial %d: path=%s %s\n", __LINE__, path, strerror(errno));
		}

		serial_struct.flags |= ASYNC_LOW_LATENCY;
		serial_struct.flags &= ~ASYNC_SPD_CUST;
		if(custom_baud)
		{
			serial_struct.flags |= ASYNC_SPD_CUST;
			serial_struct.custom_divisor = (int)((float)serial_struct.baud_base / 
				(float)custom_baud + 0.5);
			baud = B38400;
		}  
/*
 * printf("init_serial: %d serial_struct.baud_base=%d serial_struct.custom_divisor=%d\n", 
 * __LINE__,
 * serial_struct.baud_base,
 * serial_struct.custom_divisor);
 */


// Do setserial on the command line to ensure it actually worked.
		if(ioctl(fd, TIOCSSERIAL, &serial_struct) < 0)
		{
			printf("init_serial %d: path=%s %s\n", __LINE__, path, strerror(errno));
		}
	}
#endif // !__clang__
/*
 * printf("init_serial: %d path=%s iflag=0x%08x oflag=0x%08x cflag=0x%08x\n", 
 * __LINE__, 
 * path, 
 * term.c_iflag, 
 * term.c_oflag, 
 * term.c_cflag);
 */
 
	tcflush(fd, TCIOFLUSH);
	cfsetispeed(&term, baud);
	cfsetospeed(&term, baud);
//	term.c_iflag = IGNBRK;
	term.c_iflag = 0;
	term.c_oflag = 0;
	term.c_lflag = 0;
//	term.c_cflag &= ~(PARENB | PARODD | CRTSCTS | CSTOPB | CSIZE);
//	term.c_cflag |= CS8;
//    term.c_cflag |= CRTSCTS; // flow control
	term.c_cc[VTIME] = 1;
	term.c_cc[VMIN] = 1;

// printf("init_serial: %d path=%s iflag=0x%08x lflag=0x%08x oflag=0x%08x cflag=0x%08x\n", 
// __LINE__, 
// path, 
// term.c_iflag, 
// term.c_lflag, 
// term.c_oflag, 
// term.c_cflag);

	if(tcsetattr(fd, TCSANOW, &term))
	{
		printf("init_serial %d: path=%s %s\n", __LINE__, path, strerror(errno));
		close(fd);
		return -1;
	}

	printf("init_serial %d: opened %s\n", __LINE__, path);
	return fd;
}

// Read a character
unsigned char read_char(int fd)
{
	unsigned char c;
	int result;

//printf("read_char %d\n", __LINE__);
	do
	{
		result = read(fd, &c, 1);
//printf("read_char %d %d %d\n", __LINE__, result, c);

		if(result <= 0)
		{
			printf("Unplugged\n");
			quit();
		}

	} while(result <= 0);
	return c;
}

// Send a character
void write_char(int fd, unsigned char c)
{
	int result;
	do
	{
		result = write(fd, &c, 1);
	} while(!result);
}


// trap SIGINT
static void sig_catch(int sig)
{
    sigint_count++;
//    printf("sig_catch %d: sig=%d\n", __LINE__, sig);
}

static void help()
{
    printf("Usage: terminal [options] [tty path]\n");
    printf("-c don't forward ctrl-c.  Otherwise hit ctrl-c %d times to quit the terminal\n",
        MAX_SIGINTS);
    printf("-e local echo\n");
    printf("-b baud rate\n");
    printf("-x hex output\n");
    exit(0);
}

int main(int argc, char *argv[])
{
	int baud = 115200;
	int baud_enum;
	int custom_baud = 0;
	char *path = 0;
	int i;

	if(argc > 1)
	{
		for(i = 1; i < argc; i++)
		{
            if(!strcmp(argv[i], "-h"))
            {
                help();
            }
            else
            if(!strcmp(argv[i], "-c"))
            {
                send_sigint = 0;
            }
            else
			if(!strcmp(argv[i], "-e"))
			{
				local_echo = 1;
			}
			else
			if(!strcmp(argv[i], "-b"))
			{
				baud = atoi(argv[i + 1]);
				i++;
			}
			else
			if(!strcmp(argv[i], "-x"))
			{
				hex_output = 1;
				ascii_output = 0;
			}
			else
			{
				path = argv[i];
			}
		}
	}

	switch(baud)
	{
		case 300: baud_enum = B300; break;
		case 1200: baud_enum = B1200; break;
		case 2400: baud_enum = B2400; break;
		case 9600: baud_enum = B9600; break;
		case 19200: baud_enum = B19200; break;
		case 38400: baud_enum = B38400; break;
		case 57600: baud_enum = B57600; break;
		case 115200: baud_enum = B115200; break;
		case 230400: baud_enum = B230400; break;
#ifndef __clang__
		case 460800: baud_enum = B460800; break;
		case 500000: baud_enum = B500000; break;
		case 921600: baud_enum = B921600; break;
		case 1000000: baud_enum = B1000000; break;
		case 1152000: baud_enum = B1152000; break;
		case 2000000: baud_enum = B2000000; break;
#endif // !__clang__
		default:
			baud_enum = B115200;
			custom_baud = baud;
			break;
	}

	printf("Fake terminal.  baud=%d hex_output=%d local_echo=%d send_cr=%d send SIGINT=%d output_delay=%d\n", 
		baud,
		hex_output,
		local_echo,
        send_cr,
        send_sigint,
        output_delay);
	if(path) printf("path=%s\n", path);

// This prevents ctrl-c from killing the terminal but doesn't allow us to
// forward it to the serial port.
//     sigset_t mask;
//     sigemptyset(&mask);
//     sigaddset(&mask, SIGINT);
//     if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
//         perror("Error masking SIGINT");
//         return 1;
//     }

    signal(SIGINT, sig_catch);

	FILE *fd = fopen(LOG_FILE, "w");
	if(!fd)
	{
		printf("Couldn't open %s\n", LOG_FILE);
	}

	int serial_fd = -1;
	if(path) serial_fd = init_serial(path, baud_enum, custom_baud);
	if(serial_fd < 0) serial_fd = init_serial("/dev/ttyUSB0", baud_enum, custom_baud);
	if(serial_fd < 0) serial_fd = init_serial("/dev/ttyUSB1", baud_enum, custom_baud);
	if(serial_fd < 0) serial_fd = init_serial("/dev/ttyUSB2", baud_enum, custom_baud);

	int test_state = 0;
	unsigned char test_buffer[32];
	int distance[4];
	int counter = 0;
	int counter2 = 0;
	int result2 = 0;
	int result3 = 0;
	double gyro_accum = 0;
	double temp_accum = 0;
	int sync_code_state = 0;

#ifdef TEST_GYRO
	int test_state2 = 0;
	int gyro_result1 = 0;
	int gyro_result2 = 0;
	double gyro_difference = 0;
	double prev_gyro_center1 = -1000000;
	double prev_gyro_center2 = -1000000;
	double gyro_center = 0;
	double gyro_center1 = 0;
	double gyro_accum1 = 0;
	double gyro_center2 = 0;
	double gyro_accum2 = 0;
	int gyro_counter = 0;
#endif


#ifdef TEST_RADIO
	int buffer_size = 0;
	int good_packets = 0;
#endif
	struct timeval start_time;
	struct timeval current_time;
	gettimeofday(&start_time, 0);

	struct termios info;
	tcgetattr(fileno(stdin), &info);
	info.c_lflag &= ~ICANON;
	info.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &info);

	fd_set rfds;

	while(1)
	{
		FD_ZERO(&rfds);
		FD_SET(serial_fd, &rfds);
		FD_SET(0, &rfds);
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;
		int result = select(serial_fd + 1, 
			&rfds, 
			0, 
			0, 
			&timeout);
//printf("main %d serial_fd=%d result=%d\n", __LINE__, serial_fd, result);

// got a signal.  
// So far, we're just handling SIGINT so pass it through
        if(result < 0) 
        {
            if(send_sigint)
                write_char(serial_fd, 0x3);
            if(sigint_count >= MAX_SIGINTS || !send_sigint)
            {
                quit();
            }
            continue;
        }

// data from serial port
		if(FD_ISSET(serial_fd, &rfds))
		{

#ifdef ADD_NEWLINES
// Newlines if delayed
			gettimeofday(&current_time, 0);
			if(current_time.tv_usec / 1000 + current_time.tv_sec * 1000 -
				start_time.tv_usec / 1000 - start_time.tv_sec * 1000 > 250)
			{
				printf("\n");
			}
			start_time = current_time;
#endif



			unsigned char c = read_char(serial_fd);

			if(fd)
			{
				fputc(c, fd);
				fflush(fd);
			}

			if(hex_output)
			{
				printf("%02x ", (unsigned char)c);
	//			if(c == 0xff) printf("\n");
	// 0xff2dd4 causes linefeed
				switch(sync_code_state)
				{
					case 1:
						if(c == 0x2d) 
						{
							sync_code_state = 2;
						}
						else
						{
							sync_code_state = 0;
						}
						break;

					case 2:
						if(c == 0xd4) 
						{
							printf("\n");
							counter = 0;
						}
						sync_code_state = 0;
						break;

					default:
						if(c == 0xff) sync_code_state = 1;
						break;
				}
				fflush(stdout);
			}


#ifdef TEST_IR

			switch(test_state)
			{
				case 0:
					if(c == 0x55)
						test_state++;
					break;
				case 1:
					if(c = 0x40)
					{
						test_state++;
						counter = 0;
					}
					else
						test_state = 0;
					break;
				default:
					test_buffer[counter++] = c;
					if(counter >= 8)
					{
						test_state = 0;
						distance[0] += (test_buffer[0] << 8) |
							test_buffer[1];
						distance[1] += (test_buffer[2] << 8) |
							test_buffer[3];
						distance[2] += (test_buffer[4] << 8) |
							test_buffer[5];
						distance[3] += (test_buffer[6] << 8) |
							test_buffer[7];
						counter2++;
						if(counter2 >= 3)
						{
							printf("%d %d %d %d\n", 
								distance[0] / counter2,
								distance[1] / counter2,
								distance[2] / counter2,
								distance[3] / counter2);
							counter2 = 0;
							distance[0] = 0;
							distance[1] = 0;
							distance[2] = 0;
							distance[3] = 0;
						}
					}
					break;
			}

#endif // TEST_IR










#ifdef TEST_MPU9150
		if(counter == 0)
		{
			int offset = 1;
			printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
				(int16_t)((test_buffer[offset + 0] << 8) | test_buffer[offset + 1]), 
				(int16_t)((test_buffer[offset + 2] << 8) | test_buffer[offset + 3]), 
				(int16_t)((test_buffer[offset + 4] << 8) | test_buffer[offset + 5]), 
				(int16_t)((test_buffer[offset + 6] << 8) | test_buffer[offset + 7]), 
				(int16_t)((test_buffer[offset + 8] << 8) | test_buffer[offset + 9]), 
				(int16_t)((test_buffer[offset + 10] << 8) | test_buffer[offset + 11]), 
				(int16_t)((test_buffer[offset + 12] << 8) | test_buffer[offset + 13]), 
				(int16_t)((test_buffer[offset + 14] << 8) | test_buffer[offset + 15]), 
				(int16_t)((test_buffer[offset + 16] << 8) | test_buffer[offset + 17]));
		}
		
		if(counter >= sizeof(test_buffer)) counter--;
		test_buffer[counter++] = c;
		

#endif // TEST_MPU9150




#ifdef TEST_ADC
			switch(test_state)
			{
				case 0:
					if(c == 0xf9) test_state++;
					break;
				case 1:
				case 2:
				case 3:
					test_buffer[test_state - 1] = c;
					test_state++;
					break;
				case 4:
					test_buffer[test_state - 1] = c;
					test_state = 0;
					gyro_accum += (int16_t)(test_buffer[0] | (test_buffer[1] << 8));
					temp_accum += (uint16_t)(test_buffer[2] | (test_buffer[3] << 8));
					counter++;

					if(counter >= 100)
					{
						gyro_accum /= counter;
						temp_accum /= counter;
//						result3 += result2 + 1;
/*
 * 						printf("%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d ", 
 * 							(result2 & 0x8000) ? 1 : 0,
 * 							(result2 & 0x4000) ? 1 : 0,
 * 							(result2 & 0x2000) ? 1 : 0,
 * 							(result2 & 0x1000) ? 1 : 0,
 * 							(result2 & 0x0800) ? 1 : 0,
 * 							(result2 & 0x0400) ? 1 : 0,
 * 							(result2 & 0x0200) ? 1 : 0,
 * 							(result2 & 0x0100) ? 1 : 0,
 * 							(result2 & 0x0080) ? 1 : 0,
 * 							(result2 & 0x0040) ? 1 : 0,
 * 							(result2 & 0x0020) ? 1 : 0,
 * 							(result2 & 0x0010) ? 1 : 0,
 * 							(result2 & 0x0008) ? 1 : 0,
 * 							(result2 & 0x0004) ? 1 : 0,
 * 							(result2 & 0x0002) ? 1 : 0,
 * 							(result2 & 0x0001) ? 1 : 0);
 */
						printf("%f\t%f\n", gyro_accum, temp_accum);
//						printf("%d\n", result3);
						gyro_accum = 0;
						temp_accum = 0;
						counter = 0;
					}

//					if(result > 20000) counter = 0;
//					counter++;
//					if(counter == 5)
//					{
//						counter2++;
//						result2 += result;
//						if(counter2 >= 30)
//						{
//							printf("%d\n", result2);
//							result2 = 0;
//							counter2 = 0;
//						}
//					}
					break;
			}
#endif

#ifdef TEST_GYRO
			switch(test_state)
			{
				case 0:
					if(c == 0xf9) test_state++;
					break;

				case 1:
				case 2:
				case 3:
					test_buffer[test_state - 1] = c;
					test_state++;
					break;
				
				case 4:
					test_buffer[3] = c;

					gyro_result1 = (int16_t)(test_buffer[0] | (test_buffer[1] << 8));
					gyro_result2 = (int16_t)(test_buffer[2] | (test_buffer[3] << 8));


//gyro_result2 = 0;

//					printf("%d\t%d\n", gyro_result1, gyro_result2);


					gyro_difference = gyro_result2 - gyro_result1;
					if(test_state2 == 0)
					{
						gyro_center += gyro_difference;
						gyro_center1 += gyro_result1;
						gyro_center2 += gyro_result2;
						gyro_counter++;
						if(gyro_counter > 1024)
						{
							test_state2++;
							gyro_center = gyro_center / gyro_counter;
							gyro_center1 = gyro_center1 / gyro_counter;
							gyro_center2 = gyro_center2 / gyro_counter;
							gyro_counter = 0;
							printf("gyro center=%f\t%f\t%f\n", gyro_center, gyro_center1, gyro_center2);

							if(fabs(prev_gyro_center1 - gyro_center1) > 0.1 ||
								fabs(prev_gyro_center2 - gyro_center2) > 0.1)
							{
								prev_gyro_center1 = gyro_center1;
								prev_gyro_center2 = gyro_center2;

// Try again
								test_state2 = 0;
								gyro_center = 0;
								gyro_center1 = 0;
								gyro_center2 = 0;
							}
						}
						else
						{
//							printf("%f\n", gyro_result);
						}
					}
					else
					if(test_state2 == 1)
					{
						gyro_accum += gyro_difference - gyro_center;
						gyro_accum1 += (double)gyro_result1 - gyro_center1;
						gyro_accum2 += (double)gyro_result2 - gyro_center2;
						gyro_counter++;
						if(gyro_counter > 10)
						{
							gyro_counter = 0;
							printf("%f\t%f\t%f\n", 
								gyro_accum / 300000 * 90, 
								gyro_accum1 / 150000 * 90, 
								gyro_accum2 / 150000 * 90);
						}
					}



					test_state = 0;
					break;
			}

#endif




			if(ascii_output)
			{
				printf("%c", c);
	//			if(c == 0xd) printf("\n");
				fflush(stdout);
			}



#ifdef TEST_RADIO
// Scan for start code
			if(buffer_size == 0)
			{
				if(c == 0xe5)
				{
					test_buffer[0] = c;
					buffer_size++;
				}
			}
			else
			{
				test_buffer[buffer_size++] = c;
				if(buffer_size >= sizeof(test_buffer))
				{
// Test CRC
					uint16_t chksum = get_chksum(test_buffer, sizeof(test_buffer) - 2);
					uint16_t pic_chksum = 
						*(uint16_t*)(test_buffer + sizeof(test_buffer) - 2);
					if(pic_chksum == chksum)
					{
						good_packets++;
//printf("main %d good_packets=%d\n", __LINE__, good_packets);
					}
					else
					{
printf("main %d\n", __LINE__);
					}

					buffer_size = 0;
				}
			}
#endif
		}

// data from console
		if(FD_ISSET(0, &rfds))
		{
			int i;
			int bytes = read(0, test_buffer, sizeof(test_buffer));
            sigint_count = 0;

			for(i = 0; i < bytes; i++)
			{
				char c = test_buffer[i];
				
				if(local_echo)
				{
					if(c < 0xa)
						printf("0x%02x ", c);
					else
						printf("%c", c);

					fflush(stdout);
				}

				if(c == 0xa)
				{
					if(send_cr)
					{
						write_char(serial_fd, 0xd);
					}
					
// delay to avoid overflowing a 9600 passthrough
// doesn't print without local echo
					usleep(output_delay);
					write_char(serial_fd, 0xa);
				}
				else
				{
				
					write_char(serial_fd, c);
				}

// delay to avoid overflowing a 9600 passthrough
				usleep(output_delay);
			}
		}


#ifdef TEST_RADIO
		gettimeofday(&current_time, 0);
		if(current_time.tv_usec / 1000 + current_time.tv_sec * 1000 -
			start_time.tv_usec / 1000 - start_time.tv_sec * 1000 > 1000)
		{
			start_time = current_time;
			printf("main %d bits/sec:\t%d\n", 
				__LINE__, 
				good_packets * sizeof(test_buffer) * 8);
			good_packets = 0;
		}
#endif

	}
}







