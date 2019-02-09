#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>

void error_handler(char *alert)
{
	perror(alert);
	exit(1);
}

int establish_communication(int socket_in, int socket_out)
{
	int accept_socket;

	if( (accept_socket = accept(socket_in, NULL, NULL))==-1 )
		error_handler("accept");


	/*returning the socket*/
	return accept_socket;
}

void send_data(int socket_in, int socket_out)
{
	ssize_t size; /*variable used in read*/
	char buff[60000];

	/*receiving data*/
	if( (size = read(socket_in, buff, sizeof(buff)))==-1 )
		error_handler("read");

	if(size>0)
	{
		/*conveying data*/
		if(write(socket_out, buff, size)==-1)
			error_handler("write");
	}
	
}

int create_sock_listen(char *local_port)
{
	int sockfd;
	struct addrinfo hints, *res;
	struct addrinfo *iterator; /*variable used for iterating through addrinfo structures*/
	int option_value = 1; /*value used for setsockopt*/
	int getaddrerror;
	int option = 1; /*used for ioctl*/



	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; /*     TYLKO IPV4       */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; /*wild card address INADDR_ANY*/

	if( (getaddrerror = getaddrinfo(NULL, local_port, &hints, &res))!=0 )
	{
		printf("\ngetaddrinfo: %s\n", gai_strerror(getaddrerror));
		exit(1);
	}

	/*trying to bind with help of addrinfo structure 'res'*/
	for(iterator = res; iterator!=NULL; iterator = iterator->ai_next)
	{

		if( (sockfd = socket(iterator->ai_family, iterator->ai_socktype, iterator->ai_protocol))==-1 )
			error_handler("socket");

		/*non blocking socket*/
    		if (ioctl(sockfd, FIONBIO, &option) == -1)
			error_handler("iosctl");
        		

		/************setsockopt***********************/
		if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int))==-1 )
			error_handler("setsockopt");

		if( bind(sockfd, iterator->ai_addr, iterator->ai_addrlen)==-1 )
		{
			close(sockfd);
			perror("bind");
			continue; /*try with another addrinfo structure*/
		}
			
		break; /*if everything's okay, stop iterating*/
	}

	if(iterator == NULL)
		error_handler("failure to bind to all available addresses");
			

	/**********listen****************************/
	if( listen(sockfd, 10)==-1 )
		error_handler("listen");

	freeaddrinfo(res);
	
	return sockfd;
}

int create_sock_write(char *remote_host, char *remote_port)
{
	int sockfd;
	struct addrinfo hints, *res;
	struct addrinfo *iterator; /*variable used for iterating through addrinfo structures*/
	int getaddrerror;
	int option = 1;

	/*preparing structure addrinfo*/
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM; /*tcp*/
	
	if( (getaddrerror = getaddrinfo(remote_host, remote_port, &hints, &res))!=0 )
	{
		printf("\ngetaddrinfo: %s\n", gai_strerror(getaddrerror));
		exit(1);
	}

	for(iterator = res; iterator!=NULL; iterator = iterator->ai_next)
	{
		if( (sockfd = socket(iterator->ai_family, iterator->ai_socktype, iterator->ai_protocol))==-1 )
			error_handler("socket");

		/*connect to remote host*/
		if( connect(sockfd, iterator->ai_addr, iterator->ai_addrlen)==-1 )
		{
			close(sockfd);
			perror("failure to connect");
			continue;
		}

		break;
	}

	if(iterator == NULL)
	{
		printf("\nconnection failure\n");
		exit(0);
	}

	if (ioctl(sockfd, FIONBIO, &option) == -1)
		error_handler("ioctl");

	freeaddrinfo(res);

	return sockfd;
}

pid_t *children;

