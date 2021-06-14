#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if(argc < 5)
	{
		fprintf(stderr, "Usage: <start date> <start time> <minutes> <command line>\n"
"Start up something at the given time, let it run for the given duration,\n"
"then kill it with SIGINT.\n"
"Used for things which absolutely must work.\n"
"start date - formatted as MM/DD\n"
"start time - formatted as HH:MM 24 hour clock\n");
		exit(1);
	}

// Convert date string to system date
	char start_time_string[1024];
	struct tm tm_struct;
	time_t time_value;
	time(&time_value);
	localtime_r(&time_value, &tm_struct);
	tm_struct.tm_sec = 0;
	sprintf(start_time_string, "%s %s", argv[1], argv[2]);
	strptime(start_time_string, "%m/%d %H:%M", &tm_struct);
	int minutes = atoi(argv[3]);
	int seconds = 60 * (atof(argv[3]) - minutes);

	char *ptr = asctime(&tm_struct);
	strcpy(start_time_string, ptr);

	fprintf(stderr, "Start time: %s", start_time_string);
	fprintf(stderr, "Duration: %dh:%dm:%ds\n", 
		minutes / 60,
		minutes % 60,
		seconds);

	char command_line[1024];
	char **argv_terminated = calloc(sizeof(char*), argc);
	int i;
	for(i = 4; i < argc; i++)
	{
		argv_terminated[i - 4] = strdup(argv[i]);
		strcat(command_line, argv[i]);
		strcat(command_line, " ");
	}
	fprintf(stderr, "Command: %s\n", command_line);

//	char **line = argv_terminated;
//	while(*line) fprintf(stderr, "%s\n", *line++);


// Delay section
	int64_t start_time = mktime(&tm_struct);
	while(time(0) < start_time)
	{
		sleep(1);

		time(&time_value);
		localtime_r(&time_value, &tm_struct);
		ptr = asctime(&tm_struct);
		strcpy(start_time_string, ptr);
		start_time_string[strlen(start_time_string) - 1] = 0;
		fprintf(stderr, "Current time: %s        \r", start_time_string);
		fflush(stderr);
	}

	fprintf(stderr, "\nStarting command\n");
	int pid = fork();

// Run section
	if(pid > 0)
	{
		start_time = time(0);
		int64_t stop_time = start_time + minutes * 60 + seconds;
		while(time(0) < stop_time)
		{
			sleep(1);
			time(&time_value);
			time_value -= start_time;
			fprintf(stderr, "Elapsed time: %dh:%dm:%ds     \r", 
				time_value / 3600,
				(time_value % 3600) / 60,
				time_value % 60);
			fflush(stderr);
		}

		fprintf(stderr, "\nSending SIGKILL to command\n");
		kill(pid, SIGKILL);
		waitpid(pid, 0, 0);
		exit(0);
	}
	else
	{
		int result = execve(argv_terminated[0], argv_terminated, environ);
		perror("execve");
//		system(command_line);
	}
}









