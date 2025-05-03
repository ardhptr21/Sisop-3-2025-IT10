#define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shop.c"

#define PORT 1337

struct Enemy {
    int baseHealth;
    int currentHealth;
};

struct AttackStats {
    int damage;
    int reward;
    int isCritical;
    int isPassive;
    int isDead;

    const char *passive;
    const char *passiveDetail;
};

int make_socket();
void reap_zombies(int sig);
void handler(int sock);
void battle(int sock, struct Player *player);

int get_stats(int sock, struct Player *player);
int get_inventory(int sock, struct Player *player);
int change_weapon(int sock, struct Player *player);

int random_reward();
int random_enemy(int sock, int *enemyHealth);
int attack(int sock, struct Player *player, struct Enemy *enemy);

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
        if (strcmp(buffer, "exit") == 0) break;
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
        if (strcmp(buffer, "battle") == 0) {
            battle(sock, &player);
            continue;
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

int random_enemy(int sock, int *enemyHealth) {
    srand(time(NULL));
    int randomValue = (rand() % 151) + 50;

    *enemyHealth = randomValue;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Health=%d", randomValue);
    int len = strlen(buffer);
    if (send(sock, buffer, len, 0) != len) return -1;
}

int random_reward() {
    srand(time(NULL));
    int randomValue = (rand() % 81) + 20;
    return randomValue;
}

void battle(int sock, struct Player *player) {
    char buffer[1024];
    int buflen;

    struct Enemy enemy;
    int enemyHealth;
    random_enemy(sock, &enemyHealth);
    enemy.baseHealth = enemyHealth;
    enemy.currentHealth = enemyHealth;

    while (1) {
        buflen = recv(sock, buffer, sizeof(buffer), 0);
        if (buflen <= 0) continue;
        buffer[buflen - 1] = '\0';

        if (strcmp(buffer, "exit") == 0) break;
        if (strcmp(buffer, "attack") == 0) {
            attack(sock, player, &enemy);
        }
    }
}

int attack(int sock, struct Player *player, struct Enemy *enemy) {
    srand(time(NULL));
    struct AttackStats attackStats = {0, 0, 0, 0, 0, NULL, NULL};
    struct Weapon *weapon = &player->inventory.weapons[player->equippedWeapon];
    int baseDamage = weapon->damage;

    attackStats.damage = baseDamage;

    // 45% chance for critical hit
    attackStats.isCritical = (rand() % 100) <= 45;
    // 20% chance for passive
    attackStats.isPassive = (rand() % 100) <= 20;

    if (attackStats.isPassive && weapon->hasPassive) {
        attackStats.passive = WeaponPassiveStr[weapon->passiveType];
        if (weapon->passiveType == CRITICAL) {
            if (!attackStats.isCritical) {
                attackStats.isCritical = (rand() % 100) <= weapon->passivePercentage + 45;
            }
            if (attackStats.isCritical) {
                attackStats.passiveDetail = "Critical Passive active! You hit the enemy with a more critical hit chance!";
            } else {
                attackStats.passiveDetail = "Critical Passive active! Oh no, you missed, the chance is not in your favor!";
            }
        } else if (weapon->passiveType == INSTANTKILL) {
            if (rand() % 100 <= weapon->passivePercentage) {
                enemy->currentHealth = 0;
                attackStats.passiveDetail = "Instant Kill Passive active! You killed the enemy instantly, no damage taken!";
                attackStats.damage = 0;
            } else {
                attackStats.damage += (attackStats.damage * weapon->passivePercentage) / 100;
                attackStats.passiveDetail = "Instant Kill Passive active! but not your lucky day, but your damage is increased!";
            }
        }
    } else {
        attackStats.isPassive = 0;
    }

    if (attackStats.isCritical) {
        attackStats.damage *= 2;
    }

    enemy->currentHealth -= attackStats.damage;
    player->gold += attackStats.reward;

    if (enemy->currentHealth <= 0) {
        attackStats.reward = random_reward();
        attackStats.isDead = 1;

        player->gold += attackStats.reward;
        enemy->currentHealth = 0;
        player->kills++;
    }

    char buffer[1024];

    snprintf(buffer, sizeof(buffer), "BaseHealth=%d;CurrHealth=%d;Reward=%d;Damage=%d;IsDead=%d;IsCritical=%d;IsPassive=%d;Passive=%s;PassiveDetail=%s\n",
             enemy->baseHealth,
             enemy->currentHealth,
             attackStats.reward,
             attackStats.damage,
             attackStats.isDead,
             attackStats.isCritical,
             attackStats.isPassive,
             attackStats.passive == NULL ? "NONE" : attackStats.passive,
             attackStats.passiveDetail == NULL ? "NONE" : attackStats.passiveDetail);

    int len = strlen(buffer);
    if (send(sock, buffer, len, 0) != len) return -1;
    sleep(0.01);
    if (attackStats.isDead) {
        int newHealth;
        random_enemy(sock, &newHealth);
        enemy->baseHealth = newHealth;
        enemy->currentHealth = newHealth;
    };
    return 0;
}