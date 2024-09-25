/*
 * PUSH
 * Copyright (C) 2023-2024 Adam Williams <broadcast at earthling dot net>
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




// push multiple files to a phone directory
// TODO: match device with minimal number of chars
// gcc -o push push.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>


#define TEXTLEN 1024
char string[TEXTLEN];
char device_text[TEXTLEN]  = { 0 };
int dry_run = 0;

static int compare(const void *ptr1, const void *ptr2)
{
	char *item1 = *(char**)ptr1;
	char *item2 = *(char**)ptr2;
//printf("compare %s %s\n", item1, item2);
	return strcasecmp(item1, item2);
}

int push_file(char *src, char *dst)
{
    struct stat ostat;
    int i;
    if(!stat(src, &ostat))
    {
        if(S_ISDIR(ostat.st_mode))
		{
// create directory
            sprintf(string, "adb %s shell mkdir \"%s/%s\"", 
                device_text,
                dst,
                src);
            printf("%s\n", string);
            if(!dry_run) system(string);

// descend into directory
            char new_dst[TEXTLEN];
            sprintf(new_dst, "%s/%s", dst, src);
            printf("push_file %d: descending into %s\n", __LINE__, src);

            char **new_src = malloc(sizeof(char*));
            int new_total = 0;
            int new_alloc = 1;
            DIR *dirstream;
            dirstream = opendir(src);
            if(!dirstream)
            {
                printf("push_file %d: %s: %s\n",
                    __LINE__,
			        src,
			        strerror(errno));
                return 1;
            }
            
            struct dirent *new_filename;
            while(new_filename = readdir(dirstream))
	        {
// skip these files
                if(new_filename->d_name != 0 &&
	                strcmp(new_filename->d_name, ".") &&
	                strcmp(new_filename->d_name, "..") &&
                    new_filename->d_name[0] != '.')
                {
//printf("push_file %d %s\n", __LINE__, new_filename->d_name);
                    new_total++;
                    if(new_total > new_alloc)
                    {
                        new_alloc *= 2;
                        new_src = realloc(new_src, sizeof(char*) * new_alloc);
                    }
                    char *string2 = malloc(strlen(new_filename->d_name) + strlen(src) + 2);
                    sprintf(string2, "%s/%s", src, new_filename->d_name);
                    new_src[new_total - 1] = string2;
                }
            }
            
            qsort(new_src, new_total, sizeof(char*), compare);
            for(int i = 0; i < new_total; i++)
            {
//                printf("    %s\n", new_src[i]);
                push_file(new_src[i], new_dst);
            }
            
            for(int i = 0; i < new_total; i++)
            {
                free(new_src[i]);
            }
            free(new_src);
        }
        else
        {
            sprintf(string, "adb %s push \"%s\" \"%s\"", device_text, src, dst);
            printf("%s\n", string);
            if(!dry_run) system(string);
        }
    }
    else
    {
		printf("push_file %d: %s: %s\n",
            __LINE__,
			src,
			strerror(errno));
        return 1;
    }

}


int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        printf("Push multiple files to a phone directory\n");
        printf("Usage: push [-n] [-s device ID] [files] [phone directory]\n");
        printf(" -n dry run\n");
        printf("Example: push -n satriani spear tori /sdcard/Music\n");
        
        exit(1);
    }
    
    int i;
    int first;

    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-s"))
        {
            i++;
            sprintf(device_text, "-s %s", argv[i]);
        }
        else
        if(!strcmp(argv[i], "-n"))
        {
            dry_run = 1;
        }
        else
        {
            first = i;
            break;
        }
    }    
    
    
    char *dst = argv[argc - 1];
    
    for(i = first; i < argc - 1; i++)
    {
//printf("main %d: %s\n", __LINE__, argv[i]);
        push_file(argv[i], dst);
    }
    
    
    
    
    
}







