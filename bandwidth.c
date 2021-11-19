// gcc -O2 -o bandwidth bandwidth.c


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>



#define TEXTLEN 1024

#define CAPTURE_PATH "bandwidth.cap"
#define MAX_CAPTURE 0x100000 * 2
#define USECOND_INTERVAL 1000000


char device[TEXTLEN];
unsigned char *capture_buffer = 0;
int capture_it = 0;
int capture_allocated = 0;
int capture_size = 0;
struct timeval current_time;
struct timeval new_time;

// store a command's output in a dynamically allocated string
char* download(char *command)
{
    char *result = 0;
    char buffer[TEXTLEN];
    int total_len = 0;
    FILE *fd = popen(command, "r");
    
    if(fd)
    {
        while(1)
        {
            int bytes_read = fread(buffer, 1, TEXTLEN, fd);
            if(bytes_read > 0)
            {
                result = realloc(result, total_len + bytes_read + 1);
                memcpy(result + total_len, buffer, bytes_read);
                total_len += bytes_read;
            }
            else
            {
                break;
            }
        }

        pclose(fd);
    }
    else
    {
        printf("download %d: Failed to open %s\n", __LINE__, command);
        exit(1);
    }
// null terminate the string
    result[total_len] = 0;
    return result;
}


void capture_text(char *text)
{
    if(capture_it)
    {
        if(capture_size > MAX_CAPTURE) return;
        int len = strlen(text);
        if(capture_size + len > capture_allocated)
        {
            capture_buffer = realloc(capture_buffer, capture_size + len);
            capture_allocated = capture_size + len;
        }
        memcpy(capture_buffer + capture_size, text, len);
        capture_size += len;
    }
}

void signal_handler(int signum)
{
    if(capture_it)
    {
        FILE *out = fopen(CAPTURE_PATH, "w");
        fwrite(capture_buffer, capture_size, 1, out);
        fclose(out);
    }
    exit(0);
}



int main(int argc, char *argv[])
{
	int64_t rx_bytes;
	int64_t tx_bytes;
	int64_t prev_rx_bytes = 0;
	int64_t prev_tx_bytes = 0;
	int use_bytes = 1;
    int use_table = 0;
	int i;
    device[0] = 0;

	if(argc > 1)
    {
		for(i = 1; i < argc; i++)
		{
        	if(!strcmp(argv[i], "-b"))
    			use_bytes = 0;
        	else
        	if(!strcmp(argv[i], "-t"))
            {
            	use_table = 1;
            }
			else
            {
				strcpy(device, argv[i]);
            }
		}
    }

    signal(SIGINT, signal_handler);



// get the 1st device
    if(device[0] == 0)
    {
        char *string = download("ifconfig");
        char *ptr = string;
        char *ptr2 = device;
        while(*ptr && *ptr != ' ' && *ptr != ':')
        {
            *ptr2++ = *ptr++;
        }
        free(string);
    }



	while(1)
	{
		char string[TEXTLEN];
		sprintf(string, "ifconfig %s", device);
        char *buffer = download(string);
        
//printf("main %d command=%s %s\n", __LINE__, string, buffer);
// Get RX bytes
#define RX_BYTES "RX bytes:"
		char *ptr = strstr((const char*)buffer, RX_BYTES);
		if(ptr)
		{
			ptr += strlen(RX_BYTES);
			sscanf(ptr, "%ld", &rx_bytes);
		}
        else
        {
#define RX_PACKETS "RX packets "
            ptr = strstr((const char*)buffer, RX_PACKETS);
            if(ptr)
            {
#define BYTES "bytes "
                ptr = strstr((const char*)ptr, BYTES);
                if(ptr)
                {
                    ptr += strlen(BYTES);
                    sscanf(ptr, "%ld", &rx_bytes);
                }
            }
        }

#define TX_BYTES "TX bytes:"
		ptr = strstr((const char*)buffer, TX_BYTES);
		if(ptr)
		{
			ptr += strlen(TX_BYTES);
			sscanf(ptr, "%ld", &tx_bytes);
		}
        else
        {
#define TX_PACKETS "TX packets "
            ptr = strstr((const char*)buffer, TX_PACKETS);
            if(ptr)
            {
                ptr = strstr((const char*)ptr, BYTES);
                if(ptr)
                {
                    ptr += strlen(BYTES);
                    sscanf(ptr, "%ld", &tx_bytes);
                }
            }
        }
        
        
		if(!prev_rx_bytes) prev_rx_bytes = rx_bytes;
		if(!prev_tx_bytes) prev_tx_bytes = tx_bytes;


//		if(rx_bytes != prev_rx_bytes || tx_bytes != prev_tx_bytes)
        gettimeofday(&new_time, 0);
        double seconds_elapsed = (double)(new_time.tv_usec - current_time.tv_usec) / 1000000 +
            (new_time.tv_sec - current_time.tv_sec);
        current_time = new_time;

        if(seconds_elapsed > 0.01)
		{
            char string[TEXTLEN];
            if(use_table)
            {
                sprintf(string, 
                    "%.1f\t%.1f\n", 
                    (double)(rx_bytes - prev_rx_bytes) * 8 / seconds_elapsed,
                    (double)(tx_bytes - prev_tx_bytes) * 8 / seconds_elapsed);
            }
            else
			if(use_bytes)
			{
				sprintf(string, "%s in: %.1f/%.1f  out: %.1f/%.1f\n",
                    device,
					(double)(rx_bytes - prev_rx_bytes) * 8 / seconds_elapsed,
					(float)(rx_bytes - prev_rx_bytes) / seconds_elapsed / 1024,
					(double)(tx_bytes - prev_tx_bytes) * 8 / seconds_elapsed,
					(float)(tx_bytes - prev_tx_bytes) / seconds_elapsed / 1024);
			}
			else
			{
				sprintf(string, "%s in: %ld (%.1f bits/sec) out: %ld (%.1f bits/sec)    \n",
                    device,
					rx_bytes,
					(double)(rx_bytes - prev_rx_bytes) * 8 / seconds_elapsed,
					tx_bytes,
					(double)(tx_bytes - prev_tx_bytes) * 8 / seconds_elapsed);
			}

            printf("%s", string);
            capture_text(string);
			fflush(stdout);
			prev_rx_bytes = rx_bytes;
			prev_tx_bytes = tx_bytes;
			seconds_elapsed = 0;
		}

		usleep(USECOND_INTERVAL);
		seconds_elapsed += (double)USECOND_INTERVAL / 1000000;
        free(buffer);
	}
}
