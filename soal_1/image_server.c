#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 1337

void daemonize();
int make_socket();
void handler(int sock);
void revstr(char *str);
int fromhex(char *hex, unsigned char *bin, size_t bin_size);

int decoder_file(int sock);
int downloader_file(int sock);

int main(int argc, char *argv[]) {
    // daemonize();
    int sock = make_socket();
    if (sock < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    handler(sock);

    close(sock);

    return 0;
}

void daemonize() {
    pid_t pid = fork();
    int status;

    if (pid < 0) exit(1);
    if (pid > 0) exit(0);
    if (setsid() < 0) exit(1);

    umask(0);
    for (int x = sysconf(_SC_OPEN_MAX); x > 0; x--) close(x);
}

int make_socket() {
    struct sockaddr_in address;
    int sockfd, sock;
    socklen_t addrlen = sizeof(address);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if ((bind(sockfd, (struct sockaddr *)&address, addrlen)) < 0) return -1;
    if (listen(sockfd, 3) < 0) return -1;
    if ((sock = accept(sockfd, (struct sockaddr *)&address, &addrlen)) < 0) return -1;

    return sock;
}

void handler(int sock) {
    char action[1024];
    while (1) {
        ssize_t buflen = recv(sock, action, sizeof(action), 0);
        if (buflen <= 0) break;
        action[buflen] = '\0';
        if (action[buflen - 1] == '\n') action[buflen - 1] = '\0';

        if (strcmp(action, "exit") == 0)
            break;
        else if (strcmp(action, "decode") == 0)
            decoder_file(sock);
        else if (strcmp(action, "download") == 0)
            downloader_file(sock);
    }
}

void revstr(char *str) {
    if (!str) return;
    int i = 0;
    int j = strlen(str) - 1;
    while (i < j) {
        char c = str[i];
        str[i] = str[j];
        str[j] = c;
        i++;
        j--;
    }
}

int fromhex(char *hex, unsigned char *bin, size_t bin_size) {
    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;
    size_t i;
    for (i = 0; i < len / 2; i++) {
        if (sscanf(&hex[2 * i], "%2hhx", &bin[i]) != 1) return -1;
    }
    return i;
}

int decoder_file(int sock) {
    return 0;
}
int downloader_file(int sock) {
    char *message = "File download initiated";
    send(sock, message, strlen(message), 0);
    return 0;
}