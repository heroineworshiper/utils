// transfer a file over UDP

// gcc -O3 -o udptest udptest.c
// ~grid/compile.sh udptest.c udptest

// run the receiver 1st, then run the sender

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>


#define TEXTLEN 1024
#define HEADER_SIZE 4
#define REQ_HEADER 8
// data you want to transfer + HEADER_SIZE
#define PACKET_SIZE 1028
// maximum packets per request.  Limited by the size of the request packet.
#define MAX_PACKETS (PACKET_SIZE - REQ_HEADER)
// just the data
#define PACKET_DATA (PACKET_SIZE - HEADER_SIZE)
// data to write per request
#define BUFFER_SIZE (packets * PACKET_DATA)


// Commands sent by receiver
#define RESEND 0x1
#define START 0x2
#define FILENAME 0x3

char hostname[TEXTLEN];
char infile[TEXTLEN] = { 0 };
char outfile[TEXTLEN] = { 0 };
int port = -1;
// how many packets per request.  Limited by memory allocation in wisun-mc
int packets = 1;
FILE *out = 0;
FILE *in = 0;
uint8_t data[PACKET_SIZE] = { 0 };
uint8_t packet[PACKET_SIZE] = { 0 };
uint8_t received[MAX_PACKETS];
uint8_t *buffer = 0;
int read_socket = -1;
int write_socket = -1;
int ipv6 = 0;
int do_receive = 0;
int64_t buffer_start = 0;
struct sockaddr_in6 peer_addr;
struct sockaddr_in peer_addr4;
socklen_t peer_addr_len = sizeof(struct sockaddr_in6);
socklen_t peer_addr_len4 = sizeof(struct sockaddr_in);



typedef struct
{
	struct timeval start_time;
	struct timeval last_time;
	int64_t bytes_transferred;
	int64_t total_bytes;
} status_t;
status_t status;

void init_status(status_t *status)
{
	gettimeofday(&status->start_time, 0);
	gettimeofday(&status->last_time, 0);
	status->bytes_transferred = 0;
	status->total_bytes = 0;
}

void update_status(status_t *status, int bytes)
{
	struct timeval new_time;
	status->bytes_transferred += bytes;
	status->total_bytes += bytes;
	gettimeofday(&new_time, 0);
	if(new_time.tv_sec - status->last_time.tv_sec > 2)
	{
        const char *text = "received";
        if(!do_receive)
        {
            text = "sent";
        }
        int time_diff = (new_time.tv_sec - status->start_time.tv_sec) * 1000 +
            (new_time.tv_usec - status->start_time.tv_usec) / 1000;
		fprintf(stderr, 
            "%jd bytes %s.  %d bytes/sec         \n", 
			status->total_bytes,
            text,
			(int)(status->bytes_transferred * 1000 / time_diff));
		fflush(stdout);

		gettimeofday(&status->start_time, 0);
		gettimeofday(&status->last_time, 0);
		status->bytes_transferred = 0;
//		status->last_time = new_time;
	};
}

void stop_status()
{
	fprintf(stderr, "\nDone.\n");
}






int read_int32(uint8_t *data)
{
	unsigned int a, b, c, d;
	a = data[0];
	b = data[1];
	c = data[2];
	d = data[3];
	return (a << 24) | 
		(b << 16) | 
		(c << 8) | 
		(d);
}

void write_int32(uint8_t *data, int number)
{
	data[0] = (number >> 24) & 0xff;
	data[1] = (number >> 16) & 0xff;
	data[2] = (number >> 8) & 0xff;
	data[3] = number & 0xff;
}

int send_packet(int packet_number)
{
	fseek(in, packet_number * PACKET_DATA, SEEK_SET);
    bzero(data + 4, PACKET_DATA);
	int bytes_read = fread(data + 4, 1, PACKET_DATA, in);
	data[0] = (packet_number >> 24) & 0xff;
	data[1] = (packet_number >> 16) & 0xff;
	data[2] = (packet_number >> 8) & 0xff;
	data[3] = packet_number & 0xff;

//	int size = bytes_read + 4;
//	int i;
// printf("send_packet %d size=%d\n", __LINE__, size);
// for(i = 0; i < size; i++)
// printf("%02x ", (uint8_t)data[i]);
// printf("\n");

	int bytes_written = write(write_socket, data, PACKET_SIZE);
/*
* 		printf("bytes_written=%d packet_number=%lld\n", 
* 			bytes_written, 
* 			packet_number);
*/
	packet_number++;
	return packet_number;
}


