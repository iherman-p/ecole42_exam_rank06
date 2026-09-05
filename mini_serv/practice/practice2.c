#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/select.h>

fd_set  all_fds;
fd_set  rfds;

int sockfd;
int	largest_fd;
int	client_amount = 0;

int     uids[1024];
char*   ubuffers[1024];

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
	write(STDERR_FILENO, "Fatal error!\n", 14);
	exit(1);
}

void	broadcast(int fd, char* msg)
{
	for (int i = 0; i <= largest_fd; ++i)
	{
		if (!FD_ISSET(i, &all_fds) || i == sockfd || i == fd)
			continue ;

		send(i, msg, strlen(msg), 0);
	}
}

void	accept_client()
{
	struct sockaddr_in	addr;
	socklen_t			len = sizeof(struct sockaddr_in);

	int	fd = accept(sockfd, (struct sockaddr*)&addr, &len);

	if (fd < 0)
		return ;

	ubuffers[fd] = NULL;
	uids[fd] = client_amount;
	FD_SET(fd, &all_fds);

	client_amount++;

	if (largest_fd < fd)
		largest_fd = fd;

	char	msg[50];
	sprintf(msg, "client %d just joined\n", uids[fd]);
	broadcast(fd, msg);
}

void	disconnect_client(int fd)
{
	close(fd);
	free(ubuffers[fd]);
	FD_CLR(fd, &all_fds);

	char	msg[50];
	sprintf(msg, "client %d just left\n", uids[fd]);
	broadcast(fd, msg);
}

int	recv_from_client(int fd)
{
	char	buf[1024];

	int	amount = recv(fd, buf, 1023, 0);

	if (amount <= 0)
		return 0;

	buf[amount] = 0;

	ubuffers[fd] = str_join(ubuffers[fd], 	buf);

	return 1;
}

int main(int argc, char** argv)
{
	struct sockaddr_in servaddr; 

	if (argc < 2)
	{
		write(STDERR_FILENO, "Wrong amount of arguments!\n", 28);
		return 1;
	}

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal_error();

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
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

		if (select(largest_fd + 1, &rfds, NULL, NULL, NULL) <= 0)
			continue ;

		for (int i = 0; i <= largest_fd; ++i)
		{
			if (!FD_ISSET(i, &rfds))
				continue ;

			if (i == sockfd)
			{
				accept_client();
				continue;
			}

			if (!recv_from_client(i))
				disconnect_client(i);

			while (1)
			{
				char*	extracted = NULL;
				int	status = extract_message(&ubuffers[i], &extracted);

				if (status == -1)
					fatal_error();

				if (status == 0)
					break ;

				char*	prefix = calloc(50, sizeof(char));
				if (prefix == NULL)
					fatal_error();
				sprintf(prefix, "client %d: ", uids[i]);
				char*	msg = str_join(prefix, extracted);

				broadcast(i, msg);
			}
			
		}
	}
	close(sockfd);
}