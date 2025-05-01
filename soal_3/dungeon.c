#define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shop.c"

#define PORT 1337

int make_socket();
void reap_zombies(int sig);
void handler(int sock);

int get_stats(int sock, struct Player *player);
int get_inventory(int sock, struct Player *player);
int change_weapon(int sock, struct Player *player);

int main(int argc, char *argv[]) {
    struct sigaction sa;
    sa.sa_handler = reap_zombies;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
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

void handler(int sock) {
    struct Weapon defaultWeapon = {"Fists", 0, 5, 0, 0, NONE};

    struct Inventory inventory = {NULL, 5, 0};
    inventory.weapons = malloc(sizeof(struct Weapon) * inventory.capacity);
    if (inventory.weapons == NULL) return;
    inventory.weapons[inventory.size++] = defaultWeapon;

    struct Player player = {500, 0, inventory, 0};

    char buffer[1024];
    int buflen;
    while (1) {
        buflen = recv(sock, buffer, sizeof(buffer), 0);
        if (buflen <= 0) continue;
        buffer[buflen - 1] = '\0';
        if (strcmp(buffer, "exit") == 0) continue;
        if (strcmp(buffer, "stats") == 0) {
            if (get_stats(sock, &player) < 0) continue;
        }
        if (strcmp(buffer, "inventory") == 0) {
            if (get_inventory(sock, &player) < 0) continue;
        }
        if (strcmp(buffer, "change") == 0) {
            if (change_weapon(sock, &player) < 0) continue;
        }
        if (strcmp(buffer, "weapons") == 0) {
            if (available_weapons(sock) < 0) continue;
        }
        if (strcmp(buffer, "buy") == 0) {
            if (buy_weapon(sock, &player) < 0) continue;
        }
    }

    free(inventory.weapons);
}

int get_stats(int sock, struct Player *player) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "Gold=%d;Equipped Weapon=%s;Base Damage=%d;Kills=%d;Passive=%s;Passive Value=%d\n",
             player->gold,
             player->inventory.weapons[player->equippedWeapon].name,
             player->inventory.weapons[player->equippedWeapon].damage,
             player->kills,
             WeaponPassiveStr[player->inventory.weapons[player->equippedWeapon].passiveType],
             player->inventory.weapons[player->equippedWeapon].passivePercentage);
    int len = strlen(buffer);
    if (send(sock, buffer, len, 0) != len) return -1;
}

int get_inventory(int sock, struct Player *player) {
    char buffer[1024];
    int len = 0;
    for (int i = 0; i < player->inventory.size; i++) {
        struct Weapon *weapon = &player->inventory.weapons[i];
        len += snprintf(buffer + len, sizeof(buffer) - len,
                        "Name=%s:Passive=%s:Equipped=%d:Passive Value=%d;",
                        weapon->name,
                        WeaponPassiveStr[weapon->passiveType],
                        (player->equippedWeapon == i ? 1 : 0),
                        weapon->passivePercentage);
    }
    buffer[len - 1] = '\0';

    if (send(sock, buffer, len, 0) != len) return -1;
    return 0;
}

int change_weapon(int sock, struct Player *player) {
    char buffer[128];
    int len = recv(sock, buffer, sizeof(buffer), 0);
    if (len <= 0) return -1;
    buffer[len - 1] = '\0';
    int index = atoi(buffer);

    if (index < 0 || index >= player->inventory.size) {
        snprintf(buffer, sizeof(buffer), "Invalid weapon index\n");
        send(sock, buffer, strlen(buffer), 0);
        return -1;
    }

    player->equippedWeapon = index;
}