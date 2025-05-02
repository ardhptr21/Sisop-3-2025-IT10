#include "shm_common.h"
#include <signal.h>
#include <unistd.h>

int shm_id = -1;
struct SystemData *system_data;

// SOAL B: Menampilkan Semua Hunter
void tampilkan_semua_hunter() {
    printf("=== Daftar Hunter ===\n");
    for (int i = 0; i < system_data->num_hunters; i++) {
        struct Hunter h = system_data->hunters[i];
        printf("Username: %s | Lv: %d | EXP: %d | ATK: %d | HP: %d | DEF: %d | Status: %s\n",
            h.username, h.level, h.exp, h.atk, h.hp, h.def, h.banned ? "Banned" : "Active");
    }
}

// SOAL D: Generate Dungeon
void generate_dungeon() {
    if (system_data->num_dungeons >= MAX_DUNGEONS) {
        printf("Jumlah dungeon maksimal tercapai!\n");
        return;
    }

    // List of dungeons
    const char* dungeon_names[] = {
        "Double Dungeon", "Demon Castle", "Pyramid Dungeon", "Red Gate Dungeon", 
        "Hunters Guild Dungeon", "Busan A-Rank Dungeon", "Insects Dungeon", 
        "Goblins Dungeon", "D-Rank Dungeon", "Gwanak Mountain Dungeon", 
        "Hapjeong Subway Station Dungeon"
    };

    // Define the stats for each dungeon
    const int min_levels[] = {1, 2, 3, 1, 2, 4, 1, 1, 2, 3, 4};
    const int exps[] = {150, 200, 250, 180, 220, 300, 160, 170, 190, 210, 250};
    const int atks[] = {130, 150, 140, 120, 130, 150, 140, 125, 130, 135, 145};
    const int hps[] = {60, 80, 70, 65, 75, 90, 70, 65, 70, 85, 80};
    const int defs[] = {30, 40, 35, 30, 35, 45, 40, 30, 35, 40, 45};

    // Use the index of the current dungeon in the array
    int idx = system_data->num_dungeons;
    struct Dungeon *d = &system_data->dungeons[idx];
    
    // Assign the name from the predefined list
    strcpy(d->name, dungeon_names[idx]);
    d->min_level = min_levels[idx];
    d->exp = exps[idx];
    d->atk = atks[idx];
    d->hp = hps[idx];
    d->def = defs[idx];

    // Create a unique key for each dungeon
    d->shm_key = ftok("/tmp", d->name[0]);
    system_data->num_dungeons++;

    printf("Dungeon generated!\n");
    printf("Name: %s\n", d->name);
    printf("Minimum Level: %d\n", d->min_level);
}

// SOAL D: Tampilkan Semua Dungeon
void tampilkan_semua_dungeon() {
    printf("== DUNGEON INFO ==\n");
    for (int i = 0; i < system_data->num_dungeons; i++) {
        struct Dungeon d = system_data->dungeons[i];
        printf("[Dungeon %d]\n", i + 1);
        printf("Name: %s\n", d.name);
        printf("Minimum Level: %d\n", d.min_level);
        printf("EXP Reward: %d\n", d.exp);
        printf("ATK: %d\n", d.atk);
        printf("HP: %d\n", d.hp);
        printf("DEF: %d\n", d.def);
        printf("Key: %d\n", d.shm_key);
    }
}

// SOAL G: Fitur duel antar hunter
void duel() {
    char user1[50], user2[50];
    printf("Masukkan username hunter 1: ");
    scanf("%s", user1);
    printf("Masukkan username hunter 2: ");
    scanf("%s", user2);

    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user1) == 0) idx1 = i;
        if (strcmp(system_data->hunters[i].username, user2) == 0) idx2 = i;
    }

    if (idx1 == -1 || idx2 == -1) {
        printf("Hunter tidak ditemukan.\n");
        return;
    }

    struct Hunter *h1 = &system_data->hunters[idx1];
    struct Hunter *h2 = &system_data->hunters[idx2];

    int total1 = h1->atk + h1->hp + h1->def;
    int total2 = h2->atk + h2->hp + h2->def;

    if (total1 == total2) {
        printf("Pertarungan imbang. Tidak ada perubahan.\n");
        return;
    }

    struct Hunter *winner = (total1 > total2) ? h1 : h2;
    struct Hunter *loser = (total1 > total2) ? h2 : h1;
    key_t loser_key = loser->shm_key;

    winner->atk += loser->atk;
    winner->hp += loser->hp;
    winner->def += loser->def;

    // hapus loser
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, loser->username) == 0) {
            for (int j = i; j < system_data->num_hunters - 1; j++)
                system_data->hunters[j] = system_data->hunters[j + 1];
            system_data->num_hunters--;
            break;
        }
    }

    shmctl(shmget(loser_key, sizeof(struct Hunter), 0666), IPC_RMID, NULL);
    printf("Pemenang: %s. Stats musuh telah diambil.\n", winner->username);
}

