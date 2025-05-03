#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define HOST "127.0.0.1"
#define PORT 1337

void cleanline();
int connect_socket();
int main_menu();
void handler(int sockfd, int opt);
void battle(int sockfd);
void health_bar(int min, int max);

int recv_enemy(int sockfd, int should_new);
int recv_attack(int sockfd, char *buffer);
int get_stats(int sockfd);
int get_inventory(int sockfd);
int get_weapons(int sockfd);
int buy_weapon(int sockfd, int available);
int change_weapon(int sockfd, int opt);

int main(int argc, char *argv[]) {
    int sockfd = connect_socket();

    if (sockfd < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    int opt;
    while (1) {
        opt = main_menu();
        if (opt == 5) break;
        handler(sockfd, opt);
    }
    close(sockfd);
    return 0;
}

void cleanline() {
    while (getchar() != '\n');
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

int main_menu() {
    int opt;
    printf("\e[34m==== MAIN MENU ====\e[0m\n");
    printf("1. Show Player Stats\n");
    printf("2. Shop (Buy Weapons)\n");
    printf("3. View Inventory & Equip Weapons\n");
    printf("4. Battle Mode\n");
    printf("5. Exit Game\n");
    printf("\e[33mChoose an option: \e[0m");
    scanf("%d", &opt);
    cleanline();
    return opt;
}

void handler(int sockfd, int opt) {
    switch (opt) {
        case 1:
            get_stats(sockfd);
            break;
        case 2:
            get_weapons(sockfd);
            break;
        case 3:
            get_inventory(sockfd);
            break;
        case 4:
            battle(sockfd);
            break;
        default:
            printf("\e[31mInvalid option. Please try again.\e[0m\n");
    }
    printf("\n");
}

void health_bar(int min, int max) {
    int barLength = 50;
    int healthUnits = (min * barLength) / max;

    printf("[");
    for (int i = 0; i < barLength; i++) {
        if (i < healthUnits) {
            printf("\e[42m \e[0m");
        } else {
            printf("\e[107m \e[0m");
        }
    }
    printf("] %d/%d HP\n\n", min, max);
}

void battle(int sockfd) {
    char buffer[1024];
    char input[64];

    printf("\e[31m==== BATTLE STARTED ====\e[0m\n\n");

    char *action = "battle\n";
    int len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return;

    recv_enemy(sockfd, 0);
    printf("Type \e[32m'attack'\e[0m to attack or \e[31m'exit'\e[0m to leave battle\n");
    while (1) {
        printf("> ");
        scanf("%s", input);

        if (strcmp(input, "exit") == 0) {
            char *action = "exit\n";
            int len = strlen(action);
            if (send(sockfd, action, len, 0) != len) return;
            break;
        }

        if (strcmp(input, "attack") != 0) {
            printf("Invalid command. Type \e[32m'attack'\e[0m to attack or \e[31m'exit'\e[0m to leave battle.\n\n");
            continue;
        }

        char *action = "attack\n";
        int len = strlen(action);
        if (send(sockfd, action, len, 0) != len) return;

        int buflen = recv(sockfd, buffer, sizeof(buffer), 0);
        if (buflen <= 0) return;
        buffer[buflen - 1] = '\0';

        recv_attack(sockfd, buffer);
    }
}

int recv_enemy(int sockfd, int should_new) {
    char buffer[128];

    int buflen = recv(sockfd, buffer, sizeof(buffer), 0);
    if (buflen <= 0) return -1;
    buffer[buflen] = '\0';
    char *sep = strchr(buffer, '=');
    if (!sep) return -1;
    *sep = '\0';
    char *key = buffer;
    char *value = sep + 1;
    int value_int = atoi(value);

    if (should_new) {
        printf("\e[34m==== NEW ENEMY ====\e[0m\n");
    } else {
        printf("Enemy appeared with:\n");
    }

    health_bar(value_int, value_int);
    printf("\n");
    return 0;
}

int recv_attack(int sockfd, char *buffer) {
    char *token = strtok(buffer, ";");
    int baseHealth = 0, currHealth = 0, reward = 0, damage = 0, isDead = 0, isCritical = 0, isPassive = 0;
    char *passive = NULL, *passiveDetail = NULL;

    while (token != NULL) {
        char *sep = strchr(token, '=');
        if (!sep) continue;
        *sep = '\0';
        char *key = token;
        char *value = sep + 1;

        if (strcmp(key, "BaseHealth") == 0) {
            baseHealth = atoi(value);
        } else if (strcmp(key, "CurrHealth") == 0) {
            currHealth = atoi(value);
        } else if (strcmp(key, "Reward") == 0) {
            reward = atoi(value);
        } else if (strcmp(key, "Damage") == 0) {
            damage = atoi(value);
        } else if (strcmp(key, "IsDead") == 0) {
            isDead = atoi(value);
        } else if (strcmp(key, "IsCritical") == 0) {
            isCritical = atoi(value);
        } else if (strcmp(key, "IsPassive") == 0) {
            isPassive = atoi(value);
        } else if (strcmp(key, "Passive") == 0) {
            passive = value;
        } else if (strcmp(key, "PassiveDetail") == 0) {
            passiveDetail = value;
        }
        token = strtok(NULL, ";");
    }

    if (isPassive) {
        printf("\e[35m==== PASSIVE ACTIVE: %s ====\e[0m\n", passive);
        printf("%s\n\n", passiveDetail);
    }

    if (isCritical) {
        printf("\e[33m==== CRITICAL HIT! ====\e[0m\n");
    }

    if (isDead) {
        if (damage > 0) {
            printf("You dealt \e[31m%d damage\e[0m and defeated the enemy!\n\n", damage);
        } else {
            printf("You dealt \e[31m∞ damage\e[0m and defeated the enemy!\n\n");
        }
        printf("\e[35m==== REWARD ====\e[0m\n");
        printf("You earned \e[33m%d gold\e[0m!\n\n", reward);
        recv_enemy(sockfd, 1);
        return 0;
    }

    printf("You dealt \e[31m%d\e[0m damage!\n\n", damage);

    printf("\e[34m==== ENEMY STATUS ====\e[0m\n");
    health_bar(currHealth, baseHealth);
}

int get_stats(int sockfd) {
    char buffer[1024];

    char *action = "stats\n";
    int len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;

    int buflen = recv(sockfd, buffer, sizeof(buffer), 0);
    if (buflen <= 0) return -1;
    buffer[buflen - 1] = '\0';

    printf("==== PLAYER STATS ====\n");

    char *token = strtok(buffer, ";");
    int hasPassive = 0, passiveValue = 0;
    char *passive = NULL;

    while (token != NULL) {
        char *sep = strchr(token, '=');
        if (!sep) continue;
        *sep = '\0';
        char *key = token;
        char *value = sep + 1;
        if (strcmp(key, "Gold") == 0) {
            printf("| \e[33m%s\e[0m: %s | ", key, value);
        } else if (strcmp(key, "Equipped Weapon") == 0) {
            printf("\e[32m%s\e[0m: %s | ", key, value);
        } else if (strcmp(key, "Base Damage") == 0) {
            printf("\e[31m%s\e[0m: %s | ", key, value);
        } else if (strcmp(key, "Kills") == 0) {
            printf("\e[34m%s\e[0m: %s | ", key, value);
        } else if (strcmp(key, "Passive") == 0) {
            if (strcmp(value, "NONE") != 0) {
                passive = value;
                hasPassive = 1;
            };
        } else if (strcmp(key, "Passive Value") == 0) {
            passiveValue = atoi(value);
        }

        token = strtok(NULL, ";");
    }
    if (hasPassive) {
        printf("\e[35mPassive\e[0m: Increased %s Chance (%d%%) | ", passive, passiveValue);
    }
    printf("\n");
}

void display_inventory(char *buffer, int pos) {
    int isEquipped = 0, hasPassive = 0, passiveValue = 0;
    char *name = NULL, *passive = NULL;

    char *start = buffer;
    while (*start) {
        char *end = strchr(start, ':');
        if (end) *end = '\0';

        char *sep = strchr(start, '=');
        if (sep) {
            *sep = '\0';
            char *key = start;
            char *value = sep + 1;

            if (strcmp(key, "Name") == 0) {
                name = value;
            } else if (strcmp(key, "Equipped") == 0) {
                isEquipped = atoi(value);
            } else if (strcmp(key, "Passive") == 0) {
                if (strcmp(value, "NONE") != 0) {
                    passive = value;
                    hasPassive = 1;
                }
            } else if (strcmp(key, "Passive Value") == 0) {
                passiveValue = atoi(value);
            }
        }

        if (end)
            start = end + 1;
        else
            break;
    }

    if (isEquipped) {
        if (hasPassive) {
            printf("\e[32m[%d] %s\e[0m \e[35m(Passive: %d%% %s)\e[0m \e[33m(EQUIPPED)\e[0m\n", pos, name, passiveValue, passive);
        } else {
            printf("\e[32m[%d] %s\e[0m \e[33m(EQUIPPED)\e[0m\n", pos, name);
        }
    } else {
        if (hasPassive) {
            printf("[%d] %s (Passive: %d%% %s)\n", pos, name, passiveValue, passive);
        } else {
            printf("[%d] %s\n", pos, name);
        }
    }
}
int get_inventory(int sockfd) {
    char buffer[1024];

    char *action = "inventory\n";
    int len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;

    int buflen = recv(sockfd, buffer, sizeof(buffer), 0);
    if (buflen <= 0) return -1;
    buffer[buflen - 1] = '\0';

    printf("==== YOUR INVENTORY ====\n");

    char *token = strtok(buffer, ";");
    int pos = 1;
    while (token != NULL) {
        display_inventory(token, pos++);
        token = strtok(NULL, ";");
    }
    printf("\n");

    change_weapon(sockfd, pos - 1);
}

int display_weapon(char *buffer, int pos) {
    int price = 0, damage = 0, hasPassive = 0, passiveValue = 0;
    char *name = NULL, *passive = NULL;

    char *start = buffer;
    while (*start) {
        char *end = strchr(start, ':');
        if (end) *end = '\0';

        char *sep = strchr(start, '=');
        if (sep) {
            *sep = '\0';
            char *key = start;
            char *value = sep + 1;

            if (strcmp(key, "Name") == 0) {
                name = value;
            } else if (strcmp(key, "Price") == 0) {
                price = atoi(value);
            } else if (strcmp(key, "Damage") == 0) {
                damage = atoi(value);
            } else if (strcmp(key, "Passive") == 0) {
                if (strcmp(value, "NONE") != 0) {
                    passive = value;
                    hasPassive = 1;
                }
            } else if (strcmp(key, "Passive Value") == 0) {
                passiveValue = atoi(value);
            }
        }
        if (end)
            start = end + 1;
        else
            break;
    }

    if (hasPassive) {
        printf("\e[32m[%d] %s\e[0m - Price: \e[33m%d gold\e[0m, Damage: \e[31m%d\e[0m \e[35m(Passive: %d%% %s)\e[0m\n", pos, name, price, damage, passiveValue, passive);
    } else {
        printf("\e[32m[%d] %s\e[0m - Price: \e[33m%d gold\e[0m, Damage: \e[31m%d\e[0m\n", pos, name, price, damage);
    }

    return 0;
}
int get_weapons(int sockfd) {
    char buffer[1024];

    char *action = "weapons\n";
    int len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;

    int buflen = recv(sockfd, buffer, sizeof(buffer), 0);
    if (buflen <= 0) return -1;
    buffer[buflen - 1] = '\0';

    printf("==== WEAPONS SHOP ====\n");

    char *token = strtok(buffer, ";");
    int pos = 1;
    while (token != NULL) {
        display_weapon(token, pos++);
        token = strtok(NULL, ";");
    }
    printf("\n");
    buy_weapon(sockfd, pos - 1);
}

int buy_weapon(int sockfd, int available) {
    int opt;
    printf("Enter weapon number to buy [1-%d] (0 to cancel): ", available);
    scanf("%d", &opt);
    cleanline();

    if (opt < 0 || opt > available) {
        printf("Invalid option. Please try again.\n");
        return buy_weapon(sockfd, available);
    }

    if (opt == 0) return 0;

    opt -= 1;

    char action[32];
    int len;

    snprintf(action, sizeof(action), "buy\n");
    len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;
    sleep(0.01);
    snprintf(action, sizeof(action), "%d\n", opt);
    len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;
}

int change_weapon(int sockfd, int available) {
    int opt;
    printf("Enter weapon number to change [1-%d] (0 to cancel): ", available);
    scanf("%d", &opt);
    cleanline();

    if (opt < 0 || opt > available) {
        printf("Invalid option. Please try again.\n");
        return change_weapon(sockfd, available);
    }

    if (opt == 0) return 0;

    opt -= 1;

    char action[32];
    int len;

    snprintf(action, sizeof(action), "change\n");
    len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;
    sleep(0.01);
    snprintf(action, sizeof(action), "%d\n", opt);
    len = strlen(action);
    if (send(sockfd, action, len, 0) != len) return -1;
}