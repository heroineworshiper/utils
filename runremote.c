#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>




void usage()
{
	printf("Usage:\n"
		"runremote 1234        listen on port for commands\n"
		"runremote host:1234 command           run command remotely\n"); 
}


int read_socket(int socket_fd, char *data, int len)
{
	int bytes_read = 0;
	int offset = 0;

	while(len > 0 && bytes_read >= 0)
	{
		bytes_read = read(socket_fd, data + offset, len);
		if(bytes_read >= 0)
		{
			len -= bytes_read;
			offset += bytes_read;
		}
	}
	return offset;
}


int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		usage();
		return 1;
	}

	char *hostname;
	hostname = argv[1];
	char *ptr = strchr(hostname, ':');
	int do_server;

	if(!ptr)
	{
		do_server = 1;
	}
	else
	{
		do_server = 0;
		if(argc < 3)
		{
			printf("No command supplied.\n");
			usage();
			return 1;
		}
	}

	

	if(do_server)
	{
		int port = atoi(hostname);
		int socket_fd;
		struct sockaddr_in addr;
		if((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		{
			perror("socket");
			return 1;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons((unsigned short)port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if(bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		{
			perror("bind");
			return 1;
		}

		while(1)
		{
			if(listen(socket_fd, 1) < 0)
			{
				perror("listen");
				return 1;
			}

			struct sockaddr_in clientname;
			socklen_t size = sizeof(clientname);
			int new_socket_fd;
			if((new_socket_fd = accept(socket_fd,
				(struct sockaddr*)&clientname,
				&size)) < 0)
			{
				perror("accept");
				return 1;
			}
			
			char remote_command[1024];
			read_socket(new_socket_fd, remote_command, sizeof(remote_command));

			printf("Got \"%s\"\n", remote_command);

			system(remote_command);
		}
	}
	else
	{
		struct sockaddr_in addr;
		struct hostent *hostinfo;
		char *ptr = strchr(hostname, ':');
		char name[1024];
		int port;
		if(ptr)
		{
			ptr++;
			port = atoi(ptr);
		}
		strcpy(name, hostname);
		ptr = strchr(name, ':');
		if(ptr) *ptr = 0;

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		hostinfo = gethostbyname(name);
		if(hostinfo == NULL)
		{
			perror("gethostbyname");
			return 1;
		}
		addr.sin_addr = *(struct in_addr *)hostinfo->h_addr;


		int socket_fd;
		if((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		{
			perror("socket");
			return 1;
		}
		
		
		if(connect(socket_fd, 
			(struct sockaddr*)&addr, 
			sizeof(addr)) < 0)
		{
			perror("connect");
			return 1;
		}

		char *command = argv[2];
		char string[1024];
		bzero(string, sizeof(string));
		strcpy(string, command);
		write(socket_fd, string, strlen(string) + 1);
	}
}








