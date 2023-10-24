#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>


// REMOVED HEAP MEMORY ALLOCATION!!!
// This version simply puts (*buf) pointing to the remaining buffer and (*msg)
// to the message without the '\n'. It will swap the newline by a '\0'.
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
            *msg = *buf;
            (*msg)[i] = 0;
            *buf = *buf + i + 1;
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

void fatal_error(void) {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    exit(1);
}

fd_set fds, readfds, writefds;
int max_fd, next_id;
int ids[2000];
char buffer[65000];
char buffer_tmp[65000];

void notify(char *msg, int self) {
    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &writefds) == 0 || fd == self) { continue; }
        send(fd, msg, strlen(msg), 0);
    }
}

int main(int ac, char **av) {
    if (ac != 2) { write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n")); exit(1); }
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { fatal_error(); }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_port = htons(atoi(av[1]));
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { fatal_error(); }
    if (listen(sockfd, 10) != 0) { fatal_error(); }

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    max_fd = sockfd;
    while (1) {
        readfds = writefds = fds;
        if (select(max_fd+1, &readfds, &writefds, 0, 0) < 0) { fatal_error(); }
        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &readfds) == 0) { continue; }
            if (fd == sockfd) { // new client
                len = sizeof(cli);
                connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                if (connfd < 0) { fatal_error(); }
                if (max_fd < connfd) { max_fd = connfd; } // +1 to max_fd

                ids[connfd] = next_id++; // set client id
                char msg[128] = {0};
                sprintf(msg, "server: client %d just arrived\n", ids[connfd]);
                notify(msg, -1);
                fcntl(connfd, F_SETFL, O_NONBLOCK);
                FD_SET(connfd, &fds);
            } else { // old client
                int bytes, total = 0;
                while ((bytes = recv(fd, buffer + total, sizeof(buffer) - total, 0)) > 0) { total += bytes; }
                if (total > 0) { // new msg
                    char *msg = 0; char *ret = 0; char *buf = buffer;

                    while (extract_message(&buf, &msg)) {
                        sprintf(buffer_tmp, "client %d: %s\n", ids[fd], msg);
                        ret = str_join(ret, buffer_tmp);
                        bzero(buffer_tmp, strlen(buffer_tmp));
                    }

                    notify(ret, fd);
                    free(ret);
                } else { // close conn
                    char msg[128] = {0};
                    sprintf(msg, "server: client %d just left\n", ids[fd]);
                    notify(msg, fd);

                    FD_CLR(fd, &fds);
                    close(fd);
                }
                bzero(buffer, strlen(buffer));
            }
        }
    }
}
