#include <stdlib.h>

struct Inventory {
    struct Weapon *weapons;
    int capacity;
    int size;
};

struct Player {
    int gold;
    int kills;

    struct Inventory inventory;
    int equippedWeapon;
};

enum WeaponPassive {
    NONE,
    CRITICAL,
    INSTANTKILL,
};

const char *WeaponPassiveStr[] = {
    "NONE",
    "Critical",
    "Instant Kill",
};

struct Weapon {
    char *name;
    int price;
    int damage;

    int hasPassive;
    int passivePercentage;
    enum WeaponPassive passiveType;
};

struct Weapon weapons[5] = {
    {"Terra Blade", 50, 10, 0, 0, NONE},
    {"Flint & Steel", 150, 25, 0, 0, NONE},
    {"Kitchen Knife", 200, 35, 0, 0, NONE},
    {"Staff of Light", 120, 20, 1, 10, CRITICAL},
    {"Terra Blade", 50, 10, 1, 30, INSTANTKILL},
};

int available_weapons(int sock) {
    char buffer[1024];
    int len = 0;
    int size = sizeof(weapons) / sizeof(weapons[0]);
    for (int i = 0; i < size; i++) {
        struct Weapon *weapon = &weapons[i];
        len += snprintf(buffer + len, sizeof(buffer) - len,
                        "Name=%s:Price=%d:Damage=%d:Passive=%s:Passive Value=%d;",
                        weapon->name,
                        weapon->price,
                        weapon->damage,
                        WeaponPassiveStr[weapon->passiveType],
                        weapon->passivePercentage);
    }
    buffer[len - 1] = '\0';

    if (send(sock, buffer, len, 0) != len) return -1;
    return 0;
}

int buy_weapon(int sock, struct Player *player) {
    char buffer[128];
    int len = recv(sock, buffer, sizeof(buffer), 0);
    if (len <= 0) return -1;
    buffer[len - 1] = '\0';

    int weaponIndex = atoi(buffer);
    if (weaponIndex < 0 || weaponIndex >= sizeof(weapons) / sizeof(weapons[0])) return -1;

    struct Weapon *weapon = &weapons[weaponIndex];
    if (player->gold < weapon->price) {
        snprintf(buffer, sizeof(buffer), "Not enough gold\n");
        send(sock, buffer, strlen(buffer), 0);
        return -1;
    }
    player->gold -= weapon->price;
    player->inventory.weapons[player->inventory.size++] = *weapon;
}