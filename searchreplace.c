#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>



#define UNDO_PATH "/tmp/searchreplace_undo"
FILE *undo_fd = 0;
int contents_offset = 0;
int total_contents = 0;
char **contents_files = 0;
int replace_all = 0;
int read_only = 0;

// Reset undo buffer
int reset_undo()
{
	remove(UNDO_PATH);
}


// Append filename to undo buffer
int append_undo(char *path)
{
	FILE *file = fopen(UNDO_PATH, "a");
	fprintf(file, "%s\n", path);
	fclose(file);
}

void get_backup_path(char *original, char *backup)
{
	sprintf(backup, "%s.orig", original);
}

int got_it(char *buffer_start, 
	char *ptr, 
	char *buffer_end, 
	char *search_text, 
	int len)
{
	int current_len = 0;

	if(!replace_all)
	{
// Preceeding character must be non alphanumeric or start of buffer
		if(ptr > buffer_start &&
			isalnum(*(ptr - 1)) &&
			*(ptr - 1) != '_')
			return 0;

// Following character must be non alphanumeric or end of buffer
		if(ptr + len < buffer_end &&
			isalnum(*(ptr + len)) &&
			*(ptr + len) != '_')
			return 0;
	}

	while(*ptr && 
		*search_text && 
		ptr < buffer_end &&
		current_len < len)
	{
// Character differs
		if(*ptr != *search_text) return 0;
		ptr++;
		search_text++;
		current_len++;
	}

// Both ended at same point and had same characters
	if(current_len == len) return 1;


// One string ended before len
	return 0;
}

int do_searchreplace(char *path, 
	char *search_text, 
	char *replace_text, 
	int write_it)
{
	int search_len = strlen(search_text);
	int replace_len = strlen(replace_text);



	FILE *file = fopen(path, "r");
	if(!file)
	{
		printf("couldn't open %s: %s\n", path, strerror(errno));
	}
	else
	{
		struct stat stat_buf;
		stat(path, &stat_buf);
		int size = stat_buf.st_size;
		int total_instances = 0;
		if(size > 0x10000000)
		{
			printf("file %s too big.\n");
		}
		else
		{
// Read file
			char *buffer = malloc(size);
			char *buffer_end = buffer + size;
			fread(buffer, 1, size, file);
			fclose(file);
			file = 0;

// Write file
			if(write_it && !read_only)
			{
// Back up original
				char backup_path[1024];
				get_backup_path(path, backup_path);
				FILE *backup = fopen(backup_path, "w");
				if(fwrite(buffer, 1, size, backup) < size)
				{
					fprintf(stderr, 
						"failed to back up %s: %s\n", 
						path,
						strerror(errno));
					return 1;
				}
				fclose(backup);

				append_undo(path);

// Overwrite original
				file = fopen(path, "w");
			}

// Scan for occurrences of search_string and write new file
			char *ptr = buffer;
			char *prev_ptr = buffer;
			while(ptr < buffer + size - search_len)
			{
				if(got_it(buffer, ptr, buffer_end, search_text, search_len))
				{
// Abort with success
					if(!write_it) return 1;

// Write data leading up to pointer
					if(write_it && !read_only) fwrite(prev_ptr, 1, ptr - prev_ptr, file);
// Skip search string.
					ptr += search_len;
// Set new start point for copying
					prev_ptr = ptr;
// Write replacement string
					if(write_it && !read_only) fwrite(replace_text, 1, replace_len, file);
					total_instances++;
				}
				else
					ptr++;
			}

// Write last of file
			if(write_it && !read_only)
			{
//printf("%p %p %p %p\n", prev_ptr, buffer_end, buffer_end - prev_ptr, file);
				fwrite(prev_ptr, 1, buffer_end - prev_ptr, file);
				fclose(file);
			}

			free(buffer);


	    	if(total_instances) printf("Replaced %d instances of \"%s\" with \"%s\" in %s\n", 
				total_instances,
	    	 	search_text, 
	    	 	replace_text,
	    		path);
/*
 *  			if(total_instances)
 * 	    		printf("Replaced %d instances in %s\n", 
 * 					total_instances,
 * 	    			path);
 */
		}
	}
	return 0;
}

