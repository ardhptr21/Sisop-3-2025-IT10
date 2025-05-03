#define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PORT 1337

void daemonize();
int make_socket();
void handler(int sock);
int recvto_file(int sock, char *filename, size_t size);
char *read_file(char *filename, long *file_size);
int write_file(char *filename, char *content, size_t size);
int send_file(int sockfd, const char *filename);
void revstr(char *str);
size_t hexs2bin(char *hex, unsigned char **out);
int decoder_file(int sock);
int downloader_file(int sock);
void reap_zombies(int sig);
void logger(char *from, char *type, char *msg);

int main(int argc, char *argv[]) {
    daemonize();

    struct sigaction sa;
    sa.sa_handler = reap_zombies;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    int sockfd = make_socket();
    if (sockfd < 0) {
        perror("socket setup failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) continue;

        pid_t pid = fork();
        if (pid < 0) {
            close(client_sock);
            continue;
        } else if (pid == 0) {
            close(sockfd);
            handler(client_sock);
            close(client_sock);
            exit(0);
        } else {
            close(client_sock);
        }
    }

    close(sockfd);
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

void reap_zombies(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int make_socket() {
    struct sockaddr_in address;
    int sockfd;
    socklen_t addrlen = sizeof(address);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if ((bind(sockfd, (struct sockaddr *)&address, addrlen)) < 0) return -1;
    if (listen(sockfd, 5) < 0) return -1;

    return sockfd;
}

void logger(char *from, char *type, char *msg) {
    FILE *fp = fopen("server.log", "a");
    if (fp == NULL) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "[%s][%s] [%s]: [%s]\n", time_str, from, type, msg);
    fflush(fp);
    fclose(fp);
}

void handler(int sock) {
    char action[1024];
    while (1) {
        ssize_t buflen = recv(sock, action, sizeof(action), 0);
        if (buflen <= 0) break;
        action[buflen] = '\0';
        if (action[buflen - 1] == '\n') action[buflen - 1] = '\0';
        if (strcmp(action, "exit") == 0) {
            logger("Client", "EXIT", "Client requested to exit");
            break;
        } else if (strcmp(action, "decode") == 0) {
            decoder_file(sock);
        } else if (strcmp(action, "download") == 0) {
            downloader_file(sock);
        }
    }
}

int recvto_file(int sock, char *filename, size_t size) {
    FILE *fp;
    char buffer[1024];
    size_t total_received = 0;
    int threshold = 50;

    fp = fopen(filename, "wb");
    if (!fp) return -1;

    while (total_received < size - threshold) {
        size_t remaining = size - total_received;
        size_t to_recv = sizeof(buffer);
        if (remaining < to_recv) to_recv = remaining;

        ssize_t buflen;
        do {
            buflen = recv(sock, buffer, to_recv, 0);
        } while (buflen == -1 && errno == EINTR);

        if (buflen <= 0) {
            fflush(fp);
            fclose(fp);
            return -1;
        }

        fwrite(buffer, 1, buflen, fp);
        total_received += buflen;
    }

    fflush(fp);
    fclose(fp);
    return 0;
}

char *read_file(char *filename, long *file_size) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(*file_size + 1);
    if (buffer == NULL) {
        fflush(file);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, *file_size, file);
    if (bytes_read != *file_size) {
        free(buffer);
        fflush(file);
        fclose(file);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    fflush(file);
    fclose(file);
    return buffer;
}

int write_file(char *filename, char *content, size_t size) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    fwrite(content, 1, size, fp);
    fflush(fp);
    fclose(fp);
}

int send_file(int sock, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%ld\n", file_size);

    if (send(sock, size_str, strlen(size_str), 0) < 0) {
        fflush(fp);
        fclose(fp);
        return -1;
    }
    sleep(0.1);

    char *buffer = malloc(file_size);
    if (!buffer) {
        fflush(fp);
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);
    fflush(fp);
    fclose(fp);

    if (read_size != file_size) {
        free(buffer);
        return -1;
    }

    if (send(sock, buffer, file_size, 0) < 0) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
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

// https://nachtimwald.com/2017/09/24/hex-encode-and-decode-in-c/#hex-to-binary
int hexchr2bin(char hex, char *out) {
    if (out == NULL)
        return 0;

    if (hex >= '0' && hex <= '9') {
        *out = hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        *out = hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        *out = hex - 'a' + 10;
    } else {
        return 0;
    }

    return 1;
}

size_t hexs2bin(char *hex, unsigned char **out) {
    size_t len;
    char b1, b2;
    size_t i;

    if (hex == NULL || *hex == '\0' || out == NULL)
        return 0;

    len = strlen(hex);
    if (len % 2 != 0)
        return 0;
    len /= 2;

    *out = malloc(len + 1);
    if (*out == NULL)
        return 0;

    for (i = 0; i < len; i++) {
        if (!hexchr2bin(hex[i * 2], &b1) || !hexchr2bin(hex[i * 2 + 1], &b2)) {
            free(*out);
            *out = NULL;
            return 0;
        }
        (*out)[i] = (b1 << 4) | b2;
    }

    (*out)[len] = '\0';

    return len;
}

int decoder_file(int sock) {
    logger("Client", "DECRYPT", "Text data");

    char *folder = "database";
    struct stat st;
    if (stat(folder, &st) == -1) {
        if (mkdir(folder, 0700) == -1) return -1;
    };

    time_t timestamp = time(NULL);
    char strtime[32];
    char filename[64];
    char filepath[128];

    snprintf(strtime, sizeof(strtime), "%ld", timestamp);
    snprintf(filename, sizeof(filename), "%s.jpeg", strtime);
    snprintf(filepath, sizeof(filepath), "%s/%s", folder, filename);

    char size_buffer[32];
    ssize_t size_len = recv(sock, size_buffer, sizeof(size_buffer) - 1, 0);
    if (size_len <= 0) return -1;
    size_buffer[size_len] = '\0';
    size_t expected_size = strtoul(size_buffer, NULL, 10);

    if (recvto_file(sock, filepath, expected_size) < 0) return -1;

    long file_size;
    char *file_content = read_file(filepath, &file_size);
    if (!file_content) return -1;

    revstr(file_content);

    unsigned char *decoded;
    size_t decoded_size = hexs2bin(file_content, &decoded);
    if (decoded_size == 0) {
        free(file_content);
        free(decoded);
        return -1;
    }

    write_file(filepath, (char *)decoded, decoded_size);

    free(file_content);
    free(decoded);

    char message[128];
    snprintf(message, sizeof(message), "Text decrypted and saved as %s", filename);
    send(sock, message, strlen(message), 0);

    logger("Client", "SAVE", filename);
    return 0;
}

int downloader_file(int sock) {
    char filename[128];
    char filepath[256];
    char *folder = "database";

    ssize_t buflen = recv(sock, filename, sizeof(filename) - 1, 0);
    if (buflen <= 0) return -1;
    filename[buflen] = '\0';
    if (filename[buflen - 1] == '\n') filename[buflen - 1] = '\0';

    logger("Client", "DOWNLOAD", filename);

    snprintf(filepath, sizeof(filepath), "%s/%s", folder, filename);
    struct stat st;
    if (stat(filepath, &st) == -1) {
        char message[128];
        snprintf(message, sizeof(message), "Gagal menemukan file untuk dikirim ke client");
        send(sock, message, strlen(message), 0);
        return -1;
    }

    send_file(sock, filepath);

    logger("Server", "UPLOAD", filename);
    return 0;
}