int read_packet(int *packet_number)
{
	int bytes_read;

    if(!ipv6)
    {
//        bytes_read = read(read_socket, packet, PACKET_SIZE);
        bytes_read = recvfrom(read_socket,
            packet, 
            PACKET_SIZE, 
            0,
            (struct sockaddr *) &peer_addr4, 
            &peer_addr_len4);
    }
    else
    {
        bytes_read = recvfrom(read_socket,
            packet, 
            PACKET_SIZE, 
            0,
            (struct sockaddr *) &peer_addr, 
            &peer_addr_len);
    }
    
	*packet_number = -1;

	if(bytes_read > 3)
    {
		*packet_number = read_int32(packet);
    }

// printf("read_packet: got %d bytes packet_number=%d\n", 
// bytes_read,
// *packet_number);
	if(bytes_read < PACKET_SIZE)
		printf("read_packet: got %d bytes\n", bytes_read);
	return bytes_read;
}

int select_read_socket(int total_packets)
{
	fd_set read_set;
	struct timeval tv;
	FD_ZERO(&read_set);
	FD_SET(read_socket, &read_set);
// needs to be greater than the duration of NWK_WAKEUP_TIMEOUT_TICKS
// must wait for all expected packets to expire to avoid running out of memory in the RL78
	tv.tv_sec = 5 * total_packets;
	tv.tv_usec = 0;

	return select(read_socket + 1, &read_set, 0, 0, &tv);
}


static void sig_catch(int sig)
{
    exit(0);
}



void send_it()
{
    int i, j;
	int64_t total_bytes = 0;

	printf("Sending to %s\n"
        "ipv6=%d\n"
		"port=%d\n"
		"infile=%s\n",
		hostname,
        ipv6,
		port,
		infile);

    if(strchr(hostname, ':'))
    {
        ipv6 = 1;
    }



    int got_it = 0;
    if(ipv6)
    {
// create write socket for IPV6
        struct addrinfo hints;
        struct addrinfo *addrinfo_result = 0;
        struct addrinfo *rp = 0;
        char port_text[TEXTLEN];
        sprintf(port_text, "%d", port);
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET6;    /* Allow IPv4 or IPv6 */
        hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
        hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
        int result = getaddrinfo(hostname, 
            port_text, 
            &hints, 
            &addrinfo_result);
        int got_it = 0;

        write_socket = socket(AF_INET6,
            SOCK_DGRAM,
            IPPROTO_UDP);
        if((result = connect(write_socket, 
            addrinfo_result->ai_addr, 
            addrinfo_result->ai_addrlen)) != -1)
        {
            printf("send_it %d connected write_socket=%d\n", 
                __LINE__, 
                write_socket);
        }
        else
        {
            printf("send_it %d connecting write_socket returned %d\n", __LINE__, result);
            exit(1);
        }

// create read socket for IPV6
        struct sockaddr_in6 serveraddr;
        read_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin6_family = AF_INET6;
        serveraddr.sin6_port   = htons(port);
        serveraddr.sin6_addr   = in6addr_any;
        if((result = bind(read_socket, 
            (struct sockaddr *)&serveraddr, 
            sizeof(serveraddr)) == 0))
        {
            printf("send_it %d bound read_socket=%d\n", __LINE__, read_socket);
        }
        else
        {
            printf("send_it %d binding read_socket failed\n", __LINE__);
            exit(1);
        }
    }
    else
    {
	    write_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	    struct sockaddr_in write_addr;
	    struct hostent *hostinfo;

	    write_addr.sin_family = AF_INET;
	    write_addr.sin_port = htons((unsigned short)port);
	    hostinfo = gethostbyname(hostname);

	    if(hostinfo == NULL)
        {
    	    fprintf (stderr, "send_it: unknown host %s.\n", hostname);
    	    exit(1);
        }

	    write_addr.sin_addr = *(struct in_addr *)hostinfo->h_addr;

	    if(connect(write_socket, 
		    (struct sockaddr*)&write_addr, 
		    sizeof(write_addr)) < 0)
	    {
		    perror("send_it: connect");
            exit(1);
	    }

// create read socket for IPV4
	    read_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	    struct sockaddr_in read_addr;
	    read_addr.sin_family = AF_INET;
	    read_addr.sin_port = htons((unsigned short)port);
	    read_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	    if(bind(read_socket, 
		    (struct sockaddr*)&read_addr, 
		    sizeof(read_addr)) < 0)
	    {
		    perror("send_it: bind");
	    }
        else
        {
            got_it = 1;
        }
    }









	init_status(&status);
	int packet_number = 0;
    int packets = 0;



// Send filename and size
	struct stat file_status;
	bzero(&file_status, sizeof(struct stat));
	stat(infile, &file_status);
	uint64_t file_size = file_status.st_size;
	uint8_t *ptr = data;
	*ptr++ = FILENAME;
	strcpy((char*)ptr, infile);
	ptr += strlen(infile);
	*ptr++ = 0;
	*ptr++ = (file_size >> 56) & 0xff;
	*ptr++ = (file_size >> 48) & 0xff;
	*ptr++ = (file_size >> 40) & 0xff;
	*ptr++ = (file_size >> 32) & 0xff;
	*ptr++ = (file_size >> 24) & 0xff;
	*ptr++ = (file_size >> 16) & 0xff;
	*ptr++ = (file_size >> 8) & 0xff;
	*ptr++ = file_size & 0xff;
	int size = ptr - data;

//printf("send_it %d size=%d\n", __LINE__, size);
// for(i = 0; i < size; i++)
// printf("%02x ", data[i]);
// printf("\n");
	int _ = write(write_socket, data, size);


	in = fopen(infile, "r");
	while(1)
	{
// Get receiver request
		int bytes_read = 0;

printf("send_it %d waiting for receiver\n", __LINE__);
        if(!ipv6)
        {
            bytes_read = read(read_socket, data, PACKET_SIZE);
        }
        else
        {
            struct sockaddr_storage peer_addr;
            socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
            bytes_read = recvfrom(read_socket,
                data, 
                PACKET_SIZE, 
                0,
                (struct sockaddr *) &peer_addr, 
                &peer_addr_len);
        }

// printf("receive_it %d bytes_read=%d\n", __LINE__, bytes_read);
// for(i = 0; i < bytes_read; i++)
// {
//     printf("%c ", data[i]);
// }
// printf("\n");

		if(bytes_read)
		{
			switch(data[0])
			{
				case START:
					if(bytes_read >= REQ_HEADER)
					{
						packet_number = read_int32(data + 1);
                        packets = (((uint32_t)data[5]) << 16) |
                            (((uint32_t)data[6]) << 8) |
                            ((uint32_t)data[7]);
// copy packets received
                        for(i = 0; i < packets; i++)
                        {
                            received[i] = data[REQ_HEADER + i];
                        }
printf("receive_it %d receiver requested packets %d - %d\n", 
__LINE__, 
packet_number,
packet_number + packets);
					}
					break;
			}
		}

printf("send_it %d sending packets %d - %d\n", 
__LINE__, 
packet_number,
packet_number + packets);
        for(i = 0; i < packets; i++)
        {
            if(!received[i])
            {
    		    send_packet(i + packet_number);
		        total_bytes += PACKET_DATA;
		        update_status(&status, PACKET_DATA);
            }
        }
	}
}





