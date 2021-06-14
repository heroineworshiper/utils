// print the bits per second of stdin

// gcc -o catbps catbps.c
// netcat -l -p 1234 | catbps

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUFLEN 1024

int main(int argc, char *argv[])
{
    unsigned char buffer[BUFLEN];
    struct timespec ts1;
    clock_gettime(CLOCK_REALTIME, &ts1);
    int64_t total_bytes = 0;
    int64_t total_bytes2 = 0;

    if(argc > 1)
    {
        if(!strcmp(argv[1], "-h"))
        {
            printf("print the bits per second of stdin\n");
            printf("Example: netcat -l -p 1234 | catbps\n");
            exit(0);
        }
    }

    while(1)
    {
        int64_t bytes_read = read(STDIN_FILENO, buffer, BUFLEN);
        
        if(bytes_read <= 0)
        {
            printf("main %d: unplugged\n", __LINE__);
            break;
        }
        total_bytes += bytes_read;
        total_bytes2 += bytes_read;
        
    
        struct timespec ts2;
        clock_gettime(CLOCK_REALTIME, &ts2);
        int64_t diff = (ts2.tv_nsec / 1000000 + ts2.tv_sec * 1000) -
            (ts1.tv_nsec / 1000000 + ts1.tv_sec * 1000);
        if(diff > 1000)
        {
            printf("main %d: %d bits/sec total_bytes=%jd\n", 
                __LINE__,
                (int)(total_bytes * 1000 * 8 / diff),
                total_bytes2);
            total_bytes = 0;
            ts1 = ts2;
        }
    }
}













