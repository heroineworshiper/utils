// download a segmented video file using a formatting code for the number


// gcc -O2 -o segments2 segments2.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXTLEN 1024

int main(int argc, char *argv[])
{
    if(argc < 4)
    {
        printf("Download a segmented video file by replacing the formatting code with the number\n");
        printf("Usage: segments URL <start number> <end number inclusive>\n");
        printf(" -n dry run\n");
        printf("Example: segments2 https://ga.video.cdn.pbs.org/videos/some_long_filename_%%05d.ts 1 268\n");
        exit(1);
    }
    
    int i;
    int dry_run = 0;
    const char *url;
    const char *start_number;
    const char *end_number;
    int argument = 0;
    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-n"))
        {
            dry_run = 1;
        }
        else
        if(argument == 0)
        {
            url = argv[i];
            argument++;
        }
        else
        if(argument == 1)
        {
            start_number = argv[i];
            argument++;
        }
        else
        if(argument == 2)
        {
            end_number = argv[i];
            argument++;
        }
    }

    
    int start_number1 = atoi(start_number);
    int end_number1 = atoi(end_number);
    char *url2 = malloc(strlen(url) + TEXTLEN);
    char *string2 = malloc(strlen(url) + TEXTLEN);

    for(i = start_number1; i <= end_number1; i++)
    {
        sprintf(url2, url, i);
        sprintf(string2, "wget %s", url2);
        printf("main %d: %s\n", __LINE__, string2);
        if(!dry_run)
        {
            system(string2);
        }
    }
    
    
    
}