int restore()
{
	FILE *file = fopen(UNDO_PATH, "r");
	if(!file)
	{
		fprintf(stderr, "can't open %s to perform undos: %s\n",
			UNDO_PATH,
			strerror(errno));
		return 1;
	}

// Swap each file in the list
	while(!feof(file))
	{
// Read original data
		char original_path[1024];
		int result = fscanf(file, "%s", original_path);
		if(result < 0) return 0;
		char backup_path[1024];
		get_backup_path(original_path, backup_path);

		struct stat stat_buf;
		int original_size = 0;
		int backup_size = 0;
		if(stat(original_path, &stat_buf))
		{
			fprintf(stderr, "can't stat %s: %s\n", original_path, strerror(errno));
			fclose(file);
			return 1;
		}
		original_size = stat_buf.st_size;
		if(stat(backup_path, &stat_buf))
		{
			fprintf(stderr, "can't stat %s: %s\n", backup_path, strerror(errno));
			fclose(file);
			return 1;
		}
		backup_size = stat_buf.st_size;

		char *original_buffer = malloc(original_size);
		char *backup_buffer = malloc(backup_size);
		FILE *original_fd = fopen(original_path, "r");
		if(!original_fd)
		{
			fprintf(stderr, "can't open %s for reading: %s\n", original_path, strerror(errno));
			fclose(file);
			return 1;
		}
		FILE *backup_fd = fopen(backup_path, "r");
		if(!original_fd)
		{
			fprintf(stderr, "can't open %s for reading: %s\n", backup_path, strerror(errno));
			fclose(file);
			fclose(original_fd);
			return 1;
		}
		if(fread(original_buffer, 1, original_size, original_fd) < original_size)
		{
			fprintf(stderr, "can't read %s: %s\n", original_path, strerror(errno));
			fclose(file);
			fclose(original_fd);
			fclose(backup_fd);
			return 1;
		}
		if(fread(backup_buffer, 1, backup_size, backup_fd) < backup_size)
		{
			fprintf(stderr, "can't read %s: %s\n", backup_path, strerror(errno));
			fclose(file);
			fclose(original_fd);
			fclose(backup_fd);
			return 1;
		}

		fclose(original_fd);
		fclose(backup_fd);




// Write swapped data
		if(!read_only)
		{
			original_fd = fopen(original_path, "w");
			if(!original_fd)
			{
				fprintf(stderr, "can't open %s for writing: %s\n", original_path, strerror(errno));
				fclose(file);
				return 1;
			}
			if(fwrite(backup_buffer, 1, backup_size, original_fd) < backup_size)
			{
				fprintf(stderr, "can't write %s: %s\n", original_path, strerror(errno));
				fclose(file);
				fclose(original_fd);
				return 1;
			}
			fclose(original_fd);

			backup_fd = fopen(backup_path, "w");
			if(!backup_fd)
			{
				fprintf(stderr, "can't open %s for writing: %s\n", backup_path, strerror(errno));
				fclose(file);
				return 1;
			}
			if(fwrite(original_buffer, 1, original_size, backup_fd) < original_size)
			{
				fprintf(stderr, "can't write %s: %s\n", backup_path, strerror(errno));
				fclose(file);
				fclose(backup_fd);
				return 1;
			}

		}
		fclose(backup_fd);

		printf("restored %s\n", original_path);
		free(original_buffer);
		free(backup_buffer);
	}

	fclose(file);
}

int main(int argc, char *argv[])
{
	int first_arg = 1;
	int do_restore = 0;
	int i;

	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-r"))
		{
			read_only = 1;
		}
		else
		if(!strcmp(argv[i], "-u"))
		{
			do_restore = 1;
		}
		else
		if(!strcmp(argv[i], "-a"))
		{
			replace_all = 1;
			first_arg++;
		}
		else
		{
			first_arg = i;
			break;
		}
	}

	if(do_restore)
	{
		restore();
		return 0;
	}


	if(argc < 3)
	{
		printf("Usage: \n");
		printf("searchreplace <options> <search text> <replace text> <file> <file> ...\n");
		printf("searchreplace -u - undo all the files in %s\n", 
			UNDO_PATH);
		printf("searchreplace -r - don't write anything\n");
		printf("searchreplace -a - replace all occurances instead of just word.\n");
		printf("\n");
		printf("Search and replace a text string in a file.\n");
		printf("The original file is backed up in <file>.orig.\n");
		printf("The search is case sensitive.\n");
		printf("The search string must be preceded and followed by non alphanumeric characters to match.\n");
		printf("Use the -a option to get it to count every occurance.\n");
		printf("Restore original files by running searchreplace -u.\n");
		printf("Redo replacement by running searchreplace -u again.\n");
		printf("This way you have undo and redo support.\n");
		printf("Only the last run of searchreplace can be undone.\n");
		printf("Files ending in .orig are always ignored.\n");
		exit(1);
	}

	char *search_text = argv[first_arg++];
	char *replace_text = argv[first_arg++];
	char *files[argc];
	int total_files = 0;
	for(i = first_arg; i < argc; i++)
	{
		char *ptr = argv[i] + strlen(argv[i]) - strlen(".orig");
		if(ptr < argv[i] ||
			strcmp(ptr, ".orig"))
			files[total_files++] = argv[i];
	}

	reset_undo();

	for(i = 0; i < total_files; i++)
	{
/*
 * 		printf("replacing \"%s\" with \"%s\" in %s\n", 
 * 			search_text, 
 * 			replace_text,
 * 			files[i]);
 */
// Don't overwrite the file initially.
		int result = do_searchreplace(files[i], search_text, replace_text, 0);
// Got 1 if the text was found.
		if(result) do_searchreplace(files[i], search_text, replace_text, 1);
	}
	return 0;
}