void receive_it()
{
    int i, j;
	int64_t file_size = 0;

	printf("receiving ipv6=%d\n"
        "packets=%d\n"
		"port=%d\n",
		ipv6,
        packets,
		port);

    

    buffer = (uint8_t*)malloc(BUFFER_SIZE);

    int got_it = 0;
    if(ipv6)
    {
// create read socket for IPV6
        struct sockaddr_in6 serveraddr;
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin6_family = AF_INET6;
        serveraddr.sin6_port   = htons(port);
        serveraddr.sin6_addr   = in6addr_any;
	    read_socket = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

        int result;
        if((result = bind(read_socket, 
            (struct sockaddr *)&serveraddr, 
            sizeof(serveraddr))) != 0)
        {
            printf("receive_it %d bind returned %d %s\n", 
                __LINE__, 
                result,
                strerror(errno));
            exit(1);
        }

        got_it = 1;
        printf("receive_it %d got read_socket=%d\n", __LINE__, read_socket);

    }
    else
    {
// create read socket for IPV4
	    read_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	    struct sockaddr_in read_addr;
	    read_addr.sin_family = AF_INET;
	    read_addr.sin_port = htons((unsigned short)port);
	    read_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	    if(bind(read_socket, 
		    (struct sockaddr*)&read_addr, 
		    sizeof(read_addr)) < 0)
	    {
		    perror("receive_it: bind");
            exit(1);
	    }
    }

    

	int expected_packet_number = 0;
    struct timeval file_start_time;
	while(1)
	{
// Filename and size.
		int got_file = 0;
		int packet_number = -1;
		int bytes_read = read_packet(&packet_number);
		if(bytes_read > 0)
		{
			if(packet[0] == FILENAME)
			{
				strcpy(outfile, (char*)(packet + 1));
				uint8_t *ptr = packet + 1 + strlen(outfile) + 1;
				uint32_t sizeh = read_int32(ptr);
				ptr += 4;
				uint32_t sizel = read_int32(ptr);
				file_size = (((uint64_t)sizeh) << 32) | sizel;
				printf("Receiving '%s' size=%ld\n", outfile, file_size);
				got_file = 1;
                gettimeofday(&file_start_time, 0);
			}
            else
            {
                printf("receive_it %d: got unknown packet.  Got\n", 
                    __LINE__);
//                packet[bytes_read] = 0;
//                printf("%s\n", packet + 10);
                for(i = 0; i < bytes_read; i++)
                {
                    printf("%02x ", packet[i]);
                }
                printf("\n");
            }
		}

		if(got_file)
		{
// create the write socket to the peer
            if(ipv6)
            {
                printf("receive_it %d peer=%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port=%d\n", 
                    __LINE__, 
                    peer_addr.sin6_addr.s6_addr[0], 
                    peer_addr.sin6_addr.s6_addr[1], 
                    peer_addr.sin6_addr.s6_addr[2], 
                    peer_addr.sin6_addr.s6_addr[3], 
                    peer_addr.sin6_addr.s6_addr[4], 
                    peer_addr.sin6_addr.s6_addr[5], 
                    peer_addr.sin6_addr.s6_addr[6], 
                    peer_addr.sin6_addr.s6_addr[7], 
                    peer_addr.sin6_addr.s6_addr[8], 
                    peer_addr.sin6_addr.s6_addr[9], 
                    peer_addr.sin6_addr.s6_addr[10], 
                    peer_addr.sin6_addr.s6_addr[11], 
                    peer_addr.sin6_addr.s6_addr[12], 
                    peer_addr.sin6_addr.s6_addr[13], 
                    peer_addr.sin6_addr.s6_addr[14], 
                    peer_addr.sin6_addr.s6_addr[15], 
                    peer_addr.sin6_port);


// connection to peer
                if(addrinfo_result)
                {
// create write socket for IPV6
                    write_socket = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

printf("receive_it %d ai_addr port=%d peer_port=%d\n", 
__LINE__,
((struct sockaddr_in6*)addrinfo_result->ai_addr)->sin6_port,
peer_addr.sin6_port);
//                    if((result = connect(write_socket, 
//                        addrinfo_result->ai_addr, 
//                        addrinfo_result->ai_addrlen)) != -1)

                    peer_addr.sin6_port = htons((unsigned short)port);
                    if((result = connect(write_socket, 
                        (const struct sockaddr *)&peer_addr, 
                        peer_addr_len)) != -1)
                    {
                        got_it = 1;
                        printf("receive_it %d connected to peer write_socket=%d\n", __LINE__, write_socket);
                    }
                    else
                    {
                        printf("receive_it %d connect returned %d\n", __LINE__, result);
                    }
                }
            }
            else
            {
// IPV4
                printf("receive_it %d peer=%s port=%d\n", 
                    __LINE__, 
                    inet_ntoa(peer_addr4.sin_addr),
                    port);

// create write socket for IPV4
	            write_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	            struct sockaddr_in write_addr;
	            write_addr.sin_family = AF_INET;
	            write_addr.sin_port = htons((unsigned short)port);
                write_addr.sin_addr = peer_addr4.sin_addr;

// symbolic connection to the peer
	            if(connect(write_socket, 
		            (struct sockaddr*)&write_addr, 
		            sizeof(write_addr)) < 0)
	            {
		            perror("receive_it: connect");
	            }


            }

			out = fopen(outfile, "w");
            if(!out)
            {
                printf("receive_it %d failed to open %s\n", __LINE__, outfile);
                exit(1);
            }
            
            
			init_status(&status);
            int expected_packet_number = 0;
            int total_received = 0;
            for(i = 0; i < packets; i++)
            {
                received[i] = 0;
            }
			while(1)
			{
				int packet_number = -1;

printf("receive_it %d requesting packets %d-%d\n", 
__LINE__, 
expected_packet_number,
expected_packet_number + packets);
				packet[0] = START;
				write_int32(packet + 1, expected_packet_number);
                packet[5] = (packets >> 16) & 0xff;
                packet[6] = (packets >> 8) & 0xff;
                packet[7] = packets & 0xff;
// send the packets we already have
                for(i = 0; i < packets; i++)
                {
                    packet[REQ_HEADER + i] = received[i];
                }

				int _ = write(write_socket, packet, 8 + packets);
                int failed = 0;
                int done = 0;

                while(!done)
                {
// wait for it
				    int read_status = select_read_socket(packets - total_received);
//printf("got packet %d read_status=%d\n", __LINE__, read_status);
				    if(read_status > 0)
				    {
 					    int bytes_read = read_packet(&packet_number);
printf("receive_it %d got packet %d bytes_read=%d\n", 
__LINE__, 
packet_number, 
bytes_read);

					    if(bytes_read == PACKET_SIZE &&
                            packet_number >= expected_packet_number &&
                            packet_number < expected_packet_number + packets)
					    {
// got a packet in the range
                            memcpy(buffer + 
                                    PACKET_DATA * 
                                    (packet_number - expected_packet_number),
                                packet + HEADER_SIZE,
                                PACKET_DATA);
                            received[packet_number - expected_packet_number] = 1;
// test if all done
                            done = 1;
                            total_received = 0;
                            for(j = 0; j < packets; j++)
                            {
                                if(!received[j])
                                {
                                    done = 0;
                                }
                                else
                                {
                                    total_received++;
                                }
                            }
                        }
                    }
                    else
                    {
// timed out
                        break;
                    }
                }

// test if all done
                done = 1;
                for(j = 0; j < packets; j++)
                {
                    if(!received[j])
                    {
                        done = 0;
                        break;
                    }
                }
                
// write all received packets
                if(done)
                {
                    int fragment = BUFFER_SIZE;
                    if(status.total_bytes + fragment > file_size)
                    {
                        fragment = file_size - status.total_bytes;
                    }
                    update_status(&status, fragment);
                    fwrite(buffer, fragment, 1, out);
                    if(status.total_bytes >= file_size) break;

// advance to the next set of packets
                    expected_packet_number += packets;
                    total_received = 0;
                    for(i = 0; i < packets; i++)
                    {
                        received[i] = 0;
                    }
                }

			}

			if(out) fclose(out);
            printf("receive_it %d done receiving it\n", __LINE__);
            struct timeval file_end_time;
            gettimeofday(&file_end_time, 0);
            int time_diff = file_end_time.tv_sec - file_start_time.tv_sec;
            printf("%dh:%dm:%ds elapsed\n", 
                (time_diff / 3600),
                (time_diff % 3600) / 60,
                (time_diff % 60));
			out = 0;
		}
	}
}





