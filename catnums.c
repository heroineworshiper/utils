// cat a bunch of files by filename numbers


// gcc -O2 -o catnums catnums.c

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>





#define TEXTLEN 1024
#define BUFSIZE 65536

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

    for(i = start_number1; i <= end_number1; i++)
    {
        sprintf(filename2, filename, i);
//        sprintf(string2, "cat \"%s\"", filename2);

        FILE *fd = fopen(filename2, "r");
        if(!fd)
        {
            fprintf(stderr, "Failed to open %s: %s\n", filename2, strerror(errno));
            exit(1);
        }
        else
        {
            struct stat ostat;
            stat(filename2, &ostat);
            int64_t size = ostat.st_size;
            fprintf(stderr, "Opened %s %ld bytes\n", filename2, (long)size);
            if(!dry_run)
            {
                uint8_t buffer[BUFSIZE];
                while(!feof(fd))
                {
                    int bytes_read = fread(buffer, 1, BUFSIZE, fd);
                    if(bytes_read > 0)
                    {
                        fwrite(buffer, 1, bytes_read, stdout);
                    }
                    else
                    {
                        fprintf(stderr, "Failed to read.  %s\n", strerror(errno));
                        break;
                    }
                }
            }
        }
        
        fclose(fd);
    }
    
}


