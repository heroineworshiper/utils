// push multiple files to a phone directory


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define TEXTLEN 1024

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage: push [-s device ID] [files] [phone directory]\n");
        
        exit(1);
    }
    
    int i;
    int first;
    char string[TEXTLEN];
    char device_text[TEXTLEN]  = { 0 };
    
    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-s"))
        {
            i++;
            sprintf(device_text, "-s %s", argv[i]);
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
        sprintf(string, "adb %s push %s %s", device_text, argv[i], dst);
        
        printf("%s\n", string);
        system(string);
    }
    
    
    
    
    
}







