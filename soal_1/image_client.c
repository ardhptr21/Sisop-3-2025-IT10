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
int recvto_file(int sock, char *filename, size_t size);

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
            send(sockfd, "decode\n", strlen("decode\n"), 0);
            send_file(sockfd, target);
            break;
        case 2:
            printf("Enter the file name to download: ");
            scanf("%s", filename);
            send(sockfd, "download\n", strlen("download\n"), 0);
            snprintf(target, sizeof(target), "%s\n", filename);
            sleep(0.1);
            send(sockfd, target, strlen(target), 0);
            char size_buffer[32];
            ssize_t size_len = recv(sockfd, size_buffer, sizeof(size_buffer) - 1, 0);
            if (size_len <= 0) break;
            size_buffer[size_len] = '\0';
            size_t expected_size = strtoul(size_buffer, NULL, 10);
            recvto_file(sockfd, filename, expected_size);
            break;
        case 3:
            close(sockfd);
            exit(EXIT_SUCCESS);
            return;
        default:
            printf("Invalid option\n");
    }
}

int send_file(int sockfd, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%ld\n", file_size);

    if (send(sockfd, size_str, strlen(size_str), 0) < 0) {
        fclose(fp);
        return -1;
    }
    sleep(0.1);

    char *buffer = malloc(file_size);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);
    fclose(fp);

    if (read_size != file_size) {
        free(buffer);
        return -1;
    }

    if (send(sockfd, buffer, file_size, 0) < 0) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

int recvto_file(int sock, char *filename, size_t size) {
    ssize_t buflen;
    FILE *fp;
    char buffer[1024];
    size_t total_received = 0;

    fp = fopen(filename, "wb");
    if (!fp) return -1;

    while (total_received < size) {
        size_t remaining = size - total_received;
        size_t to_recv = sizeof(buffer);
        if (remaining < to_recv) to_recv = remaining;

        buflen = recv(sock, buffer, to_recv, 0);
        if (buflen <= 0) {
            fclose(fp);
            return -1;
        }

        fwrite(buffer, 1, buflen, fp);
        total_received += buflen;
    }

    fclose(fp);
    return 0;
}