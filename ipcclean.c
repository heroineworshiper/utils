#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// erase all IPC objects.  Use for killing shitty goog programs
// gcc -o ipcclean ipcclean.c

#define TEXTLEN 1024

int main(int argc, char *argv[])
{
    FILE *fd = popen("ipcs", "r");
    char string[TEXTLEN];
    char string2[TEXTLEN];
    char *state = "";

    while(!feof(fd))
    {
        char *result = fgets(string, TEXTLEN, fd);
        
        if(!result)
        {
            break;
        }
        
        if(strcasestr(string, "Message Queues"))
        {
            state = "msg";
        }
        else
        if(strcasestr(string, "Shared Memory Segments"))
        {
            state = "shm";
        }
        else
        if(strcasestr(string, "Semaphore Arrays"))
        {
            state = "sem";
        }
        else
        if(string[0] == '0' && string[1] == 'x')
        {
// get the ID
            char *ptr = strchr(string, ' ');
            
            if(ptr)
            {
                while(*ptr == ' ')
                {
                    ptr++;
                }

                char *ptr2 = strchr(ptr, ' ');
                if(ptr2)
                {
                    *ptr2 = 0;
                }
                
                sprintf(string2, "ipcrm %s %s", state, ptr);
                
                
                printf("main %d %s\n", __LINE__, string2);
                system(string2);
            }
        }
    }
}




