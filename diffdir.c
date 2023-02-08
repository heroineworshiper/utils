/*
 * diffdir
 * Copyright (C) 2020 Adam Williams <broadcast at earthling dot net>
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




// find differences between 2 directories between files of only the given types
// then print the missing files

// gcc -g diffdir.c -o /usr/bin/diffdir

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_EXTENSIONS 64
#define TEXTLEN 1024
#define MAX(x, y) ((x) > (y) ? (x) : (y))

char dir1[TEXTLEN] = { 0 };
char dir2[TEXTLEN] = { 0 };
char *extensions[MAX_EXTENSIONS] = { 0 };
int total_extensions = 0;
// only show filenames
int files_only = 1;
// ignore files starting in .
int ignore_hidden = 1;

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

vector_t dir1_files;
vector_t dir2_files;

typedef struct
{
// item in dir1_files
    int index1;
// item in dir2_files
    int index2;
} diff_order_t;
diff_order_t *diff_order;
int total_diffs = 0;


typedef struct
{
    file_t *file;
    char *dir1;
    char *dir2;
} missing_file_t;
missing_file_t *missing_files;
int total_missing_files = 0;

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

char *date_to_string(struct timespec date, char *string)
{
    struct tm *mod_time = localtime(&date.tv_sec);
	int month = mod_time->tm_mon + 1;
	int day = mod_time->tm_mday;
	int year = mod_time->tm_year + 1900;
    int hour = mod_time->tm_hour;
    int minute = mod_time->tm_min;
    int second = mod_time->tm_sec;
    sprintf(string, 
        "%d/%d/%d %d:%d:%d", 
        month,
        day,
        year,
        hour,
        minute,
        second);
    return string;
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

// char* get_relpath(vector_t *dir_vector, char *dir, int index)
// {
//     char *relpath = dir_vector->files[index].string + strlen(dir);
//     while(*relpath && *relpath == '/')
//     {
//         relpath++;
//     }
// 
//     return relpath;
// }

char* get_relpath(file_t *file, char *dir)
{
    char *relpath = file->string + strlen(dir);
    while(*relpath && *relpath == '/')
    {
        relpath++;
    }

    return relpath;
}

int sort_compare(const void *ptr1, const void *ptr2)
{
	file_t *item1 = (file_t*)ptr1;
	file_t *item2 = (file_t*)ptr2;
	return item2->date.tv_sec < item1->date.tv_sec;
}

// escape special characters
// void escape_chars(char *string)
// {
//     int i;
//     int len = strlen(string);
//     for(i = len - 1; i >= 0; i--)
//     {
//         
//     }
// }

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
            append_vector(dst, string, ostat.st_mtim);
        }
    }
    fclose(fd);
    
    if(!got_line)
    {
        printf("listdir %d: command failed command=%s fd=%p\n", 
            __LINE__, 
            string2, 
            fd);
    }
// sort by date
    qsort(dst->files, dst->size, sizeof(file_t), sort_compare);
}


void the_diff(char *relpath,
    file_t *file1, 
    file_t *file2)
{
    char string[TEXTLEN];
    char *abspath1 = file1->string;
    char *abspath2 = file2->string;
    
    
    sprintf(string, "diff \"%s\" \"%s\"", abspath1, abspath2);
//    printf("the_diff %d: %s\n", __LINE__, string);
    
    FILE *fd = popen(string, "r");
    int total = 0;
    while(!feof(fd))
    {
        char *result = fgets(string, TEXTLEN, fd);
        if(!result)
        {
            break;
        }

        if(total == 0)
        {
// show most recent date
            char date_string[TEXTLEN];
            if(file1->date.tv_sec > file2->date.tv_sec)
            {
                date_to_string(file1->date, date_string);
            }
            else
            {
                date_to_string(file2->date, date_string);
            }
            
            
            if(files_only)
            {
                printf("%s meld \"%s\" \"%s\"\n", 
                    date_string, 
                    abspath1, 
                    abspath2);
            }
            else
            {
                printf("\n*** %s:\n", relpath);
            }
        }
        total++;




        if(!files_only)
        {
            printf("%s", string);
        }
    }
    
    fclose(fd);
}


int diff_order_compare(const void *ptr1, const void *ptr2)
{
	diff_order_t *item1 = (diff_order_t*)ptr1;
	diff_order_t *item2 = (diff_order_t*)ptr2;

// files in diff operation 1
    file_t *file1 = &dir1_files.files[item1->index1];
    file_t *file2 = &dir2_files.files[item1->index2];
    int64_t date1 = MAX(file1->date.tv_sec, file2->date.tv_sec);
    
// files in diff operation 2
    file1 = &dir1_files.files[item2->index1];
    file2 = &dir2_files.files[item2->index2];
    int64_t date2 = MAX(file1->date.tv_sec, file2->date.tv_sec);

	return date1 < date2;
}

int missing_file_compare(const void *ptr1, const void *ptr2)
{
	missing_file_t *item1 = (missing_file_t*)ptr1;
	missing_file_t *item2 = (missing_file_t*)ptr2;

	return item1->file->date.tv_sec < item2->file->date.tv_sec;
}


void get_missing_files(vector_t *dir1_files, 
    vector_t *dir2_files,
    char *dir1,
    char *dir2)
{
    int i, j;
    for(i = 0; i < dir1_files->size; i++)
//    for(i = 0; i < dir1_files->size && i < 1; i++)
    {
        char *relpath1 = get_relpath(&dir1_files->files[i], dir1);
        int got_it = 0;
        
        for(j = 0; j < dir2_files->size; j++)
        {
            char *relpath2 = get_relpath(&dir2_files->files[j], dir2);
            
            if(!strcmp(relpath1, relpath2))
            {
                got_it = 1;
                break;
            }
            else
            {
//printf("get_missing_files %d relpath1=%s relpath2=%s\n", __LINE__, relpath1, relpath2);
            }
        }
        
        if(!got_it)
        {
            missing_files[total_missing_files].file = &dir1_files->files[i];
            missing_files[total_missing_files].dir1 = dir1;
            missing_files[total_missing_files].dir2 = dir2;
            total_missing_files++;
        }
    }
}

void main(int argc, char *argv[])
{
    if(argc < 3)
    {
        printf("Usage: diffdir <directory 1> <directory 2> [file extension] [file extension]\n");
        printf("Example: diffdir old new java kt c\n");
        exit(1);
    }

    int i;
    for(i = 1; i < argc; i++)
    {
        if(i == 1)
        {
            strcpy(dir1, argv[i]);
        }
        else
        if(i == 2)
        {
            strcpy(dir2, argv[i]);
        }
        else
        if(total_extensions < MAX_EXTENSIONS)
        {
            extensions[total_extensions++] = argv[i];
        }
    }

    if(strlen(dir1) > 0 && dir1[strlen(dir1) - 1] != '/')
    {
        strcat(dir1, "/");
    }

    if(strlen(dir2) > 0 && dir1[strlen(dir2) - 1] != '/')
    {
        strcat(dir2, "/");
    }

// list the contents of the directories
    bzero(&dir1_files, sizeof(vector_t));
    bzero(&dir2_files, sizeof(vector_t));
    listdir(dir1, &dir1_files);
    listdir(dir2, &dir2_files);
    
    int j;

// calculate the order in which to diff the files
    diff_order = calloc(sizeof(diff_order_t), MAX(dir1_files.size, dir2_files.size));
    total_diffs = 0;
    for(i = 0; i < dir1_files.size; i++)
    {
        char *relpath1 = get_relpath(&dir1_files.files[i], dir1);
        int got_it = 0;
        
        for(j = 0; j < dir2_files.size; j++)
        {
            char *relpath2 = get_relpath(&dir2_files.files[j], dir2);
            
            if(!strcmp(relpath1, relpath2))
            {
                got_it = 1;
                diff_order[total_diffs].index1 = i;
                diff_order[total_diffs].index2 = j;
                total_diffs++;
                break;
            }
        }
    }

// sort the diffs by the most recent of the 2 files
    qsort(diff_order, total_diffs, sizeof(diff_order_t), diff_order_compare);

// calculate the diffs
    for(i = 0; i < total_diffs; i++)
    {
        int index1 = diff_order[i].index1;
        int index2 = diff_order[i].index2;
        
        char *relpath1 = get_relpath(&dir1_files.files[index1], dir1);
        char *relpath2 = get_relpath(&dir2_files.files[index2], dir2);
            
        the_diff(relpath1, 
            &dir1_files.files[index1], 
            &dir2_files.files[index2]);
    }

// show files which don't exist
    missing_files = calloc(sizeof(missing_file_t), dir1_files.size + dir2_files.size);
    get_missing_files(&dir1_files, &dir2_files, dir1, dir2);
    get_missing_files(&dir2_files, &dir1_files, dir2, dir1);
    
    
    qsort(missing_files, total_missing_files, sizeof(missing_file_t), missing_file_compare);

    
    for(i = 0; i < total_missing_files; i++)
    {
        file_t *file = missing_files[i].file;
        char *relpath1 = get_relpath(file, missing_files[i].dir1);
        char date_string[TEXTLEN];
        printf("%s %s exists in %s but not %s\n", 
            date_to_string(file->date, date_string),
            relpath1,
            missing_files[i].dir1,
            missing_files[i].dir2);
    }

}




