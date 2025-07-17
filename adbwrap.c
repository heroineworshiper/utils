/*
 * adbwrap
 * Copyright (C) 2023-2025 Adam Williams <broadcast at earthling dot net>
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


// incomplete C version for testing problems in python
// gcc -O2 -o adbwrap adbwrap.c


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <termios.h>

#define TEXTLEN 1024
#define ADB_EXEC "adb"
#define MAX_DEVICES 256
#define MAX(x, y) ((x) > (y) ? (x) : (y))

int do_push = 0;
int do_get = 0;
int do_shell = 0;
int do_list = 0;
int do_del = 0;
int do_mkdir = 0;
int dry_run = 0;
int do_wait = 1;
int command_start = -1;
char device_name[TEXTLEN];
char **src_files;
char dst_path[TEXTLEN];

// array of strings
void init_vector(char ***vector)
{
    (*vector) = calloc(sizeof(char*), 1);
}

int vector_size(char **vector)
{
    int i = 0;
    while(vector[i] != 0) i++;
    return i;
}

void append_vector(char ***vector, char *string)
{
// expand the vector
    int total = vector_size(*vector);
//printf("append_vector %d string=%s total=%d\n", __LINE__, string, total);
    total++;
    char **vector2 = calloc(sizeof(char*), total + 1);
    memcpy(vector2, (*vector), sizeof(char*) * total);
    free((*vector));
    (*vector) = vector2;
    (*vector)[total - 1] = strdup(string);
    (*vector)[total] = 0;
}

void insert_vector(char ***vector, int before, char *string)
{
// expand the vector
    int total = vector_size(*vector);
    total++;
    char **vector2 = calloc(sizeof(char*), total + 1);
    memcpy(vector2, (*vector), sizeof(char*) * total);
    free((*vector));
    (*vector) = vector2;
    memmove((*vector) + before + 1, 
        (*vector) + before, 
        sizeof(char*) * (total - before));
    (*vector)[before] = strdup(string);
}

void delete_vector(char ***vector)
{
    int i = 0;
    while((*vector)[i])
    {
        free((*vector)[i]);
        i++;
    }
    free((*vector));
    (*vector) = 0;
}

void print_vector(char **vector)
{
    int i = 0;
    while(vector[i] != 0)
    {
        printf("%s ", vector[i]);
        i++;
    }
    printf("\n");
}

void init_console()
{
	struct termios info;
	tcgetattr(fileno(stdin), &info);
	info.c_lflag &= ~ICANON;
	info.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &info);    
}

void reset_console()
{
	struct termios info;
	tcgetattr(fileno(stdin), &info);
	info.c_lflag |= ICANON;
	info.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &info);
}

int have_mode()
{
    return do_push ||
        do_get ||
        do_shell |
        do_list ||
        do_del ||
        do_mkdir;
}

int text_to_mode(const char *text)
{
//printf("text_to_mode %d %s\n", __LINE__, text);

    const char *ptr = strrchr(text, '/');
    if(!ptr) ptr = text;
    else
        ptr++; // get the /

    if(!strcmp(ptr, "push") || !strcmp(ptr, "pU"))
        do_push = 1;
    else
    if(!strcmp(ptr, "pull") || !strcmp(ptr, "get") || !strcmp(ptr, "gE"))
        do_get = 1;
    else
    if(!strcmp(ptr, "shell") || !strcmp(ptr, "sH"))
        do_shell = 1;
    else
    if(!strcmp(ptr, "del") || !strcmp(ptr, "dE"))
        do_del = 1;
    else
    if(!strcmp(ptr, "list") || !strcmp(ptr,  "lI"))
        do_list = 1;
    else
    if(!strcmp(ptr, "mkdir"))
        do_mkdir = 1;

    return have_mode();
}

char** get_devices()
{
    char string[TEXTLEN];
    char string2[TEXTLEN];
    char string3[TEXTLEN];
    char **result;
    init_vector(&result);

    sprintf(string, "%s devices", ADB_EXEC);
    FILE *fd = popen(string, "r");
    char *ptr = string2;
    while(!feof(fd))
    {
        char c = fgetc(fd);
        *ptr++ = c;
//        printf("%c", c);
        if(c == '\n')
        {
            *ptr = 0;
//            printf("%d %s", (int)strlen(string2), string2);
            if(strncmp(string2, "List of", 7))
            {
                ptr = string2;
                char *ptr2 = string3;
                while(*ptr != 0 && *ptr != '\n' && *ptr != ' ' && *ptr != '\t')
                {
                    *ptr2++ = *ptr++;
                }
                *ptr2 = 0;

                if(strlen(string3) > 0)
                {
//                    printf("device: %s\n", string3);
                    append_vector(&result, string3);
                }
            }

            string2[0] = 0;
            ptr = string2;
        }
    }
    fclose(fd);


    return result;
}

// trap SIGINT
static void handler(int sig)
{
    printf("\nYou pressed Ctrl+C!\n");
//    if(sig == SIGINT) sigint_count++;
//    printf("sig_catch %d: sig=%d\n", __LINE__, sig);
}

void do_it(char ***args)
{
    int offset = 0;
    insert_vector(args, offset++, ADB_EXEC);
// insert the device option
    if(device_name[0])
    {
        insert_vector(args, offset++, "-s");
        insert_vector(args, offset++, device_name);
    }
    if(do_wait) insert_vector(args, offset++, "wait-for-usb-device");
    printf("do_it %d: ", __LINE__);
    print_vector(*args);
    if(!dry_run)
    {
        int stdin_fd[2];
        int stdout_fd[2];
        int stderr_fd[2];
        int _ = pipe(stdin_fd);
        _ = pipe(stdout_fd);
        _ = pipe(stderr_fd);
        pid_t pid = fork();
        if(pid == 0) // child process
        {
// Redirect stdin, stdout, stderr
            dup2(stdin_fd[0], STDIN_FILENO);
            dup2(stdout_fd[1], STDOUT_FILENO);
            dup2(stderr_fd[1], STDOUT_FILENO);
            setvbuf(stdout, NULL, _IONBF, 0); // No buffering
            setvbuf(stderr, NULL, _IONBF, 0); // No buffering

            execvp((*args)[0], (*args));

// Close unused pipe ends
            close(stdin_fd[1]);
            close(stdout_fd[0]);
            close(stderr_fd[0]);
            exit(0);
        }
        else // parent process
        {
// Close unused pipe ends
            close(stdin_fd[0]);
            close(stdout_fd[1]);
            close(stderr_fd[1]);

            while(1)
            {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(stdout_fd[0], &rfds);
                FD_SET(stderr_fd[0], &rfds);
                FD_SET(STDIN_FILENO, &rfds);
                
                
                int result = select(MAX(stdout_fd[0], stderr_fd[0]) + 1, 
			        &rfds, 
			        0, 
			        0, 
			        0);
                if(result < 0)
                {
// got a signal
                    printf("do_it %d: got signal\n", __LINE__);
                    return;
                }

                if(FD_ISSET(stdout_fd[0], &rfds))
                {
                    char c;
                    int _ = read(stdout_fd[0], &c, 1);
                    putchar(c);
                    fflush(stdout);
                }

                if(FD_ISSET(stderr_fd[0], &rfds))
                {
                    char c;
                    int _ = read(stderr_fd[0], &c, 1);
                    putchar(c);
                    fflush(stdout);
                }
                
                if(FD_ISSET(STDIN_FILENO, &rfds))
                {
                    char c = getchar();
                    int _ = write(stdin_fd[1], &c, 1);
                }
            }

// reader sees EOF
            close(stdin_fd[1]);
            close(stdout_fd[0]);
            close(stderr_fd[0]);
        }
    }
}

int main(int argc, char *argv[])
{
    if(argc < 2 && !text_to_mode(argv[0]))
    {
        printf("Wrapper for common ADB functions\n");
        printf("The program must be run as a symbolic link named pU, gE, lI, sH, dE\n");
        printf("    pU -> send a file to device\n");
        printf("    gE -> get a file from the device\n");
        printf("    lI -> list a directory on the device\n");
        printf("    sH -> shell on the device\n");
        printf("    dE -> delete file on the device\n");
        printf("    Aliases don't work.  Only symbolically linking the alternate name to adbwrap works\n");
        printf("Shell command: sH [-s device hint] [command]\n");
        printf("Push mode: pU [-n] [-s device hint] [files] [dst directory]\n");
        printf("    Push multiple files to a directory\n");
        printf("    -n dry run\n");
        printf("    device hint may be the minimum number of characters to match a device ID\n");
        printf("Example: pU -s Z -n satriani spear tori /sdcard/Music\n");
        exit(0);
    }
    
    device_name[0] = 0;
    dst_path[0] = 0;
    init_vector(&src_files);
    text_to_mode(argv[0]);

    int i;
    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-n")) dry_run = 1;
        else
        if(!strcmp(argv[i], "-s")) 
        {
            i++;
            strcpy(device_name, argv[i]);
            char **devices = get_devices();
            
            int j = 0;
            int got_it = 0;
            while(devices[j])
            {
// 1st characters match
//printf("main %d: %s %s\n", __LINE__, devices[j], device_name);
                if(!strncmp(devices[j], device_name, strlen(device_name)))
                {
                    strcpy(device_name, devices[j]);
                    printf("Using device %s\n", device_name);
                    got_it = 1;
                    break;
                }
                j++;
            }
            
            delete_vector(&devices);
            if(!got_it)
            {
                printf("Device %s not found\n", device_name);
                exit(0);
            }
        }
        else
        if(do_push)
        {
            if(i < argc - 1)
                append_vector(&src_files, argv[i]);
            else
                strcpy(dst_path, argv[i]);
        }
        else
        if(do_shell || do_list)
        {
            command_start = i;
            break;
        }
    }
    
    
    if(do_shell)
    {
        char **args;
        init_vector(&args);
        append_vector(&args, "shell");
        if(command_start > 0)
        {
            for(i = command_start; i < argc; i++)
                append_vector(&args, argv[i]);
        }

        signal(SIGINT, handler);
        signal(SIGTSTP, handler);
        init_console();
        do_it(&args);
        reset_console();
        delete_vector(&args);
    }
}





