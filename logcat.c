/*
 * command line logcat
 *
 * Copyright (C) 2018 Adam Williams <broadcast at earthling dot net>
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



// gcc -o logcat logcat.c -lpthread




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define TEXTLEN 1024
#define OUTPUT "logcat.cap"
int verbose = 1;
int info_only = 0;
FILE *cap = 0;
FILE *fd = 0;
char *keyword = 0;
int pid = -1;
char *device = 0;

char types[TEXTLEN];

void reader(void *ptr)
{
    char string[TEXTLEN];
    while(!feof(fd))
    {
        char *result = fgets(string, TEXTLEN, fd);
        if(!result)
        {
            break;
        }
        
        
        if(strlen(string) > 33)
        {
    //        printf("%s", result);
            int pid2 = atoi(string + 19);
    //        printf("%d %d\n", pid, pid2);
            char type = string[31];

//printf("reader %d pid2=%d pid=%d type=%c\n", __LINE__, pid2, pid, type);
            if(pid2 == pid && 
                (verbose || type != 'V') &&
                (!info_only || type == 'I'))
            {
                printf("%s", result + 31);
                fprintf(cap, "%s", result + 31);
                fflush(cap);
            }
        }
    }
}
void main(int argc, char *argv[])
{
    char string[TEXTLEN];
    types[0] = 0;

    if(argc < 2)
    {
        printf("Usage: logcat [-iIs] keyword\n");
        printf(" -i - show only info & above.  Default is verbose\n");
        printf(" -I - show only info\n");
        printf(" -s - device\n");
        printf(" keyword - keyword from the process table to attach to\n");
        exit(1);
    }

    int i;
    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-i"))
        {
            verbose = 0;
        }
        else
        if(!strcmp(argv[i], "-I"))
        {
            verbose = 0;
            info_only = 1;
        }
        else
        if(!strcmp(argv[i], "-s"))
        {
            device = argv[i + 1];
            i++;
        }
        else
        {
            keyword = argv[i];
        }
    }

    if(!keyword)
    {
        printf("No keyword provided.\n");
        exit(1);
    }

// the capture file
    cap = fopen(OUTPUT, "w");
    if(!cap)
    {
        printf("Couldn't open %s\n", OUTPUT);
        exit(1);
    }




// the debugging output
    if(device)
        sprintf(string, "adb -s %s logcat", device);
    else
        sprintf(string, "adb logcat");
    fd = popen(string, "r");
    if(!fd)
    {
        printf("%s failed\n", string);
        exit(1);
    }

// start the reader
	pthread_attr_t  attr;
	pthread_attr_init(&attr);
	pthread_t tid;
	pthread_create(&tid, 
		&attr, 
		(void*)reader, 
		0);

    while(1)
    {
        if(device)
            sprintf(string, "adb -s %s shell ps | grep %s", device, keyword);
        else
            sprintf(string, "adb shell ps | grep %s", keyword);
//        sprintf(string, "adb shell ps | grep %s", keyword);
        int new_pid = -1;
        FILE *ps_fd = popen(string, "r");

        if(!ps_fd)
        {
            printf("%s failed\n", string);
            exit(1);
        }

        while(!feof(ps_fd))
        {
            char *result = fgets(string, TEXTLEN, ps_fd);
            if(!result)
            {
                break;
            }

//                printf("%s", result);

            char *ptr = strchr(result, ' ');
            while(ptr != 0 &&
                *ptr != 0 &&
                *ptr == ' ')
            {
                ptr++;
            }


            new_pid = atoi(ptr);
            if(new_pid != pid)
            {
                sprintf(string, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nUsing pid %d\n", new_pid);
                printf("%s", string);
                fprintf(cap, "%s", string);
                fflush(cap);
                pid = new_pid;
            }
            break;
        }


        fclose(ps_fd);

        sleep(1);
    }



}