int main(int argc, char *argv[])
{
	int i, j;
    int got_hostname = 0;
    int got_port = 0;
    int got_filename = 0;
    
	if(argc < 2)
	{
		printf("Usage: udptest [-6] [-r] [-n packets to receive at a time] <local port>\n");
		printf("udptest -6 -r -n 8 1234     receive 8 packets at a time using IPV6 to port 1234\n");
        printf("udptest 2017:0000:0000:0000:7690:5000:0000:0001 1234 file.txt    send a file over IPV6 to port 1234\n");
        exit(1);
	}



    signal(SIGHUP, sig_catch);
    signal(SIGINT, sig_catch);
    signal(SIGQUIT, sig_catch);
    signal(SIGTERM, sig_catch);

	char outfile[TEXTLEN];
    for(i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-6"))
        {
            ipv6 = 1;
        }
        else
        if(!strcmp(argv[i], "-r"))
        {
            do_receive = 1;
        }
        else
        if(!strcmp(argv[i], "-n"))
        {
            if(i >= argc - 1)
            {
                printf("main %d: need a number of packets\n", __LINE__);
                exit(1);
            }

            packets = atoi(argv[i + 1]);
            if(packets >= MAX_PACKETS)
            {
                packets = MAX_PACKETS;
            }
            i++;
        }
        else
        if(do_receive)
        {
    	    port = atoi(argv[i]);
        }
        else
        if(!got_hostname)
        {
            strcpy(hostname, argv[i]);
            got_hostname = 1;
        }
        else
        if(!got_port)
        {
            port = atoi(argv[i]);
            got_port = 1;
        }
        else
        if(!got_filename)
        {
            strcpy(infile, argv[i]);
            got_filename = 1;
        }
        else
        {
            printf("main %d: unknown option %s\n", __LINE__, argv[i]);
            exit(1);
        }
    }

    if(do_receive)
    {
        receive_it();
    }
    else
    {
        send_it();
    }

}



