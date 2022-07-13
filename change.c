// recursively change all the files to the user & group
// the chown commands don't descend into every directory or change every file

// gcc -o change change.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define TEXTLEN 1024
char string[TEXTLEN];
char string2[TEXTLEN];

typedef struct
{
    char *string;
} file_t;

typedef struct
{
    file_t *files;
    int size;
    int allocated;
} vector_t;

vector_t files;


void append_vector(vector_t *vector, char *string)
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
    vector->size++;
}

void listdir(char *dir, vector_t *dst)
{
    int i;
    sprintf(string2, "find -L \"%s\" -type d", dir);

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
        
        append_vector(dst, string);
    }
    fclose(fd);
}


void main(int argc, char *argv[])
{
    int i;
    if(argc < 4)
    {
        printf("Usage: change [-n] <user> <group> <files>\n");
        printf("Example: change grid grid .\n");
        printf("Dry run: change -n grid grid .\n");
        exit(1);
    }

    int dry_run = 0;
    int argument = 0;
    char *user = 0;
    char *group = 0;
    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-n"))
        {
            dry_run = 1;
        }
        else
        if(argument == 0)
        {
            user = argv[i];
            argument++;
        }
        else
        if(argument == 1)
        {
            group = argv[i];
            argument++;
        }
        else
        {
            listdir(argv[i], &files);
        }
    }

    printf("Changing %d directories user=%s group=%s\n", files.size, user, group);

    for(i = 0; i < files.size; i++)
    {
        if(dry_run)
        {
            printf("%s\n", files.files[i].string);
        }
        else
        {
            printf("%s\r", files.files[i].string);
            fflush(stdout);

            sprintf(string, "chown -h %s %s", user, files.files[i].string);
//            printf("%s\r", string);
//            fflush(stdout);
            system(string);
            sprintf(string, "chown -h %s %s/*", user, files.files[i].string);
            system(string);

            sprintf(string, "chgrp -h %s %s", user, files.files[i].string);
//            printf("%s\r", string);
//            fflush(stdout);
            system(string);
            sprintf(string, "chgrp -h %s %s/*", user, files.files[i].string);
            system(string);
        }
    }
}






