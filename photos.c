// Extract photos from broken memory stick

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 0x1000000
//#define START_OFFSET (2LL * 1024LL * 1024LL * 1024LL)
#define START_OFFSET 0

FILE *in = 0;
unsigned char *buffer = 0;
int buffer_ptr = 0;
int buffer_size = 0;
int64_t file_start = START_OFFSET;
int64_t pattern_offset = START_OFFSET;


unsigned char pattern[] = { 0xff, 0xd8, 0xff, 0xe0 };

// Delete number of bytes from buffer offset 0 and shift the remainder
// to byte 0.
void shift_buffer(int bytes)
{
	memcpy(buffer, buffer + bytes, buffer_size - bytes);
	buffer_size -= bytes;
	buffer_ptr -= bytes;
	file_start += bytes;
}



// Return 1 if pattern found.
// the start of the pattern in the file is put in pattern_offset
int next_pattern()
{
	int pattern_size = sizeof(pattern);
	int i;
	int got_it = 0;

	while(1)
	{
		if(buffer_size - buffer_ptr < pattern_size)
		{
// Extend buffer with new data
			int bytes_read = fread(buffer + buffer_size,
				1, 
				BUFFER_SIZE - buffer_size,
				in);
			if(bytes_read <= 0) return 0;
			buffer_size += bytes_read;
		}

		printf("next_pattern %d: offset=%lld\n", __LINE__, ftello64(in));

		while(buffer_ptr <= buffer_size - pattern_size)
		{
// Got it
/*
 * printf("%02x %02x %02x %02x\n", 
 * buffer[buffer_ptr],
 * buffer[buffer_ptr + 1],
 * buffer[buffer_ptr + 2],
 * buffer[buffer_ptr + 3]);
 */
			if(!memcmp(buffer + buffer_ptr, pattern, pattern_size))
			{
				pattern_offset = file_start + buffer_ptr;
				shift_buffer(buffer_ptr);
				buffer_ptr++;
				return 1;
			}
			buffer_ptr++;
		}

		shift_buffer(buffer_ptr);
	}

	return got_it;
}


int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("Extract photos from broken memory stick and put them in the current directory.\n");
		printf("Usage: photos <memory stick image>\n");
		exit(1);
	}


	char *inpath = argv[1];
	char outpath[1024];
	int64_t image_start;
	int64_t image_end;
	int64_t last_offset = START_OFFSET;
	int out_number = 0;
	in = fopen64(inpath, "r");
	if(!in)
	{
		fprintf(stderr, "fopen %s: %s", inpath, strerror(errno));
		exit(1);
	}
	fseeko64(in, START_OFFSET, SEEK_SET);

	buffer = calloc(1, BUFFER_SIZE);

// Find start of first image
	if(next_pattern())
	{
		while(1)
		{
			image_start = pattern_offset;
			if(next_pattern())
			{
				image_end = pattern_offset;
				unsigned char *image_buffer = calloc(1, image_end - image_start);
				fseek(in, image_start, SEEK_SET);
				fread(image_buffer, image_end - image_start, 1, in);
				fseek(in, file_start + buffer_size, SEEK_SET);

				sprintf(outpath, "image%06d.jpg", out_number);
				FILE *out = fopen(outpath, "r");
				if(out)
				{
					printf("File %s already exists.  Won't overwrite.\n",
						outpath);
//					exit(1);
				}

				out = fopen(outpath, "w");
				if(!out)
				{
					fprintf(stderr, "fopen %s for writing: %s\n",
						outpath,
						strerror(errno));
					exit(1);
				}

				fwrite(image_buffer, image_end - image_start, 1, out);
				fclose(out);
				free(image_buffer);

				printf("Wrote %s\n", outpath);
				out_number++;
			}
			else
				break;
		}
	}

	return 0;
}
