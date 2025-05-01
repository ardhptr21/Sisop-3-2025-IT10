#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define HOST "127.0.0.1"
#define PORT 1337

int connect_socket();
void handler(int sockfd, int opt);

int main(int argc, char *argv[]) {
    // int sockfd = connect_socket();

    // if (sockfd < 0) {
    //     perror("connection failed");
    //     exit(EXIT_FAILURE);
    // }
    // printf("Connected to address %s:%d\n", HOST, PORT);

    int opt;

    while (1) {
        printf("1. Send input file to server\n");
        printf("2. Download file from server\n");
        printf("3. Exit\n");
        printf(">>> ");
        scanf("%d", &opt);
        // handler(sockfd, opt);
    }

    // close(sockfd);
    return 0;
}

int connect_socket() {
    int sockfd;
    struct sockaddr_in address;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = inet_addr(HOST);

    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) return -1;

    return sockfd;
}

void handler(int sockfd, int opt) {}