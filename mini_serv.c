#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>

void fatal_error() {
    write(STDERR_FILENO, "Fatal error\n", strlen("Fatal error\n"));
    exit(1);
}

void notify(fd_set const *managed_fds, int highest_fd, int sockfd, char const *msg) {
    for (int fd = sockfd + 1; fd < highest_fd + 1; ++fd) {
        if (FD_ISSET(fd, managed_fds)) {
            size_t bytes_sent = 0;
            size_t msg_len = strlen(msg);
            while ((bytes_sent += send(fd, msg + bytes_sent, strlen(msg + bytes_sent), 0)) < msg_len);
            printf("client[%d] has been notified:\n{%s}\n", fd, msg);
        }
    }
}

int main(int ac, char **av) {
    if (ac != 2) {
        write(STDERR_FILENO, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) fatal_error();
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
    servaddr.sin_port = htons(atoi(av[1]));

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) fatal_error();
    if (listen(sockfd, 10) != 0) fatal_error();

    fd_set managed_fds, ready_fds;
    FD_ZERO(&managed_fds);
    FD_SET(sockfd, &managed_fds);
    int highest_fd = sockfd;
    int next_id = 0;
    // Since we don't have access to more advanced data structures, let's sacrifice space and lower the complexity...
    int client_id[FD_SETSIZE + 32] = {0};
    char buffer[1024 * 10] = {0};
    char msg[sizeof(buffer) + 64] = {0};

    while (1) {
        ready_fds = managed_fds;
        printf("waiting for events...\n");
        if (select(highest_fd + 1, &ready_fds, NULL, NULL, NULL) < 0) fatal_error();
        for (int fd = 0; fd < highest_fd + 1; ++fd) {
            if (!FD_ISSET(fd, &ready_fds)) continue; // Filter unwanted fds
            if (fd == sockfd) {
                len = sizeof(cli);
                if ((connfd = accept(sockfd, (struct sockaddr *) &cli, &len)) < 0) fatal_error();
                if (highest_fd < connfd)
                    highest_fd = connfd;
                client_id[connfd] = next_id++;
                printf("new connection... client with fd[%d] and id[%d]\n", connfd, client_id[connfd]);
                sprintf(msg, "server: client %d just arrived\n", client_id[connfd]);
                notify(&managed_fds, highest_fd, sockfd, msg);
                // For testing only, not allowed in the exam... otherwise send/recv will block when reading from connfd
                fcntl(connfd, F_SETFL, O_NONBLOCK);
                FD_SET(connfd, &managed_fds);
            } else {
                long recv_bytes, total_bytes = 0;
                while ((recv_bytes = recv(fd, buffer + total_bytes, sizeof(buffer) - total_bytes, 0)) > 0)
                    total_bytes += recv_bytes;
                if (total_bytes) {
                    sprintf(msg, "client %d: %s\n", client_id[fd], buffer);
                    notify(&managed_fds, highest_fd, sockfd, msg);
                } else {
                    FD_CLR(fd, &managed_fds);
                    close(fd); // we don't want to leak fds :^)
                    printf("client with fd[%d] and id[%d] disconnected...\n", fd, client_id[fd]);
                    sprintf(msg, "server: client %d just left\n", client_id[fd]);
                    notify(&managed_fds, highest_fd, sockfd, msg);
                }
            }
        }
    }
}