// SOAL H: Ban Hunter
void ban_hunter() {
    char user[50];
    printf("Masukkan username yang ingin diban: ");
    scanf("%s", user);
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user) == 0) {
            system_data->hunters[i].banned = 1;
            printf("%s telah diban.\n", user);
            return;
        }
    }
    printf("Hunter tidak ditemukan.\n");
}

// SOAL I: Reset Hunter ke stats awal
void reset_hunter() {
    char user[50];
    printf("Masukkan username yang ingin direset: ");
    scanf("%s", user);
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user) == 0) {
            system_data->hunters[i].level = 1;
            system_data->hunters[i].exp = 0;
            system_data->hunters[i].atk = 10;
            system_data->hunters[i].hp = 100;
            system_data->hunters[i].def = 5;
            printf("%s telah direset.\n", user);
            return;
        }
    }
    printf("Hunter tidak ditemukan.\n");
}

// SOAL L: Unban Hunter
void unban_hunter() {
    char user[50];
    printf("Masukkan username yang ingin di-unban: ");
    scanf("%s", user);
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user) == 0) {
            system_data->hunters[i].banned = 0;
            printf("%s telah di-unban.\n", user);
            return;
        }
    }
    printf("Hunter tidak ditemukan.\n");
}

// SOAL K: Hapus semua shared memory saat exit
void sigint_handler(int sig) {
    printf("\nSystem shutting down...\n");

    for (int i = 0; i < system_data->num_hunters; i++) {
        shmctl(shmget(system_data->hunters[i].shm_key, sizeof(struct Hunter), 0666), IPC_RMID, NULL);
    }
    for (int i = 0; i < system_data->num_dungeons; i++) {
        shmctl(shmget(system_data->dungeons[i].shm_key, sizeof(struct Dungeon), 0666), IPC_RMID, NULL);
    }

    shmdt(system_data);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("Semua shared memory telah dihapus.\n");
    exit(0);
}

int main() {
    signal(SIGINT, sigint_handler);
    key_t key = get_system_key();
    shm_id = shmget(key, sizeof(struct SystemData), IPC_CREAT | 0666);
    system_data = shmat(shm_id, NULL, 0);
    if (system_data->num_hunters == 0 && system_data->num_dungeons == 0) {
        system_data->num_hunters = 0;
        system_data->num_dungeons = 0;
        system_data->current_notification_index = 0;
    }

    srand(time(NULL));
    printf("System aktif!\n");

    int cmd;
    while (1) {
        printf("== SYSTEM MENU ==\n");  // Soal C (Menu memantau Hunter)
        printf("1. Hunter Info\n");
        printf("2. Dungeon Info\n");
        printf("3. Generate Dungeon\n");
        printf("4. Ban Hunter\n");
        printf("5. Reset Hunter\n");
        printf("6. Exit\n");
        printf("Choice: ");

        scanf("%d", &cmd);
        if (cmd == 1) tampilkan_semua_hunter();         // SOAL 
        else if (cmd == 3) generate_dungeon();          // Soal D
        else if (cmd == 2) tampilkan_semua_dungeon();   // Soal E
        else if (cmd == 4) duel();
        else if (cmd == 5) ban_hunter();
        else if (cmd == 6) reset_hunter();
        else if (cmd == 7) unban_hunter();              // SOAL D
        else if (cmd == 99) raise(SIGINT);
    }

    return 0;
}