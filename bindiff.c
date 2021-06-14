#define LARGEFILE_SOURCE
#define LARGEFILE64_SOURCE 
#define FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>


#define BUFFER_SIZE 0x100000
#define THRESHOLD 0

int main(int argc, char *argv[])
{
    int threshold = THRESHOLD;
	int i;
    int current_file = 0;
    FILE *file1 = 0;
    FILE *file2 = 0;
    
	if(argc < 3)
	{
		printf("Need 2 files to compare.\n");
        printf("Example: bindiff -t 2048 <file 1> <file 2>\n");
        printf("    Show differences separated by over 2048 bytes.\n");
		exit(1);
	}


    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-t"))
        {
            threshold = atoi(argv[i + 1]);
            i++;
        }
        else
        {
	        if(current_file == 0)
            {
                file1 = fopen64(argv[i], "r");
	            if(!file1)
	            {
		            perror("Opening file 1");
		            exit(1);
	            }
                current_file++;
            }
            else
            if(current_file == 1)
            {

	            file2 = fopen64(argv[i], "r");
	            if(!file2)
	            {
		            perror("Opening file 2");
		            exit(1);
	            }
                current_file++;
            }
        }
    }

	unsigned char *buffer1 = calloc(1, BUFFER_SIZE);
	unsigned char *buffer2 = calloc(1, BUFFER_SIZE);
	int64_t current_position = 0;
    int got_diff = 0;
    int64_t last_diff = -0x7fffffff;
	while(!feof(file1) && !feof(file2))
	{
		printf("Testing byte 0x%lx\r", current_position);
		fflush(stdout);
		int file1_bytes = fread(buffer1, 1, BUFFER_SIZE, file1);
		int file2_bytes = fread(buffer2, 1, BUFFER_SIZE, file2);
		for(i = 0; i < file1_bytes && i < file2_bytes; i++)
		{
			if(buffer1[i] != buffer2[i])
			{
                last_diff = current_position + i;
                if(!got_diff)
                {
                    got_diff = 1;
    				printf("Got a difference in range 0x%lx-", current_position + i);
                }
//				exit(0);
			}
            else
            if(got_diff && current_position + i - last_diff > threshold)
            {
                got_diff = 0;
                printf("0x%lx\n", last_diff + 1);
            }
		}
		if(file1_bytes != file2_bytes)
		{
			printf("Files are different lengths.\n");
			exit(0);
		}
		current_position += file1_bytes;
	}
}
