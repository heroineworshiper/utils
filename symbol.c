// what library in the current directory contains the symbol?

// gcc -O3 symbol.c -o symbol

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Need a symbol & some libraries\n");
        printf("Example: symbol *.so timer_create\n");
        return;
    }
    
    
    int i;
    char *symbol = argv[argc - 1];
    for(i = 1; i < argc - 1; i++)
    {
        char string[1024];
        char *path = argv[i];
        char *ptr = strrchr(path, '.');

// test file extension
        if(ptr && strcmp(ptr, ".o"))
        {
            sprintf(string, "bash -c 'nm %s |& grep %s'\n", path, symbol);
            //printf("Running %s", string);
            FILE *fd = popen(string, "r");
            while(!feof(fd))
            {
                char *result = fgets(string, 1024, fd);
                if(!result)
                {
                    break;
                }

                // error or junk
                if(strstr(string, "nm:"))
                {
                }
                else
                // line contains the symbol
                if((ptr = strstr(string, symbol)) != 0)
                {
                    // exact match
                    if(ptr > string && *(ptr - 1) == ' ' &&
                        *(ptr + strlen(symbol)) == '\n')
                    {
                        ptr = string;

                        // skip address
                        while(*ptr && isalnum(*ptr))
                        {
                            ptr++;
                        }

                        // skip whitespace
                        while(*ptr && (*ptr == ' ' || *ptr == '\t'))
                        {
                            ptr++;
                        }

                        // has a definition
                        if(*ptr == 'T' || *ptr == 't')
                        {
                            printf("%s: %s", path, string);
                        }
                    }
                }
            }

            fclose(fd);
        }
    }
}


