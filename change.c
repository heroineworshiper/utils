/*
 * The final changer.
 * Copyright (C) 2023  Adam Williams <broadcast at earthling dot net>
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



// recursively change all the files in a large directory to the user & group
// does not dereference symbolic links
// TODO: permission bits?

// gcc -O3 -o /usr/bin/change change.c

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>


#define TEXTLEN 65536
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


int is_file(const char *path)
{
    struct stat ostat;
// treat failed stat & links as files
    if(stat(path, &ostat))
        return 1;

// S_ISLNK is broken, so test for links with readlink
    ssize_t link_len = readlink(path, string, TEXTLEN - 1);
//     if(link_len > -1) string[link_len] = 0;
//     printf("is_file %d: %s link_len=%d %s S_ISDI=%d S_ISLNK=%d\n",
//         __LINE__,
//         path,
//         link_len,
//         string,
//         S_ISDIR(ostat.st_mode),
//         S_ISLNK(ostat.st_mode));
// exit(1);

    if(link_len >= 0 ||  // symbolic link if >= 0
        !S_ISDIR(ostat.st_mode) || 
        S_ISLNK(ostat.st_mode)) // broken
        return 1;
    return 0;
}

void append_vector(vector_t *vector, char *string)
{
// test for dup
//     int i;
//     for(i = 0; i < vector->size; i++)
//     {
//         if(!strcmp(vector->files[i].string, string))
//         {
//             printf("append_vector %d: dup %s\n", __LINE__, string);
//             return;
//         }
//     }

//    printf("append_vector %d: %s\n", __LINE__, string);

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
    DIR *dirstream;
    struct dirent *new_filename;
    dirstream = opendir(dir);
    if(!dirstream) return;

    while(new_filename = readdir(dirstream))
	{
        if(!strcmp(new_filename->d_name, ".") ||
            !strcmp(new_filename->d_name, "..")) continue;
        char string[TEXTLEN];
        sprintf(string, "%s", dir);
        if(strlen(string) > 0 && string[strlen(string) - 1] != '/')
            strcat(string, "/");
        strcat(string, new_filename->d_name);
        append_vector(dst, string);

        if(dst->size > 0 && !(dst->size % 1000))
        {
            printf("listdir %d: total: %d\r", __LINE__, dst->size);
            fflush(stdout);
        }

        if(!is_file(string))
            listdir(string, dst);
    }
	closedir(dirstream);
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
            append_vector(&files, argv[i]);
            if(!is_file(argv[i]))
            {
                printf("Scanning %s\n", argv[i]);
                listdir(argv[i], &files);
            }
        }
    }
    printf("\n");

    printf("main %d: Changing %d files user=%s group=%s\n", 
        __LINE__,
        files.size, 
        user, 
        group);

    int uid, gid;
    for(i = 0; i < files.size; i++)
    {
        if(dry_run)
        {
            printf("%s\n", files.files[i].string);
        }
        else
        {
            const char *path = files.files[i].string;
            if(i == 0)
            {
// change just 1 with the command line to get the ID's
                sprintf(string, "chown -h %s %s", user, path);
                system(string);

                sprintf(string, "chgrp -h %s %s", user, path);
                system(string);
                
                struct stat ostat;
                if(stat(path, &ostat))
                {
                    printf("main %d: stat %s failed\n", __LINE__, path);
                    return;
                }
                
                uid = ostat.st_uid;
                gid = ostat.st_gid;
                printf("main %d: UID=%d GID=%d\n", __LINE__, uid, gid);
            }
            else
            {
                struct stat ostat;
                if(stat(path, &ostat))
                {
//                    printf("main %d: stat %s failed\n", __LINE__, path);
//                    return;
                }

// do it in C for speed
                if(ostat.st_uid != uid ||
                    ostat.st_gid != gid)
                    lchown(path, uid, gid);
            }
            
            if(i > 0 && !(i % 1000)) 
            {
                printf("main %d: Changed %d\r", __LINE__, i);
                fflush(stdout);
            }
        }
    }
    printf("\n");
}






