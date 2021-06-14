#include <sched.h>
#include <stdio.h>
#include <stdlib.h>



main(int argc, char *argv[])
{
	struct sched_param params;
	char *args[argc], file[1024];
	int i;
	int is_command = 0;
	int pid;
	args[0] = 0;

	if(argc < 2)
	{
		printf("Need a command or a PID you Mor*on!\n");
		exit(1);
	}

	if(!isdigit(argv[1][0]))
	{
		is_command = 1;
	}

	if(is_command)
	{
		strcpy(file, argv[1]);

		for(i = 1; i < argc; i++)
		{
			args[i - 1] = malloc(strlen(argv[i]) + 1);
			strcpy(args[i - 1], argv[i]);
		}

		args[i - 1] = 0;

		printf("Running ");
		for(i = 0; args[i] != 0; i++)
		  printf("%s ", args[i]);

		printf("in realtime\n");

		params.sched_priority = 1;
		if(sched_setscheduler(0, SCHED_RR, &params))
			perror("sched_setscheduler");

		execvp(file, args);
	}
	else
	{
		pid = atol(argv[1]);
		printf("Setting pid=%d to realtime\n", pid);
		params.sched_priority = 1;
		if(sched_setscheduler(pid, SCHED_RR, &params))
			perror("sched_setscheduler 1\n");
	}
}