void close_and_clean(int signal)
{
	int i = 0;	
	while( children[i]!=0 )
	{
		if(children[i]==1)
			continue;

		kill( children[i], SIGTERM );
		i++;
	}

	printf("\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	struct pollfd *pollfd;
	char *local_port;
	char *remote_host;
	char *remote_port;
	int poll_outcome; /*value returned by poll*/
	int pollfd_count;
	pid_t child;
	pid_t terminated;

	children = (pid_t *)malloc( (argc)*sizeof(pid_t) );

	signal(SIGINT, close_and_clean);

	printf("pid: %d\n", getpid());

	int i;
	for(i = 1; i<argc; i++)
	{

		if( (child = fork())==0 )
		{
			/*claryfying arguments of the program*/			
			local_port = strtok_r(argv[i], ":", &argv[i]);
			remote_host = strtok_r(NULL, ":", &argv[i]);
			remote_port = strtok_r(NULL, ":", &argv[i]);			

			pollfd = (struct pollfd *)calloc( 1, sizeof(struct pollfd) );

			/*receiving first connections from outside*/
			pollfd[0].fd = create_sock_listen(local_port);
			pollfd[0].events = POLLIN | POLLRDHUP;
			pollfd_count = 1;

			while(1)
			{
				/************POLL******************/
				poll_outcome = poll(pollfd, pollfd_count, 5*60*1000);

				if(poll_outcome==-1)
				{
					error_handler("poll");
				}
				else if(poll_outcome==0)
				{
					printf("\ntimeout\n");
					exit(1);
				}
				/**********************************/


				/*checking all descryptors after poll*/
				for(i = 0; i<pollfd_count; i++)
				{

					/*CHECKING POLLIN*/
					/*first contact with client who sent request*/					
					if( ((pollfd[i].revents & POLLIN)==POLLIN) && (i == 0) )
					{						
						pollfd_count = pollfd_count + 2;						
						
						/*reallocation of memory*/
						pollfd = (struct pollfd *)realloc( pollfd, pollfd_count*sizeof(struct pollfd) );
						if(pollfd == NULL)
							error_handler("pollfd");
						
						/*even i without zero - connection to remote host we transfer the data to*/
						pollfd[pollfd_count-1].fd = create_sock_write(remote_host, remote_port);

						pollfd[pollfd_count-1].events = POLLIN | POLLRDHUP | POLLPRI;
						
						/*not even i - connection with the client who sent request first*/

						pollfd[pollfd_count-2].fd = establish_communication(pollfd[0].fd, pollfd[pollfd_count-1].fd);

						pollfd[pollfd_count-2].events = POLLIN | POLLRDHUP | POLLPRI;
							
					}
					else if(  ( ((pollfd[i].revents & POLLIN)==POLLIN) && (i%2==1) ) /*client who sent request first*/
							|| ( ((pollfd[i].revents & POLLPRI)==POLLPRI) && (i%2==1) ) )
					{
						send_data(pollfd[i].fd, pollfd[i+1].fd);
					}
					else if( ( ((pollfd[i].revents & POLLIN)==POLLIN) && (i%2==0) ) /*client whom we sent data to at first*/
							|| ( ((pollfd[i].revents & POLLPRI)==POLLPRI) && (i%2==0) ) )
					{
						send_data(pollfd[i].fd, pollfd[i-1].fd);
					}

					/*CHECKING POLLRDHUP*/
					if( ((pollfd[i].revents & POLLRDHUP)==POLLRDHUP) && (i%2==1) )
					{						
						pollfd[i].fd = -1;
						pollfd[i+1].fd = -1;
					}

					if( ((pollfd[i].revents & POLLRDHUP)==POLLRDHUP) && (i%2==0) )
					{
						pollfd[i].fd = -1;
						pollfd[i-1].fd = -1;
					}			
					
				}
		
			}
		}
		children[i-1] = child;
	}
	children[argc-1] = 0;

	while(1)
	{
		terminated = wait(NULL);

		for(i = 0; i<(argc-1); i++)
		{
			if( children[i]==terminated )
				children[i] = 1;
		}
	}	

	return 0;
}
