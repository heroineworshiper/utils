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
        printf("Usage: segments URL <start number> <end number inclusive> [step size]\n");
        printf(" -n dry run\n");
        printf("Example: segments2 \"https://ga.video.cdn.pbs.org/videos/some_long_filename_%%05d.ts\" 1 268\n");
        printf("Example: segments2 \"https://ga.video.cdn.pbs.org/videos/some_long_filename_%%08X.ts\" 1 268\n");
        exit(1);
    }
    
    int i;
    int dry_run = 0;
    const char *url;
    int start_number = 0;
    int end_number = 0;
    int step = 1;
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
            start_number = atoi(argv[i]);
            argument++;
        }
        else
        if(argument == 2)
        {
            end_number = atoi(argv[i]);
            argument++;
        }
        else
        if(argument == 3)
        {
            step = atoi(argv[i]);
        }
    }

    
    char *url2 = malloc(strlen(url) + TEXTLEN);
    char *string2 = malloc(strlen(url) + TEXTLEN);

    printf("\nDownloading %s\nstart=%d\nend=%d\nstep=%d\n",
        url,
        start_number,
        end_number,
        step);
    printf("Press enter to continue\n");
    fgetc(stdin);

    for(i = start_number; i <= end_number; i += step)
    {
        sprintf(url2, url, i);
        sprintf(string2, "wget --no-check-certificate %s", url2);
        printf("main %d: %s\n", __LINE__, string2);
        if(!dry_run)
        {
            int _ = system(string2);
        }
    }
    
    
    
}



