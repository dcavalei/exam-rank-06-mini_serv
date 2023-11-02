#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

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

// ------------------------------------------------
void fatal_error(void) {
    write(2, "Fatal error\n", strlen("Fatal error\n")); exit(1);
}

fd_set readfds, writefds, fds;
int next_id, max_fd;
int ids[2000];
char buffer[65000];
char *remain[2000];

void notify(char *msg, int self)
{
    if (!msg) return;
    int len = strlen(msg);
    for (int fd = 0; fd <= max_fd; fd++)
    {
        if (fd == self || FD_ISSET(fd, &writefds) == 0) continue;
        send(fd, msg, len, 0);
    }
}

int main(int ac, char **av) {
    if (ac != 2) { write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n")); exit(1); }
    int sockfd, connfd; unsigned int len;
    struct sockaddr_in servaddr, cli;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) fatal_error();
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_port = htons(atoi(av[1]));
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {fatal_error();}
    if (listen(sockfd, 10) != 0) fatal_error();


    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    max_fd = sockfd;

    while (1)
    {
        readfds = writefds = fds;
        if (select(max_fd + 1, &readfds, &writefds, 0, 0) < 0) continue;
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &readfds) == 0) continue;
            if (fd == sockfd) // new conn
            {
                len = sizeof(cli);
                connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                if (connfd < 0) continue;
                if (max_fd < connfd) max_fd = connfd;
                ids[connfd] = ++next_id;
                FD_SET(connfd, &fds);

                char msg[128] = {0};
                sprintf(msg, "server: client %d just arrived\n", ids[connfd]);
                notify(msg, connfd);
                fcntl(connfd, F_SETFL, O_NONBLOCK); // for testing
            }
            else // old conn
            {
                int bytes = 0, sum = 0;
                while ((bytes = recv(fd, buffer + sum, sizeof(buffer) - sum, 0)) > 0) sum += bytes;
                if (sum == 0) // close
                {
                    free(remain[fd]);
                    remain[fd] = 0;
                    FD_CLR(fd, &fds);
                    FD_CLR(fd, &writefds);
                    close(fd);

                    char msg[128] = {0};
                    sprintf(msg, "server: client %d just left\n", ids[fd]);
                    notify(msg, fd);
                }
                else // new msg
                {
                    remain[fd] = str_join(remain[fd], buffer);
                    for (char *msg = 0; extract_message(&remain[fd], &msg); free(msg))
                    {
                        sprintf(buffer, "client %d: %s", ids[fd], msg);
                        notify(buffer, fd);
                    }
                    bzero(buffer, sizeof(buffer));
                }
            }
        }
    }
}
