// what file in the current directory contains the symbol?
// currently finds the definitions & usages

// gcc -O3 symbol.c -o symbol

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define TEXTLEN 1024
// ignore files starting in .
int ignore_hidden = 1;
#define MAX_EXTENSIONS 64
char *extensions[MAX_EXTENSIONS] = { 0 };
int total_extensions = 0;

typedef struct
{
    char *string;
    struct timespec date;
} file_t;

typedef struct
{
    file_t *files;
    int size;
    int allocated;
} vector_t;

void append_vector(vector_t *vector, char *string, struct timespec date)
{
    if(vector->allocated < vector->size + 1)
    {
        int new_allocated = vector->allocated * 2;
        if(new_allocated == 0)
        {
            new_allocated = 64;
        }
        
        file_t *new_files = calloc(sizeof(file_t), new_allocated);
        
        if(vector->files)
        {
            memcpy(new_files, vector->files, sizeof(file_t) * vector->size);
            free(vector->files);
        }
        
        vector->allocated = new_allocated;
        vector->files = new_files;
        
        
    }
    
    
    vector->files[vector->size].string = strdup(string);
    vector->files[vector->size].date = date;
    vector->size++;
}

char* get_filename(char *string)
{
    char *ptr = string + strlen(string) - 1;
    while(ptr > string)
    {
        if(*ptr == '/')
        {
            ptr++;
            break;
        }
        ptr--;
    }
    return ptr;
}

void listdir(char *dir, vector_t *dst)
{
    int i;
    char string[TEXTLEN];
    char string2[TEXTLEN];
    sprintf(string2, "find \"%s\" ", dir);
    if(total_extensions > 0)
    {
        strcat(string2, "\\( ");
        for(i = 0; i < total_extensions; i++)
        {
            if(i > 0)
            {
                strcat(string2, "-o ");
            }
            sprintf(string, "-name '*.%s' ", extensions[i]);
            strcat(string2, string);
        }
        strcat(string2, "\\)");
    }
    

//printf("listdir %d: %s\n", __LINE__, string2);
    FILE *fd = popen(string2, "r");
    int got_line = 0;
    while(!feof(fd))
    {
        char *result = fgets(string, TEXTLEN, fd);
        if(!result)
        {
            break;
        }
        got_line = 1;

// strip \n
        char *ptr = string + strlen(string);
        while(ptr >= string)
        {
            ptr--;
            if(*ptr == '\n')
            {
                *ptr = 0;
            }
        }
        
        int skip = 0;
// ignore directories
        struct stat ostat;
        stat(string, &ostat);
        if(S_ISDIR(ostat.st_mode))
        {
            skip = 1;
        }

// ignore .git
        if(!skip)
        {
            if(strstr(string, ".git/"))
            {
                skip = 1;
            }
        }
        
// ignore hidden files
        if(ignore_hidden && !skip)
        {
            char *filename = get_filename(string);
//          printf("listdir %d: %s\n", __LINE__, filename);
            if(strlen(filename) > 2 &&
                filename[0] == '.')
            {
                skip = 1;
            }
        }


        
        if(!skip)
        {
//printf("listdir %d %s\n", __LINE__, string);
            append_vector(dst, string, ostat.st_mtim);
        }
    }
    fclose(fd);
    
    if(!got_line)
    {
        printf("listdir %d: no files found. command=%s fd=%p\n", 
            __LINE__, 
            string2, 
            fd);
    }
}

void main(int argc, char *argv[])
{
    if(argc < 3)
    {
        printf("Usage: symbol <the symbol> [file extension] [file extension]\n");
        printf("Example: symbol timer_create o a so ko\n");
        return;
    }

    vector_t dir_files;
    bzero(&dir_files, sizeof(vector_t));

    int i;
    char *symbol = 0;
    for(i = 1; i < argc; i++)
    {
        if(i == 1)
        {
            symbol = argv[i];
        }
        else
        if(total_extensions < MAX_EXTENSIONS)
        {
            extensions[total_extensions++] = argv[i];
        }
    }
    listdir(".", &dir_files);

//printf("symbol=%s\n", symbol);

    for(i = 0; i < dir_files.size; i++)
    {
        char string[TEXTLEN];
        char *path = dir_files.files[i].string;
        char *ptr = strrchr(path, '.');

        sprintf(string, "bash -c 'nm %s |& grep %s'\n", path, symbol);
//printf("Running %s", string);
        FILE *fd = popen(string, "r");
        while(!feof(fd))
        {
            char *result = fgets(string, TEXTLEN, fd);
            if(!result)
            {
                break;
            }
//printf("    result=%s", result);

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
                    else
// has a usage
                    if(*ptr == 'U' || *ptr == 'u')
                    {
                        printf("%s: %s", path, string);
                    }
                }
            }
        }

        fclose(fd);
    }
}


