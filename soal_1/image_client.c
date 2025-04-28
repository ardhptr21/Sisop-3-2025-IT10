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
int send_file(int sockfd, const char *filename);

int main(int argc, char *argv[]) {
    int sockfd = connect_socket();

    if (sockfd < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to address %s:%d\n", HOST, PORT);

    int opt;

    while (1) {
        printf("1. Send input file to server\n");
        printf("2. Download file from server\n");
        printf("3. Exit\n");
        printf(">>> ");
        scanf("%d", &opt);
        handler(sockfd, opt);
    }

    close(sockfd);
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

void handler(int sockfd, int opt) {
    char *folder = "secrets";
    char filename[256];
    char target[512];

    switch (opt) {
        case 1:
            printf("Enter the file name: ");
            scanf("%s", filename);
            snprintf(target, sizeof(target), "%s/%s", folder, filename);
            if (send_file(sockfd, target) < 0) {
                perror("send failed");
            };
            break;
        case 2:
            printf("Enter the file name to download: ");
            scanf("%s", filename);
            break;
        case 3:
            close(sockfd);
            return;
        default:
            printf("Invalid option\n");
    }
}

int send_file(int sockfd, const char *filename) {
    char *action = "decode\n";
    if (send(sockfd, action, strlen(action), 0) < 0) return -1;

    FILE *fp;
    char buffer[1024];

    fp = fopen(filename, "rb");
    if (!fp) return -1;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (send(sockfd, buffer, sizeof(buffer), 0) < 0) {
            break;
        }
        bzero(buffer, sizeof(buffer));
    }

    fclose(fp);
    return 0;
}