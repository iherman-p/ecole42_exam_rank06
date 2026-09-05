#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/select.h>

int	sockfd;

int		largest_fd = 0;
int		client_amount = 0;

fd_set	rfds;
fd_set	wfds;
fd_set	all_fds;

int		client_ids[1024];
char*	client_rdbuffer[1024];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void    fatal_error()
{
	printf("Fatal error\n");
	exit(1);
}

void	broadcast_msg(int fd, char* msg)
{
	for (int recipient = 0; recipient <= largest_fd; ++recipient)
	{
		if (recipient == fd || recipient == sockfd || ! FD_ISSET(recipient, &all_fds))
			continue ;
		
		send(recipient, msg, strlen(msg), 0);
	}
}

void	disconnect_client(int fd)
{
	close(fd);
	FD_CLR(fd, &all_fds);
	free(client_rdbuffer[fd]);

	char	msg[50];

	sprintf(msg, "client %d just left\n", client_ids[fd]);

	broadcast_msg(fd, msg);
}

int	recv_from_client(int fd)
{
	const size_t	buffer_size = 1023;
	char			buffer[buffer_size + 1];

	ssize_t	amount = recv(fd, buffer, buffer_size, 0);

	if (amount <= 0)
		return 0;

	buffer[buffer_size] = '\0';
	client_rdbuffer[fd] = str_join(client_rdbuffer[fd], buffer);
	return 1;
}

void	accept_client()
{
	socklen_t			len = sizeof(struct sockaddr_in);
	struct sockaddr_in	addr;

	int	fd = accept(sockfd, (struct sockaddr*)&addr, &len);

	if (fd < 0)
		return ;

	client_ids[fd] = client_amount;
	client_amount++;
	client_rdbuffer[fd] = NULL;
	FD_SET(fd, &all_fds);

	if (largest_fd < fd)
		largest_fd = fd;

	char	msg[50];

	sprintf(msg, "client %d just joined\n", client_ids[fd]);

	broadcast_msg(fd, msg);
}

int main(int argc, char** argv)
{
	struct sockaddr_in servaddr;
	
	if (argc < 2)
		fatal_error();
	
	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal_error();
	
	bzero(&servaddr, sizeof(servaddr)); 
	
	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = INADDR_ANY; //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
	
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal_error();

	if (listen(sockfd, 10) != 0)
		fatal_error();

	FD_ZERO(&all_fds);
	FD_SET(sockfd, &all_fds);

	largest_fd = sockfd;

	while (1)
	{
		rfds = all_fds;
		wfds = all_fds;

		if (select(largest_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
			continue ;

		for (int fd = 0; fd <= largest_fd; ++fd)
		{
			if (!FD_ISSET(fd, &rfds))
				continue ;

			if (fd == sockfd)
			{
				accept_client();
				continue ;
			}

			if (!recv_from_client(fd))
				disconnect_client(fd);

			while (1)
			{
				char*	extracted;

				int status = extract_message(&client_rdbuffer[fd], &extracted);

				if (status == -1)
					fatal_error();
				if (status == 0)
					break ;

				char*	prefix = calloc(50, sizeof(char));

				sprintf(prefix, "client %d: ", fd);

				char*	msg = str_join(prefix, extracted);

				broadcast_msg(fd, msg);
			}
		}


	}

}
