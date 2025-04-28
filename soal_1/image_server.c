#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 1337

void daemonize();
int make_socket();
void handler(int sock);
int recvto_file(int sock, char *filename);
char *read_file(char *filename, long *file_size);
int write_file(char *filename, char *content, size_t size);
void revstr(char *str);
size_t hexs2bin(const char *hex, unsigned char **out);

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

int recvto_file(int sock, char *filename) {
    ssize_t buflen;
    FILE *fp;
    char buffer[1024];

    fp = fopen(filename, "wb");
    if (!fp) return -1;

    while (1) {
        buflen = recv(sock, buffer, sizeof(buffer), 0);
        if (buflen <= 0) {
            fclose(fp);
            return -1;
        };
        fprintf(fp, "%s", buffer);
        bzero(buffer, sizeof(buffer));
    }
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
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, *file_size, file);
    if (bytes_read != *file_size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

int write_file(char *filename, char *content, size_t size) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    fwrite(content, 1, size, fp);
    fclose(fp);
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
int hexchr2bin(const char hex, char *out) {
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

size_t hexs2bin(const char *hex, unsigned char **out) {
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
    printf("called decoder_file\n");
    char *folder = "database";
    struct stat st;
    if (stat(folder, &st) == -1) {
        if (mkdir(folder, 0700) == -1) return 1;
    }

    time_t timestamp = time(NULL);
    char strtime[32];
    char filename[128];

    snprintf(strtime, sizeof(strtime), "%ld", timestamp);
    snprintf(filename, sizeof(filename), "%s/%s.jpeg", folder, strtime);

    if (recvto_file(sock, filename) < 0) {
        perror("recv failed");
        return -1;
    };

    long file_size;
    char *file_content = read_file(filename, &file_size);
    if (!file_content) return -1;

    revstr(file_content);

    unsigned char *decoded;

    size_t decoded_size = hexs2bin(file_content, &decoded);
    if (decoded_size < 0) {
        free(file_content);
        free(decoded);
        return -1;
    }

    write_file(filename, decoded, decoded_size);

    free(file_content);
    free(decoded);
    return 0;
}

int downloader_file(int sock) {
    char *message = "File download initiated";
    send(sock, message, strlen(message), 0);
    return 0;
}