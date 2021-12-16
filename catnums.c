// cat a bunch of files by filename numbers


// gcc -O2 -o catnums catnums.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>





#define TEXTLEN 1024

int main(int argc, char *argv[])
{
    if(argc < 4)
    {
        printf("Cat a bunch of files by filename numbers.  The files are catted in the right order\n");
        printf("so 100 comes before 1000.\n");
        printf("Usage: catnums <filename format> <start number> <end number inclusive>\n");
        printf(" -n dry run\n");
        printf("Example: catnums %%d.ts 0 1600\n");
        exit(1);
    }
    
    int i;
    int dry_run = 0;
    const char *filename;
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
            filename = argv[i];
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
    char *filename2 = malloc(strlen(filename) + TEXTLEN);
    char *string2 = malloc(strlen(filename) + TEXTLEN);

    for(i = start_number1; i <= end_number1; i++)
    {
        sprintf(filename2, filename, i);
        sprintf(string2, "cat %s", filename2);
        if(!dry_run)
        {
            system(string2);
        }
        else
        {
            printf("main %d: %s\n", __LINE__, string2);
        }
    }
    
}


